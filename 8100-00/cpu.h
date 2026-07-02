#ifndef cpu_H_INCLUDED
#define cpu_H_INCLUDED
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2005
// _____________________________________________________________________________
//
// File:                cpu.h
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains CPU-dependent defintions for the TI MSP430 processor
//
// Design Notes:
// -------------
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/08/10 23:24:20 $
// This file based on --> $Revision: 1.4 $
// _____________________________________________________________________________
// _____________________________________________________________________________
//

#include <msp430.h>
#include "msp430_bitaccess.h"

/*
 * Conditional Compile Control
 */
#define RADIO_WATCHDOG_ERROR      (0)     /* Radio Inialization Watchdog failure enable
                                           * 0 => Normal Radio Inialization Watchdog failure level
                                           *      250 => 25% radio quality level
                                           *             25% failure level
                                           * 1 => Forced Radio Inialization Watchdog failure enable
                                           *      1500 => 150% radio quality level
                                           *              Continual failure trigger
                                           * Release setting = 0
                                           */

// Unlike the PIC, the TI doesn't have a dedicated boolean address space.
// To make this as portable across microprocessor architectures as possible
// define some macros for manipulating bit values as efficiently as possible.

#define CPU_SET_BIT(bit_class, bitlabel, bitvalue)  (bitvalue != 0 ? (bit_class ## _bits |= bit_class ## _bit_ ## bitlabel) : (bit_class ## _bits &= ~ bit_class ## _bit_ ## bitlabel) )
#define CPU_TST_BIT(bit_class, bitlabel)  ((bit_class ## _bits & bit_class ## _bit_ ## bitlabel) != 0)

// Platform independent types. U and S indicate Unsigned or Signed type followed
// by the number of bits in the type, e.g. S16 is a signed 16-bit type.
typedef          char  Bool;            // TI430 compilers don't have a bit type
typedef unsigned char  U8;
typedef   signed char  S8;
typedef unsigned short U16;
typedef   signed short S16;
typedef unsigned long  U32;
typedef   signed long  S32;

//
// Hardware related definitions
//

#define CPU_IO_BIT_HIGH 1
#define CPU_IO_BIT_LOW  0

#define CPU_DATA_OUTPUT 0
#define CPU_DATA_INPUT  1

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public Constants
//

#define  XT09_REV_B               (1)

#if XT09_REV_B
#define CPU_XTAL_FREQ_HZ          (7999728)
#else
#define CPU_XTAL_FREQ_HZ          (8388608)
#endif

#define CPU_ACLK_FREQ_HZ          CPU_XTAL_FREQ_HZ/8
#define CPU_EXT_ACLK_FREQ_HZ      CPU_XTAL_FREQ_HZ/8

#define CPU_SMCLK_FREQ_HZ         CPU_XTAL_FREQ_HZ/1
#define CPU_MCLK_FREQ_HZ          CPU_XTAL_FREQ_HZ/1

// When we're running off the mHz crystal, we set Timer A to divide
// the input clock by eight in order to improve the range of the timer.
// This isn't required (or desired) when using the 32kHz crystal.

#define CPU_NUM_100MS_PER_SECOND    10
#define CPU_TIME_100MS              (CPU_ACLK_FREQ_HZ / CPU_NUM_100MS_PER_SECOND / 8)

// #define TACTL_VALUE (TASSEL0 | TACLR)                   // ACLK, clear TAR, clock/1
// #define TACTL_VALUE (TASSEL0 | TACLR | ID0 | ID1)    // ACLK, clear TAR, clock/8
// #define TACTL_RUN   ((TACTL_VALUE)|MC0)                 // Timer A running in count up mode

//
// Define macros for processor states
//

#define CPU_SLEEP_ENTER    LPM1
#define CPU_SLEEP_EXIT     LPM1_EXIT
#define CPU_POWEROFF_ENTER LPM4
#define CPU_POWEROFF_EXIT  LPM4_EXIT

//
// Definitions for serial port
//
// Note that when using the 32kHz crystal, ACLK is too slow to run
// 19200 baud.  You must use the 1mHz SMCLK in that case.
// Unfortunately, the onboard resistor and the DCO are notoriously
// erratic, and using a DCO-derived clock for async serial has proven
// highly problematic.  It *may* be possible with some combination of
// an external resistor and/or active DCO calibration against the 32kHz
// crystal.  I've never put in the time to figure out how to make this
// work well, so by default, use a MHZ crystal if you need 19.2k baud.
//
// Obviously, using the DCO means that the UART stops working in LPM2 or LPM3.
// With the mHz crystals, we can use ACLK for the UART clock source, which
// means that the UART will continue to work fine in LPM2/LPM3.
//

