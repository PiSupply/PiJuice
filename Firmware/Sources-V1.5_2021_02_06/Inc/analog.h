/*
 * analog.h
 *
 *  Created on: 11.12.2016.
 *      Author: milan
 */

#ifndef ANALOG_H_
#define ANALOG_H_

extern ADC_AnalogWDGConfTypeDef analogWDGConfig;

void ANALOG_Init(const uint32_t sysTime);
void ANALOG_Service(const uint32_t sysTime);
void ANALOG_Shutdown(void);

uint16_t ANALOG_GetAVDDMv(void);
uint16_t ANALOG_GetMv(const uint8_t channelIdx);
uint16_t ANALOG_GetBatteryMv(void);
uint16_t ANALOG_Get5VRailMv(void);
int16_t ANALOG_Get5VRailMa(void);
uint8_t ANALOG_AnalogSamplesReady(void);
int16_t ANALOG_GetMCUTemp(void);


void AnalogStop(void);
void AnalogStart(void);
void AnalogPowerIsGood(void);

void AnalogAdcWDGEnable(uint8_t enable);
HAL_StatusTypeDef AnalogAdcWDGConfig(uint8_t channel, uint16_t voltThresh_mV);
__STATIC_INLINE uint16_t GetAdcWDGThreshold() {
	return analogWDGConfig.LowThreshold;
}

#endif /* ANALOG_H_ */
