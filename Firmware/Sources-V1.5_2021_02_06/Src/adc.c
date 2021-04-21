// ----------------------------------------------------------------------------
/*!
 * @file		adc.c
 * @author    	John Steggall
 * @date       	19 March 2021
 * @brief       Lowish level driver for the ADC peripheral. Starts the ADC and using
 * 				DMA transfer in a periodic fashion and updates an averaging filter
 * 				dependant on its update period. On startup and wakeup from stop the
 * 				module is reset and the filter buffers are cleared. Once they've
 * 				completed one non periodic cycle the average filter ready flag is set
 * 				and the averaging is performed periodically as set by the defines.
 * 				There is a special current sense filter also that operates on the
 * 				difference between the CS1 and CS2 channels. The filter totals of
 * 				these channels are used to try and get some more resolution but it
 * 				does mean the reading is quite unstable. The filter operates in mA.
 *
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"
#include "ave_filter.h"
#include "time_count.h"
#include "util.h"

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


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
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

	AVE_FILTER_S32_InitPeriodic(&m_currentSenseFilter, sysTime, FILTER_PERIOD_MS_ISENSE);

	m_aveFilterReady = false;

	m_adcIntRefCal = *(VREFINT_CAL_ADDR);

	HAL_ADCEx_Calibration_Start(&hadc);

	HAL_ADC_Start_DMA(&hadc, (uint32_t*)&m_adcVals[0u], MAX_ANALOG_CHANNELS);

	MS_TIMEREF_INIT(m_lastAdcStartTime, sysTime);
}


// ****************************************************************************
/*!
 * ADC_Shutdown prepares the module for a low power stop mode.
 * @param	none
 * @retval	none
 */
// ****************************************************************************
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
	int16_t iVal;

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

			if ( (true == m_aveFilterReady) && (m_aveFilters[ANALOG_CHANNEL_INTREF].average > 0u) )
			{
				m_adcRefScale = (0x00010000u * ((uint32_t)m_adcIntRefCal)) / (uint32_t)m_aveFilters[ANALOG_CHANNEL_INTREF].average;
			}

			// Convert sense resistor value to mA (not calibrated!)
			iVal = UTIL_FixMul_U32_S16(ADC_RES_TO_MA_K,(int32_t)(m_aveFilters[ANALOG_CHANNEL_CS1].total - m_aveFilters[ANALOG_CHANNEL_CS2].total));

			// update filter
			AVE_FILTER_S32_UpdatePeriodic(&m_currentSenseFilter, iVal, sysTime);

			if (0u == m_aveFilters[0].nextValueIdx)
			{
				m_aveFilterReady = true;
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
 * ADC_GetCurrentSenseAverage gets the special current sense filter value. The
 * value is effectively oversampled to give a higher resolution. The returned
 * value is not corrected for avdd tolerance
 *
 * @param	none
 * @retval	int16_t		Oversampled difference between CS1 and CS2
 */
// ****************************************************************************
int16_t ADC_GetCurrentSenseAverage(void)
{
	// TODO - Make sure this copies and not works on actual value
	int32_t result = m_currentSenseFilter.average;

	if (result < INT16_MIN)
	{
		result = INT16_MIN;
	}

	if (result > INT16_MAX)
	{
		result = INT16_MAX;
	}

	return (int16_t)result;
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


// ****************************************************************************
/*!
 * ADC_SetIFilterPeriod changes the update rate for the current sense filter,
 * it means the calibration routine can use a more stable reading averaged over
 * a greater period and less susceptible to the inherent noise in the measurement.
 *
 * @param	newFilterPeriodMs	new filter update period in mS
 * @retval	none
 */
// ****************************************************************************
void ADC_SetIFilterPeriod(const uint32_t newFilterPeriodMs)
{
	m_currentSenseFilter.filterPeriodMs = newFilterPeriodMs;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
