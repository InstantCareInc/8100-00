// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                serialA.c
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains the Serial Communications Interface (SCI) UART driver for the first
// serial port on the TI MSP430.
//
// Design Notes:
// -------------
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/06/01 15:18:10 $
// This file based on --> $Revision: 1.3 $
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Header files
//

#include "cpu.h"
#include "wabs.h"

static inline void serialA_rts_true(void);
static inline void serialA_rts_false(void);

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

// unsigned int serialA_InTotal  = 0;
// unsigned int serialA_OutTotal = 0;

U16 serialA_Error   = 0;
U16 serialA_Overrun = 0;

unsigned char *serialA_OutPut;      // Data put pointer
unsigned char *serialA_OutGet;      // Data get pointer
int            serialA_OutCount;    // Number of characters in ring
int            serialA_OutSize;     // Amount of space remaining in ring
unsigned char  serialA_OutBuffer[SERIAL_A_OUT_RING_SIZE]; // Ring buffer

unsigned char *serialA_InPut;       // Data put pointer
unsigned char *serialA_InGet;       // Data get pointer
int            serialA_InCount;     // Number of characters in ring
int            serialA_InSize;      // Amount of space remaining in ring

unsigned int serialA_OutCtr = 0;
unsigned char  serialA_InBuffer[SERIAL_A_IN_RING_SIZE]; // Ring buffer
char serialA_ignoreCTS_flag = FALSE;

