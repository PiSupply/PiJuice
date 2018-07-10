/*
 * button.c
 *
 *  Created on: 28.03.2017.
 *      Author: milan
 */

#include "button.h"
#include "stm32f0xx_hal.h"
#include "time_count.h"
#include "nv.h"

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
	ButtonEvent_T tempEvent;
	uint32_t pressTimer;
	uint8_t staticLongPressEvent;
} Button_T;

Button_T buttons[3] = {
	{ // sw1
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_FUNC_POWER_ON,
		800,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_FUNC_SYS_EVENT|1,
		10000,
		BUTTON_EVENT_FUNC_POWER_OFF,
		20000
	},
	{ // sw2
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_FUNC_USER_EVENT|1,
		400,
		BUTTON_EVENT_FUNC_USER_EVENT|2,
		600,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0
	},
	{ // sw3
		BUTTON_EVENT_FUNC_USER_EVENT|3,
		0,
		BUTTON_EVENT_FUNC_USER_EVENT|4,
		0,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0,
		BUTTON_EVENT_NO_FUNC,
		0
	}
};

ButtonEventCb_T buttonEventCbs[BUTTON_EVENT_FUNC_NUMBER] = {
	NULL, // BUTTON_EVENT_NO_FUNC
	PowerOnButtonEventCb, // BUTTON_EVENT_FUNC_POWER_ON
	PowerOffButtonEventCb, // BUTTON_EVENT_FUNC_POWER_OFF
	ButtonEventFuncPowerResetCb, // BUTTON_EVENT_FUNC_POWER_RESET
};

static int8_t writebuttonConfigData = -1;
Button_T buttonConfigData;

static ButtonFunction_T GetFuncOfEvent(uint8_t b) {
	switch (buttons[b].event) {
	case BUTTON_EVENT_PRESS:
		return buttons[b].pressFunc;
	case BUTTON_EVENT_RELEASE:
		return buttons[b].releaseFunc;
	case BUTTON_EVENT_SINGLE_PRESS:
		return buttons[b].singlePressFunc;
	case BUTTON_EVENT_DOUBLE_PRESS:
		return buttons[b].doublePressFunc;
	case BUTTON_EVENT_LONG_PRESS1:
		return buttons[b].longPressFunc1;
	case BUTTON_EVENT_LONG_PRESS2:
		return buttons[b].longPressFunc2;
	default:
		return BUTTON_EVENT_NO_FUNC;
	}
}

/*__STATIC_INLINE*/ void ProcessButton( uint8_t b, GPIO_PinState pinState ) {
	ButtonEvent_T oldEv = buttons[b].event;

	if ( pinState != buttons[b].state ) {
		if ( pinState == GPIO_PIN_SET ) {
			if ( buttons[b].doublePressTime && buttons[b].doublePressFunc!=BUTTON_EVENT_NO_FUNC && MS_TIME_COUNT(buttons[b].pressTimer)  < buttons[b].doublePressTime ) {
				buttons[b].tempEvent = BUTTON_EVENT_DOUBLE_PRESS;
				buttons[b].event = BUTTON_EVENT_DOUBLE_PRESS;
			} else if ( buttons[b].pressFunc != BUTTON_EVENT_NO_FUNC ) {
				buttons[b].tempEvent = BUTTON_EVENT_PRESS;
				buttons[b].event = BUTTON_EVENT_PRESS;
			}
			MS_TIME_COUNTER_INIT(buttons[b].pressTimer);
		} else {
			// if release
			if (buttons[b].tempEvent != BUTTON_EVENT_DOUBLE_PRESS) {
				if ( buttons[b].singlePressTime && buttons[b].singlePressFunc!=BUTTON_EVENT_NO_FUNC && MS_TIME_COUNT(buttons[b].pressTimer) < buttons[b].singlePressTime ) {
					buttons[b].tempEvent = BUTTON_EVENT_SINGLE_PRESS;
					if ( buttons[b].doublePressFunc == BUTTON_EVENT_NO_FUNC ) buttons[b].event = BUTTON_EVENT_SINGLE_PRESS;
				} else if ( buttons[b].releaseFunc != BUTTON_EVENT_NO_FUNC ) {
					buttons[b].tempEvent = BUTTON_EVENT_RELEASE;
					if ( buttons[b].doublePressFunc == BUTTON_EVENT_NO_FUNC ) buttons[b].event = BUTTON_EVENT_RELEASE;
				}
			}
			buttons[b].staticLongPressEvent = 0;
		}
		buttons[b].state = pinState;
	} else if ( pinState == GPIO_PIN_SET ) {
		uint32_t timePased = MS_TIME_COUNT(buttons[b].pressTimer);
		if ( buttons[b].longPressFunc2 != BUTTON_EVENT_NO_FUNC && timePased > buttons[b].longPressTime2 ) {
			if ( buttons[b].tempEvent != BUTTON_EVENT_LONG_PRESS2 ) {
				buttons[b].tempEvent = BUTTON_EVENT_LONG_PRESS2;
				buttons[b].event = BUTTON_EVENT_LONG_PRESS2;
			}
		} else if ( buttons[b].longPressFunc1 != BUTTON_EVENT_NO_FUNC && timePased > buttons[b].longPressTime1 ) {
			if ( buttons[b].tempEvent != BUTTON_EVENT_LONG_PRESS1 ) {
				buttons[b].tempEvent = BUTTON_EVENT_LONG_PRESS1;
				buttons[b].event = BUTTON_EVENT_LONG_PRESS1;
			}
		}
		if (timePased > BUTTON_STATIC_LONG_PRESS_TIME) {
			buttons[b].staticLongPressEvent = 1;
		}
	} else if ( buttons[b].tempEvent && pinState == GPIO_PIN_RESET && buttons[b].doublePressFunc!=BUTTON_EVENT_NO_FUNC  && MS_TIME_COUNT(buttons[b].pressTimer)  > buttons[b].doublePressTime ) {
		// generate single press event only if there was no double press
		if ( buttons[b].tempEvent == BUTTON_EVENT_SINGLE_PRESS ) {
			buttons[b].event = BUTTON_EVENT_SINGLE_PRESS;
		} else if ( buttons[b].tempEvent == BUTTON_EVENT_RELEASE ) {
			buttons[b].event = BUTTON_EVENT_RELEASE;
		}
		buttons[b].tempEvent = 0;
	}

	if ( buttons[b].event  > oldEv ) {
	    volatile ButtonFunction_T func = GetFuncOfEvent(b);
		if ( func < BUTTON_EVENT_FUNC_NUMBER && buttonEventCbs[func] != NULL ) {
			buttonEventCbs[func](b, buttons[b].event);
		}
	}
}

