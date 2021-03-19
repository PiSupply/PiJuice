/*
 * analog.c
 *
 *  Created on: 11.12.2016.
 *      Author: milan
 */

#include "main.h"

#include "analog.h"
#include "stm32f0xx_hal.h"
#include "nv.h"
#include "time_count.h"
#include "power_source.h"
#include "config_switch_resistor.h"

#include "system_conf.h"
#include "adc.h"

//#define ANALOG_ADC_GET_AVDD(a)		 (((uint32_t)*VREFINT_CAL_ADDR ) * 3300 / (a)) // refVolt = vRefAdc * aVdd / 4096 //volatile uint32_t refVolt = ((uint32_t)*VREFINT_CAL_ADDR ) * 3300 / 4096;

//#define ANALOG_GET_VDG_AVG()	(4790 - (((analogIn[3] + analogIn[(uint16_t)ADC_SCAN_CHANNELS*256+3] + analogIn[(uint16_t)ADC_SCAN_CHANNELS*512+3] + analogIn[(uint16_t)ADC_SCAN_CHANNELS*768+3]) * aVdd) >> 13))

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

uint16_t ConvertADCValToBatteryMV(uint16_t value);

static uint16_t m_AVDD;
static uint16_t m_VBATT;

extern ADC_HandleTypeDef hadc;
ADC_ChannelConfTypeDef sConfig;
ADC_AnalogWDGConfTypeDef analogWDGConfig;

static uint32_t tempCalcCounter;

volatile uint32_t vRefAdc;

uint32_t analogIn[ADC_BUFFER_LENGTH];

// Returns AVDD in place of extern declaration
uint16_t GetAVDD(void)
{
	return m_AVDD;
}


/* Think this returns sample voltage in mv */
uint16_t GetSampleVoltage(uint8_t channel) {
	return ADC_GetMV(channel);
}

uint16_t GetAverageBatteryVoltage(void) {

	return m_VBATT;
}

int32_t mcuTemperature = 25; // will contain the mcuTemperature in degree Celsius

int16_t Get5vIoVoltage() {
	return ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_CS1));
}


int32_t GetSampleAverageDiff(uint8_t channel1, uint8_t channel2) {
	return ADC_GetAverageValue(channel1) - ADC_GetAverageValue(channel2);
}


// Arguments not used? Measures voltage across FB2
int32_t GetSampleAverageDiff_old(uint8_t channel1, uint8_t channel2) {
	/*return (((int32_t)analogIn[channel1] - analogIn[channel2] + analogIn[channel1 + (ADC_BUFFER_LENGTH/4)] - analogIn[channel2 + (ADC_BUFFER_LENGTH/4)] + analogIn[channel1 + (ADC_BUFFER_LENGTH/2)] - analogIn[channel2 + (ADC_BUFFER_LENGTH/2)] + analogIn[channel1 + (ADC_BUFFER_LENGTH*3/4)] - analogIn[channel2 + (ADC_BUFFER_LENGTH*3/4)]
			       + analogIn[channel1 + (ADC_BUFFER_LENGTH/8)] - analogIn[channel2 + (ADC_BUFFER_LENGTH/8)] + analogIn[channel1 + (ADC_BUFFER_LENGTH*3/8)] - analogIn[channel2 + (ADC_BUFFER_LENGTH*3/8)] + analogIn[channel1 + (ADC_BUFFER_LENGTH*5/8)] - analogIn[channel2 + (ADC_BUFFER_LENGTH*5/8)] + analogIn[channel1 + (ADC_BUFFER_LENGTH*7/8)] - analogIn[channel2 + (ADC_BUFFER_LENGTH*7/8)] )*2 + 1) >> 4;
*/
	//return (int32_t)analogIn[channel1] - analogIn[channel2];
	uint16_t i;
	int32_t diff = 0;
	for (i = 0; i < ADC_BUFFER_LENGTH; i+=(ADC_BUFFER_LENGTH/8)) {
		diff += analogIn[i] - analogIn[i+1];
	}
	return ((diff << 1) + 1) >> 4;
}

uint8_t AnalogSamplesReady() {
	return analogIn[0] != 0xFFFFFFFF && analogIn[ADC_BUFFER_LENGTH-1] != 0xFFFFFFFF;
}


// Needs to be called once ADC has filled the filter buffer
void AnalogInit(void) {

	uint32_t sysTime = HAL_GetTick();

	// Process resistorConfig
	SwitchResCongigInit(ADC_GetAverageValue(ANALOG_CHANNEL_BATTYPE));

	// Initialise AVDD value
	m_AVDD = ADC_CalibrateValue(3300u);

	// Initialise thermister period
	MS_TIMEREF_INIT(tempCalcCounter, sysTime);
}

