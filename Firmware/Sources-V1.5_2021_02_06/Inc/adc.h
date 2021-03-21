/*
 * adc.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */


#ifndef ADC_H_
#define ADC_H_

void ADC_Init(const uint32_t sysTime);
void ADC_Service(const uint32_t sysTime);
uint16_t ADC_GetInstantValue(const uint8_t channel);
uint16_t ADC_GetAverageValue(const uint8_t channel);
uint16_t ADC_CalibrateValue(const uint16_t value);
bool ADC_GetFilterReady(void);

#endif /* ADC_H_ */
