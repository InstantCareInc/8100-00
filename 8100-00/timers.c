// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                timers.c
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains all timer related initialization and usage functions.
//
// Design Notes:
// -------------
//
// TimerA is used
//
// TimerB is used
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/07/21 23:29:20 $
// This file based on --> $Revision: 1.8 $
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Header files
//
#include "cpu.h"
#include "wabs.h"
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Private Constants
//

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Private Objects
//
// (none)
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Private module functions (prototyped as 'static')
//
// (none)
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public module variables

U32 timer_loResTimer[TIMER_MAX_LO_RES_TIMER];
U8  timer_hop_timer_expired;          // Flag for hop timer expiration
U16 timerB_counter;                   // Counter used for maintaining the 16x a second timer
U8  timer_sequenceLeds = 0;
U16 timer_freerunning;

U16 timer_value;        // Used by the ElapsedTimer code
U16 timer_counter = 0;

#if 0
static int timerA_init(void)
{
    TACTL          = 0;                         // Stop Timer A
    TACCTL0        = 0x80;
    TACCTL1        = 0;
    TACCTL2        = 0;
    TACCR0         = 0x20;
    TACTL          = 0x1F4;
    return (TRUE);
}
#endif

static int timerB_init(void)
{
    // Initialize all our low-resolution timers to stopped

    memset(timer_loResTimer, 0, sizeof(timer_loResTimer));

    TBCTL          = 0;                         // Stop Timer B
    TBCCR0         = TIMER_LO_RES_TICKS;
    TBCCTL0        = CCIE;
    TBCCTL1        = 0;
    TBCCTL2        = 0;
    TBCCTL3        = 0;
    TBCCTL4        = 0;
    TBCCTL5        = 0;
    TBCCTL6        = 0;
// Timer B = ACLK / 8, Count up, 16-bit, individual, Clear
    TBCTL          = (MC_1 | TBSSEL_1 | ID_3 | TBCLR);
//    timerB_counter = TIMER_LO_RES_TICKS;
    return (TRUE);
}

void timer_init(void)
{
    _DINT();
//    timerA_init();
    timerB_init();
    _EINT();
}

