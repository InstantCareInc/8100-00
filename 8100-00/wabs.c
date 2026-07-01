// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                wabs.c
//
// Authors:              Tom Goltz, Bob Halliday
//                                    bohalliday@aol.com
//
// Description:
// ------------
// Code that makes the Lifeline WABS board happen.
//
// Design Notes:
// -------------
//
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/08/14 20:02:54 $
// This file based on --> $Revision: 1.28 $
// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Header files
//
#define BODY_WABS
#include "cpu.h"
#include "wabs.h"
#include "cpac.h"

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Private module variables (static module global 's_' prefixed for 's'tatic)
//

// Data used by the remote/repeater feature

struct msg_id
{
    unsigned short serial_number;
    unsigned char  sender;
    unsigned char  sequence;
    unsigned char  address;
} repeater_list[REPEATER_LIST_LENGTH];

int repeater_list_index = 0;
char repeat_pending     = FALSE;
unsigned long CartsProtocolList=0;

unsigned long repeater_active_list[MAX_VALID_RADIO_ADDRESS / (sizeof(unsigned long) * 8)];    // List of units that we're repeating for

// End of repeater data

//
// Array of structures that defines the alert data buffer
//

ALERT_STRUCT    alert_list[ALERT_LIST_LENGTH];

struct alert_data *active_alert_head[MAX_VALID_HOST_RS485_ADDRESS] = {0};    // Pointer to the oldest item in the selected  alert list.
struct alert_data *active_alert_tail[MAX_VALID_HOST_RS485_ADDRESS] = {0};    // Pointer to the newest item in the selected alert list.  This is what's used to add entries to the list.
int alert_list_remaining = ALERT_LIST_LENGTH;   // Counter that keeps track of number of available alert entries.

extern ALERT_STRUCT   command_list [COMMAND_LIST_LENGTH];
extern struct alert_data *active_command_head;

// bh - v2.13 - keep an accumulation of 16 ambient rssi readings for each of 32 receivers
unsigned char ambient_rssi [MAX_VALID_REMOTE_RS485_ADDRESS+1];
unsigned int  accum_rssi [MAX_VALID_REMOTE_RS485_ADDRESS+1];

#if ALERT_TRACING_ENABLE

unsigned alert_entry_index;
ALERT_MSG_ENTRY alert_entry_buffer[ALERT_TRACING_SIZE];

#endif

#if EVENT_TRACING_ENABLE || ALERT_TRACING_ENABLE

static const long rfidVals[] = {
  (0xF3030), (0x25db0), (0x25db6), (0x29C40),
  (MESSAGE_TIMEOUT_RFID), (LONG_DATA_RFID),
  (RADIO_LOAD_TRACE_RFID), (RADIO_LOAD_DONE_TRACE_RFID),
  (INVALID_MESSAGE_SIZE_RFID), (INVALID_CHECKSUM_RFID),
  (UNEXPECTED_ADDRESS_RFID), (FRAMING_ERROR_RFID),
  (OVERRUN_ERROR_RFID), (RCV_BUFFER_ERROR_RFID),
  (XMIT_BUFFER_ERROR_RFID)
};

static unsigned char rfidFlags[TESTING_RFIDS_MAX];
static unsigned char rfidChangeFlags[TESTING_RFIDS_MAX];

#endif

#if EVENT_TRACING_ENABLE

unsigned int msg_index;
MSG_ENTRY all_msg_buffer[MSG_TRACING_SIZE];

#endif

//
// Array of structures that keep track of each RS-485 device.
// Mostly keeps track of the activity counters for local and remote RS-485 devices, as well
// as the last radio protocol sequence number.  This structure is used by
// both the host and remote code in slightly different ways.
//

struct polling_data
{
     int                rs485_counter;          // Activity counter
     int                ack_counter;            // Length of time since last ACK sent
     unsigned char      bfr;                    // Buffer Percentage in use
     unsigned char      seq;                    // Last sequence number seen for this address
} polling_list[MAX_VALID_RADIO_ADDRESS + 1];

extern U32 timer_loResTimer [];
                                                // and referenced in the remote_radio code.


#if EVENT_TRACING_ENABLE

void Msg_Trace (long rfid, unsigned char src, int rssi)
{
  unsigned int index;

  for ( index = 0; index < (sizeof(rfidVals)/sizeof(long)); index++) {
    if ((src == RCVR_ID) && (rfid == rfidVals[index])) {
      all_msg_buffer[msg_index].time = timer_counter;
      all_msg_buffer[msg_index].RFID = rfid;
      all_msg_buffer[msg_index].index = rssi;
      msg_index += 1;
      if (msg_index == MSG_TRACING_SIZE) {
        msg_index = 0;
      }
      break;
    }
  }
}

void RFID_Change_Set(long rfid, unsigned char src, int event)
{
  unsigned int index;

  if ( event == RFID_INIT) {
    memset(rfidFlags, 0, sizeof(rfidFlags));
    memset(rfidChangeFlags, 0, sizeof(rfidFlags));
  }
  else if ( event == RFID_CHANGE_CLEAR) {
    memset(rfidChangeFlags, 0, sizeof(rfidFlags));
  }
  else if (event == RFID_SET) {
    for ( index = 0; index < (sizeof(rfidVals)/sizeof(long)); index++) {
      if ((src == RCVR_ID) && (rfid == rfidVals[index])) {
        if (rfidFlags[index] != 0) {
          if (rfidChangeFlags[index] != 0) {
            rfidFlags[index] = 1;
          }
          else {
            rfidFlags[index] = 2;
          }
          break;
        }
        else {
          rfidFlags[index] = 1;
          rfidChangeFlags[index] = 1;
        }
      }
    }
  }
}

unsigned int RFID_Change_Check()
{
  unsigned int index, count;

  for ( index = 0, count = 0; index < TESTING_RFIDS_MAX; index++) {
    if (rfidFlags[index] != 0) {
      count++;
    }
  }
  return(count);
}

#endif

#if ALERT_TRACING_ENABLE

void Alert_Queue_Trace (unsigned long rfid, unsigned char src, int rssi)
{
  unsigned int index;

  for ( index = 0; index < (sizeof(rfidVals)/sizeof(long)); index++) {
    if ((src == RCVR_ID) && (rfid == rfidVals[index])) {
      alert_entry_buffer[alert_entry_index].time = timer_counter;
      alert_entry_buffer[alert_entry_index].RFID = rfid;
      alert_entry_buffer[alert_entry_index].sender = src;
      alert_entry_buffer[alert_entry_index].RSSI = rssi;
      alert_entry_index += 1;
      if (alert_entry_index == ALERT_TRACING_SIZE) {
          alert_entry_index = 0;
      }
      break;
    }
  }
}

#endif

/*********************************************************
    Function:        SB_InitializeProtocolPackets ()

    Description:
        v2.13 - Bob Halliday
        In Superbus 2000,
        a data packet has been received.
        This can be composed of many smaller data packets.
        This routine sets up variables for decoding these smaller packets

    Inputs:
        data []

    Outputs:
        TkNumberOfPackets
        TkPacketSize
        TkBaseRssi
        data []

***********************************************************/
void SB_InitializeProtocolPackets ()
{
    // Save the capability code
    TkCapabilityCode = tx_data [TK_CAPABILITY_CODE];

    // set the packet size for each data packet
    if ( TkCapabilityCode == CC_0C_MODE )
  {
        // This is an 0x0C Mode new superbus 2000 data packet
        TkPacketSize = MODE_0C_MODE_PACKET_SIZE;
  }
    else
  {
        // This is a Mode 0x1A data packet
        TkPacketSize = tx_data [TK_PACKET_COUNT];
  }

    // set the number of alerts in this data transmission
    TkNumberOfPackets = (tx_data [TK_COUNT] - TK_HEADER_SIZE)/TkPacketSize;

    // set the base RSSI signal
    TkBaseRssi = (tx_data [TK_NOISE_FLOOR_RIGHT] + tx_data [TK_NOISE_FLOOR_LEFT])/2;

  // set the count byte to the new count value (14)
  tx_data [TK_COUNT] = SB_NETWORK_HYBRID_PACKET_COUNT;

  // Save the noise and status information
  TkNoiseRight    = tx_data [TK_NOISE_FLOOR_RIGHT];
  TkNoiseLeft     = tx_data [TK_NOISE_FLOOR_LEFT];
  TkStatus        = tx_data [TK_STATUS_BYTE];

    // set the response code to the legal old superbus response code
    tx_data [TK_RESPONSE_CODE] = SB_DATA_PACKET;
}

/*********************************************************
    Function:        SB_ReconfigureProtocolPacket ()

    Description:
        v2.13 - Bob Halliday
        In Superbus 2000,
        a data packet has been received.
        This can be composed of many smaller data packets.
        This routine converts the next smaller packet to the
        older superbus format and deposits it starting at aucSBBuffer [3]

    Inputs:
        data []
        i - index pointing to next packet in the aucSBBuffer string.

    Outputs:
        data []
        TkPacketSize
        TkBaseRssi

***********************************************************/
void SB_ReconfigureProtocolPacket (unsigned char index)
{
    unsigned char x, y;

    // Get the Rssi for this alert packet
    TkRssi = tx_data [index+1];

    // v5.00 ---
    if ((tx_data [index+5] & 0xF0) == 0x60)
  {
        // This is a CPAC alarm - process it differently: bits 0 - 7
        tx_data [OLD_RSSI_2] = tx_data [index+3];

    // Set RFID bits 8 - 15
        tx_data [OLD_RSSI_3] = tx_data [index+4];

    // set tx_data [6] = 4-bit Device Type
        tx_data [OLD_RSSI_4] = (tx_data [index+5] >> 2) & 0x3C;

    // Set RFID bits 16 - 23
        tx_data [OLD_F123] = tx_data [index+6];

        // set tx_data [8] = F4 + F5 bits
        tx_data [OLD_F45] = tx_data [index+7];

        // set tx_data [9] = RSSI value ---- CPAC has no rssi value.
        tx_data [OLD_RSSI] = 0;
  }
    else
  {
        // set tx_data [3] = 2 top bits of RFID
        tx_data [OLD_RSSI_1] = (tx_data [index+3] << 6) & 0xC0;

        // set tx_data [4] = next 8 bits of RFID
        x = (tx_data [index+4] << 6) & 0xC0;
        y = (tx_data [index+3] >> 2) & 0x3F;
        tx_data [OLD_RSSI_2] = x | y;

        // set tx_data [5] = next top 8 bits of RFID
        x = (tx_data [index+5] << 6) & 0xC0;
        y = (tx_data [index+4] >> 2) & 0x3F;
        tx_data [OLD_RSSI_3] = x | y;

        // set tx_data [6] = last 2 bits of RFID + 4-bit Device Type
        tx_data [OLD_RSSI_4] = (tx_data [index+5] >> 2) & 0x3F;

        // set tx_data [7] = Battery status + F1 + F2 + F3 bits
        x = (tx_data [index+6] >> 2) & 0x3E;
        y = (tx_data [index+7] << 6) & 0xC0;
        tx_data [OLD_F123] = x | y;

        // set tx_data [8] = F4 + F5 bits
        tx_data [OLD_F45] = (tx_data [index+7] >> 2) & 0x1F;

        // set tx_data [9] = RSSI value
        tx_data [OLD_RSSI] = TkRssi;
  }

  tx_data [NEW_TK_STATUS_BYTE]       = TkStatus;
  tx_data [NEW_TK_NOISE_FLOOR_RIGHT] = TkNoiseRight;
  tx_data [NEW_TK_NOISE_FLOOR_LEFT]  = TkNoiseLeft;

    // tx_data [10] holds the checksum
  tx_data [tx_data[RS485_MSG_O_SIZE]-1] = checksum(tx_data, tx_data[RS485_MSG_O_SIZE]-1);
}

/*********************************************************
  Function:   SB2000_InitializeProtocolPackets ()

    Description:
    In Superbus 2000, a data packet has been received.
        This can be composed of many smaller data packets.
        This routine sets up variables for decoding these smaller packets

    Inputs:
        data []

    Outputs:
        TkNumberOfPackets
        TkPacketSize
        TkBaseRssi
        data []

***********************************************************/
void SB2000_InitializeProtocolPackets (unsigned char *buffer, unsigned char *data)
{
    // Save the capability code
    TkCapabilityCode = data [TK_CAPABILITY_CODE];

    // set the packet size for each data packet
    if ( TkCapabilityCode == CC_0C_MODE )
  {
        // This is an 0x0C Mode new superbus 2000 data packet
        TkPacketSize = MODE_0C_MODE_PACKET_SIZE;
  }
    else
  {
        // This is a Mode 0x1A data packet
        TkPacketSize = data [TK_PACKET_COUNT];
  }

    // set the number of alerts in this data transmission
    TkNumberOfPackets = (data [TK_COUNT] - TK_HEADER_SIZE)/TkPacketSize;

    // set the base RSSI signal
    TkBaseRssi = (data [TK_NOISE_FLOOR_RIGHT] + data [TK_NOISE_FLOOR_LEFT])/2;

  // set the count byte to the new count value (17)
  buffer [TK_COUNT] = TK_MSG_MAX_SIZE;

  // Save the noise and status information
  TkNoiseRight    = data [TK_NOISE_FLOOR_RIGHT];
  TkNoiseLeft     = data [TK_NOISE_FLOOR_LEFT];
  TkStatus        = data [TK_STATUS_BYTE];
  TkAddress       = data [TK_ADDRESS];

  // set the response code to the legal superbus 2000 response code
  buffer [TK_RESPONSE_CODE] = SB_SLAVE_ACK;
}

/*********************************************************
  Function:   SB2000_ReconfigureProtocolPacket ()

    Description:
    In Superbus 2000, a data packet has been received.
        This can be composed of many smaller data packets.
        This routine converts the next smaller packet to the
        older superbus format and deposits it starting at aucSBBuffer [3]

    Inputs:
        data []
        i - index pointing to next packet in the aucSBBuffer string.

    Outputs:
        data []
        TkPacketSize
        TkBaseRssi

***********************************************************/
void SB2000_ReconfigureProtocolPacket (unsigned char *buffer, unsigned char *data, unsigned char index)
{
#if EVENT_TRACING_ENABLE
  long alertRFID;
#endif

  // Get the Rssi for this alert packet
  TkRssi = data [index+1];

  // v5.00 ---
  if ((data [index+5] & 0xF0) == 0x60)
  {
    // Set the packet id
    buffer [TK_PACKET_TYPE]       = 0;

        // This is a CPAC alarm - process it differently: bits 0 - 7
    buffer [TK_RFID_3]            = data [index+3];

    // Set RFID bits 8 - 15
    buffer [TK_RFID_2]            = data [index+4];

    // set data [6] = 4-bit Device Type
    buffer [TK_RFID_1_DEVICETYPE] = data [index+5];

    // Set RFID bits 16 - 23
    buffer [TK_BATTERY_LOW_F1_F2] = data [index+6];

    // set data [8] = F3 + F4 + F5 bits
    buffer [TK_F3_F4_F5]          = data [index+7];

        // set data [9] = RSSI value ---- CPAC has no rssi value.
    buffer [TK_RSSI]              = 0;
  }
    else
  {
    // Load the packet data
    buffer [TK_RSSI]                = data[index + 1];
    buffer [TK_PACKET_TYPE]         = 0;
    buffer [TK_RFID_3]              = data[index + 3];
    buffer [TK_RFID_2]              = data[index + 4];
    buffer [TK_RFID_1_DEVICETYPE]   = data[index + 5];
    buffer [TK_BATTERY_LOW_F1_F2]   = data[index + 6];
    buffer [TK_F3_F4_F5]            = data[index + 7];
    buffer [TK_REPEATER_ID]         = 0;
  }

#if EVENT_TRACING_ENABLE
  // Calculate the RFID for the packet
  alertRFID = (buffer[TK_RFID_1_DEVICETYPE] & 0xf);
  alertRFID = (alertRFID * 256) + buffer[TK_RFID_2];
  alertRFID = (alertRFID * 256) + buffer[TK_RFID_3];
#endif
  MSG_TRACE(alertRFID, TkAddress, index);

  // Packet Header
  buffer [TK_ADDRESS]                  = TkAddress;
  buffer [TK_RESPONSE_CODE]            = SB_SLAVE_ACK;
  buffer [TK_CAPABILITY_CODE]          = TkCapabilityCode;
  buffer [TK_STATUS_BYTE]              = TkStatus;
  buffer [TK_NOISE_FLOOR_RIGHT]        = TkNoiseRight;
  buffer [TK_NOISE_FLOOR_LEFT]         = TkNoiseLeft;
  buffer [TK_PACKET_COUNT]             = TkPacketSize;

  // set the count byte to the new count value
  buffer [TK_COUNT] = /* TkPacketSize */ MODE_0C_MODE_PACKET_SIZE + TK_HEADER_SIZE + 1;

  // buffer [10] holds the checksum
  buffer [(buffer[TK_COUNT] - 1)] = checksum(buffer, (buffer[RS485_MSG_O_SIZE] - 1));
}

