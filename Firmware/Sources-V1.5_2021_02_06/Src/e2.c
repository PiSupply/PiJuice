// ----------------------------------------------------------------------------
/*!
 * @file		e2.c
 * @author    	John Steggall, milan
 * @date       	31 March 2021
 * @brief       Basically just sets the i2c address and write protect of the
 * 				onboard eeprom. Taken the code from the original main.c.
 *
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"
#include "eeprom.h"
#include "nv.h"
#include "iodrv.h"

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * E2_Init configures the eeprom address and sets the write protect.
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void E2_Init(void)
{
	uint8_t tempU8;
	bool EE_AddrPinValue = GPIO_PIN_RESET;

	IODRV_SetPin(IODRV_PIN_EE_WP, GPIO_PIN_SET);

	if (NV_ReadVariable_U8(ID_EEPROM_ADR_NV_ADDR, &tempU8))
	{
		EE_AddrPinValue = (0u != (tempU8 & 0x02u));
	}

	IODRV_SetPin(IODRV_PIN_EE_A, EE_AddrPinValue);
}
