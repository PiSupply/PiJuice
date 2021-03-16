/*
 * button.h
 *
 *  Created on: 28.03.2017.
 *      Author: milan
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#include "stdint.h"

#define BUTTON_STATIC_LONG_PRESS_TIME	19600

// Button event function definitions, special functions 0 - 15, user button event functions 15 - 31, other values reserved
typedef enum ButtonFunction_T {
	BUTTON_EVENT_NO_FUNC = 0,
	BUTTON_EVENT_FUNC_POWER_ON,
	BUTTON_EVENT_FUNC_POWER_OFF,
	BUTTON_EVENT_FUNC_POWER_RESET,
	BUTTON_EVENT_FUNC_NUMBER
} ButtonFunction_T;

#define BUTTON_EVENT_FUNC_USER_EVENT	0x20
#define BUTTON_EVENT_FUNC_SYS_EVENT		0x10

typedef enum ButtonEvent_T {
	BUTTON_EVENT_PRESS = 1,
	BUTTON_EVENT_RELEASE,
	BUTTON_EVENT_SINGLE_PRESS,
	BUTTON_EVENT_DOUBLE_PRESS,
	BUTTON_EVENT_LONG_PRESS1,
	BUTTON_EVENT_LONG_PRESS2
} ButtonEvent_T;

typedef void (*ButtonEventCb_T)(uint8_t b, ButtonEvent_T event);

void ButtonInit(void);
#if !defined(RTOS_FREERTOS)
void ButtonTask(void);
#endif
ButtonEvent_T GetButtonEvent(uint8_t b);
void ButtonRemoveEvent(uint8_t b);
uint8_t IsButtonEvent(void);
void ButtonSetConfiguarion(uint8_t b, uint8_t data[], uint8_t len);
void ButtonGetConfiguarion(uint8_t b, uint8_t data[], uint16_t *len);
void PowerOnButtonEventCb(uint8_t b, ButtonEvent_T event);
void PowerOffButtonEventCb(uint8_t b, ButtonEvent_T event);
void ButtonEventFuncPowerResetCb(uint8_t b, ButtonEvent_T event);
void ButtonDualLongPressEventCb(void);
int8_t IsButtonActive(void);

#endif /* BUTTON_H_ */