/*********************************************************************
 * Function:   SB_SmokeDetectSupervisorProtocolPacket (unit8 index)
 *
 *  Description:
 *    v5.02 - Dwight Hardin
 *    Function to decide whether to block or allow processing of a smoke
 *    detector supervision message
 *
 *  Inputs:
 *    uint8 *dataBuff Pointer to input Smoke Detector message
 *    uint8 index     Base offset for message
 *
 *  Outputs:
 *    Bool    Return flag
 *            FALSE => Message can be processed
 *            TRUE  => Message should not be processed
 *
 * Remarks
 *    A data packet has been received.
 *    This processes a one of the smaller data packets to identify
 *    whether it:
 *      It is a smoke detector supervision packet
 *      The supervisor filter has been activated
 *      The packet is in Superbus 2000 format
 *    If not,
 *      This returns a FALSE response
 *    Else
 *      Check if the supervisor lock timer has expired
 *      If yes,
 *        If smoke supervision transmission is enabled
 *          Set a 1 hour timer
 *          Disable smoke supervision transmission
 *          Return TRUE
 *        Else
 *          Set a 2 minute timer
 *          Enable smoke supervision transmission
 *          Return FALSE
 *        Endif
 *      Else
 *        If smoke supervision transmission is enabled
 *          Return FALSE
 *        Else
 *          Return TRUE
 *        Endif
 *      Endif
 *    Endif
 *
 *
***********************************************************/
Bool SB_SmokeDetectSupervisorProtocolPacket (unsigned char *dataBuff, U8 index)
{
  static U8 SuperSmokeCount = TIMER_SUPERVISOR_MAX_STEPS;

  /* Check if Super Bus 2000 protocol */
  if ((!supervisor_filter)
      || ((dataBuff[RS485_MSG_O_FUNC] != SB_SLAVE_ACK) && (dataBuff[RS485_MSG_O_FUNC] != SB_DATA_PACKET))) {
    /* No - pass through */
    return(FALSE);
  }

  /* Check if a supervisor message */
  if ( dataBuff[index + SUPERBUS_2000_ALARM_OFFSET] != SUPERBUS_2000_SUPERVISOR_VALUE) {
    /* No - pass through */
    return(FALSE);
  }

  /* Check if smoke detector */
  if ((dataBuff[index + SUPERBUS_2000_TYPE_OFFSET] & SUPERBUS_2000_TYPE_MASK) != SUPERBUS_2000_SMOKE_TYPE) {
    /* No - pass through */
    return(FALSE);
  }

  /* Check if timer has expired */
  if (timer_chkLoResTimerExpired(TIMER_LO_RES_SUPERVISOR_LOCK)) {
    /* Count events */
    SuperSmokeCount = (SuperSmokeCount + 1) % TIMER_SUPERVISOR_MAX_STEPS;

    /* Check if short time - enabled */
    if (SuperSmokeCount == TIMER_SUPERVISOR_UNLOCK_STEP) {
      /* Start the unlock timer */
      timer_setLoResTimer(TIMER_LO_RES_SUPERVISOR_LOCK,
                          TIMER_SUPERVISOR_UNLOCK_TIME);
    }
    else {
      /* Start the lock timer */
      timer_setLoResTimer(TIMER_LO_RES_SUPERVISOR_LOCK,
                          TIMER_SUPERVISOR_LOCK_TIME);
    }
  }

  /* Return the lock/unlock state */
  return (SuperSmokeCount != TIMER_SUPERVISOR_UNLOCK_STEP);
}

/*********************************************************
    Function:        capture_ambient_rssi ()

    Description:
        v2.13 - Bob Halliday
        Anytime a 0x91 message is received, the remote radio must
        save the 2 ambient RSSI values reported.
        They must be thrown into a collection of 16 readings.
        The collection must be shrunk by 2/16 before adding the 2 new readings.

    Inputs:
        data

    Outputs:
        accum_rssi []
        ambient_rssi []

***********************************************************/
void capture_ambient_rssi (unsigned char * data)
{
    unsigned char index;
    unsigned int total;

    // Get the index into the ambient array
    index = data [RS485_MSG_O_ADDR];

    // subtract out 1/8 of the accumulated readings
    total = accum_rssi [index] >> 3;
    accum_rssi [index] -= total;

    // now add in the two new RSSI readings
    // rule out values over 80
    total = data [RS485_RSSI_1] + data [RS485_RSSI_2];
    accum_rssi [index] += total;

    // now take the final reading by dividing the sum by 16
    total = accum_rssi [index] >> 4;
    ambient_rssi [index] = (unsigned char) total;
}

/*********************************************************
    Function:        adjust_retransmit_timer ()

    Description:
        v2.13 - Bob Halliday
        Formerly inline code has been moved to a subroutine.

    Inputs:

    Outputs:

***********************************************************/
void adjust_retransmit_timer ()
{
  if (active_alert_head == NULL) {
        // bh - v5.00 - 500-2000ms
    timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM2_INTERVAL() + TIMER_500MS));
  }

        // v5.00 - deleted
