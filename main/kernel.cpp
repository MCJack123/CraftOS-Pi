#include "kernel.hpp"
#include "hid.hpp"
#include "event.hpp"
#include <ff.h>
#include <craftos.h>
#include <circle_glue.h>
#include <circle/net/syslogdaemon.h>

extern uintptr IRQReturnAddress;
extern "C" const luaL_Reg speaker_methods[];
const char * speaker_types[] = {"speaker", NULL};

extern const craftos_func_t funcs;
extern unsigned int craftos_terminal_defaultPalette[16];
extern void speaker_init(CSoundBaseDevice *m_Sound);

CKernel* CKernel::kernel = nullptr;

static const TNetDeviceType deviceTypes[] = {
    NetDeviceTypeWLAN,
    NetDeviceTypeEthernet,
    NetDeviceTypeUnknown
};

CKernel::CKernel (void):
    m_Width(m_Options.GetWidth() == 0 ? 640 : m_Options.GetWidth()),
    m_Height(m_Options.GetHeight() == 0 ? 400 : m_Options.GetHeight()),
    m_Framebuffer (m_Width, m_Height, 8),
    m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer),
    m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
    m_Console (&m_Serial, &m_Serial),
    mpPartitionName ("SD:"),
    m_DeviceType (deviceTypes[m_Options.GetAppOptionDecimal("nettype", 0)]),
    m_WLAN ("SD:/firmware/"),
    m_Net(
        (const u8*)m_Options.GetAppOptionString("ipaddr"),
        (const u8*)m_Options.GetAppOptionString("netmask"),
        (const u8*)m_Options.GetAppOptionString("gateway"), 
        (const u8*)m_Options.GetAppOptionString("dns"),
        DEFAULT_HOSTNAME, m_DeviceType),
    m_WPASupplicant ("SD:/wpa_supplicant.conf"),
    m_DNSClient(&m_Net),
    m_TLSSupport(&m_Net),
    m_Throttle(CPUSpeedMaximum)
{
    CKernel::kernel = this;
    switch (m_Options.GetAppOptionDecimal("snddevice", 1)) {
        case 0: m_Sound = new CPWMSoundBaseDevice(&m_Interrupt, 48000, 1024); break;
        case 1: m_Sound = new CHDMISoundBaseDevice(&m_Interrupt, 48000, 1536); break;
        default: m_Sound = NULL; break;
    }
}

CKernel::~CKernel (void) {

}

static void terminal_task() {
    craftos_terminal_render(CKernel::kernel->machine->term, CKernel::kernel->framebuffer, CKernel::kernel->framebufferPitch, 8, 2, 2);
    uint32_t palette[16];
    for (int i = 0; i < 16; i++) {
        uint32_t c = CKernel::kernel->machine->term->palette[i];
        palette[i] = (c & 0xFF) << 16 | (c & 0xFF00) | (c & 0xFF0000) >> 16 | 0xFF000000;
    }
    CKernel::kernel->WritePalette(palette);
    CKernel::kernel->SetLED(CKernel::kernel->machine->term->blink);
}

static int logY = 0;
static void logTerm() {
    TLogSeverity level;
    char source[LOG_MAX_SOURCE], message[LOG_MAX_MESSAGE];
    time_t time;
    unsigned htime;
    int tz;
    while (CKernel::kernel->m_Logger.ReadEvent(&level, source, message, &time, &htime, &tz)) {
        uint8_t colors;
        switch (level) {
            case LogDebug: colors = 0xF9; break;
            case LogNotice: colors = 0xF0; break;
            case LogWarning: colors = 0xF4; break;
            case LogError: colors = 0xFE; break;
            case LogPanic: colors = 0xF2; break;
        }
        if (logY >= CKernel::kernel->machine->term->height) {
            //craftos_terminal_scroll(1, 0xF0);
            logY--;
        }
        craftos_terminal_write_string(CKernel::kernel->machine->term, 0, logY++, message, colors);
    }
    terminal_task(); // force render
}

static void panicTerm() {
    //craftos_terminal_clear(-1, 0xF0);
    logY = 1;
    logTerm();
    craftos_terminal_write_literal(CKernel::kernel->machine->term, 0, 0, "CraftOS-Pi has crashed!", 0xFE);
    terminal_task(); // force render
}

class USBTask: public CTask {
    CKernel *kernel;

