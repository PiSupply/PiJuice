/*
 * ave_filter.c
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:
#include <string.h>
#include <stdint.h>
#include "ave_filter.h"

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
 * AVE_FILTER_U16_Update updates the filter with a new value and calculates the
 * average
 *
 * @param	p_filter	filter to update
 * @param	newValue	uint16 value to update the filter with
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_U16_Update(AVE_FILTER_U16_t * const p_filter, const uint16_t newValue)
{
	if (NULL != p_filter)
	{
		/* Add in new value */
		p_filter->total -= p_filter->elements[p_filter->nextValueIdx];
		p_filter->total += newValue;
		p_filter->elements[p_filter->nextValueIdx] = newValue;

		/* Calculate filter average */
		p_filter->average = p_filter->total / AVE_FILTER_ELEMENT_COUNT;

		/* Update pointer to next value index */
		if (AVE_FILTER_ELEMENT_COUNT ==  ++ p_filter->nextValueIdx)
		{
			p_filter->nextValueIdx = 0u;
		}

		p_filter->lastVal = newValue;
	}
}


// ****************************************************************************
/*!
 * AVE_FILTER_U16_Reset resets all the internal filter values
 *
 * @param	p_filter	filter to reset
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_U16_Reset(AVE_FILTER_U16_t * const p_filter)
{
	if (NULL != p_filter)
	{
		p_filter->total = 0u;
		p_filter->average = 0u;
		p_filter->nextValueIdx = AVE_FILTER_ELEMENT_COUNT;
		p_filter->lastVal = 0u;

		while (p_filter->nextValueIdx > 0u)
		{
			p_filter->nextValueIdx--;
			p_filter->elements[p_filter->nextValueIdx] = 0u;
		}
	}
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