#define SERIAL_IN_FLAG_INITIALIZED 0x80

#define putch(c) putchar(c)

// Serial port hardware flow control definitions
// CTS flow control MUST be used
// RTS flow control has not been fully debugged.
#define SERIAL_A_CTS_FLOW_CTRL                  1
#define SERIAL_A_RTS_FLOW_CTRL                  1

/// #define SERIAL_A_BAUD_RATE 38400
#define SERIAL_A_BAUD_RATE 9600

/* The maximum RF packet length is: 
 * 238 bytes ==> ((num_of_payloads * payload_size) + size_of_header) + checksum
 *           ==> ((13 * 17) + 16) + 1 
 * Hence:
 * -> I_BUFFER equates to (238 bytes + 18) * 1.5; for better buffer perfomance
 * -> O_BUFFER equates to (238 bytes + 18) * 1.0; for extreme cases
*/
#define SERIAL_A_IN_RING_SIZE  384        // I_BUFFER (See above.) --^
#define SERIAL_A_OUT_RING_SIZE 256        // O_BUFFER (See above.) --^

// Drop CTS when only this space (1/4th) available in ring
#define SERIAL_A_IN_STOP  (SERIAL_A_IN_RING_SIZE/4)

// Amount of space required (3/4th of the buffer) before raising CTS
#define SERIAL_A_IN_START (SERIAL_A_IN_RING_SIZE - (SERIAL_A_IN_RING_SIZE/4))

#if (SERIAL_A_BAUD_RATE == 2400 && CPU_ACLK_FREQ_HZ == 32768)
    #define SERIAL_A_UBR1_VAL 0
    #define SERIAL_A_UBR0_VAL 0x0D
    #define SERIAL_A_MCTL_VAL 0x6B
#endif
#if (SERIAL_A_BAUD_RATE == 9600 && CPU_ACLK_FREQ_HZ == 32768)
    #define SERIAL_A_UBR1_VAL 0
    #define SERIAL_A_UBR0_VAL 3
    #define SERIAL_A_MCTL_VAL 0x4A
#endif

#ifndef SERIAL_A_UBR0_VAL
    #define SERIAL_A_MCTL_VAL        0
    #define SERIAL_A_UBR1_VAL        ((unsigned char) (((CPU_ACLK_FREQ_HZ / SERIAL_A_BAUD_RATE) - 0) >> 8 & 0xFF))
    #define SERIAL_A_UBR0_VAL        ((unsigned char) (((CPU_ACLK_FREQ_HZ / SERIAL_A_BAUD_RATE) - 0)      & 0xFF))
#endif

//
// Definitions for second serial port
//

//#define SERIAL_B_BAUD_RATE 9600
#define SERIAL_B_BAUD_RATE 19200

/* The maximum RS485 packet length is: 
 * 249 bytes ==> ((num_of_payloads * payload_size) + size_of_header) + checksum
 *           ==> (( 30 * 8) + 8 + 1
 * Hence:
 * -> I_BUFFER equates to (249 bytes + 7) * 1.5; for better buffer perfomance
 * -> O_BUFFER equates to (249 bytes + 7) * 1.0; for extreme cases
*/
#define SERIAL_B_IN_RING_SIZE 384
#define SERIAL_B_OUT_RING_SIZE 256

#if (SERIAL_B_BAUD_RATE == 2400 && CPU_ACLK_FREQ_HZ == 32768)
    #define SERIAL_B_UBR1_VAL 0
    #define SERIAL_B_UBR0_VAL 0x0D
    #define SERIAL_B_MCTL_VAL 0x6B
#endif
#if (SERIAL_B_BAUD_RATE == 9600 && CPU_ACLK_FREQ_HZ == 32768)
    #define SERIAL_B_UBR1_VAL 0
    #define SERIAL_B_UBR0_VAL 3
    #define SERIAL_B_MCTL_VAL 0x4A
#endif

#ifndef SERIAL_B_UBR0_VAL
    #define SERIAL_B_MCTL_VAL        0
    #define SERIAL_B_UBR1_VAL        ((unsigned char) (((CPU_ACLK_FREQ_HZ / SERIAL_B_BAUD_RATE) - 0) >> 8 & 0xFF))
    #define SERIAL_B_UBR0_VAL        ((unsigned char) (((CPU_ACLK_FREQ_HZ / SERIAL_B_BAUD_RATE) - 0)      & 0xFF))
#endif

extern char serialA_ignoreCTS_flag;


// bh - v2.14 - 02/15/07
#define NUM_RECEIVERS_TRACKED        8
#define NUM_TXIDS                    25

#if RADIO_WATCHDOG_ERROR

