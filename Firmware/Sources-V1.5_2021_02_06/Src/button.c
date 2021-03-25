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


static Button_T m_buttons[3u] =
{
	{ // sw1
		.pressFunc = BUTTON_EVENT_NO_FUNC,
		.pressConfig = 0u,
		.releaseFunc = BUTTON_EVENT_NO_FUNC,
		.releaseConfig = 0u,
		.singlePressFunc = BUTTON_EVENT_FUNC_POWER_ON,
		.singlePressTime = 800u,
		.doublePressFunc = BUTTON_EVENT_NO_FUNC,
		.doublePressTime = 0u,
		.longPressFunc1 = (BUTTON_EVENT_FUNC_SYS_EVENT| 1u),
		.longPressTime1 = 10000u,
		.longPressFunc2 = BUTTON_EVENT_FUNC_POWER_OFF,
		.longPressTime2 = 20000u,
		.index = 0u
	},
	{ // sw2
		.pressFunc = BUTTON_EVENT_NO_FUNC,
		.pressConfig = 0u,
		.releaseFunc = BUTTON_EVENT_NO_FUNC,
		.releaseConfig = 0u,
		.singlePressFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 1u),
		.singlePressTime = 400u,
		.doublePressFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 2u),
		.doublePressTime = 600u,
		.longPressFunc1 = BUTTON_EVENT_NO_FUNC,
		.longPressTime1 = 0u,
		.longPressFunc2 = BUTTON_EVENT_NO_FUNC,
		.longPressTime2 = 0u,
		.index = 1u
	},
	{ // sw3
		.pressFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 3u),
		.pressConfig = 0u,
		.releaseFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 4u),
		.releaseConfig = 0u,
		.singlePressFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 5u),
		.singlePressTime = 1000u,
		.doublePressFunc = (BUTTON_EVENT_FUNC_USER_EVENT | 6u),
		.doublePressTime = 1500u,
		.longPressFunc1 = (BUTTON_EVENT_FUNC_USER_EVENT | 7u),
		.longPressTime1 = 5000u,
		.longPressFunc2 = (BUTTON_EVENT_FUNC_USER_EVENT | 8u),
		.longPressTime2 = 10000u,
		.index = 2u
	}
};

static ButtonEventCb_T m_buttonEventCallbacks[BUTTON_EVENT_FUNC_COUNT] =
{
	NULL, // BUTTON_EVENT_NO_FUNC
	BUTTON_PowerOnEventCb, // BUTTON_EVENT_FUNC_POWER_ON
	BUTTON_PowerOffEventCb, // BUTTON_EVENT_FUNC_POWER_OFF
	BUTTON_PowerResetEventCb, // BUTTON_EVENT_FUNC_POWER_RESET
};


static int8_t m_writebuttonConfigData = -1;
static Button_T m_buttonConfigData;


static void BUTTON_SetConfigData(Button_T * const p_button, const uint8_t pinref);
static void BUTTON_UpdateConfigData(Button_T * const p_button, const Button_T * const configData);
static ButtonFunction_T BUTTON_GetEventFunc(Button_T * const p_button);
static void BUTTON_ProcessButton(Button_T * const p_button, const uint32_t sysTick);
static void BUTTON_SetConfigData(Button_T * const p_button, const uint8_t pinref);
static bool BUTTON_ReadConfigurationNv(const uint8_t buttonIndex);


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
}


void BUTTON_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const uint8_t oldDualLongPressStatus = m_buttons[0u].staticLongPressEvent && m_buttons[1u].staticLongPressEvent;

	BUTTON_ProcessButton(&m_buttons[0u], sysTime); // sw1
	BUTTON_ProcessButton(&m_buttons[1u], sysTime); // sw2
	BUTTON_ProcessButton(&m_buttons[2u], sysTime); // sw3

	// Check to see if both buttons SW1 and SW2 have been held down at the same time
	if ((m_buttons[0u].staticLongPressEvent && m_buttons[1u].staticLongPressEvent) > oldDualLongPressStatus)
	{
		BUTTON_DualLongPressEventCb();
	}

	if (m_writebuttonConfigData >= 0)
	{
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

		if ( (func < BUTTON_EVENT_FUNC_COUNT) && (m_buttonEventCallbacks[func] != NULL) )
		{
			m_buttonEventCallbacks[func](p_button);
		}
	}
}


