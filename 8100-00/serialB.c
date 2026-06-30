// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                serialB.c
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains the Serial Communications Interface (SCI) UART driver for the second
// serial port on the TI MSP430.
//
// Design Notes:
// -------------
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/06/01 15:18:10 $
// This file based on --> $Revision: 1.4 $
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

//
// Private Objects
//

//
// Ring buffer used for serial data
//
// Note that it's assumed that updates to this data
// structure will be atomic, and if accessed at both interrupt
// and main program level, interrupts MUST be disabled
// while main program level is updating the ring.
//

// unsigned int serialB_InTotal  = 0;
// unsigned int serialB_OutTotal = 0;

U16 serialB_Error   = 0;
U16 serialB_Overrun = 0;

unsigned char *serialB_OutPut;      // Data put pointer
unsigned char *serialB_OutGet;      // Data get pointer
int            serialB_OutCount;    // Number of characters in ring
int            serialB_OutSize;     // Amount of space remaining in ring
unsigned char  serialB_OutBuffer[SERIAL_B_OUT_RING_SIZE]; // Ring buffer

unsigned char *serialB_InPut;       // Data put pointer
unsigned char *serialB_InGet;       // Data get pointer
int            serialB_InCount;     // Number of characters in ring
int            serialB_InSize;      // Amount of space remaining in ring
unsigned char  serialB_InBuffer[SERIAL_B_IN_RING_SIZE]; // Ring buffer
static   int   serialB_output_byte(void);

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_init
//
// Description:
// ------------
// Called at power up to do any initialization required by the serial port
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void serialB_init(void)
{
//
// Initialize serial B output ring buffer
//
    serialB_OutPut   = serialB_OutBuffer;
    serialB_OutGet   = serialB_OutBuffer;
    serialB_OutSize  = sizeof(serialB_OutBuffer) - 1;
    serialB_OutCount = 0;
    memset(serialB_OutBuffer, 0, sizeof(serialB_OutBuffer));
//
// Initialize serial B input ring buffer
//
    serialB_InPut     = serialB_InBuffer;
    serialB_InGet     = serialB_InBuffer;
    serialB_InSize    = sizeof(serialB_InBuffer) - 1;
    serialB_InCount   = 0;
    memset(serialB_InBuffer, 0, sizeof(serialB_InBuffer));
//
// Initialize serial B hardware
//
    // Allocate pins the UART and be sure Tx is an output and Rx is an input.
    P3SEL |=  BIT6 | BIT7;     // P3_6_UTXD1 | P3_7_URXD1 = device
    P3DIR |=  BIT6;            // P3_6_UTXD0
    P3DIR &= ~(BIT7);            // ~P3_7_URXD1

    // Disable UART1 interrupt generation for restartability, now safe to setup UART.
    U1CTL  = SWRST | CHAR | PENA;

    // If we're talking to a GE module, we use even parity to transmit,
    // and expect all the data to arrive with parity errors since the GE
    // transmits using odd parity.
    // If we're a 'master' unit connected to the Lifeline controller,
    // we use odd parity to transmit and expect the data from the Lifeline
    // box to have parity errors (it's using even parity.)

    if (!host_unit) {
        U1CTL |= PEV;
    }

    ME2   |= URXE1 | UTXE1;
    IE2   &= ~(URXIE1 | UTXIE1);
    U1BR1  = SERIAL_B_UBR1_VAL;
    U1BR0  = SERIAL_B_UBR0_VAL;
    U1MCTL = SERIAL_B_MCTL_VAL;
    U1TCTL = BIT4;        // Use ACLK as timing source
    U1RCTL = URXEIE;

    // Enable driver. When a character is actually sent the txIsr will be enabled.

    U1CTL &= ~SWRST;      // Clear SWRST, enable UART
    IE2 |= URXIE1;       // Allow receive interrupts immediately
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_output_byte
//
// Description:
// ------------
// Strips a single character from the ring buffer and outputs it to the serial
// port.  Called from both the ISR and 'serialB_write'
//
// Design Notes:
// -------------
// Assumes that the TX register is empty.  Caller MUST check this.
//
// _____________________________________________________________________________
//
static int serialB_output_byte(void)
{
    if (serialB_OutCount)
    {
        CPU_IO_485_TXEN = 1;        // Enable transmitter driver
        --serialB_OutCount;
        ++serialB_OutSize;

        U1TXBUF = *serialB_OutGet;
        ++serialB_OutGet;

        // Check if past end of ring
        if (serialB_OutGet >= (serialB_OutBuffer + sizeof(serialB_OutBuffer))) {
            serialB_OutGet = serialB_OutBuffer;
        }
        return (1);
    }
    return (0);
}

void serialB_txDoneWait(void)
{
    while ((serialB_OutCount != 0) || ((U1TCTL & TXEPT) == 0))   // Wait until the transmitter is done
    {
        if ((serialB_OutCount != 0) && ((U1TCTL & TXEPT) != 0)) {
            serialB_output_byte();
        }
    }
    CPU_IO_485_TXEN = 0;            // Disable transmitter driver
}

int serialB_txDone(void)
{
    if ((serialB_OutCount == 0) && ((U1TCTL & TXEPT) != 0))   // Is the transmitter done?
    {
        CPU_IO_485_TXEN = 0;                              // Disable transmitter driver
        return (TRUE);
    }

    if ((serialB_OutCount != 0) && ((U1TCTL & TXEPT) != 0)) {
        serialB_output_byte();
    }
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_tx_isr
//
// Description:
// ------------
// Interrupt service routine for the serial output done interrupt.
//
// Design Notes:
// -------------
// The ring will actually empty two interrupts before the hardware sets TXEPT.
//
// _____________________________________________________________________________
//
extern __interrupt void serialB_tx_isr(void);

#pragma vector=USART1TX_VECTOR
__interrupt void serialB_tx_isr(void)
{
    if (serialB_output_byte() == 0)   // Did we have something to output?
    {
        IE2 &= ~UTXIE1;           // No - disable the transmit interrupt
    }
}

unsigned char errBits;
unsigned int frameCount = 0;
unsigned int overrunCount = 0;
unsigned int bufferOverflowCount = 0;
extern __interrupt void serialB_rx_isr(void);
#pragma vector=USART1RX_VECTOR
__interrupt void serialB_rx_isr(void)
{
    unsigned char stat;
    // Check for serial port errors
    // NOTE: When talking to the GE modules, we use odd parity
    // when talking to the module, but it replies using even
    // parity.  Since the MSP430 UART is a little cranky about
    // switching parity modes, we check for a byte received
    // WITHOUT a parity error, which really IS a parity error

    // v5.00 - for CPAC compatibility, turn off Parity Error Checking
//    if ((errBits & PE) == 0) // Parity error (not) detected?
//    {
//       U1RCTL &= ~(RXERR|OE|FE|PE);
//        errBits = U1RXBUF;
//        return;                    // Yes - ignore the data
//    }

    stat = U1RCTL;
    U1RCTL &= ~(RXERR|OE|FE|PE);        // Always need to clear the parity error

    // No errors - pull the character out of the serial port
    // and store it in the ring buffer if space.  If no space,
    // read the serial port anyway and discard the character.

    if (stat  & (FE)) {
      frameCount++;
      MSG_TRACE(FRAMING_ERROR_RFID, RCVR_ID, 0);
    }
    if (stat  & (OE)) {
      overrunCount++;
      MSG_TRACE(OVERRUN_ERROR_RFID, RCVR_ID, 0);
    }

    if (serialB_InSize)
    {
        ++serialB_InCount;
        --serialB_InSize;

        *serialB_InPut = U1RXBUF;
        serialB_InPut += 1;

        // Check if past end of ring
        if (serialB_InPut >= (serialB_InBuffer + sizeof(serialB_InBuffer))) {
            serialB_InPut = serialB_InBuffer;
        }
        CPU_IO_DEBUG_LED2 = 1;          // Turn on the RX light
        timer_setLoResTimer(TIMER_LO_RES_LED, TIMER_LO_RES_TICKS_PER_SECOND / 20);
    }
    else {
      bufferOverflowCount++;
      MSG_TRACE(RCV_BUFFER_ERROR_RFID, RCVR_ID, 0);
      errBits = U1RXBUF;                           // No space - discard character
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_txBufEmpty
//
// Description:
// ------------
// Tells the caller if there is any serial data yet to be sent out over
// the serial port.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int serialB_txBufEmpty(void)
{
    return (serialB_OutCount == 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_clrInputBuf
//
// Description:
// ------------
// Clears the UART input buffer (the Rx ring).
//
// Design Notes:
// -------------
// UART recv. ISR is momentarily disabled while indexes are modified.
// _____________________________________________________________________________
//
void serialB_clrInputBuf(void)
{
    IE2 &= ~URXIE1;
    serialB_InPut     = serialB_InBuffer;
    serialB_InGet     = serialB_InBuffer;
    serialB_InSize    = sizeof(serialB_InBuffer) - 1;
    serialB_InCount   = 0;
    memset(serialB_InBuffer, 0, sizeof(serialB_InBuffer));
    IE2 |= URXIE1;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_rxBufEmpty
//
// Description:
// ------------
// Tells the caller if there is any serial input data yet to be sent out over
// the RF link.
//
// Design Notes:
// -------------
// The command buffer is not considered here since local station commands are not
// sent to remote stations.
// _____________________________________________________________________________
//
int serialB_rxBufEmpty(void)
{
    return (serialB_InCount == 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_read
//
// Description:
// ------------
// Called twice a second to check for data on the serial port
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int serialB_read(unsigned char *data)            // Check serial port for pending command character
{
    if (serialB_InCount)            // Is there a character pending?
    {
        IE2 &= ~URXIE1;            // Protect our ring buffer updates from the Rx ISR.

        --serialB_InCount;
        ++serialB_InSize;

        *data = *serialB_InGet++;

        // Check if past end of ring
        if (serialB_InGet >= (serialB_InBuffer + sizeof(serialB_InBuffer))) {
            serialB_InGet = serialB_InBuffer;
        }

        IE2 |= URXIE1;                // Allow receive interrupts again
        return (TRUE);
    }
    return (FALSE);                // Return No data available
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_write
//
// Description:
// ------------
// Called whenever serial B output is needed
//
// Design Notes:
// -------------
// Can only accept the amount of data that the output ring buffer can hold.
// _____________________________________________________________________________
//
int serialB_write(unsigned char data)
{
    if (serialB_OutSize > 0)            // Space available in ring?
    {
        IE2 &= ~UTXIE1;            // Protect our ring buffer updates from the Tx ISR.
        *serialB_OutPut = data;
        serialB_OutPut++;
        // Check if past end of ring
        if (serialB_OutPut >= (serialB_OutBuffer + sizeof(serialB_OutBuffer))) {
            serialB_OutPut = serialB_OutBuffer;
        }

        --serialB_OutSize;
        ++serialB_OutCount;

        if ((U1TCTL & TXEPT) != 0) {    // Is transmitter idle?
            serialB_output_byte();      // Yes - start it.
        }
        IE2 |= UTXIE1;            // Allow transmit interrupts
        return (TRUE);
    }
    else {
        MSG_TRACE(XMIT_BUFFER_ERROR_RFID, RCVR_ID, 0);
    }
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_writeN
//
// Description:
// ------------
// Called whenever serial output is needed
//
// Design Notes:
// -------------
// Can only accept the amount of data that the output ring buffer can hold.
// Any data beyond that is quietly discarded.
//
// _____________________________________________________________________________
//
void serialB_writeN(unsigned char *data, int count)
{
    timer_setLoResTimer(TIMER_LO_RES_OUTPUTB, TIMEOUT_RS485_OUTPUT); // Restart the output timer

    while (count)
    {
        while (serialB_write(*data) == FALSE) {
          if (timer_chkLoResTimerExpired(TIMER_LO_RES_OUTPUTB)) {       // Has the output timer expired?
            return;
          }
        }
        ++data;
        --count;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_write_buffer_available
//
// Description:
// ------------
// Returns the amount of space available in the serial B output buffer
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int serialB_write_buffer_available(void)
{
    return (serialB_OutSize);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialB_write_crlf
//
// Description:
// ------------
// Called to send CR LF sequence out the serial port
//
// Design Notes:
// -------------
// Can only accept the amount of data that the output ring buffer can hold.
// Any data beyond that is quietly discarded.
//
//
// _____________________________________________________________________________
//
void serialB_write_crlf(void)
{
    if (serialB_OutSize < 2) {             // If not enough space, punt
        return;
    }

    serialB_write('\r');
    serialB_write('\n');
}

void serialB_itoa(unsigned int value)
{
    char bfr[8];
    char *p;

    p = bfr;

    do
    {
        *p++ = '0' + value % 10;
        value /= 10;
    } while (value);

    do
    {
        --p;
        serialB_write(*p);
    } while (p != bfr);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: serialB.c,v $
// Revision 1.4  2006/06/01 15:18:10  tgoltz
// Reset to use production hardware, clean up minor prototype issue.
//
// Revision 1.3  2006/06/01 15:11:07  tgoltz
// Change semantics from master/slave to host/remote to better reflect the
// roles in a push-style system.
//
// Revision 1.2  2006/03/06 17:24:07  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// _____________________________________________________________________________
