/*
 * button.c
 *
 *  Created on: 28.03.2017.
 *      Author: milan
 */

#include "main.h"

#include "system_conf.h"
#include "iodrv.h"
#include "time_count.h"
#include "nv.h"
#include "util.h"

#include "button.h"

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void ButtonTask(void *argument);

static osThreadId_t buttonTaskHandle;

static const osThreadAttr_t buttonTask_attributes = {
	.name = "buttonTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 256
};
#endif

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
	const IODRV_Pin_t * p_pinInfo;
	uint8_t pressCount;
} Button_T;

static Button_T m_buttons[3u] =
{
	{ // sw1
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_FUNC_POWER_ON,
		800u,
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_FUNC_SYS_EVENT| 1u,
		10000u,
		BUTTON_EVENT_FUNC_POWER_OFF,
		20000u
	},
	{ // sw2
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_FUNC_USER_EVENT | 1u,
		400u,
		BUTTON_EVENT_FUNC_USER_EVENT | 2u,
		600u,
		BUTTON_EVENT_NO_FUNC,
		0u,
		BUTTON_EVENT_NO_FUNC,
		0u
	},
	{ // sw3
		BUTTON_EVENT_FUNC_USER_EVENT | 3u,
		0u,
		BUTTON_EVENT_FUNC_USER_EVENT | 4u,
		0u,
		BUTTON_EVENT_FUNC_USER_EVENT | 5u,
		1000u,
		BUTTON_EVENT_FUNC_USER_EVENT | 6u,
		1500u,
		BUTTON_EVENT_FUNC_USER_EVENT | 7u,
		5000u,
		BUTTON_EVENT_FUNC_USER_EVENT | 8u,
		10000u
	}
};

static ButtonEventCb_T buttonEventCbs[BUTTON_EVENT_FUNC_NUMBER] =
{
	NULL, // BUTTON_EVENT_NO_FUNC
	PowerOnButtonEventCb, // BUTTON_EVENT_FUNC_POWER_ON
	PowerOffButtonEventCb, // BUTTON_EVENT_FUNC_POWER_OFF
	ButtonEventFuncPowerResetCb, // BUTTON_EVENT_FUNC_POWER_RESET
};

static int8_t m_writebuttonConfigData = -1;
static Button_T m_buttonConfigData;


static void BUTTON_SetConfigData(Button_T * const p_button, const uint8_t pinref);
static void BUTTON_UpdateConfigData(Button_T * const p_button, const Button_T * const configData);
static ButtonFunction_T BUTTON_GetEventFunc(Button_T * const p_button);
static void BUTTON_ProcessButton(Button_T * const p_button, const uint32_t sysTick);
static void BUTTON_SetConfigData(Button_T * const p_button, const uint8_t pinref);
static bool BUTTON_ReadConfigurationNv(const uint8_t buttonIndex);


static ButtonFunction_T BUTTON_GetEventFunc(Button_T * const p_button)
{
	switch (p_button->event)
	{
	case BUTTON_EVENT_PRESS:
		return p_button->pressFunc;

	case BUTTON_EVENT_RELEASE:
		return p_button->releaseFunc;

	case BUTTON_EVENT_SINGLE_PRESS:
		return p_button->singlePressFunc;

	case BUTTON_EVENT_DOUBLE_PRESS:
		return p_button->doublePressFunc;

	case BUTTON_EVENT_LONG_PRESS1:
		return p_button->longPressFunc1;

	case BUTTON_EVENT_LONG_PRESS2:
		return p_button->longPressFunc2;

	default:
		return BUTTON_EVENT_NO_FUNC;
	}
}


/* Attempt to understand what this function should do based on previous routine:
 *
 * BUTTON_EVENT_PRESS is raised on first rising edge after 30 seconds of no activity.
 * BUTTON_EVENT_RELEASE is raised on first falling edge if time exceeds single press or no action is registered for single press.
 * BUTTON_EVENT_SINGLE_PRESS is raised after button is released and does not exceed button.singlePress time
 * BUTTON_EVENT_DOUBLE_PRESS is raised on second rising edge if the second edge occurs before button.doublePressTime
 * BUTTON_EVENT_LONG_PRESS1 is raised if the button is held and the button.longPressTime1 is exceeded.
 * BUTTON_EVENT_LONG_PRESS2 is raised if the button is held and the button.longPressTime2 is exceeded.
 *
 * In all cases: Events are not registered if actions are not allocated to the function.
 * In all cases: Events are always raised if the previous priority is exceeded with exception of BUTTON_EVENT_SINGLE_PRESS.
 *
 * Activity is cleared after 30 seconds.
 *
 * Double press is not registered on first rising edge because the timer has not been initialised and it is expected that the
 * timer is greater than double press time.
 *
 * staticButtonLongPress is evaluated to true when the button is held and time exceeds BUTTON_STATIC_LONG_PRESS_TIME
 * staticButtonLongPress is reset on button release
 *
 */
