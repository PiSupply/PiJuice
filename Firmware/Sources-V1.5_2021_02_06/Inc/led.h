/*
 * led.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef LED_H_
#define LED_H_

#define LED1	0u
#define LED2	1u

typedef enum LedFunction_T
{
	LED_NOT_USED = 0u,
	LED_CHARGE_STATUS,
	LED_ON_OFF_STATUS,
	LED_USER_LED,
	LED_NUMBER
} LedFunction_T;

void LedInit(void);
void LedTask(void);
void LedSetRGB(uint8_t led, uint8_t r, uint8_t g, uint8_t b);
void LedStop(void);
void LedStart(void);
void LedFunctionSetRGB(LedFunction_T func, uint8_t r, uint8_t g, uint8_t b);
void LedSetConfiguarion(uint8_t led, uint8_t data[], uint8_t len);
void LedGetConfiguarion(uint8_t led, uint8_t data[], uint16_t *len);
void LedCmdSetState(uint8_t led, uint8_t data[], uint8_t len);
void LedCmdGetState(uint8_t led, uint8_t data[], uint16_t *len);
void LedCmdSetBlink(uint8_t led, uint8_t data[], uint8_t len);
void LedCmdGetBlink(uint8_t led, uint8_t data[], uint16_t *len);
uint8_t LedGetParamR(uint8_t led);
uint8_t LedGetParamG(uint8_t led);
uint8_t LedGetParamB(uint8_t led);

#endif /* LED_H_ */
