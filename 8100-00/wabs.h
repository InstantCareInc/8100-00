#ifndef H_RF_BUFFER_INCLUDED
#define H_RF_BUFFER_INCLUDED
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                wabs.h
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains external definitions for the Lifeline RF buffer board
//
// Design Notes:
// -------------
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/08/14 20:02:54 $
// This file based on --> $Revision: 1.22 $

/************************************************
 define SCOPE to allow RAM variables to be defined directly or as externs
*************************************************/
#ifdef SCOPE
#undef SCOPE
#endif

#ifdef BODY_WABS
#define SCOPE
#else
#define SCOPE    extern
#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Conditional compilation controls
//

#define CPU_SERIAL_CONSOLE                  (0)   /* Uses the RF serial channel as a communication interface
                                                   * 0 => Normal RF Operation
                                                   * 1 => Disgnostic Serial Interface
                                                   * Release setting = 0
                                                   */

#define RADIO_WATCHDOG_REDUCE               (0)   /* Radio Initialization Watchdog time reduction control
                                                   * 0 => Normal Time period
                                                   *      Host = 24 hours
                                                   *      Remote = 4 hours
                                                   * 1 => Reduced time period
                                                   *      Host = 4 minutes
                                                   *      Remote = 2 minutes
                                                   * Release setting = 0
                                                   */

#define RADIO_WATCHDOG_ENABLE               (1)   /* Radio Initialization Watchdog Enable
                                                   * 0 => Radio Watchdog Disabled
                                                   * 1 => Radio Watchdog Enabled
                                                   * Release setting = 1
                                                   */

#define STATE_TRACE_ENABLE                  (0)   /* Remote/Initialization event/state tracing enable
                                                   * 0 => Remote/Initialization event/state tracing disabled
                                                   * 1 => Remote/Initialization event/state tracing Enabled
                                                   * Release setting = 0
                                                   */

#define SMOKE_SUPERVISOR_REDUCE             (0)   /* Smoke detector status message filter time trigger reduction enable
                                                   * 0 => Standard smoke detector status message filter time
                                                   *      1 hour smoke detector messages disabled
                                                   *      2 minutes smoke detector messages enabled
                                                   * 1 => Reduced smoke detector status message filter time
                                                   *      10 minute smoke detector messages disabled
                                                   *      2 minutes smoke detector messages enabled
                                                   * Release setting = 0
                                                   */

#define ALERT_LIST_SIZE_REDUCE              (0)   /* Alert/status messages recording reduction control
                                                   * 0 => Normal number of alert/status messages
                                                   *      100 message elements
                                                   * 1 => Reduced number of alert/status messages
                                                   *      25 message elements
                                                   * Release setting = 0
                                                   */

#define ALERT_TRACING_ENABLE                (0)   /* Alert/status list entry tracing enable
                                                   * 0 => Alert/status list entry tracing Disabled
                                                   * 1 => Alert/status list entry tracing Enabled
                                                   * Release setting = 0
                                                   */

#define EVENT_TRACING_ENABLE                (0)   /* Event tracing enable
                                                   * 0 => Event tracing Disabled
                                                   * 1 => Event tracing Enabled
                                                   * Release setting = 0
                                                   */

#define IO_TRACE_ENABLE                     (0)    /* I/O operations tracing enable
                                                    * 0 => I/O operations tracing Disabled
                                                    * 1 => I/O operations tracing Enabled
                                                    * Release setting = 0
                                                    */
#define MONTREAL_DEVICE_FILTER_ENABLE       (0)   /* Switch to disable any filtering of Montreal messages
                                                   * 0 => Do normal duplicate filtering on Montreal messages
                                                   * 1 => Do not do duplicate filtering on Montreal messages
                                                   * Release setting = 0
                                                   */

/*
 * Radio transciever conditional compile control macros
 */
#define RADIO_INIT_SPLIT_UP_CONFIG          (1)   /* Send all configuration messages as individual requests in the form "ATxx<CR>"
                                                   * 0 => Send the configuration messages as a single compressed message
                                                   * 1 => Send all configuration messages as individual requests
                                                   * Release setting = 1
                                                   */
#define RADIO_INIT_BYPASS_CONFIG            (0)   /* Bypass configuration process whether parameters are set correctly or not.
                                                   * 0 => Normal configuration of parameters if they are not set correctly
                                                   * 1 => Suppress configuration of parameters whether parameters are not
                                                   *      set correctly or not
                                                   * Release setting = 0
                                                   */
#define RADIO_INIT_FAIL_CONFIG              (0)   /* Force configuration process whether parameters are set correctly or not.
                                                   * 0 => Perform configuration process only when they are not set correctly
                                                   * 1 => Always perform configuration process
                                                   * Release setting = 0
                                                   */
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Header files
//

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#define TRUE  1
#define FALSE 0

#define VERSION_TEST  1           /* Beta test version Flag
                                   * 0 => Release version
                                   * n => Beta version Id
                                   */
#define VERSION_MAJOR 7
#define VERSION_MINOR 0

#define AMBIENT_MAX                    80


// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Data buffer size Constants
//


// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public Constants
//

#define TIMER_LO_RES_RADIO            0         // RS-232 radio data timer
#define TIMER_LO_RES_RS485            1         // RS-485 data timer
#define TIMER_LO_RES_DELAY            2
#define TIMER_LO_RES_OUTPUTA          3
#define TIMER_LO_RES_OUTPUTB          4
#define TIMER_LO_RES_POLLING          5         // RS-485 polling timer
#define TIMER_LO_RES_ACTIVITY         6
#define TIMER_LO_RES_UNIT_DISPLAY     7
#define TIMER_LO_RES_DEBOUNCE         8
#define TIMER_LO_RES_DIGIT            9
#define TIMER_LO_RES_PACING           10
#define TIMER_LO_RES_LED              11
#define TIMER_LO_RES_REPEATER         12
#define TIMER_LO_RES_ALIVE            13
#define TIMER_LO_RES_RETRANSMIT       14
#define TIMER_LO_RES_RADIO_INIT       15
#define TIMER_LO_RES_RADIO_WATCHDOG   16
#define TIMER_LO_RES_SUPERVISOR_LOCK  17
#define TIMER_MAX_LO_RES_TIMER        18

