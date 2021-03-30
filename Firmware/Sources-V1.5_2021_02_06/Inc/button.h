/*
 * button.h
 *
 *  Created on: 28.03.2017.
 *      Author: milan
 */

#ifndef BUTTON_H_
#define BUTTON_H_

// Required to complete Button_T
#include "iodrv.h"

// Button event function definitions, special functions 0 - 15, user button event functions 15 - 31, other values reserved
typedef enum ButtonFunction_T {
	BUTTON_EVENT_NO_FUNC = 0u,
	BUTTON_EVENT_FUNC_POWER_ON = 1u,
	BUTTON_EVENT_FUNC_POWER_OFF = 2u,
	BUTTON_EVENT_FUNC_POWER_RESET = 3u,
	BUTTON_EVENT_FUNC_COUNT = 4u
} ButtonFunction_T;

typedef enum ButtonEvent_T {
	BUTTON_EVENT_NONE = 0u,
	BUTTON_EVENT_PRESS = 1u,
	BUTTON_EVENT_RELEASE = 2u,
	BUTTON_EVENT_SINGLE_PRESS = 3u,
	BUTTON_EVENT_DOUBLE_PRESS = 4u,
	BUTTON_EVENT_LONG_PRESS1 = 5u,
	BUTTON_EVENT_LONG_PRESS2 = 6u,
	BUTTON_EVENT_COUNT = 7u
} ButtonEvent_T;

typedef struct
{
	ButtonFunction_T pressFunc;
	uint16_t pressConfig; // reserved
	ButtonFunction_T releaseFunc;
	uint16_t releaseConfig; // reserved
	ButtonFunction_T singlePressFunc;
	uint16_t singlePressTime; // 0- reserved, 1 - 255  time
	ButtonFunction_T doublePressFunc;
	uint16_t doublePressTime; // 0- reserved, 1 - 255  time
	ButtonFunction_T longPressFunc1;
	uint16_t longPressTime1;
	ButtonFunction_T longPressFunc2;
	uint16_t longPressTime2;
	GPIO_PinState state; // current press state, GPIO_PIN_RESET for release, GPIO_PIN_SET for press
	ButtonEvent_T event;
	uint8_t staticLongPressEvent;
	const IODRV_Pin_t * p_pinInfo;
	uint8_t pressCount;
	uint8_t index;
} Button_T;

typedef void (*ButtonEventCb_T)(const Button_T * const p_button);

void BUTTON_Init(void);
void BUTTON_Task(void);
bool BUTTON_IsButtonActive(void);
void BUTTON_SetConfiguarion(const uint8_t buttonIndex, const uint8_t * const data, const uint8_t len);
bool BUTTON_GetConfiguarion(const uint8_t buttonIndex, uint8_t * const data, uint16_t * const len);
ButtonEvent_T BUTTON_GetButtonEvent(const uint8_t buttonIndex);
void BUTTON_ClearEvent(const uint8_t buttonIndex);
bool BUTTON_IsEventActive(void);

void BUTTON_PowerOnEventCb(const Button_T * const p_button);
void BUTTON_PowerOffEventCb(const Button_T * const p_button);
void BUTTON_PowerResetEventCb(const Button_T * const p_button);
void BUTTON_DualLongPressEventCb(void);

#endif /* BUTTON_H_ */