static uint8_t ButtonReadConfigurationNv(uint8_t b) {
	uint8_t nvOffset = b * (BUTTON_PRESS_FUNC_SW2 - BUTTON_PRESS_FUNC_SW1) + BUTTON_PRESS_FUNC_SW1;
	uint8_t dataValid = 1;
	uint16_t var;

	EE_ReadVariable(nvOffset, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.pressFunc = var&0xFF;

	EE_ReadVariable(nvOffset + 2, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.releaseFunc = var&0xFF;

	EE_ReadVariable(nvOffset + 4, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.singlePressFunc = var&0xFF;

	EE_ReadVariable(nvOffset + 5, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.singlePressTime = var&0xFF;

	EE_ReadVariable(nvOffset + 6, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.doublePressFunc = var&0xFF;

	EE_ReadVariable(nvOffset + 7, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.doublePressTime = var&0xFF;

	EE_ReadVariable(nvOffset + 8, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.longPressFunc1 = var&0xFF;

	EE_ReadVariable(nvOffset + 9, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.longPressTime1 = var&0xFF;

	EE_ReadVariable(nvOffset + 10, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.longPressFunc2 = var&0xFF;

	EE_ReadVariable(nvOffset + 11, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	buttonConfigData.longPressTime2 = var&0xFF;

	return !dataValid;
}

static void ButtonSetConfigData(uint8_t b) {
	buttons[b].pressFunc = buttonConfigData.pressFunc;
	buttons[b].releaseFunc = buttonConfigData.releaseFunc;
	buttons[b].singlePressFunc = buttonConfigData.singlePressFunc;
	buttons[b].singlePressTime = buttonConfigData.singlePressTime * 100;
	buttons[b].doublePressFunc = buttonConfigData.doublePressFunc;
	buttons[b].doublePressTime = buttonConfigData.doublePressTime * 100;
	buttons[b].longPressFunc1 = buttonConfigData.longPressFunc1;
	buttons[b].longPressTime1 = buttonConfigData.longPressTime1 * 100;
	buttons[b].longPressFunc2 = buttonConfigData.longPressFunc2;
	buttons[b].longPressTime2 = buttonConfigData.longPressTime2 * 100;
}

void ButtonInit(void) {
	if ( ButtonReadConfigurationNv(0) == 0 ) {
		ButtonSetConfigData(0);
	}

	if ( ButtonReadConfigurationNv(1) == 0 ) {
		ButtonSetConfigData(1);
	}

	if ( ButtonReadConfigurationNv(2) == 0 ) {
		ButtonSetConfigData(2);
	}
}

int8_t IsButtonActive(void) {
	//return buttons[0].tempEvent || buttons[1].tempEvent || buttons[2].tempEvent;
	return MS_TIME_COUNT(buttons[0].pressTimer) < 2000 || MS_TIME_COUNT(buttons[1].pressTimer) < 2000 || MS_TIME_COUNT(buttons[2].pressTimer) < 2000
			|| buttons[0].state || buttons[1].state || buttons[2].state;
}

void ButtonTask(void) {

	uint8_t oldDualLongPressStatus = buttons[0].staticLongPressEvent && buttons[1].staticLongPressEvent;

	ProcessButton(0, HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)); // sw1

	ProcessButton(1, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12)); // sw2

	ProcessButton(2, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_2)); // sw3

	if ((buttons[0].staticLongPressEvent && buttons[1].staticLongPressEvent) > oldDualLongPressStatus) ButtonDualLongPressEventCb();

	if (writebuttonConfigData >= 0) {
		uint8_t nvOffset = writebuttonConfigData * (BUTTON_PRESS_FUNC_SW2 - BUTTON_PRESS_FUNC_SW1) + BUTTON_PRESS_FUNC_SW1;
		EE_WriteVariable(nvOffset, buttonConfigData.pressFunc | ((uint16_t)(~buttonConfigData.pressFunc)<<8));
		EE_WriteVariable(nvOffset + 2, buttonConfigData.releaseFunc | ((uint16_t)(~buttonConfigData.releaseFunc)<<8));
		EE_WriteVariable(nvOffset + 4, buttonConfigData.singlePressFunc | ((uint16_t)(~buttonConfigData.singlePressFunc)<<8));
		EE_WriteVariable(nvOffset + 5, buttonConfigData.singlePressTime | ((uint16_t)(~buttonConfigData.singlePressTime)<<8));
		EE_WriteVariable(nvOffset + 6, buttonConfigData.doublePressFunc | ((uint16_t)(~buttonConfigData.doublePressFunc)<<8));
		EE_WriteVariable(nvOffset + 7, buttonConfigData.doublePressTime | ((uint16_t)(~buttonConfigData.doublePressTime)<<8));
		EE_WriteVariable(nvOffset + 8, buttonConfigData.longPressFunc1 | ((uint16_t)(~buttonConfigData.longPressFunc1)<<8));
		EE_WriteVariable(nvOffset + 9, buttonConfigData.longPressTime1 | ((uint16_t)(~buttonConfigData.longPressTime1)<<8));
		EE_WriteVariable(nvOffset + 10, buttonConfigData.longPressFunc2 | ((uint16_t)(~buttonConfigData.longPressFunc2)<<8));
		EE_WriteVariable(nvOffset + 11, buttonConfigData.longPressTime2 | ((uint16_t)(~buttonConfigData.longPressTime2)<<8));

		if ( ButtonReadConfigurationNv(writebuttonConfigData) == 0 ) {
			ButtonSetConfigData(writebuttonConfigData);
		}
		writebuttonConfigData = -1;
	}
}

ButtonEvent_T GetButtonEvent(uint8_t b) {
	ButtonEvent_T event = buttons[b].event;
	//buttons[b].event = 0;
	return event;
}

void ButtonRemoveEvent(uint8_t b) {
	buttons[b].event = 0;
}

uint8_t IsButtonEvent(void) {
	return buttons[0].event || buttons[1].event || buttons[2].event;
}

void ButtonSetConfiguarion(uint8_t b, uint8_t data[], uint8_t len) {
	if (b > 3) return;
	buttonConfigData.pressFunc = data[0];
	/*if ( buttonConfigData.func >= BUTTON_FUNC_NUMBER  ) return;
	if ( buttonConfigData.func != BUTTON_FUNC_POWER_BUTTON && buttons[b].func == BUTTON_FUNC_POWER_BUTTON ) {
		// ensure this will not override existing power button, if only one defined
		int i, powBtnCnt = 0;
		for (i=0;i<3;i++) {
			if (buttons[i].func == BUTTON_FUNC_POWER_BUTTON) powBtnCnt ++;
		}
		if (powBtnCnt<2) return;
	}*/
	buttonConfigData.pressConfig = data[1];
	buttonConfigData.releaseFunc = data[2];
	buttonConfigData.releaseConfig = data[3];
	buttonConfigData.singlePressFunc = data[4];
	buttonConfigData.singlePressTime = data[5];
	buttonConfigData.doublePressFunc = data[6];
	buttonConfigData.doublePressTime = data[7];
	buttonConfigData.longPressFunc1 = data[8];
	buttonConfigData.longPressTime1 = data[9];
	buttonConfigData.longPressFunc2 = data[10];
	buttonConfigData.longPressTime2 = data[11];
	writebuttonConfigData = b;
}

void ButtonGetConfiguarion(uint8_t b, uint8_t data[], uint16_t *len) {
	data[0] = buttons[b].pressFunc;
	data[1] = buttons[b].pressConfig;
	data[2] = buttons[b].releaseFunc;
	data[3] = buttons[b].releaseConfig;
	data[4] = buttons[b].singlePressFunc;
	data[5] = buttons[b].singlePressTime / 100;
	data[6] = buttons[b].doublePressFunc;
	data[7] = buttons[b].doublePressTime / 100;
	data[8] = buttons[b].longPressFunc1;
	data[9] = buttons[b].longPressTime1 / 100;
	data[10] = buttons[b].longPressFunc2;
	data[11] = buttons[b].longPressTime2 / 100;
	*len = 12;
}

__weak void PowerOnButtonEventCb(uint8_t b, ButtonEvent_T event) {
	UNUSED(b);
	UNUSED(event);
}

__weak void PowerOffButtonEventCb(uint8_t b, ButtonEvent_T event) {
	UNUSED(b);
	UNUSED(event);
}

__weak void ButtonEventFuncPowerResetCb(uint8_t b, ButtonEvent_T event) {
	UNUSED(b);
	UNUSED(event);
}

__weak void ButtonDualLongPressEventCb(void) {

}

