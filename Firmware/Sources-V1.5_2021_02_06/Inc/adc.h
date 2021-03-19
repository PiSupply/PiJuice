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
uint16_t ADC_GetInstantValue(uint8_t channel);
uint16_t ADC_GetAverageValue(uint8_t channel);

#endif /* ADC_H_ */