//       else if ((ALERT_LIST_LENGTH - alert_list_remaining) < RADIO_MSG_ALERTS_PER_MSG)   // Already have alerts in buffer, use shorter hold off.
//        // bh - v2.12 - add 200ms
//           timer_extendLoResTimer(TIMER_LO_RES_RETRANSMIT, TIMER_200MS);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  checksum
//
// Description:
// ------------
// Calculates checksum values for the GE-format messages
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int checksum(unsigned char *data, int count)
{
    unsigned char sum = 1;

    while (--count >= 0) {
        sum += *data++;
    }
    return (sum);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  inc_radio_msg_counter
//
// Description:
// ------------
// Increment radio message counter
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void inc_radio_msg_counter(void)
{
    radio_reply_expected = 1;
    radio_msg_counter  += 1;

    // v5.00 fix:  was 0xFFFFFF
    if (radio_msg_counter > 0x0FFFFF)
    {
        radio_msg_counter /= 4;
        radio_ack_counter /= 4;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  check_duplicate
//
// Description:
// ------------
// Called by the remote code to check if a given message has already been
// processed.  Used by both the repeater and the normal remote radio code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int check_duplicate(unsigned char *data)
{
    unsigned short serial_number =
          (((unsigned short)data[RADIO_MSG_O_HDR_SERIAL_H] << 8) | data[RADIO_MSG_O_HDR_SERIAL_L]);
    struct msg_id *p = repeater_list;
    int            i = REPEATER_LIST_LENGTH + 1;

    while (--i)
    {
      if ((p->sender        == data[RADIO_MSG_O_HDR_SRC])
          && (p->serial_number == serial_number))
      {
        return (TRUE);
      }
      ++p;
    }
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  mark_duplicate
//
// Description:
// ------------
// Called by the remote code to insert a message identifier in the list of
// messages that have already been processed.  Used by both the repeater
// and the normal remote radio code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void mark_duplicate(unsigned char *data)
{
    // Here with message to repeat that we've never seen before
    // Insert it in the list of messages that have been repeated

    repeater_list[repeater_list_index].sender        = data[RADIO_MSG_O_HDR_SRC];
    repeater_list[repeater_list_index].serial_number =
          (((unsigned short)data[RADIO_MSG_O_HDR_SERIAL_H] << 8) | data[RADIO_MSG_O_HDR_SERIAL_L]);
    repeater_list_index += 1;
    if (repeater_list_index >= REPEATER_LIST_LENGTH) {
        repeater_list_index = 0;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  mark_active_by_mask
//
// Description:
// ------------
// Called by the host code with the four-byte bit-mask of units known
// to a remote.  Masks this new bit mask into the local copy of all active
// devices, and then runs down the last and updates the activity counter
// for each individual address in the "polling_list" array.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void mark_active_by_mask(unsigned char radio_address, unsigned long units)
{
  int i, j;

  // First, mask all the bits into the active list
  active_units[radio_address >> 5] |= units;

  // Mask off just the WABS portion of the address
  radio_address &= 0xE0;

  // Now reset the activity counter on all these units
  i = 1;
  j = MAX_VALID_REMOTE_RS485_ADDRESS;

  while (--j >= 0)
  {
    if ((units & 1) != 0)
    {
      // v5.00 - don't count illegal units above 249
      if  (((radio_address) | (i)) > MAX_VALID_HOST_RS485_ADDRESS) {
        return;            // Scan the entire list of RS-485 addresses
      }

      if (polling_list[radio_address | i].rs485_counter == 0) {  // Was this unit previously inactive?
        ++active_count;                 // Yes - increase the number of active units
      }

      polling_list[radio_address | i].rs485_counter = ACTIVITY_COUNT_RADIO; // And refresh the activity counter
    }
    units = units >> 1;
    ++i;
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  mark_active
//
// Description:
// ------------
// Called by the remote to mark a single RS-485/GE address
// as active.  Also resets the activity counter correctly for
// local directly-connected RS-485 devices.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void mark_active(unsigned long *list, int list_size, int address)
{
  if (address <= list_size) {
    list[address >> 5] |= (1L << ((address & 0x1F) - 1));
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  mark_rs485_active
//
// Description:
// ------------
// Called by the remote to mark a single RS-485/GE address
// as active.  Also resets the activity counter correctly for
// local directly-connected RS-485 devices.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void mark_rs485_active(int address)
{
    // v5.00 -- don't mark illegal units active
  if (address > MAX_VALID_HOST_RS485_ADDRESS) {           // Scan the entire list of RS-485 addresses
    return;
  }

  mark_active(active_units, sizeof(active_units) * 8, address);

  if (polling_list[address].rs485_counter == 0) {
    ++active_count;
  }

  if (host_unit) {
    polling_list[address].rs485_counter = ACTIVITY_COUNT_RADIO; // Use longer timeout for host unit
  }
  else {
    polling_list[address].rs485_counter = ACTIVITY_COUNT_RS485; // Use short timeout on remote units
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  clear_active
//
// Description:
// ------------
// Called by both host and remote to mark an RS-485/GE device address
// as inactive.  Also keeps track of the number of times this has happened
// for error-reporting purposes.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void clear_active(unsigned long *list, int list_size, int address)
{
  if (address <= list_size) {
    list[address >> 5] &= ~(1L << ((address & 0x1F) - 1));     // Mask out this device from the bit mask
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  clear_rs485_active
//
// Description:
// ------------
// Function to check the bit-mask to clear the RS-485 device address is active
// or not.  Called by both host and remote code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void clear_rs485_active(int address)
{
  clear_active(active_units, sizeof(active_units) * 8, address);
  --active_count;                             // Reduce the number of active devices
  if (offline_counter < 999) {                // We can't display more than 999
    ++offline_counter;                      // Increase the number of times a device has gone offline
  }
  polling_list[address].rs485_counter = 0;

  // bh - v2.13 - also clear the protocol setting
  clear_active((unsigned long *) &CartsProtocolList, MAX_VALID_REMOTE_RS485_ADDRESS, address);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  is_active
//
// Description:
// ------------
// Function to check the bit-mask to see if a given RS-485/GE device address
// is active or not.  Called by both host and remote code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int is_active(unsigned long *list, int list_size, int address)
{
  if (address <= list_size) {
    return ((list[address >> 5] & (1L << ((address & 0x1F) - 1))) != 0);
  }
  return (0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  is_rs485_active
//
// Description:
// ------------
// Function to check the bit-mask to see if a given RS-485/GE device address
// is active or not.  Called by both host and remote code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int is_rs485_active(int address)
{
  return (is_active(active_units, sizeof(active_units) * 8, address));
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  is_active
//
// Description:
// ------------
// Function to check the bit-mask to see if a given SuperBus 2000 RS-485 device
// address is active. Called by both host and remote code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int is_sb2000_active(int address)
{
  return (is_active((unsigned long *) &CartsProtocolList, MAX_VALID_REMOTE_RS485_ADDRESS, address));
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  is_registration_active
//
// Description:
// ------------
// Function to check the bit-mask to see if a given RS-485/GE device address
// is in reset mode.  Called by both host and remote code.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int is_registration_active(int address)
{
  return (is_active((unsigned long *) &CartsReprogramList, MAX_VALID_REMOTE_RS485_ADDRESS, address));
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  activity_check
//
// Description:
// ------------
// Runs down list of active RS485 devices and decrements the activity counter
// Marks any units that time out as inactive.
//
// Also handles turning on the red LED if there aren't any devices left
// active.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void activity_check(void)
{
  int i;

  if (timer_chkLoResTimerExpired(TIMER_LO_RES_ACTIVITY))      // Only do this every 1/4 second
  {
    timer_setLoResTimer(TIMER_LO_RES_ACTIVITY, TIMEOUT_ACTIVITY);

    CPU_IO_DEBUG_LED2 = 0;                          // Turn off the green light

    if (radioactive) {
      radioactive -= 1;
    }

    i = 1;
    while (i <= MAX_VALID_HOST_RS485_ADDRESS)            // Scan the entire list of RS-485 addresses
    {
      if (polling_list[i].rs485_counter != 0)           // Yes, is the activity counter non-zero?
      {
        polling_list[i].rs485_counter -= 1;           // Yes, just reduce it.
        if (polling_list[i].rs485_counter == 0) {     // Did we just count it down?
          clear_rs485_active(i);
        }
      }

      if (polling_list[i].ack_counter != 0) {         // Yes, is the acknowledgement counter non-zero?
        polling_list[i].ack_counter -= 1;           // Yes, reduce it.
      }

      ++i;
    }

    if ((bfr_percentage > 90) || (rmt_percentage > 90)) { // Are we running out of alert storage?
      CPU_IO_DEBUG_LED1 ^= 1;                           // Yes - Blink the red light
    }
    else
    {
      if (((active_units[0] | active_units[1] | active_units[2] | active_units[3]
          | active_units[4] | active_units[5] | active_units[6] | active_units[7]) == 0)
          || (radioactive == 0)) { // If no units are marked active
        // Turn on the red light
        CPU_IO_DEBUG_LED1 = 1;
      }
      else {
        CPU_IO_DEBUG_LED1 = 0;                      // Turn off the red light
      }
    }
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  remove_alert
//
// Description:
// ------------
// Removes an alert from the doubly-linked list of active alerts.
// Correctly updates the head and tail pointers and the counters.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void remove_alert(struct alert_data *p)
{
  // Is there a message following this one?
  if (p->next != NULL) {
    // Yes, Unlink the stored message from the list
    p->next->prev = p->prev;
  }

  // Is there a message before this one?
  if (p->prev != NULL) {
    // Yes.
    p->prev->next = p->next;
  }

  // Check if a Master Wireless Link
  if (host_unit != 0) {
    // Is this unit at the head of the list?
    if (active_alert_head[p->data[TK_ADDRESS]] == p) {
      // Yes - point to the next (if any) alert.
      active_alert_head[p->data[TK_ADDRESS]] = p->next;
    }

    // Is this unit at the tail of the list?
    if (active_alert_tail[p->data[TK_ADDRESS]] == p) {
      // Yes - point to the previous (if any) alert
      active_alert_tail[p->data[TK_ADDRESS]] = p->prev;
    }
  }
  else {
    // Slave Wireless Link - identify if the alert is for a Montreal button
    if (((p->data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_PHB_TYPE)
          || ((p->data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_AAHB_TYPE))  {
      // Is this unit at the head of the Montreal remote list?
      if (active_alert_head[REMOTE_MONTREAL_ADDRESS] == p) {
        // Yes - point to the next (if any) alert.
        active_alert_head[REMOTE_MONTREAL_ADDRESS] = p->next;
      }

      // Is this unit at the tail of the Montreal remote list?
      if (active_alert_tail[REMOTE_MONTREAL_ADDRESS] == p) {
        // Yes - point to the previous (if any) alert
        active_alert_tail[REMOTE_MONTREAL_ADDRESS] = p->prev;
      }
    }
    else {
      // Is this unit at the head of the standard remote list?
      if (active_alert_head[REMOTE_STANDARD_ADDRESS] == p) {
        // Yes - point to the next (if any) alert.
        active_alert_head[REMOTE_STANDARD_ADDRESS] = p->next;
      }

      // Is this unit at the tail of the standard remote list?
      if (active_alert_tail[REMOTE_STANDARD_ADDRESS] == p) {
        // Yes - point to the previous (if any) alert
        active_alert_tail[REMOTE_STANDARD_ADDRESS] = p->prev;
      }
    }
  }

  // Clear the entire alert structure
  memset(p, 0, sizeof(struct alert_data));

  // Check if the number of remaining alerts
  if (alert_list_remaining != ALERT_LIST_LENGTH) {
    alert_list_remaining += 1;                // Increase number of remaining alerts
  }

  // Update percentage of buffer remaining
  bfr_percentage = (((ALERT_LIST_LENGTH - alert_list_remaining) * 100) / (ALERT_LIST_LENGTH));
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  remove_ack_alert
//
// Description:
// ------------
// Searches the list of alerts and removes any that have been sent
// under the supplied sequence number.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void remove_ack_alert(unsigned char seq, struct alert_data *p, unsigned char flag)
{
  struct alert_data *q;     // for commands, remove commands from the command list

  if (seq == 0) {     // Sequence number of 0 is not valid
    return;         // Ignore it.
  }

  while (p != NULL)
  {
    if (p->seq == seq)
    {

      q = p;              // Remember this alert
      p = p->next;        // Advance to the next before removing the current alert
      if ( flag ) {
        remove_alert(q);      // remove the alert from the alert queue
      }
      else {
        Cpac_remove_command(q);   // remove the command from the command queue
      }
    }
    else {
      p = p->next;
    }
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  duplicate_alert_match
//
// Description:
// ------------
// Compares two alert messages to see if the duplicate each other.
// Returns 0 if the old Alert has a stronger RSSI than the new alert
// Returns -1 if transmitter addresses do not match
// Returns 1 if the old Alert has a weaker RSSI than the new alert
//
// Design Notes:
// -------------
//
// Although this code is utterly vital to allow the radios with their
// 9600 baud data rate to keep up with the RS-485's 19,200 baud data rate,
// it seems to have some undesirable and subtle side-effects on the way
// the Lifeline system works.  As a result, the filtering for duplicate
// messages is only active as the buffer utilization begins to climb.  Below
// 25% utilitization, no filtering whatsoever is performed.  Between 25 and
// 75%, duplicate messages from the same transmitter and received on the same
// receiver will be discarded, and above 75% utilization, duplicate messages
// from the same transmitter will be discarded, no matter what receiver they
// came from.  To make this work as well as possible, we'll keep the alert
// with the higher RSSI (signal strength) value.
//
// _____________________________________________________________________________
//
int duplicate_alert_match (unsigned char *data1, unsigned char *data2)
{
  // The following nasty-looking comparison deals with the fact that the
  // GE alert message is bit packed and the transmitter address is NOT neatly
  // byte-aligned.

  if ((data1[TK_RFID_3]               != data2[TK_RFID_3])
      || (data1[TK_RFID_2]            != data2[TK_RFID_2])
      || (data1[TK_RFID_1_DEVICETYPE] != data2[TK_RFID_1_DEVICETYPE])
      || (data1[TK_BATTERY_LOW_F1_F2] != data2[TK_BATTERY_LOW_F1_F2])
      || (data1[TK_F3_F4_F5]          != data2[TK_F3_F4_F5]))
  {
     // Point Addresses do not match - this is a unique alert
     return (-1);
  }

  // Check if filtering is being done
  if(CONFIG_DUPLICATE_FILTER_ENABLE()) {
#if MONTREAL_DEVICE_FILTER_ENABLE
    // Here with matching Point addresses
    if (((data1[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_PHB_TYPE)
            || ((data1[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_AAHB_TYPE)
            || ((data1[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_MAHB_TYPE))
    {
      // Do not match the alert! Accept all 7000AHB/SL and 7000PHB/SL as new
      return (-1);
    }
#endif
    // Check RSSI levels for all others
    if (data1[SUPERBUS_2000_RSSI_OFFSET] < data2[SUPERBUS_2000_RSSI_OFFSET])
    {
      // The old alert loses! - replace it with the new alert!
      return (1);
    }

    // The new alert loses! - throw it away!
    return (0);
  }
  else {
    // All messages accepted
    return(-1);
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  add_alert
//
// Description:
// ------------
// Function to add alert data for a given unit.  Used by both host and remote
// units.  Host WABS uses it for information received over the radio, remote
// uses it for information received from the RS485.
//
// Design Notes:
// -------------
//        returns:
//                    TRUE:  New alert has been added to the Active Alert List
//                    FALSE: New alert has been thrown away
//
// _____________________________________________________________________________
//
int add_alert (unsigned char *data, unsigned char cpac)
{
  struct alert_data *p;
  int                i;
  unsigned char    index;
#if ALERT_TRACING_ENABLE
  unsigned long    alertRFID;
#endif

  if ((data[RS485_MSG_O_ADDR] < 1) || (data[RS485_MSG_O_ADDR] > MAX_VALID_HOST_RS485_ADDRESS)) { // Is this an invalid address?
    return (FALSE);                                     // Yes - stop here so we don't overrun our array
  }

  if ((data[RS485_MSG_O_SIZE] > sizeof(p->data))) {       // Is this alert too big?
    return (FALSE);                                     // Yes - ignore it.
  }

  // bh - v2.13 --- reduce out of range RSSI to a nominal value
  if (!host_unit && !cpac)
  {
    if (data [RS485_MSG_O_RSSI] >= RSSI_OUT_OF_RANGE)
    {
      // Rssi is out of range - replace it with a nominal value
      data [RS485_MSG_O_RSSI] = RSSI_NOMINAL;

      // Since the RSSI value is changed, we must recalculate the checksum for this packet
      data [data[RS485_MSG_O_SIZE]-1] = checksum(data, data[RS485_MSG_O_SIZE]-1);
    }
  }

  // bh - v2.13 --- subtract ambient rssi out of received rssi
  // Get the index into the ambient array

  // v2.15 - rssi averaging uses an option switch on SW1-3.
  if (rssi_averaging && !cpac)
  {
    if (!host_unit)
    {
      index = data [RS485_MSG_O_ADDR];
      if (ambient_rssi [index] < data [RS485_MSG_O_RSSI]) {
        data [RS485_MSG_O_RSSI] -= ambient_rssi [index];
      }
      else {
        data [RS485_MSG_O_RSSI] = 0;
      }

      // Since the RSSI value is changed, we must recalculate the checksum for this packet
      data [data[RS485_MSG_O_SIZE]-1] = checksum(data, data[RS485_MSG_O_SIZE]-1);
    }

    // bh - v2.14 - accumulate the rssi value for this receiver
    if (!host_unit)
    {
      CpuSaveRssiIntoAverage (data);
    }
  }

  if (!cpac)
  {
    // Check if a Master Wireless Link
    if (host_unit != 0) {
      // now put the alert with the modified RSSI reading into the address based alert table
      p = active_alert_head[data[TK_ADDRESS]];

      // Search entire host alert array
      while (p != NULL)
      {
        // Check if an entry is present
        if ((p->size != 0) && (p->size == data[RS485_MSG_O_SIZE]))
        {
          // Process the entry for duplicates
          switch (duplicate_alert_match(p->data, data))
          {
            // bh - v2.11 - New alert has a weaker RSSI - throw it away
            case 0:                                     // Full match...same transmitter
              return (FALSE);

            // bh - v2.11 - New alert has a stronger RSSI - throw away the old alert
            case 1:                                     // Same transmitter
              remove_alert(p);                        // Discard the older message
              break;
          }
        }

        // Advance to the next alert
        p = p->next;
      }
    }
    // Process a Remote Wireless Link
    else {
      // identify if the alert is a Montreal button
      if (((data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_PHB_TYPE)
          || ((data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_AAHB_TYPE))  {
        // Select the Montreal remote alert table for search
        p = active_alert_head[REMOTE_MONTREAL_ADDRESS];
      }
      else {
        // Select the standard remote alert table for search
        p = active_alert_head[REMOTE_STANDARD_ADDRESS];
      }

      // Search selected remote alert array
      while (p != NULL)

      {
        // Check if an entry is present
        if ((p->size != 0) && (p->size == data[RS485_MSG_O_SIZE]))
        {
          // Process the entry for duplicates
          switch (duplicate_alert_match(p->data, data))
          {
            // bh - v2.11 - New alert has a weaker RSSI - throw it away
            case 0:                                     // Full match...same transmitter
              return (FALSE);

            // bh - v2.11 - New alert has a stronger RSSI - throw away the old alert
            case 1:                                     // Same transmitter
              remove_alert(p);                        // Discard the older message
              break;
          }
        }

        // Advance to the next alert
        p = p->next;
      }
    }
  }

  // Now add in the new alert that won the RSSI battle.
  p = alert_list;
  i = ALERT_LIST_LENGTH;

  while (--i >= 0)                                        // Search entire alert array
  {
    if (p->size == 0)                                   // Is this one idle?
    {
      // Store the alert data
      memcpy(p->data, data, (int)(data[RS485_MSG_O_SIZE]));
      p->size                 = data[RS485_MSG_O_SIZE];

      // Reset the sequence number to indicate this alert hasn't been sent yet
      p->seq                  = 0;

      // Save the unlinked pointer
      p->next                 = NULL;
      p->prev                 = NULL;

#if ALERT_TRACING_ENABLE
      // Calculate the RFID for the packet
      alertRFID = (data[TK_RFID_1_DEVICETYPE] & 0xf);
      alertRFID = (alertRFID * 256) + data[TK_RFID_2];
      alertRFID = (alertRFID * 256) + data[TK_RFID_3];
#endif
      ALERT_QUEUE_TRACE(alertRFID, data[RS485_MSG_O_ADDR], data[SUPERBUS_2000_RSSI_OFFSET]);

      // Check if a Master Wireless Link
      if (host_unit != 0) {
        // Link the new alert at the address based list tail
        p->prev               = active_alert_tail[data[TK_ADDRESS]];

        // Does the address based  tail point to something?
        if (active_alert_tail[data[TK_ADDRESS]] != NULL) {
          // Yes, update it's next pointer
          active_alert_tail[data[TK_ADDRESS]]->next = p;
        }

        // And point the address based tail to our new alert
        active_alert_tail[data[TK_ADDRESS]]       = p;

        // Nothing in the address based alert list?
        if (active_alert_head[data[TK_ADDRESS]] == NULL) {
          // Yes, point the head to our new alert
          active_alert_head[data[TK_ADDRESS]] = p;
        }
      }
      else {
        // identify if the alert is a Montreal button
        if (((data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_PHB_TYPE)
            || ((data[TK_RFID_1_DEVICETYPE] & SUPERBUS_2000_TYPE_MASK) == SUPERBUS_2000_AAHB_TYPE))  {
          // Link the new alert at the montreal alert list tail
          p->prev               = active_alert_tail[REMOTE_MONTREAL_ADDRESS];

          // Does the tail point to something?
          if (active_alert_tail[REMOTE_MONTREAL_ADDRESS] != NULL) {
            // Yes, update it's next pointer
            active_alert_tail[REMOTE_MONTREAL_ADDRESS]->next = p;
          }

          // And point the tail to our new alert
          active_alert_tail[REMOTE_MONTREAL_ADDRESS]       = p;

          // Nothing in the alert list?
          if (active_alert_head[REMOTE_MONTREAL_ADDRESS] == NULL) {
            // Yes, point the head to our new alert
            active_alert_head[REMOTE_MONTREAL_ADDRESS] = p;
          }
        }
        else {
          // Link the new alert at the standard alert list tail
          p->prev               = active_alert_tail[REMOTE_STANDARD_ADDRESS];

          // Does the tail point to something?
          if (active_alert_tail[REMOTE_STANDARD_ADDRESS] != NULL) {
            // Yes, update it's next pointer
            active_alert_tail[REMOTE_STANDARD_ADDRESS]->next = p;
          }

          // And point the tail to our new alert
          active_alert_tail[REMOTE_STANDARD_ADDRESS]       = p;

          // Nothing in the alert list?
          if (active_alert_head[REMOTE_STANDARD_ADDRESS] == NULL) {
            // Yes, point the head to our new alert
            active_alert_head[REMOTE_STANDARD_ADDRESS] = p;
          }
        }
      }

      // Check if list is done
      if (alert_list_remaining) {
        // Reduce number of remaining alerts
        alert_list_remaining -= 1;
      }

      // Mark the address active
      mark_rs485_active((int)(data[RS485_MSG_O_ADDR]));

      // Update percentage of buffer remaining
      bfr_percentage = (((ALERT_LIST_LENGTH - alert_list_remaining) * 100) / (ALERT_LIST_LENGTH));

      // debug
      if (data[RS485_MSG_O_ADDR] == 0x86) {
        debug_count=66;
        // end of debug
      }

      return (TRUE);
    }
    ++p;
  }
  return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_rs485_poll
//
// Description:
// ------------
// Generates an RS-485 poll message.  Used by the remote only
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void send_rs485_poll(int address)
{
    tx_data[RS485_MSG_O_ADDR]     = last_index = address;

  // This is a Superbus 2000 device
  if (is_registration_active (address))
  {
    // We must send a registration command
    tx_data[RS485_MSG_O_SIZE]     = 0x07;
    tx_data[RS485_MSG_O_FUNC]     = 0x38;
    tx_data[RS485_MSG_O_FUNC+1]   = 0x1A;
    tx_data[RS485_MSG_O_FUNC+2]   = 0x00;
    tx_data[RS485_MSG_O_FUNC+3]   = 0x03;
  }
  else
  {
    // We must send a Superbus 2000 poll
    tx_data[RS485_MSG_O_SIZE]     = 0x05;
    tx_data[RS485_MSG_O_FUNC]     = 0x15;
    tx_data[RS485_MSG_O_FUNC+1]   = 0xFF;
  }

    // Now calculate the checksum for this send packet
  tx_data [tx_data[RS485_MSG_O_SIZE]-1] = checksum(tx_data, tx_data[RS485_MSG_O_SIZE]-1);
  serialB_writeN(tx_data, tx_data[RS485_MSG_O_SIZE]);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_rs485_response
//
// Description:
// ------------
// Function to reply to an RS-485 poll.  Used by the host only.
//
// Design Notes:
// -------------
//
// If an alert is buffered for the polled address, it will be sent, regardless
// if that address is considered to be currently "active."  If no alerts are
// buffered, a 'no data' reply will only be generated if we consider the device
// to be currently "active."
// _____________________________________________________________________________
//
int send_rs485_response(int address)
{
  struct alert_data *p, *q;
  unsigned char index;

  // Is this a valid address?
  if ((address > 0) && (address < MAX_VALID_RADIO_ADDRESS))
  {
    // get the first working message
    p = active_alert_head[address];

    // Check if the header is valid
    if (p != NULL) {
      index = p->size - 1;
      
      // Copy the alert data into the outgoing buffer
      memcpy(tx_data, p->data, index);

      // Save the pointer to the next entry
      q = p->next;

      // Dump the current alert
      remove_alert(p);

      // Restore the pointer to the next entry
      p = q;

      // Add length and checksum and then send to UART for transmission.
      tx_data[TK_COUNT] = index + 1;
      tx_data[index] = checksum(tx_data, index);
      serialB_writeN(tx_data, (index + 1));
      return (TRUE);
    }
    // Is this unit known to us?
    else if (is_rs485_active(address))
    {
      tx_data[0] = (unsigned char)address;
      tx_data[1] = 0x04;                               // And build a 'no data' response for this address
      tx_data[2] = SB_NO_DATA_PACKET;
      tx_data[3] = checksum(tx_data, 3);
      serialB_writeN(tx_data, 4);
      return (TRUE);
    }
  }
  return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_radio_keepalive
//
// Description:
// ------------
// Generates a radio keepalive message.  Only used by the remote.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int  send_radio_keepalive(int ack)
{
    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = radio_address;
    rdata[RADIO_MSG_O_HDR_DST]      = 0;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_SIZE_ALIVE;
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_ALIVE; // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;

    if (CONFIG_REPEATER_OFF() == FALSE) {
        rdata[RADIO_MSG_O_ALIVE_HOP]    = radio_hop_count;  // Only show accurate hop count if repeater is enabled
    }
    else {
        rdata[RADIO_MSG_O_ALIVE_HOP]    = RADIO_OFFLINE_HOPS / 2; // Otherwise, show out of range
    }

    memcpy(&rdata[RADIO_MSG_O_ALT_MASK], &active_units[0], sizeof(active_units[0])); // Copy the bit mask of active units
    rdata[RADIO_MSG_O_ALIVE_ACK]    = ack;
    rdata[RADIO_MSG_O_ALIVE_BFR]    = bfr_percentage;       // Percent of buffer remaining
    rdata[RADIO_MSG_O_ALIVE_CHKSUM] = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));
    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
    return (TRUE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function   :  IsCPAC_Message
//
// Description: Checks the Response and Capability codes to determine whether
//              buffer data is a CPAC packet.
// Input      : packet buffer array.
// output     : TRUE; if the Capability and Response code denote a CPAC message;
//              FALSE; otherwise
// _____________________________________________________________________________
//
int IsCPAC_Message(unsigned char* buffer)
{
  // 0x3x (e.g. 0x31, 0x3F) Function codes are specific to the CPAC.
  // Capability code 0x01 and Function Code 0x82 are CPAC data messages.   
  if (((buffer[TK_RESPONSE_CODE] & CPAC_FUNC_H_BITS_MSK) == CPAC_FUNC_H_BITS_VAL) ||
      ((buffer[TK_RESPONSE_CODE] == SB_SLAVE_ACK) &&
       (buffer[TK_CAPABILITY_CODE] == SB_CPAC_CC)))
  {
    return TRUE;
  }
  else
  {
    return FALSE;
  }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_radio_alert
//
// Description:
// ------------
// Generates a radio response to a poll message.  Only used by the remote.
//  v5.00 -- Need a copy of this routine made for the Host
//
// Design Notes:
// -------------
// This message is asynchronously generated by the remote.
// To avoid having an excessively lengthy message, no more than 4
// alerts will be sent in each response.
// _____________________________________________________________________________
//
int send_radio_alert (struct alert_data *montrealPtr, struct alert_data *stdPtr)
{
    struct alert_data *p;
    int    max_alerts;

    if (radio_address == 0) {                   // Have we figured out our radio address yet?
        return (FALSE);                         // No - can't transmit yet.
    }

    if ((montrealPtr == NULL) && (stdPtr == NULL) ){
        return (FALSE);
    }

    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = radio_address;
    rdata[RADIO_MSG_O_HDR_DST]      = 0;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_O_ALT_DATA + 1; // Size of the message without any alerts
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_ALERT;     // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;

    if ((CONFIG_REPEATER_OFF() == FALSE) && (!host_unit)) {
        rdata[RADIO_MSG_O_ALIVE_HOP]    = radio_hop_count;  // Only show accurate hop count if repeater is enabled
    }
    else {
        rdata[RADIO_MSG_O_ALIVE_HOP]    = RADIO_OFFLINE_HOPS / 2; // Otherwise, show out of range
    }

    rdata[RADIO_MSG_O_ALT_SEQ]      = radio_sequence_number;    // Sequence number
    memcpy(&rdata[RADIO_MSG_O_ALT_MASK], &active_units[0], sizeof(active_units[0])); // Copy the bit mask of active units
    rdata[RADIO_MSG_O_ALT_ALERTS]   = 0;                    // Number of alerts in this message
    rdata[RADIO_MSG_O_ALT_BFRPCT]   = bfr_percentage;       // Percent of buffer remaining
    rdata_p = &rdata[RADIO_MSG_O_ALT_DATA];                 // Place to put the first alert (or checksum if no alerts)

    // Set upper limit
    max_alerts = RADIO_MSG_ALERTS_PER_MSG;

    // Scan the montreal message queue
    p          = montrealPtr;
    while (max_alerts && (p != NULL))                         // Is there a pending alert?
    {
        memcpy(rdata_p, p->data, p->size);
        
        // If not CPAC message, calculate RSSI average.
        if (!IsCPAC_Message(rdata_p))
        {
          CpuCalculateAverage (rdata_p);
        }

        rdata[RADIO_MSG_O_HDR_SIZE]   += p->size;
        rdata[RADIO_MSG_O_ALT_ALERTS] += 1;                 // Increase number of alerts in this message

        p->seq    = radio_sequence_number;                  // Remember the sequence number used to send this
        rdata_p  += p->size;
        --max_alerts;
        p = p->next;
    }

    // Scan the standard message queue
    p          = stdPtr;
    while (max_alerts && (p != NULL))                         // Is there a pending alert?
    {
        memcpy(rdata_p, p->data, p->size);
        
        // If not CPAC message, calculate RSSI average.
        if (!IsCPAC_Message(rdata_p))
        {
          CpuCalculateAverage (rdata_p);
        }

        rdata[RADIO_MSG_O_HDR_SIZE]   += p->size;
        rdata[RADIO_MSG_O_ALT_ALERTS] += 1;                 // Increase number of alerts in this message
        p->seq    = radio_sequence_number;                  // Remember the sequence number used to send this
        rdata_p  += p->size;
        --max_alerts;
        p = p->next;
    }

    *rdata_p   = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));
    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
    inc_radio_msg_counter();
    return (TRUE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_radio_wr
//
// Description:
// ------------
// Generates a radio Will-Relay message.  Only used by the remote.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void send_radio_wr(unsigned char dest)
{
    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = radio_address;
    rdata[RADIO_MSG_O_HDR_DST]      = dest;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_SIZE_WR;
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_WR; // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;
    rdata[RADIO_MSG_O_WR_CHKSUM]    = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));
    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_radio_rr
//
// Description:
// ------------
// Generates a radio relay request message.  Only used by the remote.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void send_radio_rr(unsigned char node, unsigned char dest)
{
    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = radio_address;
    rdata[RADIO_MSG_O_HDR_DST]      = dest;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_SIZE_RR;
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_RR; // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;
    rdata[RADIO_MSG_O_RR_NODE]      = node;
    rdata[RADIO_MSG_O_RR_CHKSUM]    = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));
    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  send_radio_ack
//
// Description:
// ------------
// Generates a radio ACK message.  Only used by the host.
//  v5.00 --- Now used by the Remote as well as the Host Radio.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int send_radio_ack(unsigned char seq, unsigned char dest)
{
    rdata[RADIO_MSG_O_HDR_SYNC1]    = RADIO_MSG_DATA_SYNC1;
    rdata[RADIO_MSG_O_HDR_SYNC2]    = RADIO_MSG_DATA_SYNC2;
    rdata[RADIO_MSG_O_HDR_SRC]      = 0;
    rdata[RADIO_MSG_O_HDR_DST]      = dest;
    rdata[RADIO_MSG_O_HDR_SIZE]     = RADIO_MSG_SIZE_ACK;
    rdata[RADIO_MSG_O_HDR_TYPE]     = RADIO_MSG_TYPE_ACK; // Message type
    rdata[RADIO_MSG_O_HDR_SERIAL_L] = radio_serial_number & 0xFF;
    rdata[RADIO_MSG_O_HDR_SERIAL_H] = radio_serial_number >> 8;
    radio_serial_number            += 1;
    rdata[RADIO_MSG_O_ACK_HOP]      = 0;
    rdata[RADIO_MSG_O_ACK_SEQ]      = seq;
    rdata[RADIO_MSG_O_ACK_CHKSUM]   = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));
    serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
    polling_list[dest].ack_counter  = MANDATORY_ACK_INTERVAL;   // Send a gratuitous ACK every third keepalive
    return (TRUE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  radio_repeater
//
// Description:
// ------------
// Repeats radio messages as required.  Only used by the remote.
// Returns TRUE if the message was repeated
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int radio_repeater(void)
{
    // First, check if this message is one of the types that's eligible
    // for a repeat.
    switch (rdata[RADIO_MSG_O_HDR_TYPE])
    {
        case RADIO_MSG_TYPE_ALIVE:
        case RADIO_MSG_TYPE_ALERT:
        case RADIO_MSG_TYPE_ACK:
            break;                          // All of these are eligible for a repeat.

        default:
            return (FALSE);                 // Ignore everything else
    }

    // Here with a message type that's eligible for repeat, check if we're repeating
    // for this particular node.

    if (is_active(repeater_active_list, sizeof(repeater_active_list) * 8, rdata[RADIO_MSG_O_HDR_DST]) ||
        is_active(repeater_active_list, sizeof(repeater_active_list) * 8, rdata[RADIO_MSG_O_HDR_SRC]))
    {
        // Correct type, and it's to/from a node that's in our repeater list,
        // check if it's a duplicate message.
        if (check_duplicate(rdata)) {       // Have we already repeated this message?
            return (TRUE);                  // Yes - skip it, but return the value like we
                                            // did repeat it so it won't get processed as a repeater candidate
        }
        mark_duplicate(rdata);              // No, remember it so we don't repeat it more than once

        // If this is an acknowledgement from the host, we need to
        // increase the hop count as we retransmit it so the final
        // recipient will get an accurate hop count

        if (rdata[RADIO_MSG_O_HDR_TYPE] == RADIO_MSG_TYPE_ACK)
        {
            rdata[RADIO_MSG_O_ACK_HOP] += 1;    // Increase the hop count
            rdata[RADIO_MSG_O_ACK_CHKSUM] = checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1));   // and correct the checksum
        }
        else {
            timer_extendLoResTimer(TIMER_LO_RES_RETRANSMIT, HOLDOFF_VALUE); // If we're repeating something other than an ACK,
                                                                            // holdoff our transmit to allow for the reply
        }
        // It's successfully run the gauntlet of tests, transmit the message
        serialA_writeN(rdata, rdata[RADIO_MSG_O_HDR_SIZE]);
        return (TRUE);
    }
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  radio_init
//
// Description:
// ------------
// Routine for doing power-up initialization on the MaxStream radio
//
// Will hang forever if the MaxStream radio fails to respond.
//
// Design Notes:
// -------------
//
// Although this won't return until the MaxStream radio has been
// successfully initialized, it will retry the '+++' escape sequence
// repeatedly until the MaxStream radio replies with 'OK' indicating
// it's now in command mode.
//
// If for some bizarre reason the MaxStream doesn't respond with 'OK'
// to one of the command strings (it decides to throw 'ERROR' instead),
// this function will hang.
// _____________________________________________________________________________
//

int radio_init_state      = RADIO_INIT_START;
int radio_init_next_state = RADIO_INIT_START;

char radio_init_bfr[16];
int  radio_init_count;
unsigned int radio_version;

#if RADIO_INIT_SPLIT_UP_CONFIG
unsigned char radio_init_buffers[4][13];
const unsigned char *radio_init_config_strings[] = {
  "ATBD3\r",
  "ATCD0\r",
  "ATFL0\r",
  "ATBR0\r",
  "ATCS0\r"
  "ATPK0x800\r",
  "ATRB0x800\r",
  "ATRO2\r",
  "ATRT2\r",
  "ATMT0\r",
  "ATRR0\r",
  radio_init_buffers[0],
  radio_init_buffers[1],
  radio_init_buffers[2],
  radio_init_buffers[3],
  "ATWR\r",
  NULL,
};
int radio_init_cfg_count;
int radio_init_cfg_size;

#endif

int radio_init(int power, int reset)
{
    static int n;
    unsigned char c;

    if (reset) {
        STATE_TRACE( RADIO_INIT_ID, NO_STATE, RESET_STEP_ID, NO_SUB_STEP_ID);
        radio_init_state = radio_init_next_state = RADIO_INIT_START;
    }

    switch (radio_init_state)
    {
        case RADIO_INIT_START:
            // bh - v5.01
//            timer_setLoResTimer(TIMER_LO_RES_RADIO_WATCHDOG, TIMEOUT_RADIO_WATCHDOG);
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display('0', 'F', 'F', 3);
            CPU_IO_MS_SHDN   = 0;                   // Assert /shutdown
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND * 2);   // Turn the radio off for two seconds
            radio_init_state = RADIO_INIT_OFF_WAIT;
            break;

        case RADIO_INIT_OFF_WAIT:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT)) {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_TIMEOUT_STEP_ID, NO_SUB_STEP_ID);
                radio_init_state = RADIO_INIT_POWER_UP;
            }
            else {
                break;
            }

        case RADIO_INIT_POWER_UP:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            CPU_IO_MS_SHDN   = 1;                   // Deassert /shutdown
            serialA_ignoreCTS(TRUE);                // Don't know if the radio is doing flow control, so ignore it.
            n = 5;
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on
            radio_init_state = RADIO_INIT_ON_WAIT;
            break;

        case RADIO_INIT_ON_WAIT:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_TIMEOUT_STEP_ID, NO_SUB_STEP_ID);
                cpu_write_led_display('P', 'P', 'P', 3);
                serialA_write('+');
                serialA_write('+');
                serialA_write('+');

                // Wait five seconds after turning the radio on
                timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND * 5);

                radio_init_next_state = RADIO_INIT_ECHO_OFF;
                radio_init_state      = RADIO_INIT_WAIT_O;
            }
            else {
                break;
            }

        case RADIO_INIT_WAIT_O:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, radio_init_next_state, n);
                if (--n == 0) {                                     // Have we tried the '+++' sequence too many times?
                    radio_init_state = RADIO_INIT_START;            // Yes - Power-cycle the radio again.
                }
                else {
                    radio_init_state = RADIO_INIT_ON_WAIT;          // No - Just try sending the '+++' again.
                }
                break;
            }

            if ((serialA_read(&c) != FALSE) && (c == 'O')) {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_CHAR_STEP_ID, c);
                radio_init_state = RADIO_INIT_WAIT_K;
            }
            break;

        case RADIO_INIT_WAIT_K:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT))
            {

                STATE_TRACE( RADIO_INIT_ID, radio_init_state, radio_init_next_state, n);
                if (--n == 0) {                                     // Have we tried the '+++' sequence too many times?
                    radio_init_state = RADIO_INIT_START;            // Yes - Power-cycle the radio again.
                }
                else {
                    radio_init_state = RADIO_INIT_ON_WAIT;          // No - Just try sending the '+++' again.
                }
                break;
            }

            if ((serialA_read(&c) != FALSE) && (c == 'K'))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_CHAR_STEP_ID, c);
                radio_init_state = RADIO_INIT_WAIT_CR;
            }
            break;

        case RADIO_INIT_WAIT_CR:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, radio_init_next_state, n);
                radio_init_state = radio_init_next_state;
                break;
            }

            if ((serialA_read(&c) != FALSE) && (c == '\r'))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_CHAR_STEP_ID, c);
                radio_init_state = radio_init_next_state;
            }
            break;

        case RADIO_INIT_GET_VALUE:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO_INIT))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, radio_init_next_state, n);
                if (--n == 0) {                                     // Have we tried the '+++' sequence too many times?
                    radio_init_state = RADIO_INIT_START;            // Yes - Power-cycle the radio again.
                }
                else {
                    radio_init_state = RADIO_INIT_ON_WAIT;          // No - Just try sending the '+++' again.
                }
                break;
            }

            if (serialA_read((unsigned char *)&radio_init_bfr[radio_init_count]) != FALSE)
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_CHAR_STEP_ID, c);
                if (radio_init_bfr[radio_init_count] == '\r')
                {
                    if (radio_init_count != 0)                  // Only terminate if the CR isn't first character
                    {
                        radio_init_bfr[radio_init_count] = 0;
                        radio_init_state = radio_init_next_state;
                    }
                    break;
                }
                ++radio_init_count;
            }
            break;

        case RADIO_INIT_ECHO_OFF:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display(' ', 'P', '1', 3);
            serialA_writeN("ATE0\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_next_state = RADIO_INIT_QUERY_VER;
            radio_init_state      = RADIO_INIT_WAIT_O;
            break;

        case RADIO_INIT_QUERY_VER:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display(' ', 'P', '2', 3);
            serialA_writeN("ATVR\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_DT;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_DT:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            radio_version = atoi(radio_init_bfr);                       // Store away the radio version #

            if ((radio_version == 2020) || (strcmp(((char *) radio_version), "ERROR") == 0))                                // Unsupported version!
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_VERSION_ERROR_STEP_ID, NO_SUB_STEP_ID);
                cpu_write_led_display('E', '2', '0', 0);
                radio_init_state = RADIO_INIT_VERSION_ERROR;
                break;
            }

            cpu_write_led_display(' ', 'P', '3', 3);
            serialA_writeN("ATDT\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_HP;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_HP:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // First, check the answer to the DT query
            if (atoi(radio_init_bfr) != (cpu_readNetDIP() & 0x0F))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_HP_ERROR_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
                break;
            }

            // DT is set correctly.  Query the HP setting
            cpu_write_led_display(' ', 'P', '4', 3);
            serialA_writeN("ATHP\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_PL;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_PL:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // First, check the answer to the HP query
            if (atoi(radio_init_bfr) != ((cpu_readNetDIP() & 0x0F) % 9))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_HP_ERROR_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
                break;
            }

            // HP is set correctly.  Query the PL setting
            cpu_write_led_display(' ', 'P', '5', 3);
            serialA_writeN("ATPL\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_MT;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_MT:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // First, check the answer to the PL query
            if ((power && atoi(radio_init_bfr) != 1) || (power == 0 && atoi(radio_init_bfr) != 4))
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_BUFFER_SIZE_ERROR_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
                break;
            }

            // PL is set correctly.  Query the RR setting
            cpu_write_led_display('1', 'P', '6', 3);
            serialA_writeN("ATMT\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_RR;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_RR:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // First, check the answer to the MT query
            if (atoi(radio_init_bfr) != 0)
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_BUFFER_SIZE_ERROR_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
                break;
            }

            // PL is set correctly.  Query the RR setting
            cpu_write_led_display(' ', 'P', '6', 3);
            serialA_writeN("ATRR\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_QUERY_RT;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_QUERY_RT:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // First, check the answer to the RR query
            if (atoi(radio_init_bfr) != 0)
            {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_BUFFER_SIZE_ERROR_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
                break;
            }

            // RR is set correctly.  Query the RT setting
            cpu_write_led_display(' ', 'P', '7', 3);
            serialA_writeN("ATRT\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_CHECK_RT;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_CHECK_RT:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // Check the answer to the RT query
            if (atoi(radio_init_bfr) != 2) {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_CONFIG_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_BYPASS_CONFIG
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#else
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;
#endif
#endif
            }
            else {
                STATE_TRACE( RADIO_INIT_ID, radio_init_state, RESET_ONLINE_STEP_ID, NO_SUB_STEP_ID);
#if RADIO_INIT_FAIL_CONFIG
#if RADIO_INIT_SPLIT_UP_CONFIG
                radio_init_state = RADIO_INIT_SEND_CFG1;    // Configuration is up-to-date, just go online
#else
                radio_init_state = RADIO_INIT_SEND_CFG;    // Configuration is up-to-date, just go online
#endif
#else
                radio_init_state = RADIO_INIT_GO_ONLINE;    // Configuration is up-to-date, just go online
#endif
            }
            break;

        case RADIO_INIT_SEND_CFG:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display(' ', 'P', '8', 3);
            serialA_writeN("ATBD3,CD0,FL0,BR0,CS0,", 22);
            serialA_writeN("PK0x100,RB0x100", 15);
            serialA_writeN(",RO2,RT2,MT0,RR0,", 17);
            n = sprintf((char *)rdata,
                        "PL%d,HP%d,DT%d,MY%d,WR\r",
                        power ? 1 : 4,
                        ((cpu_readNetDIP() & 0x0F) % 9), // Read the Network ID from the DIP switches
                        (cpu_readNetDIP() & 0x0F),
                        (cpu_readNetDIP() & 0x0F));
            serialA_writeN(rdata, n);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND * 2);   // Wait one second after turning the radio on

            radio_init_next_state = RADIO_INIT_GO_ONLINE;
            radio_init_state      = RADIO_INIT_WAIT_O;
            break;

#if RADIO_INIT_SPLIT_UP_CONFIG
        case RADIO_INIT_SEND_CFG1:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display('P', 'P', '8', 3);
            sprintf((char *)radio_init_buffers[0],
                        "ATPL%d\r",
                        power ? 1 : 4);
            sprintf((char *)radio_init_buffers[1],
                        "ATDT%d\r",
                        (cpu_readNetDIP() & 0x0F));
            n = sprintf((char *)radio_init_buffers[2],
                        "ATHP%d\r",
                        ((cpu_readNetDIP() & 0x0F) % 9));
            n = sprintf((char *)radio_init_buffers[3],
                        "ATMY%d\r",
                        (cpu_readNetDIP() & 0x0F));

            radio_init_cfg_count = 0;
            radio_init_cfg_size =  strlen((char *) radio_init_config_strings[radio_init_cfg_count]);
            serialA_writeN((unsigned char *)radio_init_config_strings[radio_init_cfg_count], radio_init_cfg_size);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND * 2);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_SEND_CFG_STEP;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;

        case RADIO_INIT_SEND_CFG_STEP:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display('P', 'P', '9', 3);
            radio_init_cfg_count++;
            if (radio_init_config_strings[radio_init_cfg_count] == NULL) {
              radio_init_state = RADIO_INIT_GO_ONLINE;
              break;
            }
            radio_init_cfg_size =  strlen((char *)radio_init_config_strings[radio_init_cfg_count]);
            serialA_writeN((unsigned char *)radio_init_config_strings[radio_init_cfg_count], radio_init_cfg_size);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND * 2);   // Wait one second after turning the radio on

            radio_init_count      = 0;
            radio_init_next_state = RADIO_INIT_SEND_CFG_STEP;
            radio_init_state      = RADIO_INIT_GET_VALUE;
            break;
#endif

        case RADIO_INIT_GO_ONLINE:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display(' ', 'P', '9', 3);
            serialA_writeN("ATCN\r", 5);
            timer_setLoResTimer(TIMER_LO_RES_RADIO_INIT, TIMER_LO_RES_TICKS_PER_SECOND);   // Wait one second after turning the radio on

            radio_init_next_state = RADIO_INIT_FLUSH;
            radio_init_state      = RADIO_INIT_WAIT_O;
            break;

        case RADIO_INIT_FLUSH:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            cpu_write_led_display(' ', 'F', 'L', 3);
            if (serialA_txBufEmpty())                   // Wait until the init string is sent
            {
                serialA_ignoreCTS(FALSE);               // Start using hardware flow control now
                CPU_IO_DEBUG_LED1 = 1;                  // Make sure the red light is on
                radio_init_state = RADIO_INIT_COMPLETE;
            }
            break;

        case RADIO_INIT_VERSION_ERROR:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            break;


        case RADIO_INIT_COMPLETE:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // bh - v5.01
//          timer_setLoResTimer(TIMER_LO_RES_RADIO_WATCHDOG, TIMEOUT_RADIO_WATCHDOG);
            return (TRUE);

        default:
            STATE_TRACE( RADIO_INIT_ID, radio_init_state, NO_STEP_ID, NO_SUB_STEP_ID);
            radio_init_state = RADIO_INIT_START;
    }
    return (FALSE);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  read_address_switch
//
// Description:
// ------------
// Reads the rotary switch on the Gen2 WABS hardware and returns the value
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
unsigned char read_address_switch(void)
{
    return ((P2IN >> 5) ^ 0x07);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  host_radio_state_machine
//
// Description:
// ------------
// State machine for handling messages from the remote over the MaxStream radio
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void host_radio_state_machine(void)
{
    static unsigned char *rdata_p   = rdata;
    static int radio_state          = RADIO_INIT;
    static unsigned int byte_count  = 0;
           unsigned char remote_address, count, cs;
           int i;
           unsigned char alert_buffer[TK_MSG_MAX_SIZE];
           union {
            unsigned char byte_data[4];
            unsigned int int_data[2];
            unsigned long long_data;
           } trans_buff;

    serialA_restart_output();

    // bh - v5.01 - reset the Host radio once every 24 hours
    if (CpuMonitorWatchdog ())
    {
#if RADIO_WATCHDOG_ENABLE
        radio_init (CONFIG_RADIO_POWER(), 1);
        radio_state = RADIO_INIT;
#endif
    }

    switch (radio_state)
    {
        case RADIO_INIT:
            if (radio_init(CONFIG_RADIO_POWER(), 0)) {
                // Go to idle state
                radio_state = RADIO_IDLE;

                // Hold for a message transmit time.
                timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM2_INTERVAL() + TIMER_200MS));
            }
            break;

        case RADIO_IDLE:
            rdata_p     = rdata;
            byte_count  = 2;
            radio_state = RADIO_SYNC_1;      // Yes - look for the sequence number next

        case RADIO_SYNC_1:
            // Check for an inbound character
            if (serialA_read(rdata_p) != 0)
            {
                // Do we have the beginning of the FOOF?
                if (*rdata_p == RADIO_MSG_DATA_SYNC1)
                {
                    // advance to the next character
                    ++rdata_p;
                    timer_setLoResTimer(TIMER_LO_RES_RADIO, TIMEOUT_RADIO_INPUT);

                    // Yes - look for the end of the FOOF next
                    radio_state = RADIO_SYNC_2;
                }
                else {
                    // Extend the timeout
                    timer_extendLoResTimer(TIMER_LO_RES_RETRANSMIT, HOLDOFF_VALUE);

                    // Note - unexpected characters seen
                    radio_char_unexpected++;
                }
            }
            else
            {
                // When the radio waves are quiet, look to see if an outgoing message is available to send
                CpacCheckForOutgoingMessages ();
            }
            break;                      // No - continue to look for the beginning of the FOOF

        case RADIO_SYNC_2:
            if (serialA_read(rdata_p) && (*rdata_p == RADIO_MSG_DATA_SYNC2))       // Do we have the end of the FOOF?
            {
                ++rdata_p;
                radio_state = RADIO_DATA;      // Yes - look for the sequence number next
            }
            else
            {
                if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO)) {
                    radio_state = RADIO_IDLE;
                }
                break;                      // No - continue to look for the end of the FOOF
            }
            break;

        case RADIO_DATA:
            if (serialA_read(rdata_p))       // Do we have the next data byte?
            {
                ++rdata_p;
                ++byte_count;

                if ((byte_count < sizeof(rdata)) && ((byte_count <= RADIO_MSG_O_HDR_SIZE) || (byte_count < rdata[RADIO_MSG_O_HDR_SIZE]))) {
                    break;                  // Go around and get the next byte
                }
            }
            else
            {
                if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO)) {
                    radio_state = RADIO_IDLE;
                }
                break;                      // No - continue to look for the data bytes
            }

        case RADIO_PROCESS:
            // Here with complete command, validate checksum
            if (checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1)) != rdata[rdata[RADIO_MSG_O_HDR_SIZE] - 1])   // Good checksum?
            {
                radio_state = RADIO_IDLE;
                return;
            }

            // Check if the destination address is valid
            if (rdata[RADIO_MSG_O_HDR_DST] != radio_address)
            {
              // If not - stop it
              radio_state = RADIO_IDLE;
              return;
            }

            // Note the message source
            remote_address = rdata[RADIO_MSG_O_HDR_SRC];

            switch (rdata[RADIO_MSG_O_HDR_TYPE])
            {
                // v5.00 -- Host can now receive ACKs from remote radios
                case RADIO_MSG_TYPE_ACK:                                // Response to Alert
                    // bh - v2.10
                    remove_ack_alert(rdata[RADIO_MSG_O_ACK_SEQ], active_command_head, FALSE);
                    radio_sequence_number += 1;                         // Increase the sequence number for the next message
                    if (radio_sequence_number == 0) {                   // Never use sequence number of 0, it isn't valid.
                        radio_sequence_number = 1;
                    }

                    // bh - v2.11 - lower the retransmit timer by a half
                    timer_loResTimer[TIMER_LO_RES_RETRANSMIT] /= 2;   // lower the retransmit timer
                    break;

                case RADIO_MSG_TYPE_ALERT:                              // Response to poll
                    if (check_duplicate(rdata))     // Is this a duplicate of a message we've already seen?
                    {
                        // v2.15 - send an ACK even to a duplicate message.  Otherwise, the sender will keep sending the same message
                        send_radio_ack(rdata[RADIO_MSG_O_ALT_SEQ], rdata[RADIO_MSG_O_HDR_SRC]);

                        // Restart the transmit timer after an ACK has been sent.
                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM3_INTERVAL() + TIMER_600MS));
                        radio_state = RADIO_IDLE;    // Yes - ignore it.
                        break;
                    }

                    // v2.15 - formerly marked as duplicate here.  Now moved to after the ACK
                    polling_list[remote_address].bfr           = rdata[RADIO_MSG_O_ALT_BFRPCT];
                    mark_active_by_mask(remote_address, *((unsigned long *)&rdata[RADIO_MSG_O_ALT_MASK]));
                    i       = rdata[RADIO_MSG_O_HDR_SIZE] - RADIO_MSG_O_ALT_DATA - 1;   // Get the amount of alert data carried in this message
                    rdata_p = &rdata[RADIO_MSG_O_ALT_DATA];                 // Point to the beginning of the alert data

                    while (i > 0)                                       // Until we've consumed all the data
                    {
                        // v5.00: allow any message size and any command type
                        count = *(rdata_p + 1);

                        // get the received checksum
                        cs = *(rdata_p + (count-1));
                        if (cs == checksum(rdata_p, (int)(count-1)))
                        {
                            rdata[RADIO_MSG_O_ALT_ALERTS] -= 1;

                            // We wait until here to include the bits from the WABS address, since it's
                            // possible that a future design enhancement would be up to 255 CARTS connected
                            // to each of 255 WABS.  This is a scary thought over a 9600 baud radio link, but by
                            // keeping the WABS and CART addresses seperate until this point, we can later
                            // support more CARTS by merely changing the protocol between the host WABS
                            // and the CMC without having to change the radio protocol at all.

                            // v5.00 --- allow any address, even 0.
                            rdata_p[RS485_MSG_O_ADDR] |= (remote_address & 0xE0); // Include the bits from the WABS address selector
                            rdata_p[rdata_p[RS485_MSG_O_SIZE] - 1] = checksum(rdata_p, (int)(rdata_p[RS485_MSG_O_SIZE] - 1));  // And recalculate the checksum

                            // Check for a SB data message
                            if (rdata_p[RS485_MSG_O_FUNC] == SB_DATA_PACKET) {
                              // Convert the RFID
                              trans_buff.byte_data[0]             = rdata_p[RS485_MSG_O_TXID0];
                              trans_buff.byte_data[1]             = rdata_p[RS485_MSG_O_TXID1];
                              trans_buff.byte_data[2]             = rdata_p[RS485_MSG_O_TXID2];
                              trans_buff.byte_data[3]             = rdata_p[RS485_MSG_O_TXID3];
                              trans_buff.long_data                <<= 2;

                              if ((trans_buff.byte_data[3] & 0xF0) == 0x60) {
                                // Copy the SB2000 message
                                memcpy(alert_buffer, &rdata_p[RS485_MSG_O_ADDR],
                                     rdata_p[RS485_MSG_O_SIZE]);
                                }
                              else {
                                /*
                                 * Convert the SuperBus message to SuperBus 2000
                                 */

                                // Packet Header
                                alert_buffer [TK_ADDRESS]           = rdata_p[RS485_MSG_O_ADDR];
                                alert_buffer [TK_RESPONSE_CODE]     = SB_SLAVE_ACK;
                                alert_buffer [TK_CAPABILITY_CODE]   = 0;
                                alert_buffer [TK_STATUS_BYTE]       = 0;
                                alert_buffer [TK_NOISE_FLOOR_RIGHT] = 0;
                                alert_buffer [TK_NOISE_FLOOR_LEFT]  = 0;
                                alert_buffer [TK_PACKET_COUNT]      = 8;

                                // Construct basic message data
                                alert_buffer [TK_RSSI]              = rdata_p[RS485_MSG_O_RSSI];
                                alert_buffer [TK_PACKET_TYPE]       = 0;
                                alert_buffer [TK_REPEATER_ID]       = 0;

                                // Load the RFID data
                                alert_buffer [TK_RFID_3]            = trans_buff.byte_data[1];
                                alert_buffer [TK_RFID_2]            = trans_buff.byte_data[2];
                                alert_buffer [TK_RFID_1_DEVICETYPE] = trans_buff.byte_data[3];

                                // Convert the Battery Low, F1 - F5
                                trans_buff.byte_data[0]             = rdata_p[RS485_F123];
                                trans_buff.byte_data[1]             = rdata_p[RS485_F45];
                                trans_buff.int_data[0]              <<= 2;

                                // Load the Battery Low, F1 - F5 data
                                alert_buffer [TK_BATTERY_LOW_F1_F2] = trans_buff.byte_data[0];
                                alert_buffer [TK_F3_F4_F5]          = trans_buff.byte_data[1];

                                // set the count byte to the new count value
                                alert_buffer [TK_COUNT] = TK_CHKSUM + 1;

                                // Calculate the checksum
                                alert_buffer [(alert_buffer[TK_COUNT] - 1)] = checksum(alert_buffer, (tx_data[RS485_MSG_O_SIZE] - 1));
                              }
                            }
                            // Process all other messages
                            else {
                              // Copy the SB2000 message
                              memcpy(alert_buffer, &rdata_p[RS485_MSG_O_ADDR],
                                     rdata_p[RS485_MSG_O_SIZE]);
                            }
                            
                            // Check whether this packet is a CPAC data packet.
                            if (IsCPAC_Message(alert_buffer) && (PacketSize >= TK_MIN_DATA_SIZE))
                            {
                              // YES - Call `add_alert` with CPAC flag enabled (WILL NOT apply RSSI filtering).
                              add_alert(&alert_buffer[0], TRUE);
                            }
                            else
                            {
                              // NO - Call `add_alert` with CPAC flag disabled. (WILL apply RSSI filtering)
                              add_alert(&alert_buffer[0], FALSE);
                            }

                            i -= *(rdata_p + RS485_MSG_O_SIZE);
                            rdata_p += *(rdata_p + RS485_MSG_O_SIZE);
                        }
                        else    // Garbled alert data, don't bump the sequence number
                        {
                            radio_state = RADIO_IDLE;                          // No - ignore the message
                            return;
                        }
                    }
                    if (rdata[RADIO_MSG_O_ALT_ALERTS] == 0)             // Did we consume all the alerts?
                    {
                        send_radio_ack(rdata[RADIO_MSG_O_ALT_SEQ], rdata[RADIO_MSG_O_HDR_SRC]);

                        // Restart the transmit timer after an ACK has been sent.
                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM3_INTERVAL() + TIMER_600MS));

                        // v2.15 - don't mark as duplicate until the message is ACK'ed
                        mark_duplicate(rdata);          // Keep track of this message so we won't process it a second time
                                                        // if it gets retransmitted by a repeater.
                    }

                    break;

                case RADIO_MSG_TYPE_ALIVE:
                    // Keepalive message from a remote
                    polling_list[remote_address].bfr           = rdata[RADIO_MSG_O_ALIVE_BFR];
                    mark_active_by_mask(remote_address, *((unsigned long *)&rdata[RADIO_MSG_O_ALIVE_MASK]));

                    // Was acknowledgement requested, or is it time to send one anyway?
                    if (rdata[RADIO_MSG_O_ALIVE_ACK] || (polling_list[rdata[RADIO_MSG_O_HDR_SRC]].ack_counter == 0))
                    {
                        send_radio_ack(0, rdata[RADIO_MSG_O_HDR_SRC]);  // Yes - send it.

                        // Restart the transmit timer after an ACK has been sent.
                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM3_INTERVAL() + TIMER_600MS));
                    }
                    break;

                default:                                            // We don't handle any other messages
                    break;
            }
            radio_state = RADIO_IDLE;
            break;

        default:
            radio_state = RADIO_IDLE;
            break;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  host_rs485_state_machine
//
// Description:
// ------------
// State machine for handling poll requests from the Lifeline system controller.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void host_rs485_state_machine(void)
{
    static unsigned char size;
    static int poll_state    = POLL_IDLE;

    switch (poll_state)
    {
        case POLL_IDLE:
            if (serialB_txDone() == FALSE) {
                break;
            }

            poll_state   = POLL_DATA;
            data_p       = rx_data;
            data_counter = 0;
            // Fall through into POLL_DATA

        case POLL_DATA:
            if (serialB_read(data_p))           // Did we get a byte of data?
            {
                // v5.00 --  The First Byte is the address byte.
                // The Second Byte is the count byte.
                // The Third Byte is the Function Code byte.
                if ((data_counter) == TK_RESPONSE_CODE)
                {
                    // Inspect the partial packet when 4 characters have been received.
                    if (SB_CheckFunctionCode (rx_data))
                    {
                        // The Function Code byte is illegal!
                        SB_RotateReceiver (rx_data);
                        return;
                    }
                }

                ++data_p;                      // Yes, bump pointer and counter
                *data_p = 0;                   // Initialize the next index.
                ++data_counter;

                // v 2.16 --- allow a 4- or 5-byte poll command
                timer_ResetTimer();        // Reset the inter-character timer
                if ((data_counter > TK_RESPONSE_CODE)
                  && (data_counter >= rx_data[RS485_MSG_O_SIZE])) {       // Poll command is four bytes
                    
                    // Copy rx_data information to tx_data (working buffer)
                  memcpy(tx_data, rx_data, data_counter);
                  poll_state = POLL_DONE;    // If we have that many, we're done
                }
            }
            else if ((data_counter > 0) && (timer_ElapsedTimer() > 10)) {   // Timeout during message?
                poll_state = POLL_IDLE;                                 // Yes - discard partial message
            }
            break;

        // Here to process the reply.
        case POLL_DONE:     // Here with complete command, validate checksum
            size = tx_data[RS485_MSG_O_SIZE];
            if (checksum(tx_data, size-1) != tx_data[size-1])   // Bad checksum?
            {
                poll_state = POLL_IDLE;                         // Yes - ignore the message
                break;
            }

            // v5.00 - 12/13/07 -- first make sure this is a valid network address
            if (!(is_rs485_active (tx_data [RS485_MSG_O_ADDR])))                       // Is this unit known to us?
                {
                poll_state = POLL_IDLE;                         // Yes - ignore the message
                break;
            }

            // Here with completed command and valid checksum.
            radioactive = ACTIVITY_COUNT_RS485;

            // Check if the packet is SuperBus

            switch (tx_data[RS485_MSG_O_FUNC])                     // Dispatch on command byte
            {
              // This is either a Poll Request or a Reset Alarm Request
              case SB_POLL2:
                    // A reset alarm request has a count=5 and a Capability Code = 0x01.
                    if ((size == 5) && (tx_data[RS485_MSG_O_AGC] == 1 ))
                    {
                        // This must be a reset alarm request - save the alarm request
                        CpacHostRadioSaveMessage (tx_data);
                    }
                    // Then go ahead and transmit a poll response:
                    else if (size != 4) {
                        break;
                    }

                // This is a Poll Request
                case SB_POLL1:
                case SB_POLL_2000:
                    send_rs485_response((int)(tx_data[RS485_MSG_O_ADDR])); // Send any alerts for this address.
                    break;

                // Certain commands bytes are coming in from slaves or the Master and should be thrown away
                case SB_HOST_ACK:
                case SB_NO_DATA_PACKET:
                case SB_SLAVE_ACK:
                case SB_JUST_RESET:
                case SB_DATA_PACKET:
                case SB_ACK:
                    break;

                // Check to see if this is a CPAC command:
                default:
                    // Check for Capability Code = 1.
                    if (tx_data[RS485_MSG_O_AGC] == 1 )
                    {
                        // This is a CPAC command - save it.
                        CpacHostRadioSaveMessage (tx_data);
                        send_rs485_response((int)(tx_data[RS485_MSG_O_ADDR]));
                    }
                    break;
            }
            poll_state = POLL_IDLE;
            break;

        default:
            poll_state = POLL_IDLE;
            break;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  remote_radio_state_machine
//
// Description:
// ------------
// State machine for handling the MaxStream radio.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
unsigned int last_ack;
void remote_radio_state_machine(void)
{
    static int            radio_state = RADIO_INIT;
    static unsigned char  *rdata_p     = rdata;
    static unsigned int   byte_count;
           unsigned char  count, cs;

    serialA_restart_output();

    if ((radioactive == 0) && (radio_hop_count != RADIO_OFFLINE_HOPS))    // If we haven't heard from the host in too long
    {
        radio_hop_count    = RADIO_OFFLINE_HOPS;  // set our hop count to max to indicate we're not in communications with the host.
        radio_relay_active = 0;
        memset(repeater_active_list, 0, sizeof(repeater_active_list));  // And blow away the list of nodes that we're repeating...they'll likely find a new repeater before we come back online
        radio_error_counter += 1;                   // Going offline counts as an error, too.

        // bh - v2.10 - activate hopping algorithm almost immediately
        keepalive_counter = MAX_TRANSMIT_TRIES;
    }

    // bh - v5.01 - If the remote has a success rate under 45%, reset the radio once every 12 hours
    if (CpuMonitorWatchdog ())
    {
#if RADIO_WATCHDOG_ENABLE
        STATE_TRACE( REMOTE_RADIO_ID, NO_STATE, RESET_STEP_ID, NO_SUB_STEP_ID);
        radio_error_counter += 1;               // Radio watchdog counts as an error
        radio_init(CONFIG_RADIO_POWER(), 1);
        radio_state = RADIO_INIT;
#endif
    }

    switch (radio_state)
    {
        case RADIO_INIT:
            if (radio_init(CONFIG_RADIO_POWER(), 0)) {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, RADIO_IDLE, NO_SUB_STEP_ID);
                // Go to idle state
                radio_state = RADIO_IDLE;

                // Hold for a message transmit time.
                timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM2_INTERVAL() + TIMER_200MS));
            }
            break;

        case RADIO_IDLE:
            // STATE_TRACE( REMOTE_RADIO_ID, radio_state, RADIO_IDLE, NO_SUB_STEP_ID);
            rdata_p    = rdata;
            byte_count = 2;
            // Fall through into RADIO_SYNC_1

        case RADIO_SYNC_1:
            // STATE_TRACE( REMOTE_RADIO_ID, radio_state, NO_STEP_ID, NO_SUB_STEP_ID);
            // Check if character is received
            if (serialA_read(rdata_p) != 0)
            {
              STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_CHAR_STEP_ID, *rdata_p);
              // Do we have the beginning of the FOOF?
              if (*rdata_p == RADIO_MSG_DATA_SYNC1)
              {
                  // Collect the starting synch character
                  ++rdata_p;
                  timer_setLoResTimer(TIMER_LO_RES_RADIO, TIMEOUT_RADIO_INPUT);

                  // Yes - look for the end of the FOOF next
                  radio_state = RADIO_SYNC_2;
              }
              else {
                  // Extend the timeout
                  timer_extendLoResTimer(TIMER_LO_RES_RETRANSMIT, HOLDOFF_VALUE);

                  // Note - unexpected characters seen
                  radio_char_unexpected++;

                  // Stay in looking for Synch 1
                  radio_state = RADIO_SYNC_1;
              }
            }

            //
            // Haven't received anything....check if it's time to transmit
            //

            // v5.00 - Skip the transmitting section if start of FOOF has been received.
            else if (timer_chkLoResTimerExpired(TIMER_LO_RES_RETRANSMIT))
            {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_XMITTOUT_STEP_ID, NO_SUB_STEP_ID);
                // Is the use of a repeater by this unit enabled?
                // Have we tried to talk to the host too many times without a response?
                // Do we have a good candidate for relaying our messages?
                // And finally, are we NOT in radio quality diagnostic mode?

                // bh - v2.10 - only enter this block if radio_relay_active == 0!
                if ((CONFIG_REPEATER_OFF() == FALSE)         &&
                    (keepalive_counter > MAX_TRANSMIT_TRIES) &&
                    (radio_relay_active == 0)          &&
                    ((DIAG_RADIO_RSSI() == 0) && ((DIAG_RADIO_QUALITY() == 0) || (button_state != BUTTON_STATE_HELD)))) // Too many tries?
                {
                    // bh - v2.10
//                  radio_relay_active    = 0;                                  // Yes - clear any active repeaters

                    if (radio_relay_candidate)                                  // Yes - do we have a relay candidate?
                    {
                        if (keepalive_counter > (MAX_TRANSMIT_TRIES * 2))       // Yes we do, have we tried it too many times?
//                        if (keepalive_counter > (MAX_TRANSMIT_TRIES * 3))       // bh - v2.10
                        {
                            radio_relay_ignore    = radio_relay_candidate;      // Yes - ignore the current relay candidate
                            radio_relay_candidate = 0;                          // and start the relay discovery process all over again
                        }
                        else // Try to send another relay request
                        {
                            if ((keepalive_counter % 3) == 0)
                            {
                                send_radio_rr(radio_address, radio_relay_candidate);   // Send a relay request to our current candidate relay
                                radio_relay_hops      = 255;
//                              keepalive_counter     = 0;

                                // bh - v2.10
                                keepalive_counter++;

                                retransmit_interval = TIMEOUT_RETRANSMIT;           // Reset the retransmit interval to default value
                                timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, RETRANSMIT_VALUE);
                                timer_setLoResTimer(TIMER_LO_RES_ALIVE, (TIMEOUT_KEEPALIVE + RANDOM_INTERVAL()) / 2); // And cut the keepalive interval in half
                                break;
                            }
                        }
                    }
                    else if (keepalive_counter > (MAX_TRANSMIT_TRIES * 5)) // If we've looked for another repeater for a REALLY long time
//                    else if (keepalive_counter > (MAX_TRANSMIT_TRIES * 8)) // bh - v2.10
                    {
                        radio_relay_candidate = radio_relay_ignore;
                        radio_relay_ignore    = 0;                         // Stop ignoring the last one we tried
                        if (radio_relay_candidate) {                       // Did we have a candidate that we were ignoring?
                            keepalive_counter = MAX_TRANSMIT_TRIES + 1;    // Yes - try the old one again
                        }
                        else {
                            keepalive_counter = 0;                          // No - Start the whole bloody process over again
                        }
                    }
                }

                if (send_radio_alert(active_alert_head[REMOTE_MONTREAL_ADDRESS], active_alert_head[REMOTE_STANDARD_ADDRESS]))
                    // Do we need to send an alert?
                {
                    STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_TRANSMIT_STEP_ID, NO_SUB_STEP_ID);
                    // Set the retransmit timer to give the host time to reply.
                    // Note that we extend the retransmit timer by (v5.00)ONE second
                    // for each relay that the ACK message will travel through,
                    // to make sure we don't step on our returning ACK with a
                    // repeat of the previous alert.

                    if (radio_hop_count != RADIO_OFFLINE_HOPS) {
//                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, RETRANSMIT_VALUE + (radio_hop_count * TIMER_LO_RES_TICKS_PER_SECOND * 2));
                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, RETRANSMIT_VALUE + (radio_hop_count * TIMER_LO_RES_TICKS_PER_SECOND));        //v5.00
                    }
                    else {
//                      timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, RETRANSMIT_VALUE);
                        // v5.00 - open random retransmit window to 200-2300ms
                        timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM2_INTERVAL() + TIMER_600MS));
                    }

                    // Each time we retransmit an alert, increase the basic
                    // retransmit interval by 25% to provide somewhat of a
                    // back-off in our repeat rate, which will improve radio
                    // network congestion when large numbers of alerts are
                    // being transmitted.

                    if (retransmit_interval < (TIMEOUT_KEEPALIVE / 4)) {
                        retransmit_interval += retransmit_interval / 16;     // And increase the next interval by (bh - v2.10) 06%
                    }

                    // Each time we send an alert message, that eliminates
                    // the need for a keepalive message to be sent, however
                    // we don't want to start a completely new keepalive
                    // interval, as that tends to cause all the WABS with
                    // CARTS within range of each other to synchronize their
                    // keepalive schedules, causing radio collisions.  To
                    // prevent this, we merely "extend" the keepalive time to
                    // something between 75% and 125% of the original interval.

                    if (radio_hop_count == RADIO_OFFLINE_HOPS) {
                        timer_extendLoResTimer(TIMER_LO_RES_ALIVE, TIMEOUT_KEEPALIVE / 2);
                    }
                    else {
                        timer_extendLoResTimer(TIMER_LO_RES_ALIVE, TIMEOUT_KEEPALIVE);
                    }

                    keepalive_counter += 1;                 // Count this against the number of tries even if we have a relay!
                }
                else if (timer_chkLoResTimerExpired(TIMER_LO_RES_ALIVE))    // No WR or alert messages, is it time to send a keepalive?
                {
                    STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_ALIVE_STEP_ID, NO_SUB_STEP_ID);
                    if ((button_state == BUTTON_STATE_HELD) && (DIAG_RADIO_QUALITY() || DIAG_RADIO_RSSI())) // Are we in radio diagnostic mode with the button held?
                    {
                        send_radio_keepalive(1);            // Yes, send ack'ed keepalives as fast as we can
                        inc_radio_msg_counter();
                        timer_setLoResTimer(TIMER_LO_RES_ALIVE, TIMEOUT_RETRANSMIT + (RANDOM_INTERVAL()/ 10));
                    }
                    else                                    // Normal keepalive mode
                    {
                        if (radio_hop_count != RADIO_OFFLINE_HOPS)  // Have we found the host yet?
                        {
                            send_radio_keepalive(0);            // Yes, send keepalive without ack request
                            radio_reply_expected = 0;
                        }
                        else
                        {
                            send_radio_keepalive(1);            // No - send keepalive with an ack request
                            inc_radio_msg_counter();
//                            timer_setLoResTimer(TIMER_LO_RES_ALIVE, (TIMEOUT_KEEPALIVE + RANDOM_INTERVAL())/ 4); // And cut the keepalive interval in half
                        }

                        // bh - v2.10 - keepalive interval has been reduced to 16-20 seconds
                        timer_setLoResTimer(TIMER_LO_RES_ALIVE, TIMEOUT_KEEPALIVE + RANDOM_INTERVAL());
                        keepalive_counter += 1;
                    }
                }
            }
            break;

        case RADIO_SYNC_2:
            if (serialA_read(rdata_p) && (*rdata_p == RADIO_MSG_DATA_SYNC2))       // Do we have the end of the FOOF?
            {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_SYNCH2_STEP_ID, *rdata_p);
                ++rdata_p;
                radio_state = RADIO_DATA;      // Yes - look for the sequence number next
            }
            else
            {
                if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO)) {
                    STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_SYNCH2_TOUT_STEP_ID, NO_SUB_STEP_ID);
                    radio_state = RADIO_IDLE;
                }
                break;                      // No - continue to look for the end of the FOOF
            }
            break;

        case RADIO_DATA:
            if (serialA_read(rdata_p))          // Do we have the next data byte?
            {
                // STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_DATA_STEP_ID, *rdata_p);
                ++rdata_p;                      // Yes!
                ++byte_count;
                // Do we have a complete message?
                if ((byte_count < sizeof(rdata)) && ((byte_count <= RADIO_MSG_O_HDR_SIZE) || (byte_count < rdata[RADIO_MSG_O_HDR_SIZE]))) {
                    break;                      // Incomplete message - Go around and get the next byte
                }
            }
            else                                // Don't have data...has this packet taken too long?
            {
                if (timer_chkLoResTimerExpired(TIMER_LO_RES_RADIO)) {
                    STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_DATA_TOUT_STEP_ID, NO_SUB_STEP_ID);
                    radio_state = RADIO_IDLE;   // Yes, discard the partial packet and begin looking for another.
                }
                break;                          // No - continue to look for the data bytes
            }

            // We have received a complete command packet:  validate the checksum
            srand (timer_freerunning + TBR);       // Re-seed the random number generator each time we get a message

            //  v5.00 --  If a transmission was pending, but was held off by this packet reception,
            //  reset the wait for transmission to 40 - 325ms
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_RETRANSMIT)) {
                timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, HOLDOFF_VALUE);
            }

            if (checksum(rdata, (int)(rdata[RADIO_MSG_O_HDR_SIZE] - 1)) != rdata[rdata[RADIO_MSG_O_HDR_SIZE] - 1])   // Good checksum?
            {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_CKSUMERR_STEP_ID, NO_SUB_STEP_ID);
                radio_state = RADIO_IDLE;                       // No - discard the message.
                return;
            }

            // bh - v5.01
