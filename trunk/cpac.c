/***********************************************************
    Module Name:    cpac.c

    Author:            Bob Halliday, September, 2007
                    bohalliday@aol.com

    Description:
    This file provides the core subroutines for CPAC support.

    Subroutines:    CpacInitialize()
                    CpacActivateLockingRelay ()
                    CpacDeactivateLockingRelay ()

    Revision History:


*************************************************************/

#define BODY_CPAC
#include "cpu.h"
#include "wabs.h"

#include "cpac.h"

ALERT_STRUCT   command_list [COMMAND_LIST_LENGTH];

struct alert_data *active_command_head = NULL;    // Pointer to the oldest item in the alert list.
struct alert_data *active_command_tail = NULL;    // Pointer to the newest item in the alert list.  This is what's used to add entries to the list.

/***************************************************************************
    Subroutine:        CpacHostRadioSaveMessage

    Description:
        The CAR has sent a command packet to the Host Radio
        This routine will store that packet in the outgoing message queue

    Inputs:
        data

    Outputs:
        command_list

*****************************************************************************/
void CpacHostRadioSaveMessage(unsigned char * data)
{
    // first make sure this is a valid id
    if (is_rs485_active(data [RS485_MSG_O_ADDR]))                        // Is this unit known to us?
    {
        // It is a valid ID - Save the message in the Command Queue
        CpacSaveMessage(data);
    }
}

/***************************************************************************
    Subroutine:        CpacSaveMessage

    Description:
        The CAR has sent a command packet to the Host Radio or
        The Remote Radio has received the command packet from the Host Radio
        This routine will store that packet in the outgoing message queue

    Inputs:
        data

    Outputs:
        command_list

*****************************************************************************/
void CpacSaveMessage(unsigned char * data)
{
    struct alert_data *p;
    int                i;

    // now put the command into the command table
    p = command_list;
    i = COMMAND_LIST_LENGTH;
    while (--i >= 0)                                        // Search entire command array
    {
        if (p->size == 0)                                   // Is this one idle?
        {
            // First strip out the bits from the WABS radio address selector
            if (!host_unit)
            {
                data [RS485_MSG_O_ADDR] &= SB_RECEIVER_MASK;         // Exclude the bits from the WABS address selector
                data [data[RS485_MSG_O_SIZE] - 1] = checksum(data, (int)(data[RS485_MSG_O_SIZE] - 1));  // And recalculate the checksum
            }

            // now store the command string into the queue
            memcpy(p->data, data, (int)(data[RS485_MSG_O_SIZE])); // Store the command data
            p->size                 = data[RS485_MSG_O_SIZE];
            p->seq                  = 0;                          // Reset the sequence number to indicate this command hasn't been sent yet
            p->next                 = NULL;
            p->prev                 = active_command_tail;
            if (active_command_tail != NULL) {                    // Does the tail point to something?
                active_command_tail->next   = p;                  // Yes, update its next pointer
            }
            active_command_tail       = p;                        // And point the tail to our new command
            if (active_command_head == NULL) {                    // Nothing in the command list?
                active_command_head   = p;                        // Yes, point the head to our new command
            }
            return;
        }
        ++p;
    }
}

/***************************************************************************
    Subroutine:        CpacSaveOutgoingMessage

    Description:
        The Remote Radio has received the command packet from the Host Radio
        This routine will store that packet in the outgoing message queue

    Inputs:
        data

    Outputs:
        command_list

    Returns:
            TRUE    - The command Message was accepted
            FALSE    - The command Message was rejected.

*****************************************************************************/
int CpacSaveOutgoingMessage(unsigned char * data)
{
    unsigned char       id;

    // first make sure this is a valid id
    id = data [RS485_MSG_O_ADDR];
    if ((id & SB_REMOTE_RADIO_MASK) != (radio_address & SB_REMOTE_RADIO_MASK))
    {
        // This message is not meant for this set of receivers - the radio ID is wrong
        return FALSE;
    }
    if (!(is_rs485_active(id & SB_RECEIVER_MASK)))                        // Is this unit known to us?
    {
        // This unit is not active - ignore the packet just received.
        return FALSE;
    }
    // Save the message in the Command Queue
    CpacSaveMessage(data);
    return TRUE;
}

/***************************************************************************
    Subroutine:        CpacCheckForOutgoingMessages

    Description:
        The Host radio uses this subroutine to transmit packets to the remote radio.

    Inputs:
        data

    Outputs:
        command_list

*****************************************************************************/
void CpacCheckForOutgoingMessages(void)
{
    if (timer_chkLoResTimerExpired(TIMER_LO_RES_RETRANSMIT))
    {
        // The transmit timer has expired.  It's okay to send another message
        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM2_INTERVAL() + TIMER_400MS));

        // Check for a command to send
        Cpac_host_send_radio_command(active_command_head);   // NEED A CPAC VERSION OF THIS ROUTINE!
    }
}

/************************************************************************
    Subroutine:        SB_RotateReceiver ()

    Description:
        A partially constructed packet has been found to be invalid.
        Instead of throwing the whole partial packet out,
        this routine will instead only throw out the oldest byte,
        in the front of the packet, and shift the other received
        bytes up by one position.

        This is an effective method of resychronizing to the input data stream.

    Inputs:
        buffer [ ]

    Outputs:
        buffer [ ]

    Locals:
***************************************************************************/
void SB_RotateReceiver(unsigned char * buffer)
{
    int x;

    for (x = 0;  x < 5;  x++)
    {
        buffer [x] = buffer [x+1];
    }
}