#define TIMER_LO_RES_TICKS_PER_SECOND 128U          // Force this to be unsigned
                                                    // so we can use the full 64k
                                                    // range of the timer - otherwise
                                                    // compiler will start throwing
                                                    // warnings at 32k.
#define TIMER_TENTH_OF_SECOND       13
#define TIMER_150MS                 19
#define TIMER_200MS                 26
#define TIMER_300MS                 38
#define TIMER_400MS                 51
#define TIMER_500MS                 64
#define TIMER_600MS                 77

#define TIMER_LO_RES_TIME_MS       (1000 / TIMER_LO_RES_TICKS_PER_SECOND)
#define TIMER_LO_RES_TICKS         (CPU_ACLK_FREQ_HZ / 8 / TIMER_LO_RES_TICKS_PER_SECOND)        // Timer expires 128 times a second

#if SMOKE_SUPERVISOR_REDUCE == 0

#define TIMER_SUPERVISOR_LOCK_TIME    ((U32) TIMER_500MS * 2 * 60 * 60)
#define TIMER_SUPERVISOR_UNLOCK_TIME  ((U32) TIMER_500MS * 2 * 120)


#define TIMER_SUPERVISOR_UNLOCK_STEP  1
#define TIMER_SUPERVISOR_MAX_STEPS    2

#else

#define TIMER_SUPERVISOR_LOCK_TIME    ((U32) TIMER_500MS * 2 * 60 * 10)
#define TIMER_SUPERVISOR_UNLOCK_TIME  ((U32) TIMER_500MS * 2 * 120)


#define TIMER_SUPERVISOR_UNLOCK_STEP  1
#define TIMER_SUPERVISOR_MAX_STEPS    2

#endif

//
// Radio Watchdog Timeout values
//

#if RADIO_WATCHDOG_REDUCE

#define CPU_HOST_RESET_WAIT        16                                      // Host watchdog - 4 minutes
#define CPU_REMOTE_RESET_WAIT        8                                       // Remote watchdog - 2 minutes

#define TIMEOUT_RADIO_WATCHDOG     (TIMER_LO_RES_TICKS_PER_SECOND * 15)        // 15 seconds

#else

// bh - v5.01 - wait 24 hours between each radio reset on the host unit.
#define CPU_HOST_RESET_WAIT         240
// bh - v5.01 - wait 2 hours between each radio reset on the remote unit.
#define CPU_REMOTE_RESET_WAIT       20

#define TIMEOUT_RADIO_WATCHDOG      (TIMER_LO_RES_TICKS_PER_SECOND * 360)  // 6 minutes

#endif

#define TIMEOUT_HOST_RS485_INPUT   2
//#define TIMEOUT_REMOTE_RS485_INPUT 4
#define TIMEOUT_REMOTE_RS485_INPUT 2                                        // bh - v2.13
#define TIMEOUT_RADIO_INPUT        (TIMER_LO_RES_TICKS_PER_SECOND - (TIMER_LO_RES_TICKS_PER_SECOND / 4))
#define TIMEOUT_RADIO_SHORT_INPUT  15
#define TIMEOUT_RS485_OUTPUT       (TIMER_LO_RES_TICKS_PER_SECOND / 5)
#define TIMEOUT_RS232_OUTPUT       (TIMER_LO_RES_TICKS_PER_SECOND / 2)
#define TIMEOUT_ACTIVITY           (TIMER_LO_RES_TICKS_PER_SECOND / 4)
// bh - v2.12 - shorten the time out for resending
#define TIMEOUT_RETRANSMIT         (TIMER_600MS)          // Timeout for resending an alert if ACK not received.
#define TIMEOUT_HOLDOFF            (TIMER_600MS)      // Time for holdoff following radio activity
// bh - v2.10 - 90 reduced to 16
#define TIMEOUT_KEEPALIVE          (TIMER_LO_RES_TICKS_PER_SECOND * 16)     // sped up Keepalive quantum
#define OLD_TIMEOUT_KEEPALIVE      (TIMER_LO_RES_TICKS_PER_SECOND * 90)     // v2.09 Keepalive quantum

#define TIMEOUT_REPEAT_DELAY     20
#define TIMEOUT_REPEAT_SKEW      (repeater_number + (rand() & 0x03))

#define RANDOM_INTERVAL()    ((rand() % 10) * (TIMER_LO_RES_TICKS_PER_SECOND / 2))
#define RANDOM2_INTERVAL()   ((rand() % 15) * TIMER_TENTH_OF_SECOND)            // BH - V5.00
#define RANDOM3_INTERVAL()   ((rand() % 18) * TIMER_TENTH_OF_SECOND)            // BH - V5.00
#define MAX_TRANSMIT_TRIES    11     // Ideally should be one less than a multiple of three.
//#define MAX_TRANSMIT_TRIES    8     // Ideally should be one less than a multiple of three.
// bh - v2.10 - start hop searching much earlier in the failure cycle
//#define MAX_TRANSMIT_TRIES    5     // Ideally should be one less than a multiple of three.

// bh - v2.10 - retransmit interval reduced
#define RETRANSMIT_VALUE (retransmit_interval + (RANDOM_INTERVAL() / 8))
// bh - v5.00 - holdoff between 40-360ms
#define HOLDOFF_VALUE                ((rand() % 40) + 5)

//
// Values used by the activity_check code...this only runs four times a
// second, so it has to use different time values than the LoRes timers
// above.
//

// bh - v2.10
//#define MANDATORY_ACK_INTERVAL  ((OLD_TIMEOUT_KEEPALIVE / TIMEOUT_ACTIVITY) * 3)   // Force an ACK at least every three keepalives
#define MANDATORY_ACK_INTERVAL  ((TIMEOUT_KEEPALIVE / TIMEOUT_ACTIVITY) * 3)   // Force an ACK at least every three keepalives
#define ACTIVITY_COUNT_RADIO    (((OLD_TIMEOUT_KEEPALIVE / TIMEOUT_ACTIVITY) * 4) + 1)   // Make this three times the keepalive quantum in units of 1/4 second
#define ACTIVITY_COUNT_RS485    64            // Gives a 16-second 'alive' timeout for the GE module polling

