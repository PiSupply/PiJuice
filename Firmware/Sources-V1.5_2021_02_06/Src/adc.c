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
static uint32_t m_lastAdcStartTime;

static uint16_t m_adcIntRefCal;
static float m_adcRefScale = 1.0f;


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
	uint8_t i = MAX_ANALOG_CHANNELS;

	while (i-- > 0u)
	{
		m_adcVals[i] = 0u;
		AVE_FILTER_U16_Reset(&m_aveFilters[i]);
	}

	m_adcIntRefCal = *(VREFINT_CAL_ADDR);

	HAL_ADCEx_Calibration_Start(&hadc);

	HAL_ADC_Start_DMA(&hadc, (uint32_t*)&m_adcVals[0u], MAX_ANALOG_CHANNELS);

	MS_TIMEREF_INIT(m_lastAdcStartTime, sysTime);
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
				AVE_FILTER_U16_Update(&m_aveFilters[i], m_adcVals[i]);
			}

			m_adcRefScale = (float)m_adcIntRefCal / m_aveFilters[ANALOG_CHANNEL_INTREF].average;

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
uint16_t ADC_GetInstantValue(uint8_t channel)
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
uint16_t ADC_GetAverageValue(uint8_t channel)
{
	uint16_t result = 0u;

	if (channel < MAX_ANALOG_CHANNELS)
	{
		result = m_aveFilters[channel].average;
	}

	return result;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
