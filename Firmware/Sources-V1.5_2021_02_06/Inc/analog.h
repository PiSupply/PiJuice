/*
 * analog.h
 *
 *  Created on: 11.12.2016.
 *      Author: milan
 */

#ifndef ANALOG_H_
#define ANALOG_H_

#include "stdint.h"
#include "stm32f0xx_hal.h"

#define ADC_SCAN_CHANNELS		8
#define ADC_BUFFER_LENGTH		((uint16_t)512*ADC_SCAN_CHANNELS)
#define POW_DET_SENS_CHN	4
#define ADC_VBAT_SENS_CHN	2
#define ADC_NTC_CHN	3
#define ADC_IO1_CHN	5
#define ADC_TEMP_SENS_CHN	6
#define ADC_VREF_BUFF_CHN	7
#define ADC_CONT_MODE_NORMAL	0
#define ADC_CONT_MODE_LOW_VOLTAGE 	1 // In this mode one channel in scan group is internal reference
#define ADC_GET_BUFFER_SAMPLE(i)	(analogIn[(i)])

//#define ANALOG_IS_SAMPLES_VALID()	 (HAL_IS_BIT_SET(hadc.Instance->CR, ADC_CR_ADSTART) && (analogBufferTicks > (HAL_GetTick()+100) ))

extern int32_t mcuTemperature;

extern ADC_HandleTypeDef hadc;
extern uint32_t analogIn[ADC_BUFFER_LENGTH];

extern uint16_t aVdd;
extern ADC_AnalogWDGConfTypeDef analogWDGConfig;

//extern uint32_t analogBufferTicks;

void AnalogInit(void);
#if !defined(RTOS_FREERTOS)
void AnalogTask(void);
#endif
int16_t Get5vIoVoltage();
void AnalogStop(void);
void AnalogStart(void);
void AnalogPowerIsGood(void);
//void AnalogSetAdcMode(uint8_t mode);
void AnalogAdcWDGEnable(uint8_t enable);
uint8_t AnalogSamplesReady();
HAL_StatusTypeDef AnalogAdcWDGConfig(uint8_t channel, uint16_t voltThresh_mV);
uint16_t GetSampleVoltage(uint8_t channel);
uint16_t GetAverageBatteryVoltage(uint8_t channel);

__STATIC_INLINE uint16_t GetSample(uint8_t channel) {
    int32_t pos =  __HAL_DMA_GET_COUNTER(hadc.DMA_Handle);
    int32_t ind = (((ADC_BUFFER_LENGTH - pos - 1) * (32768/ADC_SCAN_CHANNELS)) >> 15) * ADC_SCAN_CHANNELS + channel;
	if (ind > (ADC_BUFFER_LENGTH - pos - 1)) ind -= ADC_SCAN_CHANNELS; // check if calculated channel sample is fresh
	if (ind < 0) ind += ADC_BUFFER_LENGTH;
	return analogIn[ind];
}

int32_t GetSampleAverageDiff(uint8_t channel1, uint8_t channel2);

__STATIC_INLINE uint16_t GetAdcWDGThreshold() {
	return analogWDGConfig.LowThreshold;
}

#endif /* ANALOG_H_ */