#define MAX_VALID_HOST_RS485_ADDRESS    249
#define MAX_VALID_REMOTE_RS485_ADDRESS  31
#define MAX_VALID_RADIO_ADDRESS         256

#define POLL_DIVIDER                    8
//#define POLLING_RATE                    4        // bh - v2.11 - 31.25ms
#define POLLING_RATE                    3        // bh - v2.13 - 23.4375ms

#if ALERT_LIST_SIZE_REDUCE
#define ALERT_LIST_LENGTH           25    // bh - v2.13 - was 350 - put back to 350?
#else
#define ALERT_LIST_LENGTH               100        // bh - v2.13 - was 350 - put back to 350?
#endif
#define REMOTE_STANDARD_ADDRESS     (0)
#define REMOTE_MONTREAL_ADDRESS     (32)
#define REPEATER_LIST_LENGTH            64

//
// LED state machine values
//

#define LED_STATE_IDLE         0
#define LED_STATE_FIRST_DIGIT  1
#define LED_STATE_BLINK        2
#define LED_STATE_SECOND_DIGIT 3

//
// Pushbutton state machine values
//

#define BUTTON_STATE_IDLE      0
#define BUTTON_STATE_PRESSED   1
#define BUTTON_STATE_HELD      2
#define BUTTON_STATE_RELEASED  3

//
// Polling state machine values
//

#define POLL_IDLE        0
#define POLL_FOOF        1
#define POLL_SEQ         2
#define POLL_ADDRESS     3
#define POLL_COUNT       4
#define POLL_DATA        5
#define POLL_TXWAIT      6
#define POLL_SEND_HEADER 7
#define POLL_SEND_ALERTS 8
#define POLL_DONE        9
#define POLL_ACK_XMIT    10
#define POLL_POWER_UP    11

//
// Radio state machine values
//

#define RADIO_INIT    0
#define RADIO_IDLE    1
#define RADIO_SYNC_1  2
#define RADIO_SYNC_2  3
#define RADIO_DATA    4
#define RADIO_PROCESS 5
#define RADIO_TIMEOUT 6

#define RADIO_INIT_START         0
#define RADIO_INIT_OFF_WAIT      1
#define RADIO_INIT_POWER_UP      2
#define RADIO_INIT_ON_WAIT       3
#define RADIO_INIT_WAIT_O        4
#define RADIO_INIT_WAIT_K        5
#define RADIO_INIT_WAIT_CR       6
#define RADIO_INIT_GET_VALUE     7
#define RADIO_INIT_ECHO_OFF      8
#define RADIO_INIT_QUERY_VER     9
#define RADIO_INIT_QUERY_DT      10
#define RADIO_INIT_QUERY_HP      11
#define RADIO_INIT_QUERY_PL      12
#define RADIO_INIT_QUERY_RR      13
#define RADIO_INIT_QUERY_RT      14
#define RADIO_INIT_CHECK_RT      15
#define RADIO_INIT_SEND_CFG      16
#define RADIO_INIT_GO_ONLINE     17
#define RADIO_INIT_FLUSH         18
#define RADIO_INIT_VERSION_ERROR 19
#define RADIO_INIT_COMPLETE      20
#define RADIO_INIT_SEND_CFG1     34
#define RADIO_INIT_SEND_CFG_STEP 35
#define RADIO_INIT_QUERY_MT      40

//
// Radio message definitions
//

#define RADIO_MSG_TYPE_ALIVE  1     // Keepalive.  Only sent by a remote.
#define RADIO_MSG_TYPE_ALERT  2     // Alert.  Only sent by a remote
#define RADIO_MSG_TYPE_ACK    3     // Acknowledgement.  Only sent by host
#define RADIO_MSG_TYPE_NAK    4     // Negative Acknowledgement...host got the message, but didn't like it
#define RADIO_MSG_TYPE_RR     5     // Relay Request - sent by a remote to another remote
#define RADIO_MSG_TYPE_WR     6     // Will Relay...sent in response to an 'RR' message
#define RADIO_MSG_TYPE_RS     7     // Relay Status
#define RADIO_MSG_TYPE RQ     8     // Relay Query

#define RADIO_MSG_DATA_SYNC1  0xF0
#define RADIO_MSG_DATA_SYNC2  0x0F

#define SHADOW_SIZE             32

// Offsets in message.  Offsets 0 to 5 are a header common to all messages
// NOTE: If you expand the size of the radio messages, you must update the maximum
// RF packet sizes that are set via the 'PK' command in the radio_init function.

#define RADIO_MSG_O_HDR_SYNC1     0
#define RADIO_MSG_O_HDR_SYNC2     1
#define RADIO_MSG_O_HDR_SRC       2
#define RADIO_MSG_O_HDR_DST       3
#define RADIO_MSG_O_HDR_SIZE      4
#define RADIO_MSG_O_HDR_TYPE      5
#define RADIO_MSG_O_HDR_SERIAL_H  6
#define RADIO_MSG_O_HDR_SERIAL_L  7
#define RADIO_MSG_O_HDR_DATA      8
// End of common packet header

#define RADIO_MSG_HDR_SIZE        9

// Offsets following the packet header for ALIVE messages
#define RADIO_MSG_O_ALIVE_HOP     8         // Number of hops to the host
#define RADIO_MSG_O_ALIVE_BFR     9         // Buffer percentage in use
#define RADIO_MSG_O_ALIVE_MASK    10        // NOTE: This item MUST be word-aligned in the message!
#define RADIO_MSG_O_ALIVE_ACK     14        // Acknowledgement requested
#define RADIO_MSG_O_ALIVE_CHKSUM  15        // Checksum!

#define RADIO_MSG_SIZE_ALIVE      16

// Offsets following the packet header for ALERT messages