//            timer_setLoResTimer(TIMER_LO_RES_RADIO_WATCHDOG, TIMEOUT_RADIO_WATCHDOG);

            // Here to repeat messages.
            // Is this message intended for us?
            if ((rdata[RADIO_MSG_O_HDR_DST] != radio_address) && !IsCPAC_Message(&rdata[RADIO_MSG_O_ALT_DATA]))
              {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_NOTME_MSG_STEP_ID, rdata[RADIO_MSG_O_HDR_DST]);
                // Check if the repeater function is disabled, try to repeat it if not.
                if (CONFIG_REPEATER_OFF() || (radio_repeater() == FALSE)) // No - Try to repeat it.
                {
                    // Message wasn't repeated, re(set) the retransmit timer
                    // to hold off our transmissions for a while.
                    // We do this as an "extend" so that if the RS-485 code has
                    // set this to a longer delay that we don't *reduce* it
                    // here.

                    if (rdata[RADIO_MSG_O_HDR_TYPE] != RADIO_MSG_TYPE_ACK)
                    {
                        timer_extendLoResTimer(TIMER_LO_RES_RETRANSMIT, HOLDOFF_VALUE);

                        // Here with message that wasn't repeated, but is within two
                        // hops of the host, Check if it might be a better
                        // repeater candidate

                        if (radio_relay_ignore != rdata[RADIO_MSG_O_HDR_SRC])   // Only if this isn't the unit we're currently using as a relay
                        {
                            switch (rdata[RADIO_MSG_O_HDR_TYPE])
                            {
                                case RADIO_MSG_TYPE_ALIVE:                          // Keepalive message from a remote
                                    if ((rdata[RADIO_MSG_O_ALIVE_HOP] < RADIO_MAX_HOPS) && (rdata[RADIO_MSG_O_ALIVE_HOP] < radio_relay_hops))
                                    {
                                        radio_relay_hops      = rdata[RADIO_MSG_O_ALIVE_HOP];
                                        radio_relay_candidate = rdata[RADIO_MSG_O_HDR_SRC];
                                    }
                                    else if (radio_relay_candidate == rdata[RADIO_MSG_O_HDR_SRC]) { // Is this our current candidate?
                                        radio_relay_hops      = rdata[RADIO_MSG_O_ALIVE_HOP];   // Make sure we have his *current* hop count
                                    }
                                    break;

                                case RADIO_MSG_TYPE_ALERT:                              // Alert message from remote
                                    if ((rdata[RADIO_MSG_O_ALT_HOP] < RADIO_MAX_HOPS) && (rdata[RADIO_MSG_O_ALT_HOP] < radio_relay_hops))
                                    {
                                        radio_relay_hops      = rdata[RADIO_MSG_O_ALT_HOP];
                                        radio_relay_candidate = rdata[RADIO_MSG_O_HDR_SRC];
                                    }
                                    else if (radio_relay_candidate == rdata[RADIO_MSG_O_HDR_SRC]) { // Is this our current candidate?
                                        radio_relay_hops      = rdata[RADIO_MSG_O_ALT_HOP];     // Make sure we have his *current* hop count
                                    }
                                    break;

                                    // v5.00 - remote must now expect real messages from the Host.
                            }
                        }
                    }
                }
                radio_state = RADIO_IDLE;                       // And ignore it otherwise.
                break;
            }

            // The message is addressed to us, process it.
            if (check_duplicate(rdata))     // Is this a duplicate of a message we've already seen?
            {
                STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_DUP_STEP_ID, NO_SUB_STEP_ID);

                if (rdata[RADIO_MSG_O_HDR_TYPE] == RADIO_MSG_TYPE_ALERT)
                {
                  // v2.15 - send an ACK even to a duplicate message.  Otherwise, the sender will keep sending the same message
                    send_radio_ack(rdata[RADIO_MSG_O_ALT_SEQ], rdata[RADIO_MSG_O_HDR_SRC]);
                }
                // Restart the transmit timer after an ACK has been sent.
                timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM3_INTERVAL() + TIMER_600MS));
                radio_state = RADIO_IDLE;    // Yes - ignore it.
                break;
            }

            STATE_TRACE( REMOTE_RADIO_ID, radio_state, REMOTE_MSG_STEP_ID, rdata[RADIO_MSG_O_HDR_DST]);

            mark_duplicate(rdata);          // Keep track of this message so we won't process it a second time
                                            // if it gets retransmitted by a repeater.

            switch (rdata[RADIO_MSG_O_HDR_TYPE])
            {
                case RADIO_MSG_TYPE_ACK:                                // Response to Alert
                    // bh - v2.10
                    radioactive = MANDATORY_ACK_INTERVAL * 5;            //
                    last_ack = rdata[RADIO_MSG_O_ACK_SEQ];
                    remove_ack_alert(rdata[RADIO_MSG_O_ACK_SEQ], active_alert_head[REMOTE_MONTREAL_ADDRESS], TRUE);
                    remove_ack_alert(rdata[RADIO_MSG_O_ACK_SEQ], active_alert_head[REMOTE_STANDARD_ADDRESS], TRUE);
                    radio_hop_count = rdata[RADIO_MSG_O_ACK_HOP];       // Number of hops to host
                    radio_sequence_number += 1;                         // Increase the sequence number for the next message
                    if (radio_sequence_number == 0) {                    // Never use sequence number of 0, it isn't valid.
                        radio_sequence_number = 1;
                    }
                    retransmit_interval = TIMEOUT_RETRANSMIT;           // Reset the retransmit interval to default value

                    // bh - v2.11 - lower the retransmit timer by a half
                    timer_loResTimer[TIMER_LO_RES_RETRANSMIT] /= 2;   // lower the retransmit timer
                    keepalive_counter = 0;                              // And reset the number of keepalive tries

                    if (radio_reply_expected)
                    {
                        radio_ack_counter += 1;
                        // v5.00 fix:  was 0xFFFFFF
                        if (radio_ack_counter > 0x0FFFFF)
                        {
                            radio_msg_counter /= 4;
                            radio_ack_counter /= 4;
                        }
                        radio_reply_expected = 0;
                    }
                    break;

                case RADIO_MSG_TYPE_NAK:        // Don't need to do anything with this message right now.
                    break;

                // v5.00:  Remote radio can now receive full messages from the Host radio
                case RADIO_MSG_TYPE_ALERT:                              // Response to poll
                    rdata_p = &rdata[RADIO_MSG_O_ALT_DATA];                 // Point to the beginning of the alert data

                    // v5.00: allow any message size and any command type
                    count = *(rdata_p + 1);

                    // get the received checksum
                    cs = *(rdata_p + (count-1));
                    if (cs == checksum(rdata_p, (int)(count-1)))
                    {
                        // Save the received command in the command queue
                        if (CpacSaveOutgoingMessage (rdata_p))
                        {
                            send_radio_ack(rdata[RADIO_MSG_O_ALT_SEQ], rdata[RADIO_MSG_O_HDR_SRC]);
                          // Restart the transmit timer after an ACK has been sent.
                          timer_setLoResTimer(TIMER_LO_RES_RETRANSMIT, (RANDOM3_INTERVAL() + TIMER_600MS));
                        }
                    }
                    break;

                case RADIO_MSG_TYPE_RR:         // Relay request
                    if ((CONFIG_REPEATER_OFF() == FALSE)              &&
                        (radio_hop_count < RADIO_MAX_HOPS)            &&
                        ((radio_hop_count == 0) || ((radio_hop_count > 0) &&
                        (radio_relay_active))))    // Only agree to relay if we're in contact with the host
                    {
                        send_radio_wr(rdata[RADIO_MSG_O_RR_NODE]);          // Tell him that we will relay
                        if ((radio_hop_count > 0) && radio_relay_active) {    // Are we also using a relay?
                            send_radio_rr(rdata[RADIO_MSG_O_RR_NODE], radio_relay_active);
                        }
                        mark_active(repeater_active_list, sizeof(repeater_active_list) * 8, (int)(rdata[RADIO_MSG_O_RR_NODE]));
                    }
                    break;

                case RADIO_MSG_TYPE_WR:         // Will Relay
                    if (rdata[RADIO_MSG_O_WR_NODE] == radio_address) {
                        radio_relay_active = rdata[RADIO_MSG_O_HDR_SRC];
                    }
                    break;

                default:                                            // We don't handle any other messages
                    break;
            }
            radio_state = RADIO_IDLE;
            break;

        default:
            radio_state = RADIO_IDLE;
            break;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  remote_rs485_state_machine
