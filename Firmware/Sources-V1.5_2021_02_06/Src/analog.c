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

/* mcuTemperature sensor calibration value address */
#define TEMP30_CAL_ADDR ((uint16_t*) ((uint32_t) 0x1FFFF7B8))
#define VREFINT_CAL_ADDR ((uint16_t*) ((uint32_t) 0x1FFFF7BA))

#define ANALOG_ADC_GET_AVDD(a)		 (((uint32_t)*VREFINT_CAL_ADDR ) * 3300 / (a)) // refVolt = vRefAdc * aVdd / 4096 //volatile uint32_t refVolt = ((uint32_t)*VREFINT_CAL_ADDR ) * 3300 / 4096;

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

uint16_t aVdd;

extern ADC_HandleTypeDef hadc;
ADC_ChannelConfTypeDef sConfig;
ADC_AnalogWDGConfTypeDef analogWDGConfig;

static uint32_t tempCalcCounter;

volatile uint32_t vRefAdc;

uint32_t analogIn[ADC_BUFFER_LENGTH];

uint16_t GetSampleVoltage(uint8_t channel) {
    int32_t pos =  __HAL_DMA_GET_COUNTER(hadc.DMA_Handle);
    int32_t ind = (((ADC_BUFFER_LENGTH - pos - 1) * (32768/ADC_SCAN_CHANNELS)) >> 15) * ADC_SCAN_CHANNELS + channel;
	if (ind > (ADC_BUFFER_LENGTH - pos - 1)) ind -= ADC_SCAN_CHANNELS; // check if calculated channel sample is fresh
	if (ind < 0) ind += ADC_BUFFER_LENGTH;
	int16_t indRef = ind + ADC_VREF_BUFF_CHN - channel;
	return (analogIn[ind] * ((uint32_t)*VREFINT_CAL_ADDR ) * 412 / analogIn[indRef]) >>  9;
}

uint16_t GetAverageBatteryVoltage(uint8_t channel) {
	uint16_t i;
	uint32_t sum = 0;
	for (i = channel; i < ADC_BUFFER_LENGTH; i+=(ADC_BUFFER_LENGTH/8)) {
		sum += analogIn[i];
	}
	return (sum* 4535 / analogIn[ADC_VREF_BUFF_CHN] * ((uint32_t)*VREFINT_CAL_ADDR )) >> 15 ;//(sum*2267) >> 14;
	//return (sum * ((uint32_t)*VREFINT_CAL_ADDR ) * 412 / analogIn[ADC_VREF_BUFF_CHN]) >>  8;
}

int32_t mcuTemperature = 25; // will contain the mcuTemperature in degree Celsius

int16_t Get5vIoVoltage() {
	int16_t adcAvg = (analogIn[0] + analogIn[1] + analogIn[ADC_BUFFER_LENGTH/4] + analogIn[ADC_BUFFER_LENGTH/4+1] + analogIn[ADC_BUFFER_LENGTH/2] + analogIn[ADC_BUFFER_LENGTH/2+1] + analogIn[ADC_BUFFER_LENGTH*3/4] + analogIn[ADC_BUFFER_LENGTH*3/4+1]) >> 3;
	return (aVdd > 3200 && aVdd < 3400) ? (adcAvg * aVdd) >> 11 : (adcAvg * 3300) >> 11;//adcAvg * aVdd / 4096 * 2;
}

int32_t GetSampleAverageDiff(uint8_t channel1, uint8_t channel2) {
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

void AnalogInit(void) {
	//Configure for the selected ADC regular channel to be converted.
  sConfig.Channel = ADC_CHANNEL_5; // CHG_CUR
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;//ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

  volatile uint32_t resistorConfigAdc;
  HAL_ADC_Start(&hadc);
  HAL_ADC_PollForConversion(&hadc, 1);
  resistorConfigAdc = HAL_ADC_GetValue(&hadc);
  HAL_ADC_Stop(&hadc);
  SwitchResCongigInit(resistorConfigAdc);

  sConfig.Channel = ADC_CHANNEL_5; // CHG_CUR
  sConfig.Rank = ADC_RANK_NONE;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }
  sConfig.Channel = ADC_CHANNEL_17; // Vref
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;//ADC_SAMPLETIME_28CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

  HAL_ADC_Start(&hadc);
  HAL_ADC_PollForConversion(&hadc, 1);
  vRefAdc = HAL_ADC_GetValue(&hadc);
  HAL_ADC_Stop(&hadc);
  /*sConfig.Channel = ADC_CHANNEL_17; //
  sConfig.Rank = ADC_RANK_NONE;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }*/

  aVdd = ANALOG_ADC_GET_AVDD(vRefAdc);

	/**Configure for the selected ADC regular channel to be converted.
	*/
	sConfig.Channel = ADC_CHANNEL_0; // CS1
	sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
	sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;//ADC_SAMPLETIME_28CYCLES_5;
	if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}

	/**Configure for the selected ADC regular channel to be converted.
	*/
  sConfig.Channel = ADC_CHANNEL_1; // CS2
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

	/**Configure for the selected ADC regular channel to be converted.
	*/
  sConfig.Channel = ADC_CHANNEL_2;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

	/**Configure for the selected ADC regular channel to be converted.
	*/
  sConfig.Channel = ADC_CHANNEL_3; // NTC
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

	/**Configure for the selected ADC regular channel to be converted.
	*/
  sConfig.Channel = ADC_CHANNEL_4; // POW_DET_SEN
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

  /*sConfig.Channel = ADC_CHANNEL_6;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }*/

  sConfig.Channel = ADC_CHANNEL_7; // IO1
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_16; // temperature
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
	Error_Handler();
  }

  // make bufer data invalid
  analogIn[0] = 0xFFFFFFFF;
  analogIn[ADC_BUFFER_LENGTH-1] = 0xFFFFFFFF;

	// Start conversion in DMA mode
	if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
	{
		Error_Handler();
	}

   MS_TIME_COUNTER_INIT(tempCalcCounter);
