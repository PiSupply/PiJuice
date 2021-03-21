/*
 * button.h
 *
 *  Created on: 28.03.2017.
 *      Author: milan
 */

#ifndef BUTTON_H_
#define BUTTON_H_

#define BUTTON_STATIC_LONG_PRESS_TIME	19600u

// Button event function definitions, special functions 0 - 15, user button event functions 15 - 31, other values reserved
typedef enum ButtonFunction_T {
	BUTTON_EVENT_NO_FUNC = 0u,
	BUTTON_EVENT_FUNC_POWER_ON,
	BUTTON_EVENT_FUNC_POWER_OFF,
	BUTTON_EVENT_FUNC_POWER_RESET,
	BUTTON_EVENT_FUNC_NUMBER
} ButtonFunction_T;

#define BUTTON_EVENT_FUNC_USER_EVENT	0x20u
#define BUTTON_EVENT_FUNC_SYS_EVENT		0x10u

typedef enum ButtonEvent_T {
	BUTTON_EVENT_NONE = 0u,
	BUTTON_EVENT_PRESS = 1u,
	BUTTON_EVENT_RELEASE = 2u,
	BUTTON_EVENT_SINGLE_PRESS = 3u,
	BUTTON_EVENT_DOUBLE_PRESS = 4u,
	BUTTON_EVENT_LONG_PRESS1 = 5u,
	BUTTON_EVENT_LONG_PRESS2 = 6u
} ButtonEvent_T;

typedef void (*ButtonEventCb_T)(uint8_t b, ButtonEvent_T event);


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

void BUTTON_Init(void);
bool BUTTON_IsButtonActive(void);

#endif /* BUTTON_H_ */
