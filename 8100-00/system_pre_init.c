#include <msp430.h>

/*
 * TI CCS startup hook (replaces IAR __low_level_init).
 * Must be retained at -O2 or the linker uses the RTS stub that does not
 * stop the watchdog. With CINIT_HOLD_WDT enabled, holding here ensures the
 * post-init watchdog restore keeps WDTHOLD set until cpu_initCpu() runs.
 */
#pragma RETAIN(_system_pre_init)

int _system_pre_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    return 1;
}