extern __interrupt void timerB_isr(void);
#pragma vector=TIMERB0_VECTOR
__interrupt void timerB_isr(void)
{
    int i;

    TBCTL |= TBCLR;

    timer_freerunning += TIMER_LO_RES_TICKS;

    timer_counter += 1;                     // Used by elapsedTimer

    // Here with 128x/second timer

    // Decrement all our Low-resolution timers
    i = 0;
    while (i < TIMER_MAX_LO_RES_TIMER)
    {
        if (timer_loResTimer[i])
        {
            timer_loResTimer[i] -= 1;
// bh - v2.11 - DO NOT truncate this process just because one timer expires!
//            if (timer_loResTimer[i] == 0)
//                LPM1_EXIT;                  // Restart the main loop if this timer expired
        }
        ++i;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_delayHiRes
//
// Description:
// ------------
// Blocking function that allows caller to get hiRes timebased delays.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void timer_delayHiRes(U16 hiResTicks)
{
    while (--hiResTicks)
    {
        _NOP();
    }

}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_setLoResTimer
//
// Description:
// ------------
// Allows caller to set and start the millisecond timebase timer.
//
// Design Notes:
// -------------
// Caller's must convert real-time (ms) to loResTicks.
// _____________________________________________________________________________
//
void timer_setLoResTimer(int timer, U32 loResTicks)
{
    if (timer < TIMER_MAX_LO_RES_TIMER)
    {
        if (loResTicks) {
            timer_loResTimer[timer] = loResTicks;
        }
        else {
            timer_loResTimer[timer] = 1;        // If set to non-zero, force a minimum of 1 tick
        }
    }
    return;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_extendLoResTimer
//
// Description:
// ------------
// Allows caller to extend the time on the millisecond timebase timer.
//
// Design Notes:
// -------------
// Caller's must convert real-time (ms) to loResTicks.
//
// This routine will never set the timer to a value larger than 125%
// of the specified value, and may only set it to as low as 75% of the
// specified value.  This is intentionally sloppy, as this is used to
// extend the keepalive interval after an alert message is sent, and
// the sloppiness prevents the alerts from causing all the units to
// synchronize their keepalive schedules.
// _____________________________________________________________________________
//
void timer_extendLoResTimer(int timer, U16 loResTicks)
{
    if (timer < TIMER_MAX_LO_RES_TIMER)
    {
        if (timer_loResTimer[timer] < (loResTicks / 4)) {
            timer_loResTimer[timer] += loResTicks;
        }
        else if (timer_loResTimer[timer] < (loResTicks / 2)) {
            timer_loResTimer[timer] += (loResTicks / 2);
        }
        else if (timer_loResTimer[timer] < (loResTicks - (loResTicks / 4))) {
            timer_loResTimer[timer] += (loResTicks / 4);
        }
    }
    return;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_chkLoResTimerExpired
//3
// Description:
// ------------
// Allows caller to determine when the millisecond timer has expired from the
// the last set call. Returns TRUE when timeout has expired.
//
// Design Notes:
// -------------
// The millisecond timer expiration is latching so that caller's can detect the
// expiration event and then take positive action to clear it by setting a new
// timeout value.
// _____________________________________________________________________________
//
int timer_chkLoResTimerExpired(int timer)
{
    if (timer < TIMER_MAX_LO_RES_TIMER) {
        return (0 == timer_loResTimer[timer]);
    }
    return 0;    // On bad timer return not expired.
}
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_delayLoRes
//
// Description:
// ------------
// Blocking function that allows caller to get millisecond timebased delays.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void timer_delayLoRes(U16 loResTicks)
{
    timer_setLoResTimer(TIMER_LO_RES_DELAY, loResTicks);
    while (!timer_chkLoResTimerExpired(TIMER_LO_RES_DELAY)) {
        ;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_sleep
//
// Description:
// ------------
// Puts the CPU to sleep blocking the caller's program until <numSeconds>
// elapses.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void timer_sleep(U16 numSeconds)
{
    timer_delayLoRes(numSeconds * TIMER_LO_RES_TICKS_PER_SECOND);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_resetTimer
//
// Description:
// ------------
// Function to reset the Elapsed timer
//
// Design Notes:
// -------------
// This timer will wrap at 64k milliseconds!
// _____________________________________________________________________________
//
void timer_ResetTimer(void)
{
    timer_counter = 0;
    timer_value   = TBR;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  timer_ElapsedTimer
//
// Description:
// ------------
// Returns the number of milliseconds since 'timer_ResetTimer' was last called.
//
// Design Notes:
// -------------
// Will wrap at 64k milliseconds!
// _____________________________________________________________________________
//
U16 timer_ElapsedTimer(void)
{
    U16 time;
    U16 current_ticks;

    current_ticks = TBR;
    time          = timer_counter * TIMER_LO_RES_TIME_MS;

    if (current_ticks != timer_value)
    {
        if (current_ticks > timer_value) {
            time += ((current_ticks - timer_value) / (TIMER_LO_RES_TICKS / TIMER_LO_RES_TIME_MS));
        }
        else {
            time += ((TIMER_LO_RES_TICKS - timer_value + current_ticks) / (TIMER_LO_RES_TICKS / TIMER_LO_RES_TIME_MS));
        }
    }

    return (time);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: timers.c,v $
// Revision 1.8  2006/07/21 23:29:20  tgoltz
// Tweak radio timing and fix bugs in the repeater feature.
//
// Revision 1.7  2006/06/02 17:22:36  tgoltz
// Improve host WABS RS-485 code so it recovers from data errors within a single message.
//
// Revision 1.6  2006/05/31 22:20:16  tgoltz
// Additional tweaking on radio timing parameters
//
// Revision 1.5  2006/05/31 19:25:55  tgoltz
// Fix timing bugs with keepalives and transmit holdoffs.
//
// Revision 1.4  2006/05/26 21:24:40  tgoltz
// Fix bugs in radio timing and timeouts
//
// Revision 1.3  2006/03/30 23:37:33  tgoltz
// Coding continued.
//
// Revision 1.2  2006/03/06 17:24:07  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// _____________________________________________________________________________