#define RADIO_MSG_O_ALT_HOP       8         // Number of hops to the host
#define RADIO_MSG_O_ALT_BFRPCT    9
#define RADIO_MSG_O_ALT_MASK      10        // NOTE: This item MUST be word-aligned in the message!
#define RADIO_MSG_O_ALT_ALERTS    14
#define RADIO_MSG_O_ALT_SEQ       15
#define RADIO_MSG_O_ALT_DATA      16
#define RADIO_MSG_O_CMD_DATA      18
#define RADIO_MSG_O_CC_DATA       19

// Offsets following the packet header for ACK messages

#define RADIO_MSG_O_ACK_HOP       8         // Number of hops to host...incremented by the relay each time this message is repeated
#define RADIO_MSG_O_ACK_SEQ       9         // Sequence number this message is acknowledging
#define RADIO_MSG_O_ACK_CHKSUM    10        // Checksum!

#define RADIO_MSG_SIZE_ACK        11        // Size of an acknowledgement message

#define RADIO_OFFLINE_HOPS        0xFF
#define RADIO_MAX_HOPS            3         // Maximum number of allow hops in a repeater configuration

// Offsets following the packet header for RR (relay request) messages

#define RADIO_MSG_O_RR_NODE      8
#define RADIO_MSG_O_RR_CHKSUM    9         // Checksum!

#define RADIO_MSG_SIZE_RR       (RADIO_MSG_HDR_SIZE + 1)

// Offsets following the packet header for WR (will relay) messages

#define RADIO_MSG_O_WR_NODE      8
#define RADIO_MSG_O_WR_CHKSUM    9         // Checksum!

#define RADIO_MSG_SIZE_WR       (RADIO_MSG_HDR_SIZE + 1)

//
// RS485 alert message definitions
//

#define RS485_MSG_O_ADDR         0
#define RS485_MSG_O_SIZE         1
#define RS485_MSG_O_FUNC         2
#define RS485_MSG_O_AGC          3
#define RS485_MSG_MASK_AGC       3
#define RS485_MSG_O_ANT          3
#define RS485_MSG_MASK_ANT       4
#define RS485_MSG_O_TXMODE       3
#define RS485_MSG_MASK_TXMODE    8
#define RS485_MSG_O_BATTERY      3
#define RS485_MSG_MASK_BATTERY  0x10
#define RS485_MSG_O_TXID0        3
#define RS485_MSG_MASK_TXID0    0xC0
#define RS485_MSG_O_TXID1        4
#define RS485_MSG_O_TXID2        5
#define RS485_MSG_O_TXID3        6
#define RS485_MSG_MASK_TXID3    0x03
#define RS485_MSG_O_DEVTYPE      6
#define RS485_MSG_MASK_DEVTYPE  0x3C
#define RS485_MSG_O_LOCTIMR      6
#define RS485_MSG_MASK_LOCTIMR  0x40
#define RS485_MSG_O_DEBOUNCE     6
#define RS485_MSG_MASK_DEBOUNCE 0x80
// bh - v2.13
#define RS485_F123             7
#define RS485_F45         8
#define RS485_MSG_O_RSSI         9
#define RS485_MSG_O_CHECKSUM     10

/* SB2000 alert packets are the biggest single-payload packets sent over the RS485
 * bus. These packets include the header, data and checksum. The packet sizes can 
 * be calculated as follows:
 * ==> header + payload + checksum 
 * ==> 8 + 8 + 1 
 * ==> 17 Bytes
 * Hence:
 */
#define RS485_MAX_ALERT_SIZE      17
#define RADIO_MSG_ALERT_SIZE      RS485_MAX_ALERT_SIZE

// Maximum number of RS485 packets to include in one RF packet.
#define RADIO_MSG_ALERTS_PER_MSG  13

/* SB2000 RS485 packets can be concatnated to form one single RF packet.
 * The maximum single RF packet size can be calculated as follows:
 * ==> header + (RS485_MAX_ALERT_SIZE * RADIO_MSG_ALERTS_PER_MSG) + checksum
 * ==> 16 + (13 * 17) + 1 
 * ==> 238 Bytes
 * Hence:
 */

#define RADIO_MAX_ALERT_SIZE      238

// Value required to 'round`' `RADIO_MAX_ALERT_SIZE` to 256.
#define RADIO_ALERT_SIZE_PAD      18

// `RADIO_MSG_MAX_SIZE` becomes (`RADIO_MAX_ALERT_SIZE` + 18).
#define RADIO_MSG_MAX_SIZE      (RADIO_MAX_ALERT_SIZE + RADIO_ALERT_SIZE_PAD)

#define    RS485_ALL_ACTIVE        0xFFFFFFFF

#define SB_DATA_PACKET      0x93
#define SB_NO_DATA_PACKET   0x80
#define SB_ACK              0x94
#define SB_RSSI_PACKET      0x91
#define SB_JUST_RESET                0x8F
#define SB_SLAVE_ACK                0x82
#define SB_CONFIG_ACK                0x80
#define SB_CONFIG_NAK                0x8E

#define TK_HEADER_SIZE                8
//#define TK_MIN_DATA_SIZE            16
//  v5.00:    reduce minimum size to 13 bytes
#define TK_MIN_DATA_SIZE            13
#define OLD_MESSAGE_SIZE            11
#define    CC_0C_MODE                    0x0C
#define MODE_0C_MODE_PACKET_SIZE    8

// Super Bus 2000 msg offsets
#define SUPERBUS_2000_RFID_LOW_OFFSET   (3)
#define SUPERBUS_2000_RFID_MID_OFFSET   (4)
#define SUPERBUS_2000_RFID_HIGH_OFFSET  (5)
#define SUPERBUS_2000_RFID_HIGH_MASK    (0x0F)
#define SUPERBUS_2000_TYPE_OFFSET       (5)
#define SUPERBUS_2000_TYPE_MASK         (0xF0)
#define SUPERBUS_2000_SMOKE_TYPE        (0x20)
#define SUPERBUS_2000_PHB_TYPE          (0x70)
#define SUPERBUS_2000_AAHB_TYPE         (0x80)
#define SUPERBUS_2000_MAHB_TYPE         (0xC0)
#define SUPERBUS_2000_ALARM_OFFSET      (7)
#define SUPERBUS_2000_RSSI_OFFSET       (8)
#define SUPERBUS_2000_SUPERVISOR_VALUE  (0)

