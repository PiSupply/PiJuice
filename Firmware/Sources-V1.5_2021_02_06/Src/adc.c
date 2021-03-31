/*
 * adc.c
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"
#include "ave_filter.h"
#include "time_count.h"
#include "adc.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint16_t m_adcVals[MAX_ANALOG_CHANNELS];
static AVE_FILTER_U16_t	m_aveFilters[MAX_ANALOG_CHANNELS];
static AVE_FILTER_S32_t m_currentSenseFilter;
static uint32_t m_lastAdcStartTime;

static uint16_t m_adcIntRefCal;
static uint32_t m_adcRefScale = 0x00010000u;

static bool m_aveFilterReady;

// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern ADC_HandleTypeDef hadc;


// ****************************************************************************
/*!
 * ADC_Init configures the module to a known initial state
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void ADC_Init(const uint32_t sysTime)
{
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_CS1], sysTime, FILTER_PERIOD_MS_CS1);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_CS2], sysTime, FILTER_PERIOD_MS_CS2);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_VBAT], sysTime, FILTER_PERIOD_MS_VBAT);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_NTC], sysTime, FILTER_PERIOD_MS_NTC);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_POW_DET], sysTime, FILTER_PERIOD_MS_POW_DET);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_BATTYPE], sysTime, FILTER_PERIOD_MS_BATTYPE);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_IO1], sysTime, FILTER_PERIOD_MS_IO1);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_MPUTEMP], sysTime, FILTER_PERIOD_MS_MPUTEMP);
	AVE_FILTER_U16_InitPeriodic(&m_aveFilters[ANALOG_CHANNEL_INTREF], sysTime, FILTER_PERIOD_MS_INTREF);

	AVE_FILTER_S32_InitPeriodic(&m_currentSenseFilter, sysTime, FILTER_PERIOD_MS_CS1 * 8u);

	m_aveFilterReady = false;

	m_adcIntRefCal = *(VREFINT_CAL_ADDR);

	HAL_ADCEx_Calibration_Start(&hadc);

	HAL_ADC_Start_DMA(&hadc, (uint32_t*)&m_adcVals[0u], MAX_ANALOG_CHANNELS);

	MS_TIMEREF_INIT(m_lastAdcStartTime, sysTime);
}


void ADC_Shutdown(void)
{
	HAL_ADC_Stop_DMA(&hadc);
}


// ****************************************************************************
/*!
 * ADC_Service performs periodic updates for this module
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void ADC_Service(const uint32_t sysTime)
{
	uint8_t i;

	if (MS_TIMEREF_TIMEOUT(m_lastAdcStartTime, sysTime, ADC_SAMPLE_PERIOD_MS))
	{
		MS_TIMEREF_INIT(m_lastAdcStartTime, sysTime);

		if ((hadc.Instance->ISR & ADC_ISR_EOS) != 0u)
		{
			HAL_ADC_Stop_DMA(&hadc);

			for (i = 0u; i < MAX_ANALOG_CHANNELS; i++)
			{
				if (false == m_aveFilterReady)
				{
					// Spam in values to fill the buffer
					AVE_FILTER_U16_Update(&m_aveFilters[i], m_adcVals[i]);
				}
				else
				{
					AVE_FILTER_U16_UpdatePeriodic(&m_aveFilters[i], m_adcVals[i], sysTime);
				}
			}

			AVE_FILTER_S32_UpdatePeriodic(&m_currentSenseFilter,
					(int32_t)(m_aveFilters[ANALOG_CHANNEL_CS1].total - m_aveFilters[ANALOG_CHANNEL_CS2].total),
					sysTime
					);

			if (0u == m_aveFilters[0].nextValueIdx)
			{
				m_aveFilterReady = true;
			}

			if ( (true == m_aveFilterReady) && (m_aveFilters[ANALOG_CHANNEL_INTREF].average > 0u) )
			{
				m_adcRefScale = (0x00010000u * ((uint32_t)m_adcIntRefCal)) / (uint32_t)m_aveFilters[ANALOG_CHANNEL_INTREF].average;
			}

			HAL_ADC_Start_DMA(&hadc, (uint32_t*)&m_adcVals[0u], MAX_ANALOG_CHANNELS);
		}
	}
}


// ****************************************************************************
/*!
 * ADC_GetInstantValue returns the last read value of the analog channel, out of
 * bounds channel acces returns 0.
 *
 * @param	channel		channel to be accessed
 * @retval	uint16_t	value of the analog channel
 */
// ****************************************************************************
uint16_t ADC_GetInstantValue(const uint8_t channel)
{
	uint16_t result = 0u;

	if (channel < MAX_ANALOG_CHANNELS)
	{
		result = m_aveFilters[channel].lastVal;
	}

	return result;
}


// ****************************************************************************
/*!
 * ADC_GetAverageValue returns the average value of the analog channel, out of
 * bounds channel acces returns 0.
 *
 * @param	channel		channel to be accessed
 * @retval	uint16_t	averaged value of the analog channel
 */
// ****************************************************************************
uint16_t ADC_GetAverageValue(const uint8_t channel)
{
	uint16_t result = 0u;

	if (channel < MAX_ANALOG_CHANNELS)
	{
		result = m_aveFilters[channel].average;
	}

	return result;
}


// ****************************************************************************
/*!
 * ADC_CalibrateValue adjusts a value read by the adc using the internal reference
 * and its factory calibrated value.
 *
 * @param	value		ADC value
 * @retval	uint16_t	corrected value based on the intref value.
 */
// ****************************************************************************
uint16_t ADC_CalibrateValue(const uint16_t value)
{
	/* Apply fixed point multipler */
	uint32_t result = value * m_adcRefScale;

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint16_t)(result >> 16u);
}


// ****************************************************************************
/*!
 * ADC_ConvertToMV turns an ADC value to a more relevant MV value.
 *
 * @param	value		ADC value
 * @retval	uint16_t	value in MV.
 */
// ****************************************************************************
uint16_t ADC_ConvertToMV(const uint16_t value)
{
	/* Apply fixed point multipler */
	uint32_t result = value * ADC_TO_MV_K;

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint16_t)(result >> 16u);
}


// ****************************************************************************
/*!
 * ADC_GetFilterReady lets the caller know if the buffer in the average filter
 * is full. Will only not be full after a reset for a short while, once checked
 * that should be enough.
 *
 * @param	none
 * @retval	bool		True = average filter ready, false = buffer not full
 */
// ****************************************************************************
bool ADC_GetFilterReady(void)
{
	return m_aveFilterReady;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