#if defined(RTOS_FREERTOS)
static void AnalogTask(void *argument) {

	for(;;)
	{
		osDelay(2000);
		int32_t vtemp = (((uint32_t)analogIn[ADC_TEMP_SENS_CHN]) * aVdd * 10) >> 12;
		volatile int32_t v30 = (((uint32_t)*TEMP30_CAL_ADDR ) * 33000) >> 12;
		mcuTemperature = (v30 - vtemp) / 43 + 30; //avg_slope = 4.3
		//mcuTemperature = ((((int32_t)*TEMP30_CAL_ADDR - analogIn[7]) * 767) >> 12) + 30;

		aVdd = ANALOG_ADC_GET_AVDD(GetSample(ADC_VREF_BUFF_CHN));
	}
}
#else
void AnalogTask(void) {

	const uint32_t sysTime = HAL_GetTick();

	if (MS_TIMEREF_TIMEOUT(tempCalcCounter, sysTime, 2000u)) {

		int32_t vtemp = (((uint32_t)ADC_GetAverageValue(ANALOG_CHANNEL_MPUTEMP)) * m_AVDD * 10u) >> 12u;

		volatile int32_t v30 = (((uint32_t)*TEMP30_CAL_ADDR ) * 33000) >> 12;
		mcuTemperature = (v30 - vtemp) / 43 + 30; //avg_slope = 4.3
		//mcuTemperature = ((((int32_t)*TEMP30_CAL_ADDR - analogIn[7]) * 767) >> 12) + 30;

		MS_TIMEREF_INIT(tempCalcCounter, sysTime);


	}

	m_AVDD = ADC_CalibrateValue(3300u);
	m_VBATT = ConvertADCValToBatteryMV(ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_VBAT)));

}
#endif

void AnalogStop(void) {
	// Stops adc conversions and disables the analog watchdog interrupt
}

void AnalogStart(void) {
	// Starts adc conversions and enables the analog watchdog interrupt
}

void AnalogPowerIsGood(void){
	// Not worked out what this does yet!!
}

void AnalogPowerIsGood_old(void) {
	// get avdd
	if (HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART)) {
		HAL_ADC_Stop_DMA(&hadc);
		analogWDGConfig.ITMode = DISABLE;
		if (HAL_ADC_AnalogWDGConfig(&hadc, &analogWDGConfig) != HAL_OK)
		{
		  Error_Handler();
		}
	}

	/*sConfig.Channel = ADC_CHANNEL_16; //
	sConfig.Rank = ADC_RANK_NONE;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}*/

	/*sConfig.Channel = ADC_CHANNEL_17; // Vref
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;//ADC_SAMPLETIME_28CYCLES_5;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}*/

	HAL_ADC_Start(&hadc);
	HAL_ADC_PollForConversion(&hadc, 1);
	vRefAdc = HAL_ADC_GetValue(&hadc);
	HAL_ADC_Stop(&hadc);
	/*sConfig.Channel = ADC_CHANNEL_17; // remove adc ref voltage channel
	sConfig.Rank = ADC_RANK_NONE;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}*/

	/*sConfig.Channel = ADC_CHANNEL_16; // return to temperature channel
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}*/

	//m_AVDD = ANALOG_ADC_GET_AVDD(vRefAdc);

	analogWDGConfig.ITMode = ENABLE;
	if (HAL_ADC_AnalogWDGConfig(&hadc, &analogWDGConfig) != HAL_OK)
	{
	  Error_Handler();
	}

	// make bufer data invalid
	analogIn[0] = 0xFFFFFFFF;
	analogIn[ADC_BUFFER_LENGTH-1] = 0xFFFFFFFF;
	if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
	{
		Error_Handler();
	}
}


HAL_StatusTypeDef AnalogAdcWDGConfig(uint8_t channel, uint16_t voltThresh_mV) {
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
	return HAL_OK;
}

void AnalogAdcWDGEnable(uint8_t enable) {
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
}


uint16_t ConvertADCValToBatteryMV(uint16_t value)
{
	/* Apply fixed point multipler */
	uint32_t result = (value * ADC_TO_BATTMV_K);

	// Round up if halfway there
	if (0u != (result & 0x8000u))
	{
		result += 0x10000u;
	}

	return (uint16_t)(result >> 16u);
}
