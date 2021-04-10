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
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET); // ee write protect

	uint16_t var = 0;

	EE_ReadVariable(ID_EEPROM_ADR_NV_ADDR, &var);

	if ( (((~var)&0xFF) == (var>>8)) )
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, (var&0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // default ee Adr
	}
}