    static void throttleTimer() {
        CKernel::kernel->m_Throttle.Update();
        char buf[CKernel::kernel->machine->term->width];
        memset(buf, 0, CKernel::kernel->machine->term->width);
        sprintf(buf, "%d C, %d Hz, PC=%08x", CKernel::kernel->m_Throttle.GetTemperature(), CKernel::kernel->m_Throttle.GetClockRate(), IRQReturnAddress);
        craftos_terminal_write(CKernel::kernel->machine->term, 0, CKernel::kernel->machine->term->height - 1, buf, CKernel::kernel->machine->term->width, 0xF9);
    }

public:
    USBTask(CKernel *k): kernel(k) {}
    virtual void Run() override {
        //kernel->m_Timer.RegisterPeriodicHandler(USBTask::throttleTimer);
        while (true) {
            if (kernel->m_USBHCI.UpdatePlugAndPlay()) hid_init();
            kernel->m_Scheduler.Yield();
        }
    }
};

boolean CKernel::Initialize (void) {
    boolean bOK = TRUE;

    if (bOK) { bOK = m_Serial.Initialize(115200); }

    m_Serial.Write("early init\n", sizeof("early init\n"));

    if (bOK) { bOK = m_Logger.Initialize(&m_Serial); }

    if (bOK) {
        for (int i = 0; i < 16; i++) m_Framebuffer.SetPalette32(i, craftos_terminal_defaultPalette[i]);
        bOK = m_Framebuffer.Initialize();
    }

    framebuffer = (uint8_t*)(uintptr_t)m_Framebuffer.GetBuffer();
    memset(framebuffer, 15, m_Framebuffer.GetSize());
    framebufferPitch = m_Framebuffer.GetPitch();

    if (bOK) { bOK = m_Interrupt.Initialize(); }

    if (bOK) { bOK = m_Timer.Initialize(); }

    if (!bOK) return false;

    if (!m_EMMC.Initialize()) { return false; }

    char const *partitionName = mpPartitionName;

    if (f_mount(&m_FileSystem, partitionName, 1) != FR_OK) {
        m_Logger.Write("main", LogError, "Cannot mount partition: %s", partitionName);

        return false;
    }

    if (!m_USBHCI.Initialize()) { return false; }

    new USBTask(this);

    if (!m_Console.Initialize()) { return false; }

    // Initialize newlib stdio with a reference to Circle's console
    CGlueStdioInit(m_Console);

    craftos_machine_config config;
    config.id = 0;
    config.label = NULL;
    config.rom_mmfs = NULL;
    config.bios = NULL;
    config.width = m_Width;
    config.height = m_Height;
    config.size_pixel_scale = 2;
    config.base_path = "/";
    if (craftos_init(&funcs) != 0) return false;
    machine = craftos_machine_create(&config);
    if (machine == NULL) return false;

    CTimer::Get()->RegisterPeriodicHandler(terminal_task);
    craftos_terminal_write_literal(machine->term, 0, 0, "Starting CraftOS-Pi...", 0xF0);
    m_Logger.Write("main", LogNotice, "Terminal framebuffer at %p, size %u", framebuffer, m_Framebuffer.GetSize());
    m_Logger.RegisterPanicHandler(panicTerm);
    craftos_machine_peripheral_attach(machine, "left", speaker_types, speaker_methods, NULL);

    hid_init();

    if (m_DeviceType != NetDeviceTypeUnknown) {
        if (m_DeviceType == NetDeviceTypeWLAN)
        {
            if (!m_WLAN.Initialize ())
            {
                craftos_terminal_write_literal(machine->term, 0, 0, "Failed to initialize WLAN", 0xE0);
                terminal_task();
                return false;
            }
        }

        if (!m_Net.Initialize (false))
        {
            craftos_terminal_write_literal(machine->term, 0, 0, "Failed to initialize network", 0xE0);
            terminal_task();
            return false;
        }

        if (m_DeviceType == NetDeviceTypeWLAN)
        {
            if (!m_WPASupplicant.Initialize ())
            {
                craftos_terminal_write_literal(machine->term, 0, 0, "Failed to initialize WPA supplicant", 0xE0);
                terminal_task();
                return false;
            }
        }
    }

    speaker_init(m_Sound);

    //CIPAddress ip((const u8*)"192.168.7.78");
    //new CSysLogDaemon(&m_Net, ip);

    //m_Logger.RegisterEventNotificationHandler(logTerm);

    m_Logger.Write("main", LogNotice, "Compile time: " __DATE__ " " __TIME__);

    return true;
}

