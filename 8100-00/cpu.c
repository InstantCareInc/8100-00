// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                cpu.c
//
// Author:              Tom Goltz
//
// Description:
// ------------
// Contains basic low-level TI430 CPU setup code
//
// Design Notes:
// -------------
//
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/07/26 16:18:24 $
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

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Private Objects
//
unsigned char  saveCfgDIP = 0;
unsigned char  saveNetDIP = 0;
unsigned char  saveSpareDIP = 0;
unsigned char  saveRotarySwitch = 0;
unsigned char  debounceDIPcompare = 0;

// bh - v3.13 - averaging structure for received rssi values
RSSI_TXID       RssiAverage [NUM_TXIDS];

unsigned long Txid;
unsigned int  TruthTablesValue;
// bh - v5.01
unsigned int  WatchDogHoursTimer;

// _____________________________________________________________________________
// _____________________________________________________________________________
//

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_initCpu
//
// Description:
// ------------
// Initializes this particular CPU for safe startup of user code and drivers.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void cpu_initCpu(void)
{
    unsigned long i;

#ifdef CPU_CLOCK_DELTA
    unsigned int Compare, Oldcapture = 0, x;
    unsigned short old_BCSCTL1;
#endif

    _DINT();                // Disable interrupts during hardware initialization

    WDTCTL = WDTPW | WDTHOLD;                               // Stop watchdog timer

    // bh - v2.14 - clear out rssi averaging structures
    for (i = 0;  i < NUM_TXIDS; i ++)
    {
        memset(&RssiAverage [i],    0,   sizeof(RssiAverage [i]));
    }

    //
    // Hardware initialization
    //

    // Digital I/O

    // P1.7 - INPUT  - momentary button
    // P1.6 - OUTPUT - Not Used
    // P1.5 - OUTPUT - Not Used
    // P1.4 - OUTPUT - Not Used
    // P1.3 - INPUT  - Network Address DIP switch
    // P1.2 - INPUT  - Network Address DIP switch
    // P1.1 - INPUT  - Network Address DIP switch
    // P1.0 - INPUT  - Network Address DIP switch

    P1DIR = 0x70;
    P1SEL = 0x00;
    P1OUT = 0x00;
    P1IE  = 0x00;
    P1IES = 0x00;

    // P2.7 - INPUT  - Unit address rotary switch
    // P2.6 - INPUT  - Unit address rotary switch
    // P2.5 - INPUT  - Unit address rotary switch
    // P2.4 - OUTPUT - Not Used
    // P2.3 - OUTPUT - Not Used
    // P2.2 - OUTPUT - Not Used
    // P2.1 - OUTPUT - Debug LED 2
    // P2.0 - OUTPUT - Debug LED 1

    P2DIR = 0x1F;
    P2SEL = 0x00;
    P2OUT = 0x00;
    P2IES = 0x00;
    P2IE  = 0x00;

    // P3.7 - INPUT  - Serial Port 2 RX (RS-485)
    // P3.6 - OUTPUT - Serial Port 2 TX (RS-485)
    // P3.5 - INPUT  - Serial Port 1 RX (RS-232 to radio module)
    // P3.4 - OUTPUT - Serial Port 1 TX (RS-232 to radio module)
    // P3.3 - OUTPUT - GE_INTERFACE_DENABLE (RS-485 transceiver control)
    // P3.2 - OUTPUT - Not Used
    // P3.1 - OUTPUT - Not Used
    // P3.0 - OUTPUT - Not Used

    P3DIR = 0x5F;
    P3SEL = 0xF0;
    P3OUT = 0;

    // P4.7 - OUTPUT - Not Used
    // P4.6 - INPUT  - MS_RF_RX
    // P4.5 - INPUT  - MS_RF_TX_N
    // P4.4 - INPUT  - MS_CTN_N
    // P4.3 - OUTPUT - MS_RTN_N
    // P4.2 - OUTPUT - MS_CONFIG_N
    // P4.1 - OUTPUT - MS_SHDN_N
    // P4.0 - OUTPUT - MS_SLEEP

    P4DIR = 0x8F;
    P4SEL = 0x00;
    P4OUT = 0x00;

    // P5.7 - INPUT  - Spare DIP switch
    // P5.6 - INPUT  - Spare DIP switch
    // P5.5 - INPUT  - Spare DIP switch
    // P5.4 - INPUT  - Spare DIP switch
    // P5.3 - OUTPUT - LED display off
    // P5.2 - OUTPUT - LED display update
    // P5.1 - OUTPUT - LED display clock
    // P5.0 - OUTPUT - LED display data

    P5DIR = 0x0F;
    P5SEL = 0x00;
    P5OUT = 0xFF;

    // P6.7 - INPUT  - MaxStream RSSI
    // P6.6 - OUTPUT - Not used
    // P6.5 - OUTPUT - Not used
    // P6.4 - OUTPUT - Not used
    // P6.3 - INPUT  - DIP switch 1
    // P6.2 - INPUT  - DIP switch 1
    // P6.1 - INPUT  - DIP switch 1
    // P6.0 - INPUT  - DIP switch 1

    P6DIR = 0x70;
    P6SEL = BIT7;                   // Use Bit 7 for the A/D input
    P6OUT = 0x70;

    //
    // Setup for 8mhz clock using the external crystal
    //

    // SELS
    BCSCTL1 = XT2OFF | XTS | DIVA_3;                 // XT2 = off, LFXT1CLK = HF, ACLK = LFXT1CLK/8
    BCSCTL2 = SELM_3 | DIVS_0 | DIVM_1;     // Internal resistor, SMCLK = DCO/1, MCLK = LFXTCLK/2

    while ((IFG1 & OFIFG) != 0)                         // Make sure OSC error is cleared
    {

        IFG1 &= ~OFIFG;
        i = CPU_MCLK_FREQ_HZ / 32;

        while (--i && (IFG1 & OFIFG) != 0) {
            IFG1 &= ~OFIFG;
        }

        i = CPU_MCLK_FREQ_HZ / 32;

        while (--i && (IFG1 & OFIFG) != 0) {
            IFG1 &= ~OFIFG;
        }
    }

    _EINT();

    // Setup A/D

    // Init hardware

    ADC12CTL0 &= ~ENC;      // Clear the Enable Conversion bit

    // ADC on, Reference Voltage on, select 2.5v reference,
    // Multiple sample & conversion, sample time = 32 * ADC12CLK * 4
    ADC12CTL0   = ADC12ON | REFON | REF2_5V | MSC | SHT0_12 | SHT1_12;

    // Repeated Sequence of channels, clock = ADC12OSC/8,
    // SAMPCON = timer, Start with channel 0
    ADC12CTL1   = CONSEQ_3 | SHP | ADC12DIV_7;

    // Input = A0, Sref = 0, EOS = 0
    ADC12MCTL0  = INCH_7 | EOS;

    ADC12CTL0  |= (ENC | ADC12SC);  // Start the converter

    // Start Timers

    timer_init();

    // Hardware init done - enable interrupts
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_Crash
//
// Description:
// ------------
// Stops everything with a fatal error....blinks an error code on the LED's.
//
// Design Notes:
// -------------
// This function will never return
// _____________________________________________________________________________
//
void cpu_Crash()
{
    for (;;) ;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_ResetCpu
//
// Description:
// ------------
// Resets the CPU, for dev. mode only.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void cpu_ResetCpu(void)
{
    WDTCTL = 0xFF;      // Reset CPU by violating the watchdog register
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_readDIP
//
// Description:
// ------------
// Reads the DIP switch values from the low-level hardware
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//

int cpu_readCfgDIP(void)
{
    return ((P6IN & 0x0F) ^ 0x0F);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_readNetDIP
//
// Description:
// ------------
// Reads the Network DIP switch values from the low-level hardware
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int cpu_readNetDIP(void)
{
    return ((P1IN & 0x0F) ^ 0x0F);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_readSpareDIP
//
// Description:
// ------------
// Reads the Spare DIP switch values from the low-level hardware
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int cpu_readSpareDIP(void)
{
    return ((P5IN >> 4) ^ 0x0F);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  cpu_readSwitch
//
// Description:
// ------------
// Reads the DIP switch values from the low-level hardware
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
int cpu_readSwitch(void)
{
    return ((P1IN & BIT0) == 0);
}

/*****************************************************
    Subroutine:     cpu_power_up_read_switches ()
                    Bob Halliday        bohalliday@aol.com
    Description:
                    Read the three banks of option switches and save them for later comparison
    Inputs:

    Outputs:
        saveCfgDIP
        saveNetDIP
        saveSpareDIP
************************************************************/
void cpu_power_up_read_switches()
{
    saveCfgDIP = (U8) cpu_readCfgDIP();
    saveNetDIP = (U8) cpu_readNetDIP();
    saveSpareDIP = (U8) cpu_readSpareDIP();
    saveRotarySwitch = read_address_switch();
}

/*****************************************************
    Subroutine:     cpu_compare_switches ()
                    Bob Halliday        bohalliday@aol.com
    Description:
                    Read the three banks of option switches
                    If any switch has changed, go into a SOFT RESET
    Inputs:
        saveCfgDIP
        saveNetDIP
        saveSpareDIP

    Outputs:
        (Watchdog Reset)

************************************************************/
void cpu_compare_switches()
{
    if ((saveCfgDIP != (U8) cpu_readCfgDIP())
            || (saveNetDIP != (U8) cpu_readNetDIP())
            || (saveSpareDIP != (U8) cpu_readSpareDIP())
            || (saveRotarySwitch !=  read_address_switch()))
    {
        // A switch has been changed --- make sure to debounce it first
        if (++debounceDIPcompare > 250)
        {
            // Debounce has been successful, go into a Watchdog Reset.
            cpu_Crash();
        }
    }
    else
    {
        // The switches have not changed - reset the debounce counter
        debounceDIPcompare = 0;
    }
}

static unsigned char digit_table[] =
{
    0xC0,         // '0'
    0xF9,         // '1'
    0xA4,         // '2'
    0xB0,         // '3'
    0x99,         // '4'
    0x92,         // '5'
    0x82,         // '6'
    0xF8,         // '7'
    0x80,         // '8'
    0x90          // '9'
};

static unsigned char alpha_table[] =
{
    0x88,         // 'A'
    0xFF,         // 'B' looks like '8', 'b' looks like '6'
    0xC6,         // 'C'
    0xA1,         // 'd'
    0x86,         // 'E'
    0x8E,         // 'F'
    0xFF,         // 'G', looks a lot like '6'
    0x89,         // 'H'
    0xFF,         // 'I'
    0xFF,         // 'J'
    0xFF,         // 'K'
    0xC7,         // 'L'
    0xFF,         // 'M'
    0xFF,         // 'N'
    0xFF,         // 'O'
    0x8C,         // 'P'
    0xFF,         // 'Q'
    0xAF,         // 'R'
    0xFF,         // 'S' looks like '5'
    0xFF,         // 'T'
    0xE3,         // 'u'
    0xFF,         // 'V'
    0xFF,         // 'W'
    0xFF,         // 'X'
    0xFF,         // 'Y'
    0xFF         // 'Z'
};

static void cpu_shift_byte(unsigned char c)
{
    int i = 9;

    while (--i)
    {
        P5OUT &= ~BIT1;     // Set the clock bit to 0
        if ((c & 0x80) != 0) {
            P5OUT |= BIT0;
        }
        else {
            P5OUT &= ~BIT0;
        }
        P5OUT |= BIT1;      // Create rising edge on clock
        c <<= 1;
    }
    P5OUT &= ~BIT1;     // Set the clock bit to 0
}

static unsigned char cpu_get_led_digit(char chr, int period)
{
    unsigned char segment = 0;

    if (isdigit(chr)) {
        segment = digit_table[chr - '0'];
    }
    else if (isalpha(chr)) {
        segment = alpha_table[toupper(chr) - 'A'];
    }
    else
    {
        switch (chr)
        {
            case '-':
                segment = 0xBF;
                break;

            default:
                segment = 0xFF;
        }
    }

    if (period){             // Do we want the decimal point?
        segment &= 0x7F;    // Yes, turn it on!
    }

    return (segment);
}

void cpu_write_led_display(char ch1, char ch2, char ch3, int period)
{
    P5OUT &= ~BIT3;         // enable display
    P5OUT &= ~BIT2;         // Set the update bit low to prepare for update
    cpu_shift_byte(cpu_get_led_digit(ch3, period == 3));
    cpu_shift_byte(cpu_get_led_digit(ch2, period == 2));
    cpu_shift_byte(cpu_get_led_digit(ch1, period == 1));
    P5OUT |= BIT2;          // Transfer shift registers to display
}

/***************************************************************************
  Subroutine:   CpuGetTxidAndTruthTable

    Description:
        This routine will extract the Txid and Truth Table values out of the data packet.

    Inputs:
        data - holds the latest alert data

    Outputs:
        Txid
        TruthTablesValue

*****************************************************************************/
void CpuGetTxidAndTruthTable(unsigned char * data)
{
    // construct the 20-bit Txid
    Txid = (unsigned long) data[RS485_MSG_O_TXID0] & RS485_MSG_MASK_TXID0;
    Txid <<= 8;
    Txid |= (unsigned long) data[RS485_MSG_O_TXID1];
    Txid <<= 8;
    Txid |= (unsigned long) data[RS485_MSG_O_TXID2];
    Txid <<= 8;
    Txid |= (unsigned long)(data[RS485_MSG_O_TXID3] & RS485_MSG_MASK_TXID3);

    // construct the 16-bit Truth Tables value
    TruthTablesValue = (unsigned int) data [RS485_F123];
    TruthTablesValue <<= 8;
    TruthTablesValue |= (unsigned int) data [RS485_F45];
}

/***************************************************************************
    Subroutine:     CpuSaveRssiIntoAverage

    Description:
        This routine accumulates the rssi readings for a given alert from each given Receiver.
        The average values are stored and accumulated in the structure called "RssiAverage"

    Inputs:
        data - holds the latest alert data

    Outputs:

*****************************************************************************/
void CpuSaveRssiIntoAverage(unsigned char * data)
{
    unsigned char i;
    RSSI_TXID * ave;

    // Get txid and truth table values
    CpuGetTxidAndTruthTable(data);

    // search for this Txid in the averaging structure
    for (i = 0;  i < NUM_TXIDS; i++)
    {
        if ((Txid == RssiAverage [i].Txid)
                && (TruthTablesValue == RssiAverage [i].TruthTablesValue)) {
            break;
        }
    }

    if (i == NUM_TXIDS)
    {
        // No matching Txid was found - go to the first empty slot
        for (i = 0;  i < NUM_TXIDS; i++)
        {
            if (RssiAverage [i].Txid == 0)
            {
                // empty slot has been found - store the new Txid here
                RssiAverage [i].Txid = Txid;
                RssiAverage [i].TruthTablesValue = TruthTablesValue;
                // break out of the 'for' loop
                break;
            }
        }
        if (i == NUM_TXIDS) {
            // No slots available for averaging - that's okay.  This will never happen
            return;
        }
    }

    // Point to the n'th entry in the averaging structure
    ave = &RssiAverage [i];

    // Search for the receiver number in this slot
    for (i = 0;  i < NUM_RECEIVERS_TRACKED; i++)
    {
        if (ave->RssiData.Receiver [i] == data [RS485_MSG_O_ADDR]) {
            break;
        }
        else if (ave->RssiData.Receiver [i] == 0)
        {
            // The next empty Receiver Slot was found - save the new receiver value here
            ave->RssiData.Receiver [i] = data [RS485_MSG_O_ADDR];
            break;
        }
    }

    // Check for no room for an n'th receiver
    if (i == NUM_RECEIVERS_TRACKED)
    {
        // Too many receivers reporting this alarm - drop this report
        return;
    }

    // The receiver number or an empty slot was found - accumulate the RSSI average
    ave->RssiData.Average [i] += data [RS485_MSG_O_RSSI];
    // Keep track of the number of samples accumulated.
    ave->RssiData.Samples [i]++;
}

/***************************************************************************
    Subroutine:     CpuCalculateAverage

    Description:
        This routine calculates the average of each of the accumulated
        rssi readings for a given alert from each given Receiver.

    Inputs:
        data - holds the latest alert data

    Outputs:

*****************************************************************************/
void CpuCalculateAverage(unsigned char * data)
{
    unsigned char i;
    unsigned char receiver;
    unsigned char average = 1;
    unsigned char new_average;
    //  unsigned char num_samples;
    RSSI_TXID * ave;

    // Get txid and truth table values
    CpuGetTxidAndTruthTable(data);

    // search for this Txid in the averaging structure
    for (i = 0;  i < NUM_TXIDS; i++)
    {
        if ((Txid == RssiAverage [i].Txid)
                && (TruthTablesValue == RssiAverage [i].TruthTablesValue)) {
            break;
        }
    }

    if (i == NUM_TXIDS)
    {
        // No matching Txid was found - This doesn't seem possible, but we have to allow for it
        return;
    }

    // The matching Txid was found - clear out these values
    RssiAverage [i].Txid = RssiAverage [i].TruthTablesValue = 0;

    // Initialize the Receiver
    receiver = data [RS485_MSG_O_ADDR];

    // Point to the n'th entry in the averaging structure
    ave = &RssiAverage [i];

    // For each active receiver, calculate the average rssi and compare it to the current highest result
    for (i = 0;  i < NUM_RECEIVERS_TRACKED; i++)
    {
        if (ave->RssiData.Receiver [i] == 0)
        {
            // The next empty Receiver Slot was found - save the new receiver value here
            break;
        }
        else
        {
            new_average = ave->RssiData.Average [i] / ave->RssiData.Samples [i];
            // bh - v2.15 - 04/05/07 - add a weighting factor to the average: (number_of_samples - 1)
            //          num_samples = ave->RssiData.Samples [i];
            //          new_average = (ave->RssiData.Average [i] / num_samples) + (num_samples-1);

            if (new_average > average)
            {
                // The new average is higher.  Make that the new standard
                average = new_average;
                receiver = ave->RssiData.Receiver [i];
            }

            // clear out the averaging variables
            ave->RssiData.Average [i] = ave->RssiData.Samples [i] = ave->RssiData.Receiver [i] = 0;
        }
    }

    // Now save the strongest rssi and receiver number in the xmit packet
    data [RS485_MSG_O_ADDR] = receiver;
    data [RS485_MSG_O_RSSI] = average;
    // recalculate the checksum for this disturbed packet
    data [data[RS485_MSG_O_SIZE]-1] = checksum(data, data[RS485_MSG_O_SIZE]-1);
}

/***************************************************************************
    Subroutine:     CpuMonitorWatchdog

    Description:
        New v5.01 routine:
        This routine counts time.
        In a Host Radio, after 24 hours, this routine will ask to reset the Maxstream radio.

        In a Remote Radio, after 12 hours, this routine will inspect the
        success rate of transmissions.  If the success rate falls under 45%,
        this routine will ask to reset the Maxstream radio.

    Inputs:
        data - holds the latest alert data

    Outputs:

    Returns:
        TRUE - It's time to reset the radio hardware
        FALSE- Dont' touch that poor radio at this time.
*****************************************************************************/
int CpuMonitorWatchdog()
{
    register int led_value;
    int period = 0;

    // Has six minutes expired on the radio watchdog?
    if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_WATCHDOG))
    {
        // Yes - six minutes has expired.
        timer_setLoResTimer(TIMER_LO_RES_RADIO_WATCHDOG, TIMEOUT_RADIO_WATCHDOG);

        // update the hours timer
        WatchDogHoursTimer++;

        // which type of radio is operating?
        if (host_unit)
        {
            // This is a host radio - wait 24 hours before resetting the radio
            if (WatchDogHoursTimer >= CPU_HOST_RESET_WAIT)
            {
                WatchDogHoursTimer = 0;
                return TRUE;
            }
        }
        else
        {
            // This is a remote radio - wait 2 hours
            if (WatchDogHoursTimer >= CPU_REMOTE_RESET_WAIT)
            {
                WatchDogHoursTimer = 0;
                led_value = radio_quality(&period);

                // If the success rate is under 49%, reset the radio.
                if (led_value < CPU_REMOTE_ERROR_THRESHOLD) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: cpu.c,v $
// Revision 1.4  2006/07/26 16:18:24  tgoltz
// Implement reading the MaxStream RSSI line using the A/D converter.
//
// Revision 1.3  2006/06/29 21:00:04  tgoltz
// Update for GEN2 hardware.
//
// Revision 1.2  2006/03/06 17:24:06  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