#define SB_NETWORK_HYBRID_PACKET_COUNT  (14)

// Old Superbus indeces into a Receive Packet
enum {
    OLD_ADDRESS            = 0,
    OLD_COUNT,
    OLD_RESPONSE_CODE,
    OLD_RSSI_1,
    OLD_RSSI_2,
    OLD_RSSI_3,
    OLD_RSSI_4,
    OLD_F123,
    OLD_F45,
    OLD_RSSI,
    NEW_TK_STATUS_BYTE,                //   Added to v5.14
    NEW_TK_NOISE_FLOOR_RIGHT,
    NEW_TK_NOISE_FLOOR_LEFT
};

//
// RS485 response 0x91 message definitions
//

#define RS485_RSSI_1             5
#define RS485_RSSI_2             6

#define RSSI_OUT_OF_RANGE       160            // v2.13
#define RSSI_NOMINAL            95             // v2.13

// Hardware definitions

#define CPU_IO_DIP_SW8      P6IN_bit.P0     // P6.0 - INPUT  - DIP Switch 0
#define CPU_IO_DIP_SW7      P6IN_bit.P1     // P6.1 - INPUT  - DIP Switch 1
#define CPU_IO_DIP_SW6      P6IN_bit.P2     // P6.2 - INPUT  - DIP Switch 2
#define CPU_IO_DIP_SW5      P6IN_bit.P3     // P6.3 - INPUT  - DIP Switch 3
#define CPU_IO_DIP_SW4      P6IN_bit.P4     // P6.4 - INPUT  - DIP Switch 4
#define CPU_IO_DIP_SW3      P6IN_bit.P5     // P6.5 - INPUT  - DIP Switch 5
#define CPU_IO_DIP_SW2      P6IN_bit.P6     // P6.6 - INPUT  - DIP Switch 6
#define CPU_IO_DIP_SW1      P6IN_bit.P7     // P6.7 - INPUT  - DIP Switch 7

#define CPU_IO_M_SWITCH     P1IN_bit.P7     // P1.0 - INPUT  - M_SWITCH - pushbutton switch SW3

#define CPU_IO_DEBUG_LED2   P2OUT_bit.P1   // P2.1 - OUTPUT - DEBUG LED #2
#define CPU_IO_DEBUG_LED1   P2OUT_bit.P0   // P2.0 - OUTPUT - DEBUG LED #1

#define CPU_IO_MS_RF_RX     P4IN_bit.P6     // P4.6 - INPUT  - MaxStream RF receive signal
#define CPU_IO_MS_RF_TX     P4IN_bit.P5     // P4.5 - INPUT  - MaxStream TX active signal
#define CPU_IO_MS_CTS       P4IN_bit.P4     // P4.4 - INPUT  - MaxStream CTS signal
#define CPU_IO_MS_RTS       P4OUT_bit.P3   // P4.3 - OUTPUT - MaxStream RTS signal
/// Following removed...not used on GEN2 hardware
/// #define CPU_IO_MS_CONFIG    P4OUT_bit.P4OUT2   // P4.2 - OUTPUT - MaxStream CONFIG signal
#define CPU_IO_MS_SHDN      P4OUT_bit.P1   // P4.1 - OUTPUT - MaxStream Shutdown signal
#define CPU_IO_MS_SLEEP     P4OUT_bit.P0   // P4.0 - OUTPUT - MaxStream Sleep signal

#define CPU_IO_485_TXEN     P3OUT_bit.P3   // P3.3 - OUTPUT - RS-485 transmit enable

#define CPU_IO_DISP_SEG_A   P5OUT_bit.P0   // P5.0 - OUTPUT - LED digit segment 'A'
#define CPU_IO_DISP_SEG_B   P5OUT_bit.P1   // P5.1 - OUTPUT - LED digit segment 'B'
#define CPU_IO_DISP_SEG_C   P5OUT_bit.P2   // P5.2 - OUTPUT - LED digit segment 'C'
#define CPU_IO_DISP_SEG_D   P5OUT_bit.P3   // P5.3 - OUTPUT - LED digit segment 'D'
#define CPU_IO_DISP_SEG_E   P5OUT_bit.P4   // P5.4 - OUTPUT - LED digit segment 'E'
#define CPU_IO_DISP_SEG_F   P5OUT_bit.P5   // P5.5 - OUTPUT - LED digit segment 'F'
#define CPU_IO_DISP_SEG_G   P5OUT_bit.P6   // P5.6 - OUTPUT - LED digit segment 'G'
#define CPU_IO_DISP_SEG_DP  P5OUT_bit.P7   // P5.7 - OUTPUT - LED digit decimal point


#define DIAG_RADIO_QUALITY()  ((cpu_readSpareDIP() & 0x1) != 0)
#define DIAG_RADIO_RSSI()     ((cpu_readSpareDIP() & 0x2) != 0)
#define DIAG_RADIO_RELAY()    ((cpu_readSpareDIP() & 0x4) != 0)
#define DIAG_BFR_PCT()        ((cpu_readSpareDIP() & 0x8) != 0)

#define CONFIG_REPEATER_OFF() ((cpu_readCfgDIP() & 0x08) != 0)
#define CONFIG_SUPERVISORY_FILTER_ACTIVE() ((cpu_readCfgDIP() & 0x04) != 0)
#define CONFIG_RADIO_POWER()  (FALSE)
#define CONFIG_HOST_MODE()    ((cpu_readCfgDIP() & 0x01) != 0)
#define CONFIG_DUPLICATE_FILTER_ENABLE()  ((cpu_readCfgDIP() & 0x02) == 0)

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Serial port Public Constants
//
#define STX     0x02
#define ETX     0x03
#define DLE     0x10

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Rcv/Transmit Task Trace Objects
//

