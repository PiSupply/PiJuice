/*
 * util.c
 *
 *  Created on: 20.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <stdlib.h>

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
bool UTIL_NV_ParamInitCheck_U16(const uint16_t parameter)
{
	uint8_t chk = (parameter >> 8u) ^ 0xFF;

	return (parameter & 0x00FFu) == (uint16_t)chk;
}


// ****************************************************************************
/*!
 * UTIL_FixMul_U32_U16 multiplies a uint16 value by a 16/16 fixed point multiplier.
 *
 * @param	uint16_t 	parameter vaue
 * @retval	bool		true = parameter is looking initialised
 * 						false = parameter is not looking initialised
 */
// ****************************************************************************
uint16_t UTIL_FixMul_U32_U16(const uint32_t fixmul, const uint16_t value)
{
	/* Apply fixed point multipler */
	uint32_t result = (value * fixmul);

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint16_t)(result >> 16u);
}


int16_t UTIL_FixMul_U32_S16(const uint32_t fixmul, const int16_t value)
{
	const bool negative = (value < 0);
	const uint32_t result = UTIL_FixMul_U32_U16(fixmul, abs(value));

	return (true == negative) ? -result : result;
}


bool UTIL_FixMulInverse_U16_U16(const uint16_t realVal, const uint16_t divValue, uint32_t * const result)
{
	if (divValue == 0u)
	{
		// Could end badly
		return false;
	}

	// TODO - Consider overflow

	*result = (realVal << 16u) / divValue;

	return true;
}