#define CPU_REMOTE_ERROR_THRESHOLD  1500

#else

#define CPU_REMOTE_ERROR_THRESHOLD  250

#endif

typedef struct
{
    U8        Samples [NUM_RECEIVERS_TRACKED];
    U8        Receiver [NUM_RECEIVERS_TRACKED];
    U16        Average [NUM_RECEIVERS_TRACKED];
} RSSI_DATA;

typedef struct
{
    U32                Txid;
    U16                TruthTablesValue;
    RSSI_DATA        RssiData;
}  RSSI_TXID;




// Serial port Public functions (prototyped here only)

extern unsigned int serialA_OutCtr;
extern int          serialA_OutCount;
extern void serialA_init(void);
extern int  serialA_txBufEmpty(void);
extern void serialA_clrInputBuf(void);
extern int  serialA_rxBufEmpty(void);
extern int  serialA_read(unsigned char *data);
extern int  serialA_read_str(unsigned char *bfr, int size);
extern int  serialA_write(unsigned char data);
extern void serialA_writeN(unsigned char *data, int count);
extern void serialA_write_crlf(void);
extern int  serialA_write_buffer_available(void);
extern void serialA_itoa(unsigned int value);
extern void serialA_restart_output(void);
extern void serialA_flush(void);
extern void serialA_ignoreCTS(int);
extern int serialA_txBufAvail(void);

extern void serialB_init(void);
extern int  serialB_txBufEmpty(void);
extern void serialB_txDoneWait(void);
extern int  serialB_txDone(void);
extern void serialB_clrInputBuf(void);
extern int  serialB_rxBufEmpty(void);
extern int  serialB_read(unsigned char *data);
extern int  serialB_write(unsigned char data);
extern void serialB_writeN(unsigned char *data, int count);
extern void serialB_write_crlf(void);
extern int  serialB_write_buffer_available(void);
extern void serialB_itoa(unsigned int value);


extern void CpuSaveRssiIntoAverage (unsigned char * );
extern void CpuCalculateAverage (unsigned char * );
extern void CpuGetTxidAndTruthTable (unsigned char * );
extern int  CpuMonitorWatchdog (void);




// Hardware watchdog (WDTCTL): reset mode, ACLK, ~32 ms interval at 1 MHz ACLK.
#define CPU_WATCHDOG_TIMER                   1

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public Macros
//
// Helpful in determining number of elements in an array.
#define CPU_NUM_OF(array)       (sizeof (array) / sizeof (array[0]))

//
// CPU crash codes
//

#define CPU_CRASH_TIMER_FAILURE  1


//
// End of CPU crash codes
//

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public Objects
//
#ifdef __LCLINT__
    typedef char bit;
#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public functions (prototyped here only)
//

extern void cpu_ResetCpu(void);
extern void cpu_soft_reset(void);
extern void cpu_initCpu(void);
extern void cpu_Crash(void);
extern void cpu_wdt_hold(void);
extern void cpu_wdt_configure(void);
extern void cpu_wdt_kick(void);
extern void cpu_wdt_init(void);
extern int  cpu_readCfgDIP(void);
extern int  cpu_readNetDIP(void);
extern int  cpu_readSpareDIP(void);
extern int  cpu_readSwitch(void);
extern void cpu_write_led_display(char ch1, char ch2, char ch3, int period);
extern void cpu_compare_switches (void);
extern void cpu_power_up_read_switches (void);
extern void cpu_get_rocker_switches (unsigned char *);
extern void cpu_get_firmware_version (unsigned char *);
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public (Global) variables
//
// (none)
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: cpu.h,v $
// Revision 1.4  2006/08/10 23:24:20  tgoltz
// Update radio initialization so it writes current parameters out to flash when
// necessary.
//
// Revision 1.3  2006/06/29 21:00:04  tgoltz
// Update for GEN2 hardware.
//
// Revision 1.2  2006/05/26 21:24:40  tgoltz
// Fix bugs in radio timing and timeouts
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// Revision 1.4  2005/12/16 20:59:48  tgoltz
// Modify LED handling.
// Add support for pushbutton.
// Add code to stop polling when the master unit's alert list is full.
// Add support for multiple GE units on the RS485.
// Add code to ignore duplicate alerts.
//
// Revision 1.3  2005/12/13 22:57:16  tgoltz
// Many changes leading to first version with end-to-end communications.
//
// Revision 1.2  2005/11/23 17:56:43  tgoltz
// Initial coding completed
//
// Revision 1.1.1.1  2005/11/18 17:59:09  tgoltz
// Initial version of the Lifeline rf_buffer code
//
//
// _____________________________________________________________________________
//
#endif // cpu_H_INCLUDED
