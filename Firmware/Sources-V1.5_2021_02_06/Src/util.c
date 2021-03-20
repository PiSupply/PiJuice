/*
 * util.c
 *
 *  Created on: 20.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"

#include "util.h"

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * UTIL_NV_ParamInitCheck_U16 checks to see if a NV parameter has been
 * initialised
 *
 * @param	uint16_t 	parameter vaue
 * @retval	bool		true = parameter is looking initialised
 * 						false = parameter is not looking initialised
 */
// ****************************************************************************
bool UTIL_NV_ParamInitCheck_U16(uint16_t parameter)
{
	uint8_t chk = (parameter >> 8u) ^ 0xFF;

	return (parameter & 0xFF) == chk;
}