static int serialA_output_byte(void);

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_rts_true
//
// Description:
// ------------
// Sets CTS flow control to true
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
static inline void serialA_rts_true(void)
{
    if ( CPU_IO_MS_RTS != 0) {
      IO_TRACE(RCV_ID, XON_ID, 0);
    }
    CPU_IO_MS_RTS = 0;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_rts_false
//
// Description:
// ------------
// Sets CTS flow control to false
//
// Design Notes:
// -------------
// Note, this code is duplicated in two hard-coded lines of assembler
// over in serialA_isr.asm
// _____________________________________________________________________________
//
static inline void serialA_rts_false(void)
{
    if ( CPU_IO_MS_RTS == 0) {
      IO_TRACE(RCV_ID, XOFF_ID, 0);
    }
    CPU_IO_MS_RTS = 1;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_init
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
void serialA_init(void)
{
//
// Initialize serial A output ring buffer
//
    serialA_OutPut   = serialA_OutBuffer;
    serialA_OutGet   = serialA_OutBuffer;
    serialA_OutSize  = sizeof(serialA_OutBuffer) - 1;
    serialA_OutCount = 0;
    memset(serialA_OutBuffer, 0, sizeof(serialA_OutBuffer));
//
// Initialize serial A input ring buffer
//
    serialA_InPut     = serialA_InBuffer;
    serialA_InGet     = serialA_InBuffer;
    serialA_InSize    = sizeof(serialA_InBuffer) - 1;
    serialA_InCount   = 0;
    memset(serialA_InBuffer, 0, sizeof(serialA_InBuffer));
//
// Initialize serial A hardware
//
    // Allocate pins the UART and be sure Tx is an output and Rx is an input.
    P3SEL |=  BIT4 | BIT5; 	// P3_4_UTXD0 | P3_5_URXD0 = device
    P3DIR |=  BIT4;	        // P3_4_UTXD0
    P3DIR &= ~(BIT5);	        // ~P3_5_URXD0

    // Disable UART0 interrupt generation for restartability, now safe to setup UART.
    U0CTL  = SWRST | CHAR;
    ME1   |= URXE0 | UTXE0;
    IE1   &= ~(URXIE0 | UTXIE0);
    U0BR1  = SERIAL_A_UBR1_VAL;
    U0BR0  = SERIAL_A_UBR0_VAL;
    U0MCTL = SERIAL_A_MCTL_VAL;
    U0TCTL = BIT4;		// Use ACLK as timing source
    U0RCTL = URXEIE;

    // Enable driver. When a character is actually sent the txIsr will be enabled.
    U0CTL_bit.SWRST = 0;	// Clear SWRST, enable UART
    IE1   |= URXIE0;		// Allow receive interrupts immediately

    serialA_rts_true();
}

void serialA_flush(void)
{
//
// Initialize serial A output ring buffer
//
    serialA_OutPut   = serialA_OutBuffer;
    serialA_OutGet   = serialA_OutBuffer;
    serialA_OutSize  = sizeof(serialA_OutBuffer) - 1;
    serialA_OutCount = 0;
    memset(serialA_OutBuffer, 0, sizeof(serialA_OutBuffer));
//
// Initialize serial A input ring buffer
//
    serialA_InPut     = serialA_InBuffer;
    serialA_InGet     = serialA_InBuffer;
    serialA_InSize    = sizeof(serialA_InBuffer) - 1;
    serialA_InCount   = 0;
    memset(serialA_InBuffer, 0, sizeof(serialA_InBuffer));
}

void serialA_ignoreCTS(int flag)
{
    serialA_ignoreCTS_flag = (flag != 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_output_byte
//
// Description:
// ------------
// Strips a single character from the ring buffer and outputs it to the serial
// port.  Called from both the ISR and 'serialA_write'
//
// Design Notes:
// -------------
// Assumes that the TX register is empty.  Caller MUST check this.
//
// _____________________________________________________________________________
//
static int serialA_output_byte(void)
{
    IFG1_bit.UTXIFG0 = 0;           // Clear the transmitter interrupt

    if (serialA_OutCount)
    {
        --serialA_OutCount;
        ++serialA_OutSize;

        IO_TRACE(XMIT_ID, CHAR_ID, *serialA_OutGet);

        U0TXBUF = *serialA_OutGet++;
        ++serialA_OutCtr;

        // Check if past end of ring
        if (serialA_OutGet >= (serialA_OutBuffer + sizeof(serialA_OutBuffer))) {
            serialA_OutGet = serialA_OutBuffer;
        }
        return (1);
    }

    return (0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_tx_isr
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
extern __interrupt void serialA_tx_isr(void);

#pragma vector=USART0TX_VECTOR
__interrupt void serialA_tx_isr(void)
{
    // If RTS is false, or there's nothing to output, disable the transmitter.

    IFG1_bit.UTXIFG0 = 0;               // Clear the transmitter interrupt

    if ((serialA_ignoreCTS_flag == TRUE) || (CPU_IO_MS_CTS == 0))
    {
        if (serialA_output_byte() == 0) {
            IE1_bit.UTXIE0 = 0;         // Didn't start, disable the transmitter interrupt
        }
    }
    else {
        IO_TRACE(XMIT_ID, XOFF_ID, 0);
        IE1_bit.UTXIE0 = 0;             // Didn't start, disable the transmitter interrupt
    }
}

void serialA_restart_output(void)
{
    if ((serialA_ignoreCTS_flag == TRUE) || (CPU_IO_MS_CTS == 0))
    {
        if (serialA_OutCount)
        {
            if (((IE1 & UTXIE0) == 0) ||
                (((IFG1 & UTXIFG0) == 0) && ((U0TCTL & TXEPT) != 0))) // Is transmitter idle?
            {
                IE1_bit.UTXIE0 = 0;                     // Disable the interrupt first, so we can't collide with the ISR
                if (serialA_output_byte()) {  // Yes - start it.
                    IE1_bit.UTXIE0 = 1;       // If there was something to output, Enable the transmitter interrupt
                }
            }
        }
    }
}

extern __interrupt void serialA_rx_isr(void);
#pragma vector=USART0RX_VECTOR
__interrupt void serialA_rx_isr(void)
{
    unsigned char errBits = U0RCTL;

    // Check for serial port errors

    if ((errBits & (RXERR|OE|FE|PE)) != 0) // Errors detected?
    {
        IO_TRACE(RCV_ID, ERROR_ID, errBits);
        errBits = U0RXBUF;
        IO_TRACE(RCV_ID, CHAR_ID, errBits);
        U0RCTL &= ~(RXERR|OE|FE|PE);
        return;			        // Yes - ignore the data
    }

    // No errors - pull the character out of the serial port
    // and store it in the ring buffer if space.  If no space,
    // read the serial port anyway and discard the character.

    if (serialA_InSize)
    {
        if ((*serialA_InPut++ = U0RXBUF) != 0x80) {
            errBits = 0;
        }

        IO_TRACE(RCV_ID, CHAR_ID, U0RXBUF);

        ++serialA_InCount;
        --serialA_InSize;

        // Check if past end of ring
        if (serialA_InPut >= (serialA_InBuffer + sizeof(serialA_InBuffer))) {
            serialA_InPut = serialA_InBuffer;
        }

        if (serialA_InSize < SERIAL_A_IN_STOP) {        // Running out of space?
            serialA_rts_false();                        // Yes - hold off the remote
        }
    }
    else {
        IO_TRACE(RCV_ID, BUFFER_OVERFLOW_ID, 0);
        errBits = U0RXBUF;                              // No space - discard character
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_txBufEmpty
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
int serialA_txBufEmpty(void)
{
    return (serialA_OutCount == 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_clrInputBuf
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
void serialA_clrInputBuf(void)
{
    IE1_bit.URXIE0 = 0;
    serialA_InPut     = serialA_InBuffer;
    serialA_InGet     = serialA_InBuffer;
    serialA_InSize    = sizeof(serialA_InBuffer) - 1;
    serialA_InCount   = 0;
    memset(serialA_InBuffer, 0, sizeof(serialA_InBuffer));
    IE1_bit.URXIE0 = 1;
    serialA_rts_true();
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_rxBufEmpty
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
int serialA_rxBufEmpty(void)
{
    return (serialA_InCount == 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_txBufAvail
//
// Description:
// ------------
// Tells the caller amount of available space in the output ring
// the RF link.
//
// Design Notes:
// -------------
// The command buffer is not considered here since local station commands are not
// sent to remote stations.
// _____________________________________________________________________________
//
int serialA_txBufAvail(void)
{
    return (serialA_OutSize);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_read
//
// Description:
// ------------
// Called twice a second to check for data on the serial port
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int serialA_read(unsigned char *inputA_data)	        // Check serial port for pending command character
{
    if (serialA_InSize > SERIAL_A_IN_START) {   // Have enough space?
        serialA_rts_true();                     // Yes - restart the remote
    }

    if (serialA_InCount)            // Is there a character pending?
    {
        IE1_bit.URXIE0 = 0;         // Protect our ring buffer updates from the Rx ISR.

        --serialA_InCount;
        ++serialA_InSize;

        *inputA_data = *serialA_InGet++;

        // Check if past end of ring
        if (serialA_InGet >= (serialA_InBuffer + sizeof(serialA_InBuffer))) {
            serialA_InGet = serialA_InBuffer;
        }

        IE1_bit.URXIE0 = 1;         // Allow receive interrupts again

        // Restart the serial port if there's space for data now

        if (serialA_InSize > SERIAL_A_IN_START) {
            serialA_rts_true();
        }

        return (TRUE);
    }
    serialA_rts_true();
    return (FALSE);				// Return No data available
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_write
//
// Description:
// ------------
// Called whenever serial A output is needed
//
// Design Notes:
// -------------
// Can only accept the amount of data that the output ring buffer can hold.
// _____________________________________________________________________________
//
int serialA_write(unsigned char data)
{
    if (serialA_OutSize > 0)              // Space available in ring?
    {
        IE1_bit.UTXIE0 = 0;               // Protect our ring buffer updates from the Tx ISR.
        *serialA_OutPut++ = data;
        IO_TRACE(XMIT_ID, BUFFER_ID, data);

        // Check if past end of ring
        if (serialA_OutPut >= (serialA_OutBuffer + sizeof(serialA_OutBuffer))) {
            serialA_OutPut = serialA_OutBuffer;
        }

        --serialA_OutSize;
        ++serialA_OutCount;

        if ((serialA_ignoreCTS_flag == FALSE) && (CPU_IO_MS_CTS != 0)) {  // Is output flow control negative?
            return (TRUE);                // Then don't output anything
        }

        if ((U0TCTL & TXEPT) != 0)        // Is transmitter idle?
        {
            IFG1_bit.UTXIFG0 = 0;         // Clear the transmitter interrupt flag
            if (serialA_output_byte()) { // Yes - start it.
                IE1_bit.UTXIE0 = 1;      // If there was something to output, Enable the transmitter interrupt
            }
        }
        else {
            IE1_bit.UTXIE0 = 1;           // Allow transmit interrupts
        }
        return (TRUE);
    }
    IO_TRACE(XMIT_ID, BUFFER_OVERFLOW_ID, 0);
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_write_buffer_available
//
// Description:
// ------------
// Returns the amount of space available in the serial A output buffer
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int serialA_write_buffer_available(void)
{
    return (serialA_OutSize);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_write_crlf
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
void serialA_write_crlf(void)
{
    if (serialA_OutSize < 2) {            // If not enough space, punt
        return;
    }

    serialA_write('\r');
    serialA_write('\n');
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  serialA_writeN
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
void serialA_writeN(unsigned char *output_data, int count)
{
    timer_setLoResTimer(TIMER_LO_RES_OUTPUTA, TIMEOUT_RS232_OUTPUT); // Restart the output timer

    while (count)
    {
        while (serialA_write(*output_data) == FALSE) {
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_OUTPUTA)) {       // Has the output timer expired?
                return;
            }
        }
        ++output_data;
        --count;
    }
}

void serialA_itoa(unsigned int value)
{
    char bfr[8];
    char *p;

    timer_setLoResTimer(TIMER_LO_RES_OUTPUTA, TIMEOUT_RS232_OUTPUT); // Restart the output timer

    p = bfr;

    do
    {
        *p++ = '0' + value % 10;
        value /= 10;
    } while (value);

    do
    {
        --p;
        while (serialA_write(*p) == 0) {
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_OUTPUTA)) {       // Has the output timer expired?
                return;
            }
        }
    } while (p != bfr);
}

#if CPU_SERIAL_CONSOLE
int putchar(int c)
{
//    if (c == '\n')
//        while (serialA_write('\r') == FALSE)
//            ;
    while (serialA_write(c) == FALSE) {
        ;
    }
    return (c);
}

int getchar()
{
//    unsigned char c;

//    if (serialA_read(&c) == FALSE)
        return (EOF);
//    return ((int)c);
}
#endif


// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: serialA.c,v $
// Revision 1.3  2006/06/01 15:18:10  tgoltz
// Reset to use production hardware, clean up minor prototype issue.
//
// Revision 1.2  2006/03/06 17:24:07  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// _____________________________________________________________________________