void BUTTON_ProcessButton(Button_T * const p_button, const uint32_t sysTick)
{
	if ( (NULL == p_button) || (NULL == p_button->p_pinInfo) )
	{
		return;
	}

	const uint32_t previousButtonCycleTimeMs = p_button->p_pinInfo->lastPosPulseWidthTimeMs
										+ p_button->p_pinInfo->lastNegPulseWidthTimeMs;

	const uint32_t lastEdgeMs = MS_TIMEREF_DIFF(p_button->p_pinInfo->lastDigitalChangeTime, sysTick);

	ButtonEvent_T oldEv = p_button->event;
	ButtonFunction_T func;
	uint8_t i;

	// Copy value to ensure compatibility
	p_button->state = p_button->p_pinInfo->value;

	if ( (GPIO_PIN_RESET == p_button->p_pinInfo->value) )
	{
		if (lastEdgeMs > BUTTON_EVENT_EXPIRE_PERIOD_MS)
		{
			// Event timeout, remove it
			p_button->event = BUTTON_EVENT_NONE;
			oldEv = BUTTON_EVENT_NONE;
		}
		// The pulse is cleared after one day in the IODRV module
		// This check ensures that the 47 day roll over does not cause a phantom
		// button press as the button will have appeared to have been pressed
		// recently when it hasn't!
		else if (p_button->p_pinInfo->lastPosPulseWidthTimeMs > 0u)
		{

			// Check 	- single press time has not been exceeded for last press cycle
			//			- double press time has been exceeded
			//			- single press function has been allocated
			//			- single press function has not already been executed
			if ( (p_button->p_pinInfo->lastPosPulseWidthTimeMs < p_button->singlePressTime)
					&& ((lastEdgeMs + p_button->p_pinInfo->lastPosPulseWidthTimeMs) > p_button->doublePressTime)
					&& (BUTTON_EVENT_NO_FUNC != p_button->singlePressFunc)
					&& (p_button->event < BUTTON_EVENT_SINGLE_PRESS))
			{
				// Raise single press event
				p_button->event = BUTTON_EVENT_SINGLE_PRESS;
			}


			if ( (p_button->event < BUTTON_EVENT_RELEASE) && (BUTTON_EVENT_NO_FUNC != p_button->releaseFunc) )
			{
				// Raise release event
				p_button->event = BUTTON_EVENT_RELEASE;
			}
		}

		p_button->staticLongPressEvent = 0u;
	}
	else
	{
		/* Button is pressed and held */

		if ( (lastEdgeMs > p_button->longPressTime2) && (BUTTON_EVENT_NO_FUNC != p_button->longPressFunc2) )
		{
			// Raise long press 2
			p_button->event = BUTTON_EVENT_LONG_PRESS2;
		}
		else if ( (lastEdgeMs > p_button->longPressTime1) && (BUTTON_EVENT_NO_FUNC != p_button->longPressFunc1) )
		{
			// Raise long press 1
			p_button->event = BUTTON_EVENT_LONG_PRESS1;
		}
		else if ( ((lastEdgeMs + previousButtonCycleTimeMs) < p_button->doublePressTime)
				&& (BUTTON_EVENT_NO_FUNC != p_button->doublePressFunc))
		{
			// Raise double press event
			p_button->event = BUTTON_EVENT_DOUBLE_PRESS;
		}

		if ( lastEdgeMs > BUTTON_STATIC_LONG_PRESS_TIME)
		{
			p_button->staticLongPressEvent = 1u;
		}

		if ( (p_button->event < BUTTON_EVENT_PRESS) && (BUTTON_EVENT_NO_FUNC != p_button->pressFunc) )
		{
			// Raise button press event
			p_button->event = BUTTON_EVENT_PRESS;
		}
	}

	// Check if event has already been processed
	if ( p_button->event > oldEv )
	{
	    func = BUTTON_GetEventFunc(p_button);

		if ( func < BUTTON_EVENT_FUNC_NUMBER && buttonEventCbs[func] != NULL )
		{
			// Hack to get button index for now.
			i = 0u;

			while( (i < 3u) && (&m_buttons[i] != p_button) )
			{
				i++;
			}

			//buttonEventCbs[func](i, p_button->event);
		}
	}
}


