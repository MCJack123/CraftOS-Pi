#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/sched/scheduler.h>
#include <circle/bcmframebuffer.h>
#include <circle/ptrlist.h>
#include <circle/usb/usbhcidevice.h>
#include <SDCard/emmc.h>
#include <circle/input/console.h>
#include <fatfs/ff.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/dnsclient.h>
#include <circle-mbedtls/tlssimplesupport.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/cputhrottle.h>

enum TShutdownMode {
    ShutdownNone,
    ShutdownHalt,
    ShutdownReboot
};

static void logTerm();
class CKernel {
    friend void logTerm();
    friend class USBTask;
public:
    CKernel (void);
    ~CKernel (void);

    boolean Initialize (void);

    TShutdownMode Run (void);

    void LogD(const char * subsystem, const char * fmt, ...);
    void LogI(const char * subsystem, const char * fmt, ...);
    void LogW(const char * subsystem, const char * fmt, ...);
    void LogE(const char * subsystem, const char * fmt, ...);
    void SetLED(bool state);

    void CreateTask(void (*task)(void*), void* arg);
    void SleepMs(unsigned ms);
    void RegisterExitFunction(void (*fn)(void));

    void WritePalette(const uint32_t* palette);

    CDNSClient* DNSClient();
    CircleMbedTLS::CTLSSimpleSupport* TLSSimpleSupport();

    static CKernel* kernel;

private:
    static void TimerHandler (TKernelTimerHandle hTimer,
                              void *pParam, void *pContext);

public:
    uint8_t*                framebuffer;

private:
    CActLED                 m_ActLED;
    CKernelOptions          m_Options;
    int                     m_Width;
    int                     m_Height;
    CDeviceNameService      m_DeviceNameService;
    CBcmFrameBuffer         m_Framebuffer;
    CSerialDevice           m_Serial;
    CExceptionHandler       m_ExceptionHandler;
    CInterruptSystem        m_Interrupt;
    CTimer                  m_Timer;
    CLogger                 m_Logger;
    CScheduler              m_Scheduler;
    CPtrList                m_Tasks;
    CPtrList                m_AtExit;
    CUSBHCIDevice           m_USBHCI;
    CEMMCDevice             m_EMMC;
    FATFS                   m_FileSystem;
    CConsole                m_Console;
    char const             *mpPartitionName;
    TNetDeviceType          m_DeviceType;
    CBcm4343Device          m_WLAN;
    CNetSubSystem           m_Net;
    CWPASupplicant          m_WPASupplicant;
    CDNSClient              m_DNSClient;
    CircleMbedTLS::CTLSSimpleSupport m_TLSSupport;
    CSoundBaseDevice       *m_Sound;
    CCPUThrottle            m_Throttle;
};

#endif