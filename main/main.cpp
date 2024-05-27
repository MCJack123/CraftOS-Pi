#include "kernel.hpp"
#include <circle/startup.h>

extern "C" {
int getentropy(void* buffer, size_t length) {
    // TODO
    return 0;
}
}

int main (void)
{
    CKernel Kernel;
    if (!Kernel.Initialize ())
    {
        halt ();
        return EXIT_HALT;
    }

    TShutdownMode ShutdownMode = Kernel.Run ();

    switch (ShutdownMode)
    {
    case ShutdownReboot:
        reboot ();
        return EXIT_REBOOT;

    case ShutdownHalt:
    default:
        halt ();
        return EXIT_HALT;
    }
}

