/*
 * analog.c
 *
 *  Created on: 11.12.2016.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <stdlib.h>

#include "main.h"

#include "analog.h"
#include "nv.h"
#include "time_count.h"
#include "power_source.h"
#include "config_switch_resistor.h"

#include "system_conf.h"
#include "adc.h"
#include "util.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint32_t tempCalcCounter;
static int16_t m_mcuTemperature = 25; // will contain the mcuTemperature in degree Celsius


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * ANALOG_Init configures the module to a known initial state
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void ANALOG_Init(const uint32_t sysTime)
{
	// Initialise thermister period
	MS_TIMEREF_INIT(tempCalcCounter, sysTime);
}


// ****************************************************************************
/*!
 * ANALOG_Shutdown prepares the module for a low power stop mode.
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ANALOG_Shutdown(void)
{

}


// ****************************************************************************
/*!
 * ANALOG_Service performs periodic updates for this module, just works out the
 * temperature of the mcu from the internal registers
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void ANALOG_Service(const uint32_t sysTime)
{
	const uint16_t AVDD = ANALOG_GetAVDDMv();

	if (MS_TIMEREF_TIMEOUT(tempCalcCounter, sysTime, 2000u))
	{
		MS_TIMEREF_INIT(tempCalcCounter, sysTime);

		int32_t vtemp = (((uint32_t)ADC_GetAverageValue(ANALOG_CHANNEL_MPUTEMP)) * AVDD * 10u) >> 12u;

		volatile int32_t v30 = (((uint32_t)*TEMP30_CAL_ADDR ) * 33000) >> 12;
		m_mcuTemperature = (v30 - vtemp) / 43 + 30; //avg_slope = 4.3
		//mcuTemperature = ((((int32_t)*TEMP30_CAL_ADDR - analogIn[7]) * 767) >> 12) + 30;
	}
}


// ****************************************************************************
/*!
 * ANALOG_GetAVDDMv gets the voltage of the 3v3 supply rail in millivolts.
 *
 * @param	none
 * @retval	uint16_t		Voltage of the 3v3 rail in mV
 */
// ****************************************************************************
uint16_t ANALOG_GetAVDDMv(void)
{
	return ADC_CalibrateValue(3300u);
}


// ****************************************************************************
/*!
 * ANALOG_GetMv converts the direct ADC pin value to millivolts, corrected for
 * inaccuracy of the analog reference voltage (AVDD)
 *
 * @param	none
 * @retval	uint16_t		mcu pin measured value in millivolts
 */
// ****************************************************************************
uint16_t ANALOG_GetMv(const uint8_t channelIdx)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(channelIdx));

	return UTIL_FixMul_U32_U16(ADC_TO_MV_K, adcVal);
}


// ****************************************************************************
/*!
 * ANALOG_GetBatteryMv returns the measured value of the battery termial voltage
 * corrected for AVDD variance and averaged.
 *
 * @param	none
 * @retval	uint16_t		battery terminal measured value in millivolts
 */
// ****************************************************************************
uint16_t ANALOG_GetBatteryMv(void)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_VBAT));

	return UTIL_FixMul_U32_U16(ADC_TO_BATTMV_K, adcVal);
}


// ****************************************************************************
/*!
 * ANALOG_Get5VRailMv returns the measured value of the 5V rail corrected for AVDD
 * variance and averaged.
 *
 * @param	none
 * @retval	uint16_t		5v rail measured value in millivolts
 */
// ****************************************************************************
uint16_t ANALOG_Get5VRailMv(void)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_CS1));

	return UTIL_FixMul_U32_U16(ADC_TO_5VRAIL_MV_K, adcVal);
}


// ****************************************************************************
/*!
 * ANALOG_Get5VRailMa a stab in the dark, finger in the air rough estimate of the
 * current to or from the RPi. It is averaged and errors of AVDD dialed out but
 * really is probably never accurate due to the low SNR of the hardware implementation.
 *
 * @param	none
 * @retval	uint16_t		5v rail measured current in milliamps
 */
// ****************************************************************************
int16_t ANALOG_Get5VRailMa(void)
{
	const int16_t diff = ADC_GetAverageValue(ANALOG_CHANNEL_CS1) - ADC_GetAverageValue(ANALOG_CHANNEL_CS2);
	const bool neg = (diff < 0u) ? true : false;
	const uint16_t convVal = UTIL_FixMul_U32_U16(ADC_TO_5VRAIL_ISEN_K, ADC_CalibrateValue(abs(diff)));

	return (neg == true) ? -convVal : convVal;
}


// ****************************************************************************
/*!
 * ANALOG_GetMCUTemp returns the temperature of the processor in degrees
 *
 * @param	none
 * @retval	int16_t		temperature of the processor in degrees
 */
// ****************************************************************************
int16_t ANALOG_GetMCUTemp(void)
{
	return m_mcuTemperature;
}