#if STATE_TRACE_ENABLE

// State machine IDs
#define NO_STATE                    (0)
#define RADIO_INIT_ID               (1)
#define HOST_RADIO_ID               (2)
#define REMOTE_RADIO_ID             (3)

#define NO_STEP_ID                  (0)
#define RESET_STEP_ID               (1)
#define RESET_TIMEOUT_STEP_ID       (2)
#define RESET_TIMEOUT_WAIT_STEP_ID  (3)
#define RESET_CHAR_STEP_ID          (4)
#define RESET_VERSION_ERROR_STEP_ID (5)
#define RESET_HP_ERROR_STEP_ID      (6)
#define RESET_BUFFER_SIZE_ERROR_STEP_ID      (7)
#define RESET_ONLINE_STEP_ID        (8)
#define RESET_CONFIG_STEP_ID        (9)
#define REMOTE_CHAR_STEP_ID         (10)
#define REMOTE_XMITTOUT_STEP_ID     (11)
#define REMOTE_TRANSMIT_STEP_ID     (12)
#define REMOTE_ALIVE_STEP_ID        (13)
#define REMOTE_SYNCH2_STEP_ID       (14)
#define REMOTE_SYNCH2_TOUT_STEP_ID  (15)
#define REMOTE_DATA_STEP_ID         (16)
#define REMOTE_DATA_TOUT_STEP_ID    (17)
#define REMOTE_CKSUMERR_STEP_ID     (18)
#define REMOTE_DUP_STEP_ID          (19)
#define REMOTE_MSG_STEP_ID          (20)
#define REMOTE_NOTME_MSG_STEP_ID    (21)

#define NO_SUB_STEP_ID              (0)

#define STATE_RECORD_COUNT      (160)

typedef struct {
  unsigned int time;
  unsigned char SM_ID;
  unsigned char state;
  unsigned char stateStep;
  unsigned char stateSubStep;
} STATE_OP_TRACE;

#define STATE_TRACE(smId, stateId, stepId, subStepId) \
        stateBuffer[stateIndex].time = timer_counter; \
        stateBuffer[stateIndex].SM_ID = smId; \
        stateBuffer[stateIndex].state = stateId; \
        stateBuffer[stateIndex].stateStep = stepId; \
        stateBuffer[stateIndex].stateSubStep = subStepId; \
        stateIndex += 1; \
        if (stateIndex == STATE_RECORD_COUNT) stateIndex = 0;

#else

#define STATE_TRACE(smId, stateId, stepId, subStepId)

#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Rcv/Transmit I/O Trace Objects
//

#if IO_TRACE_ENABLE

#define IO_RECORD_COUNT         (300)

// I/O Ids
#define XMIT_ID             (0x00)
#define RCV_ID              (0x80)

// I/O Activities
#define CHAR_ID             (0x01)
#define BUFFER_ID           (0x02)
#define XON_ID              (0x03)
#define XOFF_ID             (0x04)
#define BUFFER_OVERFLOW_ID  (0x05)
#define ERROR_ID            (0x06)

typedef struct {
  unsigned int time[2];
  unsigned char IO_ID_Action;
  unsigned char IO_Data;
} IO_OP_TRACE;

#define IO_TRACE(ioId, actionId, data) \
        ioBuffer[ioIndex].time[0] = timer_counter; \
        ioBuffer[ioIndex].time[1] = TBR; \
        ioBuffer[ioIndex].IO_ID_Action = ioId + actionId; \
        ioBuffer[ioIndex].IO_Data = (unsigned char) data; \
        ioIndex += 1; \
        if (ioIndex >= IO_RECORD_COUNT) ioIndex = 0;
#else

#define IO_TRACE(ioId, actionId, data)

#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Alert and Event Tracing Objects
//

#define RCVR_ID               (7)

#define PACKET_TRACE_RFID           (1)
#define RADIO_LOAD_TRACE_RFID       (2)
#define RADIO_LOAD_DONE_TRACE_RFID  (3)

#define MESSAGE_TIMEOUT_RFID      (0)
#define LONG_DATA_RFID            (-1)
#define INVALID_MESSAGE_SIZE_RFID (-2)
#define INVALID_CHECKSUM_RFID     (-3)
#define UNEXPECTED_ADDRESS_RFID   (-4)
#define FRAMING_ERROR_RFID        (-5)
#define OVERRUN_ERROR_RFID        (-6)
#define RCV_BUFFER_ERROR_RFID     (-7)
#define XMIT_BUFFER_ERROR_RFID    (-8)

#define TESTING_RFIDS_MAX     (4)

#define RFID_INIT             (100)
#define RFID_CHANGE_CLEAR     (101)
#define RFID_SET              (102)


#if ALERT_TRACING_ENABLE
#define ALERT_TRACING_SIZE    (41)

//  Alert Buffer Tracing Records
typedef struct alert_trace_data
{
    unsigned short time;
    unsigned long RFID;
    unsigned char sender;
    char         RSSI;
} ALERT_MSG_ENTRY;

void Alert_Queue_Trace (unsigned long rfid, unsigned char src, int rssi);

#define ALERT_QUEUE_TRACE(rfid, src, rssi) \
        Alert_Queue_Trace ((rfid), (src), (rssi));

#else

#define ALERT_QUEUE_TRACE(rfid, src, rssi)

#endif

#if EVENT_TRACING_ENABLE

#define MSG_TRACING_SIZE      (280)

//  Event Buffer Tracing Records
typedef struct msg_trace_data
{
    unsigned short  time;
    long            RFID;
    short           index;
} MSG_ENTRY;

void Msg_Trace (long rfid, unsigned char src, int rssi);
void RFID_Change_Set(long rfid, unsigned char src, int event);
unsigned int RFID_Change_Check(void);

#define MSG_TRACE(rfid, src, index) \
        Msg_Trace ((rfid), (src), (index));

#define RFID_CHANGE_SET(rfid, src, event) \
        RFID_Change_Set((rfid), (src), (event))

#define RFID_CHANGE_CHECK() \
        RFID_Change_Check()

#else