//
// Description:
// ------------
// State machine for polling remote sensors.  Acts as both the search routine
// and the routine polling loop.
//
// Design Notes:
// -------------
//
// Will poll each device at most twice a second.  If there is a large number
// of devices connected, it is possible that we may fall behind that schedule.
// So long as we don't exceed the 1.5 second watchdog quantum of the GE modules
// that shouldn't be a problem.
//
// Each time this routines finishes polling all the known, active devices, it
// will attempt to poll one of the inactive devices and see if it has been added.
// This means that a newly added device should be detected and active within
// 16 to 32 seconds after it is connected.
//
// _____________________________________________________________________________
//
void remote_rs485_state_machine(void)
{
    static int state        = POLL_POWER_UP;
    static int poll_index   = 0;
    static int search_index = 1;
    unsigned char i, index;
    unsigned char alert_buffer[TK_MSG_MAX_SIZE];
#if EVENT_TRACING_ENABLE
    unsigned long    alertRFID;
#endif

    // Handle RS485 states for remote Wireless Link
    switch (state)
    {
      // At power up, wait a half second before starting the polling,
      // and make sure to poll Cart #1 twice in a row.
      case POLL_POWER_UP:
        if (timer_chkLoResTimerExpired(TIMER_LO_RES_PACING))
        {
#ifdef RFID_CHANGE_SET
          RFID_CHANGE_SET(0, RCVR_ID, RFID_INIT);
#endif

          timer_setLoResTimer(TIMER_LO_RES_PACING, POLLING_RATE);   // bh - pacing brought down to 24ms
          startup_sweep = 1;
          ProtocolFlag = 1;
          send_rs485_poll ( 1 );
          state = POLL_TXWAIT;
        }
        break;

        case POLL_IDLE:
          if (timer_chkLoResTimerExpired(TIMER_LO_RES_PACING))
          {
            if (++poll_index <= MAX_VALID_REMOTE_RS485_ADDRESS)
            {
              if (startup_sweep || is_rs485_active(poll_index))
              {
                // bh - pacing brought down to 24ms
                timer_setLoResTimer(TIMER_LO_RES_PACING, POLLING_RATE);

                // v5.00 -- send command as poll replacement if command is available
                if (!Cpac_send_rs485_data (poll_index)) {
                  send_rs485_poll (poll_index);
                }
                state = POLL_TXWAIT;
                break;
              }
            }
            else
            {
              // bh - v2.13 - go through 2 startup sweeps - one for each protocol
              if (startup_sweep)
              {
                startup_sweep--;            // Done with startup sweep
                poll_index    = 0;
                break;
              }

              // bh - v2.13 - now we must execute 1 slow polls - one for each protocol
              if (poll_index == (MAX_VALID_REMOTE_RS485_ADDRESS+1))
              {
                // Set the working poll index
                ++poll_index;
                if (search_index > MAX_VALID_REMOTE_RS485_ADDRESS)
                {
                  // restart to the front of the slow polling list
                  search_index = 1;
                }
              }
              else {
                // Reset the poll index
                poll_index    = 0;
              }

              //
              // Now execute a slow polling operation:
              //

              // Here to search for a new RS-485 device
              while (is_rs485_active(search_index)
                  && (search_index <= MAX_VALID_REMOTE_RS485_ADDRESS)) {
                ++search_index;
              }

              if (search_index <= MAX_VALID_REMOTE_RS485_ADDRESS)
              {
                // bh - pacing brought down to 24ms
                timer_setLoResTimer(TIMER_LO_RES_PACING, POLLING_RATE);

                // bh - v2.13 - don't update the search index here
                send_rs485_poll(search_index);

                // bh - v2.13 - only update the search_index
                search_index++;

                state = POLL_TXWAIT;
                break;
              }
              if (search_index > MAX_VALID_REMOTE_RS485_ADDRESS)
              {
                // no slow poll available - restart to the front of the list
                search_index = 1;
              }
            }
          }
          break;

        // Here to wait until the UART is done sending RS-485 data.
        // Calling 'serialB_txDone' has the side-effect of switching
        // off the RS-485 driver when the transmit is complete.
        // Since the MSP430 UART doesn't have a geniune 'transmit
        // done' interrupt, we're doing this on a polled basis.
        case POLL_TXWAIT:
          if (serialB_txDone())
          {
            timer_setLoResTimer(TIMER_LO_RES_RS485, TIMEOUT_REMOTE_RS485_INPUT);
            data_p       = rx_data;
            data_counter = 0;
            state        = POLL_DATA;
          }
          break;

        // Here when ready to receive data.  Will continue to receive
        // data until a complete message is received or the timeout expires.

        case POLL_DATA:
          if (timer_chkLoResTimerExpired(TIMER_LO_RES_RS485)) {    // Timeout without reply?
            MSG_TRACE(MESSAGE_TIMEOUT_RFID, rx_data[RS485_MSG_O_ADDR], data_counter);
            state = POLL_IDLE;
          }

          // Check whether the entire packet has been received.
          if ((data_counter > RS485_MSG_O_SIZE) && (data_counter >= rx_data[RS485_MSG_O_SIZE]))
          {
             // Copy rx_data information to tx_data (working buffer)
             memcpy(tx_data, rx_data, data_counter);
            state = POLL_DONE;
            break;
          }

          if (serialB_read(data_p))
          {
            ++data_p;
            ++data_counter;
            timer_setLoResTimer(TIMER_LO_RES_RS485, TIMEOUT_REMOTE_RS485_INPUT);
          }
          break;

        // Here to process the reply.
        case POLL_DONE:
          if (data_counter > RS485_MSG_O_SIZE)
          {
            // Checksum OK?
            if (checksum(tx_data, (int)(tx_data[RS485_MSG_O_SIZE] - 1)) == tx_data[tx_data[RS485_MSG_O_SIZE] - 1])
            {
              // Did this unit reply?
              if (tx_data[RS485_MSG_O_ADDR] == last_index)
              {
                if ((radio_address == 0) || (last_index < (radio_address & 0x1F))) {
                  radio_address = (read_address_switch() << 5) | last_index;
                }

                // bh - v2.13 - consolidate calls to this location
                mark_rs485_active(last_index);
                if (!ProtocolFlag)
                {
                  // This is the old superbus protocol
                  switch (tx_data[RS485_MSG_O_FUNC])             // What kind of a reply did we get?
                  {
                    case 0x94:                              // No data, cover open
                    case 0x80:                              // No data, cover closed
                      break;

                    case 0x93:                          // Alert!
                      adjust_retransmit_timer ();
                      add_alert(&tx_data[0], FALSE);
                      break;
                    }
                  }
                  else {
                    // This is Superbus 2000 - save the Response and Capability Codes
                    ResponseCode = tx_data[RS485_MSG_O_FUNC];
                    CapabilityCode = tx_data[RS485_MSG_O_FUNC+1];
                    PacketSize = tx_data[RS485_MSG_O_SIZE];

                    // Define this Cart as a superbus 2000 Cart
                    mark_active((unsigned long *) &CartsProtocolList, MAX_VALID_REMOTE_RS485_ADDRESS, last_index);

                    // Check for a "just got reset" response
                    if ( ResponseCode == SB_JUST_RESET )
                    {
                      // This receiver just came out of reset and must be reprogrammed next time around.
                      mark_active((unsigned long *) &CartsReprogramList, MAX_VALID_REMOTE_RS485_ADDRESS, last_index);
                    }
                    else
                    {
                      // This receiver responded with an ACK, which means it has been reprogrammed
                      clear_active ((unsigned long *) &CartsReprogramList, MAX_VALID_REMOTE_RS485_ADDRESS, last_index);
                    }

                    // Any packet from a CPAC is not passed through as is.
                    if (IsCPAC_Message(tx_data) && (PacketSize >= TK_MIN_DATA_SIZE))
                    {
                        adjust_retransmit_timer ();
                        add_alert(tx_data, TRUE);
                    }
                    else if ( ResponseCode == SB_SLAVE_ACK )
                    {
                      MSG_TRACE(PACKET_TRACE_RFID, tx_data[RS485_MSG_O_ADDR], -((int) tx_data[RS485_MSG_O_SIZE]));
#ifdef RFID_CHANGE_SET
                      RFID_CHANGE_SET(0, tx_data[RS485_MSG_O_ADDR], RFID_CHANGE_CLEAR);
#endif

                      // v2.13 --- record the rssi floor readings
                      if (rssi_averaging)
                      {
                        if ( tx_data[RS485_MSG_O_SIZE] >= TK_HEADER_SIZE ) {
                          capture_ambient_rssi (tx_data);
                        }
                      }

                      if ( tx_data[RS485_MSG_O_SIZE] >= TK_MIN_DATA_SIZE )
                      {
                        // This is a new superbus 2000 data packet
                        adjust_retransmit_timer ();
                        SB2000_InitializeProtocolPackets (alert_buffer, tx_data);
                        for ( i = 0;  i < TkNumberOfPackets; i++)
                        {
                          // calculate index into next alert packet
                          index = ((i * TkPacketSize) + (TK_HEADER_SIZE - 1));

                          // offset index by one for 0x0C Mode
                          if (TkCapabilityCode == CC_0C_MODE ) {
                            index--;
                          }

                          /* Check for smoke supervision message time */
                          if (!SB_SmokeDetectSupervisorProtocolPacket(tx_data, index)) {
                            // Construct the outbound message and send the alarm
                            SB2000_ReconfigureProtocolPacket (alert_buffer, tx_data, index);
                            add_alert(&alert_buffer[0], FALSE);
                          }

#if EVENT_TRACING_ENABLE
                          // Calculate the RFID for the packet
                          alertRFID = (tx_data[index + 5] & 0xf);
                          alertRFID = (alertRFID * 256) + tx_data[index + 4];
                          alertRFID = (alertRFID * 256) + tx_data[index + 3];
#endif
#ifdef RFID_CHANGE_SET
                          RFID_CHANGE_SET(alertRFID, tx_data[RS485_MSG_O_ADDR], RFID_SET);
#endif
                        }
                      }
                      else {
                        MSG_TRACE(INVALID_MESSAGE_SIZE_RFID, tx_data[RS485_MSG_O_ADDR], tx_data[RS485_MSG_O_SIZE]);
                      }
                    }

#ifdef RFID_CHANGE_SET
                    if (RFID_CHANGE_CHECK() >= TESTING_RFIDS_MAX) {
                      RFID_CHANGE_SET(0, tx_data[RS485_MSG_O_ADDR], RFID_INIT);
                    }
#endif

                    MSG_TRACE(PACKET_TRACE_RFID, tx_data[RS485_MSG_O_ADDR], ((int) tx_data[RS485_MSG_O_SIZE]));

                    // This response is part of the new Superbus 2000 Protocol
                    // All slave communications must be ACK'ed by the Master
                    tx_data[RS485_MSG_O_SIZE]     = 0x05;
                    tx_data[RS485_MSG_O_FUNC]     = 0x04;
                    tx_data[RS485_MSG_O_FUNC+1]   = 0x00;
                    if (( ResponseCode != SB_JUST_RESET )
                        && ( ResponseCode != SB_CONFIG_ACK ))
                    {
                      // copy in capability code:
                      tx_data[RS485_MSG_O_FUNC+1] = CapabilityCode;
                    }
                    tx_data [tx_data[RS485_MSG_O_SIZE]-1] = checksum(tx_data, tx_data[RS485_MSG_O_SIZE]-1);
                    serialB_writeN(tx_data, tx_data[RS485_MSG_O_SIZE]);

                    // special exit from here
                    state = POLL_ACK_XMIT;
                    return;
                  }
              }
              else {
                // Note an unexpected response
                MSG_TRACE(UNEXPECTED_ADDRESS_RFID, tx_data[RS485_MSG_O_ADDR], tx_data[RS485_MSG_O_ADDR]);
              }
            }
            else {
              // Note a checksum error
              MSG_TRACE(INVALID_CHECKSUM_RFID, tx_data[RS485_MSG_O_ADDR], 0);
            }
          }
          else {
            MSG_TRACE(INVALID_MESSAGE_SIZE_RFID, tx_data[RS485_MSG_O_ADDR], tx_data[RS485_MSG_O_SIZE]);
          }

          state = POLL_IDLE;
          break;

        // Stay here to wait until the UART is done sending the ACK under the Superbus 2000 protocol.
        case POLL_ACK_XMIT:
          if (serialB_txDone())
          {
            state     = POLL_IDLE;
          }
          break;

        default:
          state       = POLL_POWER_UP;
          break;
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  radio_quality
//
// Description:
// ------------
// Function to perform the mildly convoluted calculations for the radio
// quality display.  This is broken out as a function as it needs to be
// done in two difference places in the code.
//
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int radio_quality(int *period)
{
    int led_value;

    if ((radio_hop_count == RADIO_OFFLINE_HOPS) || (radio_msg_counter == 0))
    {
        *period = 1;
        return (0);                      // If offline, radio quality is 0%
    }

    if (radio_msg_counter > radio_ack_counter)
    {
        led_value = (int)(1000L * radio_ack_counter / (radio_msg_counter));

        if (led_value > 999)
        {
            *period = 3;
            return (100);
        }

        *period = 2;
        return (led_value);
    }

    *period = 3;
    return (100);                // Avoid Div by 0!
}


// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  led_state_machine
//
// Description:
// ------------
// This function contributes exactly nothing to the core functions of the
// board.  It is entirely tasked with managing the user interface (such as
// it is.)  It reads and debounces the pushbutton, and takes care of the
// details of displaying a variety of numbers on the LED display.
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void led_state_machine(void)
{
    static int led_value    = 0;
    static int unit_index;
    static int period = 0;

    if (timer_chkLoResTimerExpired(TIMER_LO_RES_LED))
    {
        timer_setLoResTimer(TIMER_LO_RES_LED, TIMER_LO_RES_TICKS_PER_SECOND);
        CPU_IO_DEBUG_LED2 = 0;          // Turn off the green light
    }

    //
    // The following is the state machine that manages the pushbutton.
    //

    switch (button_state)
    {
        case BUTTON_STATE_IDLE:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_DIGIT))
            {
                period = 0;

                if (DIAG_RADIO_QUALITY())         // Spare SW1 - Radio quality display?
                {
                    led_value = radio_quality(&period);
                }
                else if (DIAG_RADIO_RSSI())     // Spare SW2 - Radio RSSI display?
                {
                    led_value = ADC12MEM0 >> 2;
                }
                else if (DIAG_RADIO_RELAY())    // Spare SW3 - Radio Relay parameter display?
                {
                    led_value = radio_relay_active;
                }
                else if (DIAG_BFR_PCT())            // Use DIP switch 4 to control default LED display
                {
                    led_value = bfr_percentage;
                }
                else {
                    led_value = active_count;
//                  led_value = debug_count;
                }
            }

            if (timer_chkLoResTimerExpired(TIMER_LO_RES_DEBOUNCE) &&
                (CPU_IO_M_SWITCH == 0))                  // Is the button pressed?
            {
                unit_index   = 0;
                button_state = BUTTON_STATE_PRESSED;
                timer_setLoResTimer(TIMER_LO_RES_DEBOUNCE, TIMER_LO_RES_TICKS_PER_SECOND * 5);
            }
            break;

        case BUTTON_STATE_PRESSED:
            if (CPU_IO_M_SWITCH != 0)                   // Was the button released?
            {
                // Here with a quick press and release of the pushbutton

                timer_setLoResTimer(TIMER_LO_RES_DEBOUNCE, TIMER_LO_RES_TICKS_PER_SECOND / 5);

                if (DIAG_RADIO_QUALITY())         // Spare SW1 - Radio quality display?
                {
                    led_value = radio_quality(&period);
                }
                else if (DIAG_RADIO_RSSI())     // Spare SW2 - Radio RSSI display?
                {
                    led_value = ADC12MEM0 >> 2;
                }
                else if (DIAG_RADIO_RELAY())         // Spare SW3?
                {
                    // Here to show list of units in our repeater list
                    while (++unit_index <= MAX_VALID_RADIO_ADDRESS) {    // Find the address of the next active Superbus address
                        if (is_active(repeater_active_list, sizeof(repeater_active_list) * 8, unit_index)) {
                            break;
                        }
                    }

                    if (unit_index > MAX_VALID_RADIO_ADDRESS) {         // Are we at the end of the list of addresses?
                        button_state = BUTTON_STATE_IDLE;               // Yes - go back to displaying the number of units.
                    }
                    else                                                // No  - set up to display the next address
                    {
                        led_value    = unit_index;
                        period       = 3;
                        button_state = BUTTON_STATE_RELEASED;
                        timer_setLoResTimer(TIMER_LO_RES_UNIT_DISPLAY, TIMER_LO_RES_TICKS_PER_SECOND * 10);
                    }
                }
                else
                {
                    // Here to show list of active RS-485/Superbus addresses
                    while (++unit_index <= MAX_VALID_HOST_RS485_ADDRESS) {   // Find the address of the next active Superbus address
                        if (is_rs485_active(unit_index)) {
                            break;
                        }
                    }

                    if (unit_index > MAX_VALID_HOST_RS485_ADDRESS) {    // Are we at the end of the list of addresses?
                        button_state = BUTTON_STATE_IDLE;               // Yes - go back to displaying the number of units.
                    }
                    else                                                // No  - set up to display the next address
                    {
                        led_value    = unit_index;
                        period       = 3;
                        button_state = BUTTON_STATE_RELEASED;
                        timer_setLoResTimer(TIMER_LO_RES_UNIT_DISPLAY, TIMER_LO_RES_TICKS_PER_SECOND * 10);
                    }
                }
            }
            else                // Here when the button is still pressed.
            {
                if (timer_chkLoResTimerExpired(TIMER_LO_RES_DEBOUNCE))          // Was the button held for five seconds?
                {
                    if (DIAG_RADIO_QUALITY() || DIAG_RADIO_RSSI()) {  // Yes, Is this Radio quality or RSSI display mode?
                        timer_setLoResTimer(TIMER_LO_RES_ALIVE, 0);   // Yes, force immediate keepalive
                    }
                    button_state = BUTTON_STATE_HELD;                 // and go to the button held state
                }
            }
            break;

        case BUTTON_STATE_HELD:
            if (CPU_IO_M_SWITCH != 0)                   // Was the button released?
            {
                button_state = BUTTON_STATE_IDLE;                           // Yes - just go back to the idle state
                timer_setLoResTimer(TIMER_LO_RES_DEBOUNCE, TIMER_LO_RES_TICKS_PER_SECOND / 5);
                break;
            }

            if (timer_chkLoResTimerExpired(TIMER_LO_RES_DIGIT))
            {
                if (DIAG_RADIO_QUALITY())         // Spare SW1 - Radio quality display?
                {
                    led_value = radio_quality(&period);
                }
                else if (DIAG_RADIO_RSSI())     // Spare SW2 - Radio RSSI display?
                {
                    led_value = ADC12MEM0 >> 2;
                }
                else
                {
                    led_value = offline_counter;                                // Show the number of times a CART has gone offline
                    period    = 3;
                }
            }
            break;

        case BUTTON_STATE_RELEASED:
            if (timer_chkLoResTimerExpired(TIMER_LO_RES_UNIT_DISPLAY)) {
                button_state = BUTTON_STATE_IDLE;
            }
            else if (timer_chkLoResTimerExpired(TIMER_LO_RES_DEBOUNCE) && (CPU_IO_M_SWITCH == 0))     // Is the button pressed?
            {
                button_state = BUTTON_STATE_PRESSED;
                timer_setLoResTimer(TIMER_LO_RES_DEBOUNCE, TIMER_LO_RES_TICKS_PER_SECOND * 5);      // Reset the debounce timer to detect five-second button hold
            }
            break;
    }

    if (radio_init_state == RADIO_INIT_COMPLETE)        // Only use the LED digits if the radio initialization is done
    {
        if (timer_chkLoResTimerExpired(TIMER_LO_RES_DIGIT))
        {
            if (led_value > 999) {
                led_value = 999;
            }

            cpu_write_led_display(((led_value / 100) % 10) + '0', ((led_value / 10) % 10) + '0', (led_value % 10) + '0', period);
            timer_setLoResTimer(TIMER_LO_RES_DIGIT, TIMER_LO_RES_TICKS_PER_SECOND);
        }
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  main
//
// Description:
// ------------
// The application.
//
// Design Notes:
// -------------
// _____________________________________________________________________________
//
void main(void)
{
    cpu_initCpu();
    CPU_IO_DEBUG_LED1 = 1;          // Turn on the red rr12    light

#if VERSION_TEST
    cpu_write_led_display(VERSION_MAJOR + '0', ((VERSION_MINOR / 10) % 10) + '0', ((VERSION_MINOR % 10) + '0'), 0);
#else
    cpu_write_led_display(VERSION_MAJOR + '0', ((VERSION_MINOR / 10) % 10) + '0', ((VERSION_MINOR % 10) + '0'), 1);
#endif

    // Wait for two seconds so people can see the version #
    timer_setLoResTimer(TIMER_LO_RES_DIGIT, TIMER_LO_RES_TICKS_PER_SECOND * 2);
    while (timer_chkLoResTimerExpired(TIMER_LO_RES_DIGIT) == FALSE) {
        ;
    }

    cpu_write_led_display(' ', 'C', '1', 3);


    memset(repeater_list, 0, sizeof(repeater_list));    // Initialize the repeater list to known values
    memset(repeater_active_list, 0, sizeof(repeater_active_list));    // Initialize the repeater list to known values
    memset(alert_list,    0, sizeof(alert_list));       // Initialize the alert list to known values
    memset(polling_list,  0, sizeof(polling_list));     // Initialize the polling list to known values
    memset(active_units,  0, sizeof(active_units));

    memset(ambient_rssi,  0, sizeof(ambient_rssi));
    memset(accum_rssi,    0, sizeof(accum_rssi));
    memset(command_list,  0, sizeof(command_list));

    host_unit = CONFIG_HOST_MODE();     // Use DIP switch 1 as the host/remote select
    rssi_averaging = 0;                 // Always active
    supervisor_filter = CONFIG_SUPERVISORY_FILTER_ACTIVE();
                                        // Use DIP switch 2 as the rssi averaging enable switch

// Init serial ports

    serialA_init();
    serialB_init();                 // NOTE: Serial B init must be done after we've read the DIP switches,
                                    // as the parity settings are different for host vs remote units.
    serialB_txDone();               // Make sure we're set to receive
    cpu_write_led_display(' ', 'C', '2', 3);

    // Watchdog timer

#if CPU_WATCHDOG_TIMER
    CPU_WATCHDOG_INIT;                // Set Watchdog Timer
#endif

    srand(read_address_switch() + timer_freerunning + TBR);         // Seed the random number generator from timer B to prepare for the random serial number
    radio_serial_number = rand();           // Start with a random serial number

    retransmit_interval   = TIMEOUT_RETRANSMIT;         // Reset the retransmit interval to default value
    radioactive           = 0;
    radio_hop_count       = RADIO_OFFLINE_HOPS;         // Number of hops to host
    radio_sequence_number = 1;
    keepalive_counter     = 0;                          // And reset the number of keepalive tries

    // bh - v2.10 - read all switches and save their values for later comparison
    cpu_power_up_read_switches ();

    cpu_write_led_display(' ', 'C', '3', 3);

    // Flush any lingering data from the modem

    while (serialA_read(rdata)) {
        ;
    }

    radio_address = 0;                      // We don't have a remote radio address yet, and the host is always 0

    cpu_write_led_display(' ', ' ', ' ', 0);

    if (host_unit)
    {
        for (;;)
        {
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            host_radio_state_machine();         // MaxStream radio state machine
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            host_rs485_state_machine();         // State machine that handles RS-485 traffic from the Lifeline WABS controller
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            led_state_machine();                // Update the LED digits
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            activity_check();                   // And count down the timers to age out inactive units
            cpu_compare_switches ();            // bh - v2.10 - Go into Soft Reset if switches change
        }
    }
    else    // Remote polling unit
    {
        // bh - v2.13 - wait a half second at power up before commencing polling
        timer_setLoResTimer(TIMER_LO_RES_PACING, TIMER_LO_RES_TICKS_PER_SECOND / 2);

        // Do a highly randomized first keepalive to prevent them from clustering
        // if a bunch of units are powered up at the same time.
        // bh - v2.10
        timer_setLoResTimer(TIMER_LO_RES_ALIVE, (TIMEOUT_KEEPALIVE) / ((rand() % 5) + 1));

        for (;;)
        {
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            remote_rs485_state_machine();       // GE module polling state machine
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            remote_radio_state_machine();       // Run the MaxStream radio state machine
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            led_state_machine();                // Update the LED digits
            CPU_WATCHDOG_RESET;                 // Reset Watchdog Timer
            activity_check();                   // And count down the timers to age out inactive units
            cpu_compare_switches ();            // bh - v2.10 - Go into Soft Reset if switches change
        }
    }
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: wabs.c,v $
// Revision 1.28  2006/08/14 20:02:54  tgoltz
// Fix bug that caused host WABS to start receiving short packets when it was power-cycled.
//
// Revision 1.27  2006/08/11 20:42:52  tgoltz
// Add check for MaxStream version 2020 and refuse to run the radio if found.
//
// Revision 1.26  2006/08/10 23:24:20  tgoltz
// Update radio initialization so it writes current parameters out to flash when
// necessary.
//
// Revision 1.25  2006/08/09 18:46:51  tgoltz
// Fixes from code review at Lifeline
//
// Revision 1.24  2006/07/26 16:18:24  tgoltz
// Implement reading the MaxStream RSSI line using the A/D converter.
//
// Revision 1.23  2006/07/25 23:28:02  tgoltz
// Implement radio quality display, including Chase's idea that holding the button
// in this mode will cause a rapid keepalive transmit pattern.
//
// Revision 1.22  2006/07/24 22:07:31  tgoltz
// Add code so that the advertised hop counts in the alert/keepalive messages would show the fact
// that the repeater had been disabled.
//
// Revision 1.21  2006/07/24 21:11:08  tgoltz
// Fix more bugs in the repeater feature, add more diagnostic modes, add
// ability to disable repeater feature.
//
// Revision 1.20  2006/07/21 23:29:20  tgoltz
// Tweak radio timing and fix bugs in the repeater feature.
//
// Revision 1.19  2006/07/13 16:46:32  tgoltz
// Update startup status codes, shorten mandatory ACK interval to every three keepalive
// messages.
//
// Revision 1.18  2006/06/29 21:00:04  tgoltz
// Update for GEN2 hardware.
//
// Revision 1.17  2006/06/09 20:57:13  tgoltz
// Fix the fact that even with a bad checksum, the host WABS was processing
// RS-485 messages anyway.
//
// Revision 1.16  2006/06/05 16:18:07  tgoltz
// Changes from debugging session at Lifeline
//
// Revision 1.15  2006/06/02 17:26:55  tgoltz
// Remove dead code, remove code that disabled duplicate alert checking based
// on buffer utilization.  Now does duplicate alert checks all the time.
//
// Revision 1.14  2006/06/02 17:22:36  tgoltz
// Improve host WABS RS-485 code so it recovers from data errors within a single message.
//
// Revision 1.13  2006/06/01 15:11:07  tgoltz
// Change semantics from master/slave to host/remote to better reflect the
// roles in a push-style system.
//
// Revision 1.12  2006/05/31 22:20:16  tgoltz
// Additional tweaking on radio timing parameters
//
// Revision 1.11  2006/05/31 19:25:55  tgoltz
// Fix timing bugs with keepalives and transmit holdoffs.
//
// Revision 1.10  2006/05/26 21:24:40  tgoltz
// Fix bugs in radio timing and timeouts
//
// Revision 1.9  2006/04/25 15:53:45  tgoltz
// Backport the MaxStream radio watchdog feature from the WABS1 source.
//
// Revision 1.8  2006/04/14 14:07:50  tgoltz
// Bug fixes from session at Lifeline.  Bring up more than 32 CART's for the first time.
//
// Revision 1.7  2006/04/04 22:47:26  tgoltz
// Many bug fixes...mostly working now.
//
// Enhanced repeater code to correctly forward repeater requests if more than one
// hop away from the master.
//
// Revision 1.6  2006/04/03 20:50:36  tgoltz
// Added relay support, few fixes.  Main coding should now be complete.
//
// Revision 1.5  2006/03/31 00:27:21  tgoltz
// Coding continued.
//
// Revision 1.4  2006/03/30 23:37:33  tgoltz
// Coding continued.
//
// Revision 1.3  2006/03/16 23:52:22  tgoltz
// Work in progress
//
// Revision 1.2  2006/03/06 17:24:07  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// _____________________________________________________________________________
