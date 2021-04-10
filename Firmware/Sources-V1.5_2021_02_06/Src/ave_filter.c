// ----------------------------------------------------------------------------
/*!
 * @file		ave_filter.c
 * @author    	John Steggall
 * @date       	19 March 2021
 * @brief       Average filter blocks for U16 and S32, can be updated periodically
 * 				by calling the periodic functions or manage time by calling the
 * 				update functions directly. To use the periodic update, the init
 * 				function must be called for the filter.
 *
 */
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// Include section - add all #includes here:
#include "main.h"

#include "time_count.h"
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
 * AVE_FILTER_U16_UpdatePeriodic updates the filter with a new value and calculates
 * the average on a periodic basis, the filter must have been initialised using
 * the periodic init function as this sets the update period.
 *
 * @param	p_filter	filter to update
 * @param	newValue	uint16 value to update the filter with
 * @param	sysTime		current value of the system tick timer
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_U16_UpdatePeriodic(AVE_FILTER_U16_t * const p_filter,
									const uint16_t newValue,
									const uint32_t sysTime)
{
	if (MS_TIMEREF_TIMEOUT(p_filter->lastFilterUpdateTime, sysTime, p_filter->filterPeriodMs))
	{
		MS_TIMEREF_INIT(p_filter->lastFilterUpdateTime, sysTime);

		AVE_FILTER_U16_Update(p_filter, newValue);
	}
}


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
 * AVE_FILTER_U16_InitPeriodic resets all the internal filter values and initialises
 * the values for performing periodic updates
 *
 * @param	p_filter		filter to reset
 * @param	sysTime			current value of the system tick timer
 * @param	filterPeriodMs	filter update period in milliseconds
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_U16_InitPeriodic(AVE_FILTER_U16_t * const p_filter, const uint32_t sysTime, const uint16_t filterPeriodMs)
{
	MS_TIMEREF_INIT(p_filter->lastFilterUpdateTime, sysTime);
	p_filter->filterPeriodMs = filterPeriodMs;
	AVE_FILTER_U16_Reset(p_filter);
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


// ****************************************************************************
/*!
 * AVE_FILTER_S32_UpdatePeriodic updates the filter with a new value and calculates
 * the average on a periodic basis, the filter must have been initialised using
 * the periodic init function as this sets the update period.
 *
 * @param	p_filter	filter to update
 * @param	newValue	int32 value to update the filter with
 * @param	sysTime		current value of the system tick timer
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_S32_UpdatePeriodic(AVE_FILTER_S32_t * const p_filter,
									const int32_t newValue,
									const uint32_t sysTime)
{
	if (MS_TIMEREF_TIMEOUT(p_filter->lastFilterUpdateTime, sysTime, p_filter->filterPeriodMs))
	{
		MS_TIMEREF_INIT(p_filter->lastFilterUpdateTime, sysTime);

		AVE_FILTER_S32_Update(p_filter, newValue);
	}
}


// ****************************************************************************
/*!
 * AVE_FILTER_U16_Update updates the filter with a new value and calculates the
 * average
 *
 * @param	p_filter	filter to update
 * @param	newValue	int32 value to update the filter with
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_S32_Update(AVE_FILTER_S32_t * const p_filter, const int32_t newValue)
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
		if (AVE_FILTER_ELEMENT_COUNT ==  ++p_filter->nextValueIdx)
		{
			p_filter->nextValueIdx = 0u;
		}

		p_filter->lastVal = newValue;
	}
}


// ****************************************************************************
/*!
 * AVE_FILTER_S32_Reset resets all the internal filter values
 *
 * @param	p_filter	filter to reset
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_S32_Reset(AVE_FILTER_S32_t * const p_filter)
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


// ****************************************************************************
/*!
 * AVE_FILTER_S32_InitPeriodic resets all the internal filter values and initialises
 * the values for performing periodic updates
 *
 * @param	p_filter		filter to reset
 * @param	sysTime			current value of the system tick timer
 * @param	filterPeriodMs	filter update period in milliseconds
 * @retval	none
 */
// ****************************************************************************
void AVE_FILTER_S32_InitPeriodic(AVE_FILTER_S32_t * const p_filter, const uint32_t sysTime, const uint16_t filterPeriodMs)
{
	MS_TIMEREF_INIT(p_filter->lastFilterUpdateTime, sysTime);
	p_filter->filterPeriodMs = filterPeriodMs;
	AVE_FILTER_S32_Reset(p_filter);
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