#define MSG_TRACE(rfid, src, index)

#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public Objects
//

extern char host_unit;

extern unsigned char *serialA_OutGet;        // Data get pointer

struct fdata_cal
{
    unsigned long magic;
    unsigned int zero;
};

// Define the memory locations for the User and Calibration
// flash info segments (each 128 bytes)

#define LM_USR_DATA_SEG 0x1000
#define LM_CAL_DATA_SEG 0x1080

//
// Global data defined in flash.c
extern struct fdata_cal flash_data_cal;
extern U16 timer_freerunning;
extern U16 timer_counter;

typedef struct alert_data
{
    unsigned char      data[RADIO_MSG_ALERT_SIZE];    // The actual alert message received from the GE device
    char               size;                    // Size of the message in 'data'....if 0, this entry is not in use.
    unsigned char      seq;                     // Last sequence number for sending this entry to the host.  0 = never sent
    struct alert_data *next;                    // Pointer to the next alert in the list
    struct alert_data *prev;                    // Pointer to the previous alert in the list.  Mostly used to simplify unlinking this entry
} ALERT_STRUCT;

//---------------------------
//  More Public variables
//---------------------------
// NOTE: The 'rdata' buffer must be defined such that it begins on a word boundary.
// since the code later casts a byte location in it to a longword, having the buffer
// misaligned will cause that cast to behave incorrectly.  Do not seperate this definition
// from the pragma that immediately precedes it.
#ifdef BODY_WABS
#pragma DATA_ALIGN(rdata, 2)
#pragma DATA_ALIGN(rx_data, 2)
#pragma DATA_ALIGN(tx_data, 2)
#pragma DATA_ALIGN(active_units, 2)
#endif
SCOPE unsigned char rdata   [RADIO_MSG_MAX_SIZE];                 // Radio data buffer
SCOPE unsigned char rx_data [RADIO_MSG_MAX_SIZE];                 // RS-485 RX data buffer
SCOPE unsigned char tx_data [RADIO_MSG_MAX_SIZE];                 // RS-485 TX data buffer
SCOPE unsigned long active_units[256 / (sizeof(unsigned long) * 8)];

// bh - v5.03 - fix the active units mask bugs by creating a byte-wide shadow of active_units
SCOPE unsigned char ActiveUnitsShadow [SHADOW_SIZE];

#ifdef BODY_WABS
 unsigned char *data_p       = rx_data;
 unsigned char *rdata_p      = rdata;

#if STATE_TRACE_ENABLE

 STATE_OP_TRACE stateBuffer[STATE_RECORD_COUNT];
 int stateIndex;

#endif

#if IO_TRACE_ENABLE

 IO_OP_TRACE ioBuffer[IO_RECORD_COUNT];
 int ioIndex;

#endif

 unsigned short radio_serial_number   = 0;
 unsigned char  radio_hop_count       = RADIO_OFFLINE_HOPS; // Set to a very high value until we know better
 unsigned char  radio_relay_ignore    = 0;
 unsigned char  radio_relay_active    = 0;
 unsigned char  radio_relay_candidate = 0;
 unsigned long  radio_error_counter   = 0;           // Number of radio errors
 unsigned long  radio_ack_counter     = 0;           // Number of ack messages received
 unsigned long  radio_msg_counter     = 0;           // Number of alert or ack'ed keepalives sent
 unsigned char  radio_reply_expected  = 0;           // Are we expecting a reply?
 unsigned char  radio_char_unexpected = 0;           // Count unexpected characters seen
 unsigned char  radio_relay_hops      = 255;
 unsigned char  radio_sequence_number = 1;
          int   keepalive_counter     = 0;
 unsigned short retransmit_interval   = TIMEOUT_RETRANSMIT;
 unsigned char active_count    = 0;              // Counter to keep track of the number of active devices...used by both host and remote
 unsigned char bfr_percentage  = 0;              // Percentage of our local alert buffer used
 unsigned long CartsReprogramList = 0;
 unsigned char ProtocolFlag      = 0;
 unsigned char ResponseCode      = 0;
 unsigned char CapabilityCode     = 0;
 unsigned char PacketSize         = 0;
 unsigned char TkNumberOfPackets=0;
 unsigned char TkAddress       = 0;
 unsigned char TkPacketSize    = 0;
 unsigned char TkCapabilityCode= 0;
 unsigned char TkRssi          = 0;
 unsigned char TkBaseRssi      = 0;
 unsigned char TkNoiseRight     = 0;
 unsigned char TkNoiseLeft      = 0;
 unsigned char TkStatus        = 0;
 unsigned char ChecksumFailCount=0;

 char          rdata_count     = 0;              // Counter that keeps track of how many bytes of data have been received
                                                        // from the radio in the current message.
          int  radioactive     = 0;              // Radio/Lifeline box activity counter.  If this ever reaches 0, the radio
                                                        // is declared 'inactive'.
          int  startup_sweep   = TRUE;           // Sweep all addresses at startup.  Used to find radio and RS-485 devices quickly.
 unsigned int  radio_address   = 0;              // Radio address value.  Set from DIP switches SW2:1-4 at startup.
          char host_unit       = FALSE;          // Host unit flag....set from DIP switch SW1:1
          char rssi_averaging  = FALSE;          // RSSI Averaging flag.... to 1
          char supervisor_filter = FALSE;        // supervisor message filtering flag....set from DIP switch SW3
 unsigned int  last_index      = 0;              // Used by both the host and remote code to keep track of the last address
                                                        // that it tried to poll.
 unsigned char rmt_percentage  = 0;              // Lowest percentage of remote alert buffer used.  Only used by the host.
          int  offline_counter = 0;              // Number of times a RS-485 address has been declared offline.  Updated by both host and remote.
                                                        // only really used for LED display.
 unsigned int data_counter  = 0;                 // Used by RS-485 code (both host and remote)
 int button_state = BUTTON_STATE_IDLE;           // Button state value...controlled by LED display code

unsigned char debug_count;

 #else

#if STATE_TRACE_ENABLE

 extern STATE_OP_TRACE stateBuffer[];
 extern int stateIndex;