/************************************************************************
    Subroutine:        SB_CheckFunctionCode ()

    Description:
        The first 3 bytes of a Superbus packet have been received.
        The third byte must be validated as a proper Function Code.
        In addition, the first and second bytes of the packet
        must also be revalidated.

    Inputs:
        buffer [ ]

    Outputs:
        buffer [ ]

    Returns:
        TRUE ---- The packet is invalid
        FALSE --- The packet passes all validation tests
***************************************************************************/
unsigned char SB_CheckFunctionCode(unsigned char * buffer)
{
    int x;

    // Before we even check the Function Code, the first and second bytes
    //  must be validated once again:
    if ((buffer [TK_COUNT] > (SB_MESSAGE_BUFFER_SIZE-2))
            || (buffer [TK_COUNT] < SB_MIN_BUFFER_SIZE)
            || (buffer [TK_ADDRESS] == 0))
    {
        // The address or count byte is invalid
        return TRUE;
    }

    // CPAC commands are accepted by default.
    if (IsCPAC_Message(buffer))
    {
        // Match found - Function Code is validated!
        return FALSE;
    }

    // Accept Command codes from Slave receivers
    for (x = 0;  SB_FunctionCodeList [x];  x++)
    {
        if (buffer [TK_RESPONSE_CODE] == SB_FunctionCodeList [x])
        {
            // Match found - Function Code is validated!
            return FALSE;
        }
    }

    // Match not found --- The Function Code is invalid.
    return TRUE;
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  Cpac_host_send_radio_command
//
// Description:
// ------------
//  This routine enables the Host Radio to send a packet of data to the remote radio
//
// Design Notes:
// -------------
// This message is asynchronously generated by the Host.
// To avoid having an excessively lengthy message, no more than 1
// alert will be sent in each response.
// _____________________________________________________________________________
//
int Cpac_host_send_radio_command(struct alert_data *ptr)
{
    // check to see if we have any alerts in the buffer
  if (ptr == NULL) {
        return (FALSE);
    }

    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = 0;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_O_ALT_DATA + 1; // Size of the message without any alerts
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_ALERT;     // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;
    rdata[RADIO_MSG_O_ALIVE_HOP]    = RADIO_OFFLINE_HOPS / 2;   // show out of range
    rdata[RADIO_MSG_O_ALT_SEQ]      = radio_sequence_number;    // Sequence number
    memcpy(&rdata[RADIO_MSG_O_ALT_MASK], &active_units[0], sizeof(active_units[0])); // Copy the bit mask of active units
    rdata[RADIO_MSG_O_ALT_ALERTS]   = 1;                    // Number of alerts in this message
    rdata[RADIO_MSG_O_ALT_BFRPCT]   = bfr_percentage;       // Percent of buffer remaining

    rdata_p = &rdata[RADIO_MSG_O_ALT_DATA];                 // Place to put the first alert (or checksum if no alerts)

    // Get the next available alert to send to the remote
    memcpy(rdata_p, ptr->data, ptr->size);

    // v5.00 --- Pull destination address out of the packet
    rdata [RADIO_MSG_O_HDR_DST]      = *rdata_p;          // & 0xE0;

    rdata[RADIO_MSG_O_HDR_SIZE]   += ptr->size;
    ptr->seq   = radio_sequence_number;                  // Remember the sequence number used to send this
    rdata_p   += ptr->size;
    *rdata_p   = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));

    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
    inc_radio_msg_counter();
    return (TRUE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  Cpac_remove_command
//
// Description:
// ------------
// Removes an alert from the doubly-linked list of active commands.
// Correctly updates the head and tail pointers and the counters.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void Cpac_remove_command(struct alert_data *p)
{
    if (p->next != NULL) {                      // Is there a message following this one?
        p->next->prev = p->prev;                // Yes, Unlink the stored message from the list
    }
    if (p->prev != NULL) {                      // Is there a message before this one?
        p->prev->next = p->next;                // Yes.
    }

    if (active_command_head == p) {               // Is this unit at the head of the list?
        active_command_head = p->next;            // Yes - point to the next (if any) alert.
    }

    if (active_command_tail == p) {               // Is this unit at the tail of the list?
        active_command_tail = p->prev;            // Yes - point to the previous (if any) alert
    }
    memset(p, 0, sizeof(struct alert_data));    // Clear the entire alert structure
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  Cpac_send_rs485_data
//
// Description:
// ------------
// Function to send a command packet to a CPAC.  Used by the Remote Radio only.
//
// Design Notes:
// -------------
//
// If a command is buffered for the polled address, it will be sent, regardless
// if that address is considered to be currently "active."  If no commands are
// buffered, the subroutine returns FALSE.
// _____________________________________________________________________________
//
int Cpac_send_rs485_data(int address)
{
    struct alert_data *p;

    if ((address > 0) && (address < 256))  // Is this a valid address?
    {
        p = active_command_head;

        while (p != NULL)                  // Is there a pending alert?
        {
            if ((p->size) && (p->data[RS485_MSG_O_ADDR] == address))
            {
                serialB_writeN(p->data, p->size);           // Send the alert to the controller
                Cpac_remove_command(p);                     // All done with the command, remove it from the list
                return (TRUE);
            }
            p = p->next;
        }

        /***********************
                if (is_rs485_active(address))                       // Is this unit known to us?
                {
                    data_p    = data;                               // Yes - Reset to beginning of buffer
                    *data_p++ = (unsigned char)address;
                    *data_p++ = 0x04;                               // And build a 'no data' response for this address
                    *data_p++ = 0x94;
                    *data_p   = checksum(data, 3);
                    serialB_writeN(data, 4);
                    return (TRUE);
                }
                else
                    if (data[RS485_MSG_O_ADDR] == 7)
                        _NOP();
        **************************/
    }
    return (FALSE);
}

