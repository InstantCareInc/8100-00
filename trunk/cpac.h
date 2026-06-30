/***************************************************************************
    File    :    cpac.h
    Title    :    CPAC functions Header File
    Author    :    Bob Halliday
                    bohalliday@aol.com
    Created    :    September, 2007

    Description:
        CPAC Definitions for the 2378 cpac.c source file

    Contents:

    Revision History:


***************************************************************************/
#ifndef HEADER_CPAC
#define HEADER_CPAC

/************************************************
 define SCOPE to allow RAM variables to be defined directly or as externs
*************************************************/
#ifdef SCOPE
#undef SCOPE
#endif

#ifdef BODY_CPAC
#define SCOPE
#else
#define SCOPE    extern
#endif

/*******************************
*   Section 1:  Equate Definitions:
*********************************/

//#define ONE_TENTH_OF_A_SECOND                    (uint8)(100/TIME_BASE)
#define CPAC_POWER_ACTIVE                     (uint8)(250/TIME_BASE)
#define CPAC_LED1_OFF                         (uint8)(50/TIME_BASE)
#define CPAC_LED2_OFF                         (uint8)(50/TIME_BASE)
#define CPAC_LED3_OFF                         (uint8)(200/TIME_BASE)

#define CPAC_FUNC_H_BITS_MSK                  ((unsigned char) 0xF0)
#define CPAC_FUNC_H_BITS_VAL                  ((unsigned char) 0x30) 

// Superbus 2000 indeces into a Receive Packet
enum
{
  TK_ADDRESS            = 0,
  TK_COUNT,
  TK_RESPONSE_CODE,
  TK_CAPABILITY_CODE,
  TK_STATUS_BYTE,
  TK_NOISE_FLOOR_RIGHT,
  TK_NOISE_FLOOR_LEFT,
  TK_PACKET_COUNT,
  TK_RSSI,
  TK_PACKET_TYPE,
  TK_RFID_3,
  TK_RFID_2,
  TK_RFID_1_DEVICETYPE,
  TK_BATTERY_LOW_F1_F2,
  TK_F3_F4_F5,
  TK_REPEATER_ID,
  TK_CHKSUM,
  TK_MSG_MAX_SIZE,
};

// running at 18mHz  - one tick every 55ns
#define CPAC_1_MS            (uint32) 20000
#define CPAC_2_MS            (uint32) 40000
#define CPAC_3_MS            (uint32) 60000
#define CPAC_4_MS            (uint32) 80000
#define CPAC_5_MS            (uint32) 90909

#define CPAC_12_5_MS         (uint32) 227273        // the real 12.5ms time

#define CPAC_TOGGLE_TEST            0xC0000000

#define SB_SHOW_CAPABILITIES        0x02
#define SB_ID_REQUEST           0x03
#define SB_DATA_POLL_RELEASE        0x04
#define SB_DIRECT_DATA_POLL            0x15
#define SB_UNIVERSAL_DATA_POLL  0x25
#define SB_ID_POLL              0x26
#define SB_ID_POLL_RESTART            0x27
#define SB_ADDRESS_ASSIGNMENT        0x29

#define SB_CONFIGURE_OPTIONS        0x32
#define SB_ACTIVATE_RELAY       0x33
#define SB_DEACTIVATE_RELAY            0x34
#define SB_RESET_ALARM          0x35
#define SB_POLL                 0x35
#define SB_ENTER_WALK_TEST        0x36
#define SB_EXIT_WALK_TEST         0x37
#define SB_HOST_RADIO_REQUEST     0x3E
#define SB_REMOTE_RADIO_REQUEST   0x3F
#define SB_MAC_ASSIGNMENT         0x70
#define SB_CPAC_CC                0x01
#define SB_RECEIVER_CC            0x1A

#define SB_HOST_ACK             0x04
#define SB_POLL_2000            0x15
#define SB_DATA_PACKET          0x93
#define SB_NO_DATA_PACKET       0x80
#define SB_ACK                  0x94
#define SB_RSSI_PACKET          0x91

#define SB_JUST_RESET           0x8F
#define SB_SLAVE_ACK            0x82
#define SB_CONFIG_ACK           0x80
#define SB_CONFIG_NAK           0x8E


#define SB_POLL1                    0x30
#define SB_POLL2                    0x35

#define COMMAND_LIST_LENGTH            30        // bh - v5.00
#define SB_MIN_BUFFER_SIZE             4
#define SB_MESSAGE_BUFFER_SIZE        120
#define SB_RECEIVER_MASK            0x1F
#define SB_REMOTE_RADIO_MASK        0xE0

/*******************************
*   Section 2:  RAM Variables:
*********************************/

// Private Variables:
#ifdef BODY_CPAC

#endif

// Public Variables:
SCOPE        unsigned char            CpacWatchdogFlag;

/*******************************
*   Section 3:  Function prototypes
*********************************/
int  CpacSaveOutgoingMessage (unsigned char *);
void CpacHostRadioSaveMessage (unsigned char *);
void CpacSaveMessage (unsigned char * );
void CpacCheckForOutgoingMessages (void);
void SB_RotateReceiver (unsigned char *);
unsigned char SB_CheckFunctionCode (unsigned char *);
int Cpac_host_send_radio_command (struct alert_data *);
void Cpac_remove_command (struct alert_data *);
int Cpac_send_rs485_data (int);

/*******************************
*   Section 4:  Macros
*********************************/

/********************************
*   Section 5:  ROM Tables
*********************************/

#ifdef BODY_CPAC

const unsigned char SB_FunctionCodeList [] =
    {
    SB_HOST_ACK,
    SB_POLL_2000,
    SB_NO_DATA_PACKET,
    SB_SLAVE_ACK,
    SB_JUST_RESET,
    SB_DATA_PACKET,
    SB_ACK,

    0x00
    };




#endif



#undef SCOPE
#endif

/* ======== END OF FILE ======== */