#endif

#if IO_TRACE_ENABLE

 extern IO_OP_TRACE ioBuffer[];
 extern int ioIndex;

#endif


extern unsigned char *data_p;
extern unsigned char *rdata_p;

extern unsigned short radio_serial_number;
extern unsigned char  radio_hop_count;         // Set to a very high value until we know better
extern unsigned char  radio_relay_ignore;
extern unsigned char  radio_relay_active;
extern unsigned char  radio_relay_candidate;
extern unsigned long  radio_error_counter;            // Number of radio errors
extern unsigned long  radio_ack_counter;              // Number of ack messages received
extern unsigned long  radio_msg_counter;              // Number of alert or ack'ed keepalives sent
extern unsigned char  radio_reply_expected;           // Are we expecting a reply?
extern unsigned char  radio_char_unexpected;          // Count unexpected characters seen
extern unsigned char  radio_relay_hops;
extern unsigned char  radio_sequence_number;
extern           int  keepalive_counter;
extern unsigned short retransmit_interval;
extern unsigned char active_count;              // Counter to keep track of the number of active devices...used by both host and remote
extern unsigned char bfr_percentage;              // Percentage of our local alert buffer used
extern unsigned long CartsReprogramList;
extern unsigned char ProtocolFlag;
extern unsigned char ResponseCode;
extern unsigned char TkNumberOfPackets;
extern unsigned char TkAddress;
extern unsigned char TkPacketSize;
extern unsigned char TkCapabilityCode;
extern unsigned char TkRssi;
extern unsigned char TkBaseRssi;
extern unsigned char TkNoiseRight;
extern unsigned char TkNoiseLeft;
extern unsigned char TkStatus;
extern unsigned char ChecksumFailCount;

extern  char          rdata_count;            // Counter that keeps track of how many bytes of data have been received
                                              // from the radio in the current message.
extern           int  radioactive;            // Radio/Lifeline box activity counter.  If this ever reaches 0, the radio
                                              // is declared 'inactive'.
extern           int  startup_sweep;          // Sweep all addresses at startup.  Used to find radio and RS-485 devices quickly.
extern unsigned int  radio_address;           // Radio address value.  Set from DIP switches SW2:1-4 at startup.
extern          char host_unit;               // Host unit flag....set from DIP switch SW1:1
extern          char rssi_averaging;          // RSSI averaging flag....set  to 1
extern          char supervisor_filter;       // supervisor message filtering flag....set from DIP switch SW2:1
extern          int  last_index;              // Used by both the host and remote code to keep track of the last address
                                                        // that it tried to poll.
extern unsigned char rmt_percentage;              // Lowest percentage of remote alert buffer used.  Only used by the host.
extern           int  offline_counter;              // Number of times a RS-485 address has been declared offline.  Updated by both host and remote.
                                                        // only really used for LED display.
extern  int data_counter;                          // Used by RS-485 code (both host and remote)
extern  int button_state;                           // Button state value...controlled by LED display code


#endif

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Public functions
//

extern void main(void);
extern int check_duplicate(unsigned char *data);
extern void mark_duplicate(unsigned char *data);
extern int check_sequence(unsigned char *data);
extern unsigned char read_address_switch (void);
extern int  checksum(unsigned char *data, int count);
extern int IsCPAC_Message(unsigned char* buffer);

// Timers.c
void timer_init(void);
void timer_setHiResTimer(U16 hiResTicks);
int  timer_chkHiResTimerExpired(void);
void timer_delayHiRes(U16 hiResTicks);
void timer_setLoResTimer(int timer, U32 loResTicks);
void timer_extendLoResTimer(int timer, U16 loResTicks);
int  timer_chkLoResTimerExpired(int timer);
void timer_delayLoRes(U16 loResTicks);
void timer_sleep(U16 numSeconds);
void timer_ResetTimer(void);
U16  timer_ElapsedTimer(void);

// Flash.c

void flash_init(void);
int flash_write_cal(void);
int flash_read_cal(void);

//
// Function prototypes for functions declared in wabs.c
//

void remove_alert(struct alert_data *);
int  send_radio_alert(struct alert_data *, struct alert_data *);
void remove_ack_alert(unsigned char, struct alert_data *, unsigned char);

void host_rs485_state_machine(void);
void remote_rs485_state_machine(void);
void remote_radio_state_machine(void);
void host_radio_state_machine(void);
int radio_quality(int *period);
void led_state_machine(void);
int  radio_init(int power, int reset);
unsigned char read_address_switch(void);
int  radio_repeater(void);

void inc_radio_msg_counter(void);
int  duplicate_alert_match(unsigned char *data1, unsigned char *data2);
int  add_alert(unsigned char *, unsigned char);
int  send_radio_response(void);
int  send_radio_keepalive(int);
int  send_radio_ack(unsigned char seq, unsigned char dest);
void send_radio_wr(unsigned char dest);
void send_radio_rr(unsigned char node, unsigned char dest);
int  send_rs485_response(int address);
void send_rs485_poll(int address);

void activity_check(void);
void mark_active_by_mask(unsigned char radio_address, unsigned long units);
void mark_rs485_active(int address);
void clear_rs485_active(int address);
int  is_rs485_active(int address);
int  is_sb2000_active(int address);
int  is_registration_active(int address);
void CalcProtocolFlag (int address);
void adjust_retransmit_timer (void);
void SB_InitializeProtocolPackets (void);
void SB_ReconfigureProtocolPacket (unsigned char);
void SB2000_InitializeProtocolPackets (unsigned char *, unsigned char *);
void SB2000_ReconfigureProtocolPacket (unsigned char *, unsigned char *, unsigned char);
Bool SB_SmokeDetectSupervisorProtocolPacket (unsigned char *data, U8 index);

void mark_active(unsigned long *list, int list_size, int address);
void clear_active(unsigned long *list, int list_size, int address);
int  is_active(unsigned long *list, int list_size, int address);
void capture_ambient_rssi (unsigned char * );
#undef SCOPE

#endif // H_RF_BUFFER_INCLUDED