static bool BUTTON_ReadConfigurationNv(const uint8_t buttonIndex)
{
	const uint8_t nvOffset = buttonIndex * (BUTTON_PRESS_FUNC_SW2 - BUTTON_PRESS_FUNC_SW1) + BUTTON_PRESS_FUNC_SW1;

	bool dataValid = false;
	uint16_t var;

	EE_ReadVariable(nvOffset, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);

	m_buttonConfigData.pressFunc = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 2, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.releaseFunc = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 4, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.singlePressFunc = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 5, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.singlePressTime = var&0xFF;

	EE_ReadVariable(nvOffset + 6, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.doublePressFunc = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 7, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.doublePressTime = var&0xFF;

	EE_ReadVariable(nvOffset + 8, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.longPressFunc1 = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 9, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.longPressTime1 = var&0xFF;

	EE_ReadVariable(nvOffset + 10, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.longPressFunc2 = (ButtonFunction_T)(var & 0xFFu);

	EE_ReadVariable(nvOffset + 11, &var);
	dataValid |= UTIL_NV_ParamInitCheck_U16(var);
	m_buttonConfigData.longPressTime2 = (ButtonFunction_T)(var & 0xFFu);

	return dataValid;
}


static void BUTTON_SetConfigData(Button_T * const p_button, const uint8_t pinref)
{
	p_button->p_pinInfo = IODRV_GetPinInfo(pinref);

	BUTTON_UpdateConfigData(p_button, &m_buttonConfigData);
}


static void BUTTON_UpdateConfigData(Button_T * const p_button, const Button_T * const configData)
{
	p_button->pressFunc = configData->pressFunc;
	p_button->releaseFunc = configData->releaseFunc;
	p_button->singlePressFunc = configData->singlePressFunc;
	p_button->singlePressTime = configData->singlePressTime * 100u;
	p_button->doublePressFunc = configData->doublePressFunc;
	p_button->doublePressTime = configData->doublePressTime * 100u;
	p_button->longPressFunc1 = configData->longPressFunc1;
	p_button->longPressTime1 = configData->longPressTime1 * 100u;
	p_button->longPressFunc2 = configData->longPressFunc2;
	p_button->longPressTime2 = configData->longPressTime2 * 100u;
}


void BUTTON_Init(void)
{
	if ( true == BUTTON_ReadConfigurationNv(0u) )
	{
		BUTTON_SetConfigData(&m_buttons[0u], IODRV_PIN_SW1);
	}

	if ( true == BUTTON_ReadConfigurationNv(1u) )
	{
		BUTTON_SetConfigData(&m_buttons[1u], IODRV_PIN_SW2);
	}

	if ( true == BUTTON_ReadConfigurationNv(2u) )
	{
		BUTTON_SetConfigData(&m_buttons[2u], IODRV_PIN_SW3);
	}
#if defined(RTOS_FREERTOS)
	buttonTaskHandle = osThreadNew(ButtonTask, (void*)NULL, &buttonTask_attributes);
#endif
}


bool BUTTON_IsButtonActive(void)
{
	return (GPIO_PIN_SET == m_buttons[0u].state) || (GPIO_PIN_SET ==  m_buttons[1].state) || (GPIO_PIN_SET == m_buttons[2].state);
}


#if defined(RTOS_FREERTOS)
static void ButtonTask(void *argument) {
  for(;;)
  {
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

	osDelay(20);
  }
}
#else
void ButtonTask(void) {

	const uint32_t sysTime = HAL_GetTick();
	const uint8_t oldDualLongPressStatus = m_buttons[0u].staticLongPressEvent && m_buttons[1u].staticLongPressEvent;

	BUTTON_ProcessButton(&m_buttons[0u], sysTime); // sw1
	BUTTON_ProcessButton(&m_buttons[1u], sysTime); // sw2
	BUTTON_ProcessButton(&m_buttons[2u], sysTime); // sw3

	// Check to see if both buttons SW1 and SW2 have been held down at the same time
	if ((m_buttons[0u].staticLongPressEvent && m_buttons[1u].staticLongPressEvent) > oldDualLongPressStatus) ButtonDualLongPressEventCb();

	if (m_writebuttonConfigData >= 0) {
		uint8_t nvOffset = m_writebuttonConfigData * (BUTTON_PRESS_FUNC_SW2 - BUTTON_PRESS_FUNC_SW1) + BUTTON_PRESS_FUNC_SW1;
		EE_WriteVariable(nvOffset, m_buttonConfigData.pressFunc | ((uint16_t)(~m_buttonConfigData.pressFunc)<<8u));
		EE_WriteVariable(nvOffset + 2u, m_buttonConfigData.releaseFunc | ((uint16_t)(~m_buttonConfigData.releaseFunc)<<8u));
		EE_WriteVariable(nvOffset + 4u, m_buttonConfigData.singlePressFunc | ((uint16_t)(~m_buttonConfigData.singlePressFunc)<<8u));
		EE_WriteVariable(nvOffset + 5u, m_buttonConfigData.singlePressTime | ((uint16_t)(~m_buttonConfigData.singlePressTime)<<8u));
		EE_WriteVariable(nvOffset + 6u, m_buttonConfigData.doublePressFunc | ((uint16_t)(~m_buttonConfigData.doublePressFunc)<<8u));
		EE_WriteVariable(nvOffset + 7u, m_buttonConfigData.doublePressTime | ((uint16_t)(~m_buttonConfigData.doublePressTime)<<8u));
		EE_WriteVariable(nvOffset + 8u, m_buttonConfigData.longPressFunc1 | ((uint16_t)(~m_buttonConfigData.longPressFunc1)<<8u));
		EE_WriteVariable(nvOffset + 9u, m_buttonConfigData.longPressTime1 | ((uint16_t)(~m_buttonConfigData.longPressTime1)<<8u));
		EE_WriteVariable(nvOffset + 10u, m_buttonConfigData.longPressFunc2 | ((uint16_t)(~m_buttonConfigData.longPressFunc2)<<8u));
		EE_WriteVariable(nvOffset + 11u, m_buttonConfigData.longPressTime2 | ((uint16_t)(~m_buttonConfigData.longPressTime2)<<8u));

		if ( true == BUTTON_ReadConfigurationNv(m_writebuttonConfigData) )
		{
			BUTTON_UpdateConfigData(&m_buttons[m_writebuttonConfigData], &m_buttonConfigData);
		}

		m_writebuttonConfigData = -1;
	}
}
#endif

ButtonEvent_T GetButtonEvent(uint8_t b) {
	ButtonEvent_T event = m_buttons[b].event;
	//buttons[b].event = 0;
	return event;
}

void ButtonRemoveEvent(uint8_t b) {
	m_buttons[b].event = 0;
}

uint8_t IsButtonEvent(void) {
	return (BUTTON_EVENT_NONE != m_buttons[0].event) || (BUTTON_EVENT_NONE != m_buttons[1].event) || (BUTTON_EVENT_NONE != m_buttons[2].event);
}

void ButtonSetConfiguarion(uint8_t b, uint8_t data[], uint8_t len)
{
	if (b > BUTTONS_MAX_BUTTONS)
	{
		return;
	}

	m_buttonConfigData.pressFunc = data[0];
	/*if ( buttonConfigData.func >= BUTTON_FUNC_NUMBER  ) return;
	if ( buttonConfigData.func != BUTTON_FUNC_POWER_BUTTON && buttons[b].func == BUTTON_FUNC_POWER_BUTTON ) {
		// ensure this will not override existing power button, if only one defined
		int i, powBtnCnt = 0;
		for (i=0;i<3;i++) {
			if (buttons[i].func == BUTTON_FUNC_POWER_BUTTON) powBtnCnt ++;
		}
		if (powBtnCnt<2) return;
	}*/
	m_buttonConfigData.pressConfig = data[1];
	m_buttonConfigData.releaseFunc = data[2];
	m_buttonConfigData.releaseConfig = data[3];
	m_buttonConfigData.singlePressFunc = data[4];
	m_buttonConfigData.singlePressTime = data[5];
	m_buttonConfigData.doublePressFunc = data[6];
	m_buttonConfigData.doublePressTime = data[7];
	m_buttonConfigData.longPressFunc1 = data[8];
	m_buttonConfigData.longPressTime1 = data[9];
	m_buttonConfigData.longPressFunc2 = data[10];
	m_buttonConfigData.longPressTime2 = data[11];

	m_writebuttonConfigData = b;
}

void ButtonGetConfiguarion(uint8_t b, uint8_t data[], uint16_t *len) {
	data[0] = m_buttons[b].pressFunc;
	data[1] = m_buttons[b].pressConfig;
	data[2] = m_buttons[b].releaseFunc;
	data[3] = m_buttons[b].releaseConfig;
	data[4] = m_buttons[b].singlePressFunc;
	data[5] = m_buttons[b].singlePressTime / 100u;
	data[6] = m_buttons[b].doublePressFunc;
	data[7] = m_buttons[b].doublePressTime / 100u;
	data[8] = m_buttons[b].longPressFunc1;
	data[9] = m_buttons[b].longPressTime1 / 100u;
	data[10] = m_buttons[b].longPressFunc2;
	data[11] = m_buttons[b].longPressTime2 / 100u;
	*len = 12u;
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