#if defined(RTOS_FREERTOS)
   analogTaskHandle = osThreadNew(AnalogTask, (void*)NULL, &analogTask_attributes);
#endif
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

	if (MS_TIME_COUNT(tempCalcCounter) > 2000) {
		int32_t vtemp = (((uint32_t)analogIn[ADC_TEMP_SENS_CHN]) * aVdd * 10) >> 12;
		volatile int32_t v30 = (((uint32_t)*TEMP30_CAL_ADDR ) * 33000) >> 12;
		mcuTemperature = (v30 - vtemp) / 43 + 30; //avg_slope = 4.3
		//mcuTemperature = ((((int32_t)*TEMP30_CAL_ADDR - analogIn[7]) * 767) >> 12) + 30;
		MS_TIME_COUNTER_INIT(tempCalcCounter);
	}
	aVdd = ANALOG_ADC_GET_AVDD(GetSample(ADC_VREF_BUFF_CHN));
}
#endif

void AnalogStop(void) {
	if (HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART)) {
		//analogIn[0] = 0xFFFFFFFF;
		//analogIn[ADC_BUFFER_LENGTH-1] = 0xFFFFFFFF;
		HAL_ADC_Stop_DMA(&hadc);

		analogWDGConfig.ITMode = DISABLE;

		if (HAL_ADC_AnalogWDGConfig(&hadc, &analogWDGConfig) != HAL_OK)
		{
			Error_Handler();
		}
	}
}

void AnalogStart(void) {
	// Start conversion in DMA mode
	if (!HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART)) {

		/*sConfig.Channel = ADC_CHANNEL_16; //
		sConfig.Rank = ADC_RANK_NONE;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
			Error_Handler();
		}*/

		/* sConfig.Channel = ADC_CHANNEL_17; // Vref
		sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
		sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;//ADC_SAMPLETIME_28CYCLES_5;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
			Error_Handler();
		}

		HAL_ADC_Start(&hadc);
		HAL_ADC_PollForConversion(&hadc, 1);
		vRefAdc = HAL_ADC_GetValue(&hadc);
		HAL_ADC_Stop(&hadc);
		sConfig.Channel = ADC_CHANNEL_17; // remove adc ref voltage channel
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

		//aVdd = ANALOG_ADC_GET_AVDD(vRefAdc);

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
}

void AnalogPowerIsGood(void) {
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

	aVdd = ANALOG_ADC_GET_AVDD(vRefAdc);

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

/*void AnalogSetAdcMode(uint8_t mode) {
	if (mode == ADC_CONT_MODE_NORMAL) {

		uint8_t stopped = 0;
		if (HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART))  {
			HAL_ADC_Stop_DMA(&hadc);
			stopped = 1;
		}

		// Substitute internal reference voltage with channel 1
		sConfig.Channel = ADC_CHANNEL_17;
		sConfig.Rank = ADC_RANK_NONE;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
		Error_Handler();
		}

		sConfig.Channel = ADC_CHANNEL_1;
		sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
		sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
		Error_Handler();
		}

		// make buffer data invalid
		analogIn[0] = 0xFFFFFFFF;
		analogIn[ADC_BUFFER_LENGTH-1] = 0xFFFFFFFF;

		if (stopped) {
			if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
			{
				Error_Handler();
			}
		}
	} else if (mode == ADC_CONT_MODE_LOW_VOLTAGE) {

		uint8_t stopped = 0;
		if (HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART))  {
			HAL_ADC_Stop_DMA(&hadc);
			stopped = 1;
		}

		// Substitute channel 1 with internal reference voltage channel
		sConfig.Channel = ADC_CHANNEL_1;
		sConfig.Rank = ADC_RANK_NONE;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
		Error_Handler();
		}

		sConfig.Channel = ADC_CHANNEL_17; // Vref
		sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
		sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;;
		if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
		{
		Error_Handler();
		}

		// make buffer data invalid
		analogIn[0] = 0xFFFFFFFF;
		analogIn[ADC_BUFFER_LENGTH-1] = 0xFFFFFFFF;

		if (stopped) {
			if (HAL_ADC_Start_DMA(&hadc, analogIn, ADC_BUFFER_LENGTH) != HAL_OK)
			{
				Error_Handler();
			}
		}
	}
}*/

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
