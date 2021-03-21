/*
 * analog.c
 *
 *  Created on: 11.12.2016.
 *      Author: milan
 */

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


#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void AnalogTask(void *argument);

static osThreadId_t analogTaskHandle;

static const osThreadAttr_t analogTask_attributes = {
	.name = "analogTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 128
};
#endif


ADC_AnalogWDGConfTypeDef analogWDGConfig;

static uint32_t tempCalcCounter;
static int16_t m_mcuTemperature = 25; // will contain the mcuTemperature in degree Celsius


// Needs to be called once ADC has filled the filter buffer
void ANALOG_Init(uint32_t sysTime)
{
	// Process resistorConfig
	SwitchResCongigInit(ADC_GetAverageValue(ANALOG_CHANNEL_BATTYPE));

	// Initialise thermister period
	MS_TIMEREF_INIT(tempCalcCounter, sysTime);
}


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


uint16_t ANALOG_GetAVDDMv(void)
{
	return ADC_CalibrateValue(3300u);
}


uint16_t ANALOG_GetMv(uint8_t channel)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(channel));

	return UTIL_FixMul_U32_U16(ADC_TO_MV_K, adcVal);
}


uint16_t ANALOG_GetBatteryMv(void)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_VBAT));

	return UTIL_FixMul_U32_U16(ADC_TO_BATTMV_K, adcVal);
}


// Original averaged both sides of ferrite bead
uint16_t ANALOG_Get5VRailMv(void)
{
	const uint16_t adcVal = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_CS1));

	return UTIL_FixMul_U32_U16(ADC_TO_5VRAIL_MV_K, adcVal);
}


int16_t ANALOG_Get5VRailMa(void)
{
	const int16_t diff = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_CS1)) - ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_CS2));
	const bool neg = (diff < 0u) ? true : false;
	const uint16_t convVal = UTIL_FixMul_U32_U16(ADC_TO_5VRAIL_ISEN_K, abs(diff));

	return (neg == true) ? -convVal : convVal;
}


uint8_t ANALOG_AnalogSamplesReady(void)
{
	return ADC_GetFilterReady();
}


int16_t ANALOG_GetMCUTemp(void)
{
	return m_mcuTemperature;
}


void AnalogStop(void) {
	// Stops adc conversions and disables the analog watchdog interrupt
}

void AnalogStart(void) {
	// Starts adc conversions and enables the analog watchdog interrupt
}

void AnalogPowerIsGood(void){
	// Not worked out what this does yet!!
}

HAL_StatusTypeDef AnalogAdcWDGConfig(uint8_t channel, uint16_t voltThresh_mV)
{
	/*
	uint8_t convStat = ADC_IS_CONVERSION_ONGOING_REGULAR(&hadc);
	if (convStat) HAL_ADC_Stop_DMA(&hadc);

	analogWDGConfig.ITMode = ENABLE;
	analogWDGConfig.Channel = channel;
	analogWDGConfig.HighThreshold = 4095;
	//vbatAdcTresh = (uint32_t)voltThresh_mV * 2981 / 3300; // voltThresh_mV * 4096 * 1000 / 1374 / 3300;
	analogWDGConfig.LowThreshold = (uint32_t)voltThresh_mV * 2981 / 3300;
	analogWDGConfig.WatchdogMode = ADC_ANALOGWATCHDOG_SINGLE_REG;

	if (HAL_ADC_AnalogWDGConfig(&hadc, &analogWDGConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (convStat)
		if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
		{
			Error_Handler();
		}
	*/
	return HAL_OK;
}

void AnalogAdcWDGEnable(uint8_t enable)
{
	/*
	uint8_t convStat = ADC_IS_CONVERSION_ONGOING_REGULAR(&hadc);
	if (convStat) HAL_ADC_Stop_DMA(&hadc);

	analogWDGConfig.ITMode = enable;

	if (HAL_ADC_AnalogWDGConfig(&hadc, &analogWDGConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (convStat)
		if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
		{
			Error_Handler();
		}
	*/
}


