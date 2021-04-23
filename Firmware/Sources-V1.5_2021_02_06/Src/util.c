// ----------------------------------------------------------------------------
/*!
 * @file		util.c
 * @author    	John Steggall
 * @date       	20 March 2021
 * @brief       Stuff goes in here that I can't think of a good place, usually
 * 				small helper functions.
 *
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
 * The multiplier is derived by UINT16_MAX * fullscale value / resolution, rounding
 * is performed to the nearest whole number.
 *
 * @param	fixmul		fixed point value to multiply by
 * @param	value 		uint16_t parameter vaue
 * @retval	uint16_t 	muiltiplied value
 */
// ****************************************************************************
uint16_t UTIL_FixMul_U32_U16(const uint32_t fixmul, const uint16_t value)
{
	bool overflow;

	return UTIL_FixMulOvf_U32_U16(fixmul, value, &overflow);
}


uint16_t UTIL_FixMulOvf_U32_U16(const uint32_t fixmul, const uint16_t value, bool * const p_overFlow)
{
	/* Apply fixed point multipler */
	uint64_t result = (value * (uint64_t)fixmul);
	*p_overFlow = result > UINT32_MAX;

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint16_t)(result >> 16u);
}

uint32_t UTIL_FixMulOvf_U32_U32(const uint32_t fixmul, const uint32_t value, bool * const p_overFlow)
{
	uint64_t result = (value * (uint64_t)fixmul);

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint32_t)(result >> 16u);
}


uint32_t UTIL_FixMul_U32_U32(const uint32_t fixmul, const uint32_t value)
{
	bool overflow;

	return UTIL_FixMulOvf_U32_U32(fixmul, value, &overflow);
}


int32_t UTIL_FixMulOvf_U32_S32(const uint32_t fixmul, const int32_t value, bool * const p_overFlow)
{
	const bool negative = (value < 0);
	const uint32_t result = UTIL_FixMulOvf_U32_U32(fixmul, abs(value), p_overFlow);

	*p_overFlow |= result > (INT32_MAX + (true == negative) ? 1u : 0u);

	return (true == negative) ? (int32_t)-result : (int32_t)result;
}


int32_t UTIL_FixMul_U32_S32(const uint32_t fixmul, const int32_t value)
{
	bool overflow;

	return UTIL_FixMulOvf_U32_S32(fixmul, value, &overflow);
}


// ****************************************************************************
/*!
 * UTIL_FixMul_U32_S16 multiplies a int16 value by a 16/16 fixed point multiplier.
 * The multiplier is derived by UINT16_MAX * fullscale value / resolution. Rounding
 * is performed to the nearest whole value.
 *
 * @param	fixmul		fixed point value to multiply by
 * @param	value 		int16_t value to multiply
 * @retval	int16_t		multiplied return value
 */
// ****************************************************************************
int16_t UTIL_FixMul_U32_S16(const uint32_t fixmul, const int16_t value)
{
	bool overflow;

	return UTIL_FixMulOvf_U32_S16(fixmul, value, &overflow);
}


int16_t UTIL_FixMulOvf_U32_S16(const uint32_t fixmul, const int16_t value, bool * const p_overflow)
{
	const bool negative = (value < 0);
	const uint16_t result = UTIL_FixMulOvf_U32_U16(fixmul, abs(value), p_overflow);

	*p_overflow |= result > (INT16_MAX + (true == negative) ? 1 : 0);

	return (true == negative) ? -result : result;
}


// ****************************************************************************
/*!
 * UTIL_FixMulInverse_U16_U16 finds the fixed point conversion value.
 *
 * @param	realVal		value to be referenced to by the fixed point multiplier
 * @param	divValue 	number of resolved steps that represents the real value
 * @param	p_result	pointer to where the result is to be placed
 * @retval	bool		true = conversion sucessful
 * 						false = conversion unsuccesful
 */
// ****************************************************************************
bool UTIL_FixMulInverse_U16_U16(const uint16_t realVal, const uint16_t divValue, uint32_t * const p_result)
{
	if (divValue == 0u)
	{
		// Could end badly
		return false;
	}

	*p_result = (realVal << 16u) / divValue;

	return true;
}


uint16_t UTIL_FixMul_U16_U16(const uint16_t fixmul, const uint8_t value)
{
	uint32_t result = fixmul * value;

	return (uint16_t)(result >> 8u);
}

// ****************************************************************************
/*!
 * UTIL_Make_U16 returns a uint16 made up from the high byte and the low byte.
 *
 * @param	lowByte		low byte value for the uint16
 * @param	highByte 	high byte value for the uint16
 * @retval	uint16_t	result
 */
// ****************************************************************************
uint16_t UTIL_Make_U16(const uint8_t lowByte, const uint8_t highByte)
{
	return (uint16_t)lowByte | ((uint16_t)highByte << 8u);
}


// ****************************************************************************
/*!
 * UTIL_FromBytes_U16 returns a uint16 made up from the bytes in a buffer. The
 * bytes are to be ordered little endian.
 * No NULL pointer checks are made!
 *
 * @param	p_data		pointer to buffer containing 2 bytes ordered little endian
 * @retval	uint16_t	result
 */
// ****************************************************************************
uint16_t UTIL_FromBytes_U16(const uint8_t * const p_data)
{
	return (uint16_t)p_data[0u] | ((uint16_t)p_data[1u] << 8u);
}


// ****************************************************************************
/*!
 * UTIL_ToBytes_U16 writes a uint16 value to a buffer ordered little endian.
 * No NULL pointer checks are made!
 *
 * @param	value			uint16 value to be used
 * @param	p_destBuffer 	pointer to destination buffer
 * @retval	none
 *
 */
// ****************************************************************************
void UTIL_ToBytes_U16(const uint16_t value, uint8_t * const p_destBuffer)
{
	p_destBuffer[0u] = (uint8_t)(value & 0xFFu);
	p_destBuffer[1u] = (uint8_t)((value >> 8u) & 0xFFu);
}