TShutdownMode CKernel::Run (void) {
    while (true) {
        craftos_status_t status = craftos_machine_run(machine);
        if (status == CRAFTOS_MACHINE_STATUS_SHUTDOWN) {
            craftos_machine_destroy(machine);
            return TShutdownMode::ShutdownHalt;
        } else if (status == CRAFTOS_MACHINE_STATUS_RESTART) {
            craftos_machine_destroy(machine);
            return TShutdownMode::ShutdownReboot;
        } else if (status == CRAFTOS_MACHINE_STATUS_ERROR) {
            break;
        } else if (status == CRAFTOS_MACHINE_STATUS_YIELD) {
            event_t ev;
            event_wait(&ev);
            switch (ev.type) {
                case EVENT_TYPE_KEY:
                    craftos_event_key(machine, ev.key.keycode, ev.key.repeat);
                    break;
                case EVENT_TYPE_KEY_UP:
                    craftos_event_key_up(machine, ev.key.keycode);
                    break;
                case EVENT_TYPE_CHAR:
                    craftos_event_char(machine, ev.character.c);
                    break;
                case EVENT_TYPE_TIMER: case EVENT_TYPE_ALARM:
                    craftos_event_timer(machine, ev.timer.timerID);
                    break;
                case EVENT_TYPE_DISK: case EVENT_TYPE_DISK_EJECT:
                    craftos_machine_queue_event(machine, ev.type == EVENT_TYPE_DISK ? "disk" : "disk_eject", "z", "right");
                    break;
                case EVENT_TYPE_SPEAKER_AUDIO_EMPTY:
                    craftos_machine_queue_event(machine, "speaker_audio_empty", "z", "left");
                    break;
                case EVENT_TYPE_MODEM_MESSAGE:
                    /*
                    ev.modem.message_fn(L, ev.modem.message_arg);
                    craftos_machine_queue_event(machine, "modem_message", "zHHvf", "back", ev.modem.channel, ev.modem.replyChannel, L, ev.modem.distance);
                    */
                    break;
                case EVENT_TYPE_HTTP_FAILURE:
                    craftos_event_http_failure(machine, ev.http.url, ev.http.err);
                    break;
                case EVENT_TYPE_HTTP_SUCCESS:
                    craftos_event_http_success(machine, ev.http.url, ev.http.handle_arg);
                    break;
                case EVENT_TYPE_HTTP_CHECK:
                case EVENT_TYPE_WEBSOCKET_CLOSED:
                case EVENT_TYPE_WEBSOCKET_FAILURE:
                case EVENT_TYPE_WEBSOCKET_MESSAGE:
                case EVENT_TYPE_WEBSOCKET_SUCCESS:
                    break;
            }
        }
    }
    craftos_machine_destroy(machine);
    return TShutdownMode::ShutdownNone;
}

void CKernel::LogD(const char * subsystem, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    m_Logger.WriteV(subsystem, TLogSeverity::LogDebug, fmt, args);
    va_end(args);
}

void CKernel::LogI(const char * subsystem, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    m_Logger.WriteV(subsystem, TLogSeverity::LogNotice, fmt, args);
    va_end(args);
}

void CKernel::LogW(const char * subsystem, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    m_Logger.WriteV(subsystem, TLogSeverity::LogWarning, fmt, args);
    va_end(args);
}

void CKernel::LogE(const char * subsystem, const char * fmt, ...) {
    va_list args;
    va_start(args, fmt);
    m_Logger.WriteV(subsystem, TLogSeverity::LogError, fmt, args);
    va_end(args);
}

void CKernel::SetLED(bool state) {
    if (state) m_ActLED.On();
    else m_ActLED.Off();
}

class Task: public CTask {
    void (*m_main)(void*);
    void* m_arg;
public:
    Task(void (*fn)(void*), void* arg): m_main(fn), m_arg(arg) {}
    virtual void Run(void) override {
        m_main(m_arg);
    }
};

void CKernel::CreateTask(void (*task)(void*), void* arg) {
    //m_Tasks.InsertAfter(nullptr, new Task(task, arg));
}

void CKernel::SleepMs(unsigned ms) {
    //m_Scheduler.MsSleep(ms);
}

void CKernel::RegisterExitFunction(void (*fn)(void)) {
    //m_AtExit.InsertAfter(nullptr, (void*)fn);
}

void CKernel::WritePalette(const uint32_t* palette) {
    for (int i = 0; i < 16; i++) m_Framebuffer.SetPalette32(i, palette[i]);
    m_Framebuffer.UpdatePalette();
}

CDNSClient* CKernel::DNSClient() {
    return &m_DNSClient;
}

CircleMbedTLS::CTLSSimpleSupport* CKernel::TLSSimpleSupport() {
    return &m_TLSSupport;
}
