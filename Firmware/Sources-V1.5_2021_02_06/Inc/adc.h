// ----------------------------------------------------------------------------
/*!
 * @file		adc.h
 * @author    	John Steggall
 * @date       	19 March 2021
 * @brief       Header file for adc.c
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef ADC_H_
#define ADC_H_

void ADC_Init(const uint32_t sysTime);
void ADC_Service(const uint32_t sysTime);
void ADC_Shutdown(void);

uint16_t ADC_GetInstantValue(const uint8_t channel);
uint16_t ADC_GetAverageValue(const uint8_t channel);
uint16_t ADC_CalibrateValue(const uint16_t value);
bool ADC_GetFilterReady(void);
int16_t ADC_GetCurrentSenseAverage(void);
void ADC_SetIFilterPeriod(const uint32_t newFilterPeriodMs);

#endif /* ADC_H_ */
