/*
 * led.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef LED_H_
#define LED_H_


typedef enum LedFunction_T
{
	LED_NOT_USED = 0u,
	LED_CHARGE_STATUS,
	LED_ON_OFF_STATUS,
	LED_USER_LED,
	LED_NUMBER
} LedFunction_T;


void LED_Init(const uint32_t sysTime);
void LED_Service(const uint32_t sysTime);

void LED_Shutdown(void);

void LedStop(void);
void LedStart(void);

void LED_SetRGB(const uint8_t ledIdx, const uint8_t r, const uint8_t g, const uint8_t b);
void LED_FunctionSetRGB(const LedFunction_T func, const uint8_t r, const uint8_t g, const uint8_t b);
void LED_SetConfigurationData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len);
void LED_GetConfigurationData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len);
void LED_SetStateData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len);
void LED_GetStateData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len);
void LED_SetBlinkData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len);
void LED_GetBlinkData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len);
uint8_t LED_GetParamR(const uint8_t ledIdx);
uint8_t LED_GetParamG(const uint8_t ledIdx);
uint8_t LED_GetParamB(const uint8_t ledIdx);

#endif /* LED_H_ */
