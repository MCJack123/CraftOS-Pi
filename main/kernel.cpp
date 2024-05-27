#include "kernel.hpp"
#include "module/terminal.hpp"
#include "driver/hid.hpp"
#include <ff.h>
#include <circle_glue.h>
#include <circle/net/syslogdaemon.h>

extern void machine_main(void*);
extern void speaker_init(CSoundBaseDevice *m_Sound);

CKernel* CKernel::kernel = nullptr;

CKernel::CKernel (void):
    m_Framebuffer (640, 400, 8, 640, 400),
    m_Timer (&m_Interrupt),
    m_Logger (m_Options.GetLogLevel (), &m_Timer),
    m_USBHCI (&m_Interrupt, &m_Timer, TRUE),
    m_EMMC (&m_Interrupt, &m_Timer, &m_ActLED),
    m_Console (&m_Serial, &m_Serial),
    mpPartitionName ("SD:"),
    m_DeviceType (m_Options.GetAppOptionDecimal("nettype", 0) == 0 ? NetDeviceTypeWLAN : NetDeviceTypeEthernet),
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
    m_Sound(&m_Interrupt, 48000, 1024)
{
    CKernel::kernel = this;
}

CKernel::~CKernel (void) {

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
        if (logY >= TERM_HEIGHT) {
            terminal_scroll(1, 0xF0);
            logY--;
        }
        terminal_write_string(0, logY++, message, colors);
    }
    terminal_task(); // force render
}

static void panicTerm() {
    terminal_task(); // force render
}

boolean CKernel::Initialize (void) {
    boolean bOK = TRUE;

    if (bOK) { bOK = m_Serial.Initialize(115200); }

    m_Serial.Write("early init\n", sizeof("early init\n"));

    if (bOK) { bOK = m_Logger.Initialize(&m_Serial); }

    if (bOK) {
        for (int i = 0; i < 16; i++) m_Framebuffer.SetPalette32(i, defaultPalette[i]);
        bOK = m_Framebuffer.Initialize();
    }

    framebuffer = (uint8_t*)(uintptr_t)m_Framebuffer.GetBuffer();
    memset(framebuffer, 15, m_Framebuffer.GetSize());

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

    if (!m_Console.Initialize()) { return false; }

    // Initialize newlib stdio with a reference to Circle's console
    CGlueStdioInit(m_Console);

    terminal_init();
    //terminal_write_literal(0, 0, "Starting CraftOS-Pi...", 0xF0);
    m_Logger.Write("main", LogNotice, "Terminal framebuffer at %p, size %u", framebuffer, m_Framebuffer.GetSize());

    hid_init();

    if (m_DeviceType == NetDeviceTypeWLAN)
    {
        if (!m_WLAN.Initialize ())
        {
            terminal_write_literal(0, 0, "Failed to initialize WLAN", 0xE0);
            return false;
        }
    }

    if (!m_Net.Initialize (false))
    {
        terminal_write_literal(0, 0, "Failed to initialize network", 0xE0);
        return false;
    }

    if (m_DeviceType == NetDeviceTypeWLAN)
    {
        if (!m_WPASupplicant.Initialize ())
        {
            terminal_write_literal(0, 0, "Failed to initialize WPA supplicant", 0xE0);
            return false;
        }
    }

    speaker_init(&m_Sound);

    //CIPAddress ip((const u8*)"192.168.7.78");
    //new CSysLogDaemon(&m_Net, ip);

    //m_Logger.RegisterEventNotificationHandler(logTerm);
    //m_Logger.RegisterPanicHandler(panicTerm);

    m_Logger.Write("main", LogNotice, "Compile time: " __DATE__ " " __TIME__);

    return true;
}

TShutdownMode CKernel::Run (void) {
    machine_main((void*)0);
    return TShutdownMode::ShutdownHalt;
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