ButtonEvent_T BUTTON_GetButtonEvent(const uint8_t buttonIndex)
{
	return (buttonIndex < BUTTON_MAX_BUTTONS) ? m_buttons[buttonIndex].event : BUTTON_EVENT_NONE;
}


void BUTTON_ClearEvent(const uint8_t buttonIndex)
{
	if (buttonIndex < BUTTON_MAX_BUTTONS)
	{
		m_buttons[buttonIndex].event = BUTTON_EVENT_NONE;
	}
}


bool BUTTON_IsEventActive(void)
{
	return (BUTTON_EVENT_NONE != m_buttons[0].event) || (BUTTON_EVENT_NONE != m_buttons[1].event) || (BUTTON_EVENT_NONE != m_buttons[2].event);
}


bool BUTTON_IsButtonActive(void)
{
	return (GPIO_PIN_SET == m_buttons[0u].state) || (GPIO_PIN_SET ==  m_buttons[1].state) || (GPIO_PIN_SET == m_buttons[2].state);
}


void BUTTON_SetConfiguarion(const uint8_t buttonIndex, const uint8_t * const data, const uint8_t len)
{
	if (buttonIndex > BUTTON_MAX_BUTTONS)
	{
		return;
	}

	m_buttonConfigData.pressFunc = (ButtonFunction_T)data[0u];
	m_buttonConfigData.pressConfig = data[1u];
	m_buttonConfigData.releaseFunc = (ButtonFunction_T)data[2u];
	m_buttonConfigData.releaseConfig = data[3u];
	m_buttonConfigData.singlePressFunc = (ButtonFunction_T)data[4u];
	m_buttonConfigData.singlePressTime = data[5u];
	m_buttonConfigData.doublePressFunc = (ButtonFunction_T)data[6u];
	m_buttonConfigData.doublePressTime = data[7u];
	m_buttonConfigData.longPressFunc1 = (ButtonFunction_T)data[8u];
	m_buttonConfigData.longPressTime1 = data[9u];
	m_buttonConfigData.longPressFunc2 = (ButtonFunction_T)data[10u];
	m_buttonConfigData.longPressTime2 = data[11u];

	m_writebuttonConfigData = buttonIndex;
}


bool BUTTON_GetConfiguarion(const uint8_t buttonIndex, uint8_t * const data, uint16_t * const len)
{
	if (buttonIndex < BUTTON_MAX_BUTTONS)
	{
		return false;
	}

	const Button_T* p_button = &m_buttons[buttonIndex];

	data[0] = (uint8_t)p_button->pressFunc;
	data[1] = (uint8_t)p_button->pressConfig;
	data[2] = (uint8_t)p_button->releaseFunc;
	data[3] = (uint8_t)p_button->releaseConfig;
	data[4] = (uint8_t)p_button->singlePressFunc;
	data[5] = (uint8_t)(p_button->singlePressTime / 100u);
	data[6] = (uint8_t)p_button->doublePressFunc;
	data[7] = (uint8_t)(p_button->doublePressTime / 100u);
	data[8] = (uint8_t)p_button->longPressFunc1;
	data[9] = (uint8_t)(p_button->longPressTime1 / 100u);
	data[10] = (uint8_t)p_button->longPressFunc2;
	data[11] = (uint8_t)(p_button->longPressTime2 / 100u);

	*len = 12u;

	return true;
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


__weak void BUTTON_PowerOnEventCb(const Button_T * const p_button)
{
	UNUSED(p_button);
}

__weak void BUTTON_PowerOffEventCb(const Button_T * const p_button)
{
	UNUSED(p_button);
}

__weak void BUTTON_PowerResetEventCb(const Button_T * const p_button)
{
	UNUSED(p_button);
}

__weak void BUTTON_DualLongPressEventCb(void) {

}

