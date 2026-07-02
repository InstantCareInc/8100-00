#include <msp430.h>

/*
 * TI CCS startup hook (replaces IAR __low_level_init).
 *
 * Must be retained at -O2 or the linker uses the RTS stub that does not
 * stop the watchdog. With CINIT_HOLD_WDT enabled in .cproject, the linker
 * holds WDT during .cinit; this hook stops WDT before .cinit so the large
 * RAM init on MSP430F1611 does not expire the watchdog.
 *
 * Return non-zero: user handled WDT hold; cinit still proceeds (MSP430 RTS 21.6).
 * WDTHOLD remains set until cpu_initCpu() and then until cpu_wdt_init() in main().
 */
#pragma RETAIN(_system_pre_init)

int _system_pre_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;
    return 1;
}
