// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Venture Technologies, Inc.                                   Copyright 2006
// _____________________________________________________________________________
//
// File:                flash.c
//
// Author:              Tom Goltz
//
// Description:
// ------------
// flash memory routines
//
// Design Notes:
// -------------
//
// Revision Control:
// -----------------
// Last committed on  --> $Date: 2006/03/06 17:24:07 $
// This file based on --> $Revision: 1.2 $

//
// Header files
//

#include "cpu.h"
#include "wabs.h"

struct fdata_cal flash_data_cal;

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  flash_init
//
// Description:
// ------------
// Called at power up to do any initialization required by the flash routines
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
void flash_init(void)
{
    FCTL2 = FWKEY | FSSEL_0 | 15;	// Flash clock = ACLK / 16
    FCTL3 = FWKEY | LOCK;		// Lock flash access
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  flash_write
//
// Description:
// ------------
// Local-only routine that handles the heavy-lifting for writing
// data to flash memory.
//
// Design Notes:
// -------------
// Doesn't check to see if interrupts were previously enabled - don't
// call this if you don't want interrupts enabled when it completes.
//
// _____________________________________________________________________________
//
static int flash_write(unsigned char *dest, void *data, int size)
{
    unsigned char saved_IE1;

    if (size > 120) { // Will data fit in a single segment?
        return (0);		// No - return failure
    }

    if ((SVSCTL & SVSOP) != 0) {  // Are we in an extreme low voltage condition?
        return (0);               // Yes - return failure, as can't write flash
                                  // with critically low voltage.
    }

    CPU_WATCHDOG_STOP;		// Stop the watchdog for the duration of the
    				// flash write (It's very, very slow)

    _DINT();			// Disable interrupts during FLASH updates as per TI user's manual.

    saved_IE1 = IE1;
    IE1       = 0;		// Since NMIs can still occur with GIE clear, we must disable them too.

    // Clear the lock bit.
    FCTL3 = FWKEY;
    FCTL1 = FWKEY | ERASE;

    // Erase segment(s) by doing a dummy write to any address in that segment.
    *dest = 0x0000;

    // WRITE

    // Set up for single word/byte writes, much simpler than segment/block writes
    // and allows us to cross erased segment boundaries without extra checks. Not
    // that much slower either.

    FCTL1 = FWKEY | WRT;
    *dest++ = 0x22;			// Write flash signature of 0x221B
    *dest++ = 0x1B;

    memcpy(dest, data, size);

    // Set the lock to catch inadvertent writes to FLASH memory and end the
    // write cycle properly.
    FCTL3 = FWKEY | LOCK;

    // Safe to re-start interrupt servicing and timers again.

    IE1 = saved_IE1;
    _EINT();
    return (memcmp(dest, data, size) == 0);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  flash_write_cal
//
// Description:
// ------------
// Writes calibration information to flash memory
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int flash_write_cal(void)
{
    return (flash_write((unsigned char *)LM_CAL_DATA_SEG, &flash_data_cal, sizeof(flash_data_cal)));
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Function:  flash_read_cal
//
// Description:
// ------------
// Retrieves calibration parameters from flash memory
//
// Design Notes:
// -------------
//
// _____________________________________________________________________________
//
int flash_read_cal(void)
{
    unsigned char *dest = (unsigned char *)LM_CAL_DATA_SEG;

    if ((*dest++ != 0x22) || (*dest++ != 0x1B))
    {
        memset(&flash_data_cal, 0, sizeof(flash_data_cal));
        return (0);
    }

    memcpy(&flash_data_cal, dest, sizeof(flash_data_cal));

    return (1);
}

// _____________________________________________________________________________
// _____________________________________________________________________________
//
// Revisions:
// ----------
// $Log: flash.c,v $
// Revision 1.2  2006/03/06 17:24:07  tgoltz
// Update comments, remove revision histories from the previous branch.
//
// Revision 1.1.1.1  2006/03/06 17:02:57  tgoltz
// Starting point for the WABS2 code
//
// _____________________________________________________________________________
