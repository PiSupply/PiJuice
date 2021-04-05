/*
 * led.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */
#include "main.h"
#include "system_conf.h"

#include "nv.h"
#include "time_count.h"
#include "util.h"
#include "iodrv.h"

#include "led.h"
#include "led_pwm_table.h"

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim15;
extern TIM_HandleTypeDef htim17;


static Led_T m_leds[LED_COUNT] =
{
	{ LED_CHARGE_STATUS, .paramR = 60u, .paramG = 60u, .paramB = 100u, .pwmDrv_r = &TIM3->CCR1, .pwmDrv_g = &TIM3->CCR2, .pwmDrv_b = &TIM3->CCR3},
	{ LED_USER_LED, .paramR = 0u, .paramG = 0u, .paramB = 0u, .pwmDrv_r = &TIM15->CCR1, .pwmDrv_g = &TIM15->CCR2, .pwmDrv_b = &TIM17->CCR1 },
};


static const uint8_t m_ledFunctionParams[2u][4u] =
{
		{NV_LED_FUNC_1, NV_LED_PARAM_R_1, NV_LED_PARAM_G_1, NV_LED_PARAM_B_1},
		{NV_LED_FUNC_2, NV_LED_PARAM_R_2, NV_LED_PARAM_G_2, NV_LED_PARAM_B_2},
};


static void LED_SetRGBLedPtr(Led_T * const p_led, const uint8_t r, const uint8_t g, const uint8_t b);
static void LED_ProcessBlink(Led_T * const p_led, const uint32_t sysTime);
static void LED_InitFunction(const uint8_t ledIdx);


void LED_Init(const uint32_t sysTime)
{
	// Start pwm timer channels, also starts the timers
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);


	// Set all pins to drive push pull
	IODRV_SetPinType(IODRV_PIN_LED1_R, IOTYPE_PWM_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_LED1_G, IOTYPE_PWM_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_LED1_B, IOTYPE_PWM_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_LED2_R, IOTYPE_PWM_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_LED2_G, IOTYPE_PWM_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_LED2_B, IOTYPE_PWM_PUSHPULL);


	/* TODO - Set these in cube MX and just toggle alt function */
	IODRV_SetPinPullDir(IODRV_PIN_LED1_R, GPIO_PULLDOWN);
	IODRV_SetPinPullDir(IODRV_PIN_LED1_G, GPIO_PULLDOWN);
	IODRV_SetPinPullDir(IODRV_PIN_LED1_B, GPIO_PULLDOWN);
	IODRV_SetPinPullDir(IODRV_PIN_LED2_R, GPIO_PULLDOWN);
	IODRV_SetPinPullDir(IODRV_PIN_LED2_G, GPIO_PULLDOWN);
	IODRV_SetPinPullDir(IODRV_PIN_LED2_B, GPIO_PULLDOWN);


	// Initialise the led functions from NV memory
	LED_InitFunction(LED_LED1_IDX);
	LED_InitFunction(LED_LED2_IDX);
}


void LED_Shutdown(void)
{
	TIM3->CR1 &= ~(TIM_CR1_CEN);
	TIM15->CR1 &= ~(TIM_CR1_CEN);
	TIM17->CR1 &= ~(TIM_CR1_CEN);

	// Make all pins pull down
	IODRV_SetPinType(IODRV_PIN_LED1_R, IOTYPE_DIGIN);
	IODRV_SetPinType(IODRV_PIN_LED1_G, IOTYPE_DIGIN);
	IODRV_SetPinType(IODRV_PIN_LED1_B, IOTYPE_DIGIN);
	IODRV_SetPinType(IODRV_PIN_LED2_R, IOTYPE_DIGIN);
	IODRV_SetPinType(IODRV_PIN_LED2_G, IOTYPE_DIGIN);
	IODRV_SetPinType(IODRV_PIN_LED2_B, IOTYPE_DIGIN);
}


uint8_t LED_GetParamR(uint8_t func)
{
	if (m_leds[0].func == func)
	{
		return m_leds[0].paramR;
	}
	else if (m_leds[1].func == func)
	{
		return m_leds[1].paramR;
	}
	else
	{
		return 0u;
	}
}


uint8_t LED_GetParamG(const uint8_t func)
{
	if (m_leds[0].func == func)
	{
		return m_leds[0].paramG;
	}
	else if (m_leds[1].func == func)
	{
		return m_leds[1].paramG;
	}
	else
	{
		return 0u;
	}
}


uint8_t LED_GetParamB(uint8_t func)
{
	if (m_leds[0].func == func)
	{
		return m_leds[0].paramB;
	}
	else if (m_leds[1].func == func)
	{
		return m_leds[1].paramB;
	}
	else
	{
		return 0u;
	}
}


void LED_Service(const uint32_t sysTime)
{
	LED_ProcessBlink(&m_leds[LED_LED1_IDX], sysTime);
	LED_ProcessBlink(&m_leds[LED_LED2_IDX], sysTime);
}


void LED_SetRGB(const uint8_t ledIdx, const uint8_t r, const uint8_t g, const uint8_t b)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	LED_SetRGBLedPtr(&m_leds[ledIdx], r, g, b);
}


// Finds LED by function and sets rgb
void LED_FunctionSetRGB(LedFunction_T func, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t i = 0u;

	while( (i < LED_COUNT) && (m_leds[i].func != func) )
	{
		i++;
	}

	if (i > LED_LAST_LED_IDX)
	{
		return;
	}

	Led_T * p_led = &m_leds[i];

	p_led->r = r;
	p_led->g = g;
	p_led->b = b;
	p_led->blinkCount = 0u;
	p_led->blinkRepeat = 0u;

	LED_SetRGBLedPtr(p_led, r, g, b);
}


const Led_T * LED_FindHandleByFunction(const LedFunction_T func)
{
	uint8_t i = 0u;

	while( (i < LED_COUNT) && (m_leds[i].func != func) )
	{
		i++;
	}

	if (i < LED_COUNT)
	{
		return &m_leds[i];
	}

	return NULL;
}


void LedStop(void)
{
	if ((htim3.Instance->CR1 & TIM_CR1_CEN) == 0u)
	{
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);

		HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);
	}
}


void LedStart(void)
{
	const uint32_t sysTime = HAL_GetTick();

	if ((htim3.Instance->CR1 & TIM_CR1_CEN) == TIM_CR1_CEN)
	{
		LED_Init(sysTime);
	}
}


void LED_SetConfigurationData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][0u], data[0u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][1u], data[1u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][2u], data[2u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][3u], data[3u]);

	LED_InitFunction(ledIdx);
}


void LED_GetConfigurationData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*len = 0u;
		return;
	}

	data[0u] = m_leds[ledIdx].func;
	data[1u] = m_leds[ledIdx].paramR;
	data[2u] = m_leds[ledIdx].paramG;
	data[3u] = m_leds[ledIdx].paramB;

	*len = 4u;
}


void LED_SetStateData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (m_leds[ledIdx].func != LED_USER_LED) )
	{
		return;
	}

	m_leds[ledIdx].r = data[0u];
	m_leds[ledIdx].g = data[1u];
	m_leds[ledIdx].b = data[2u];

	LED_SetRGBLedPtr(&m_leds[ledIdx], data[0u], data[1u], data[2u]);
}


void LED_GetStateData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*len = 0u;
		return;
	}

	data[0u] = m_leds[ledIdx].r;
	data[1u] = m_leds[ledIdx].g;
	data[2u] = m_leds[ledIdx].b;

	*len = 3u;
}


void LED_SetBlinkData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (m_leds[ledIdx].func == LED_NOT_USED) )
	{
		return;
	}

	m_leds[ledIdx].blinkRepeat = data[0u];
	m_leds[ledIdx].blinkCount = (uint16_t)(data[0u] * 2u);
	m_leds[ledIdx].blinkR1 = data[1u];
	m_leds[ledIdx].blinkG1 = data[2u];
	m_leds[ledIdx].blinkB1 = data[3u];
	m_leds[ledIdx].blinkPeriod1 = (uint16_t)(data[4u] * 10u);
	m_leds[ledIdx].blinkR2 = data[5u];
	m_leds[ledIdx].blinkG2 = data[6u];
	m_leds[ledIdx].blinkB2 = data[7u];
	m_leds[ledIdx].blinkPeriod2 = (uint16_t)(data[8u] * 10u);

	if (0u != m_leds[ledIdx].blinkRepeat)
	{
		LED_SetRGBLedPtr(&m_leds[ledIdx], m_leds[ledIdx].blinkR1, m_leds[ledIdx].blinkG1, m_leds[ledIdx].blinkB1);
		MS_TIME_COUNTER_INIT(m_leds[ledIdx].blinkTimer);
	}
	else
	{
		LED_SetRGBLedPtr(&m_leds[ledIdx], m_leds[ledIdx].r, m_leds[ledIdx].g, m_leds[ledIdx].b);
	}

}


void LED_GetBlinkData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		// TODO - Check effect on len
		*len = 0u;
		return;
	}

	data[0u] = (m_leds[ledIdx].blinkRepeat < 255u) ? (m_leds[ledIdx].blinkCount / 2u) : 255u;
	data[1u] = m_leds[ledIdx].blinkR1;
	data[2u] = m_leds[ledIdx].blinkG1;
	data[3u] = m_leds[ledIdx].blinkB1;
	data[4u] = (uint8_t)(m_leds[ledIdx].blinkPeriod1 / 10u);
	data[5u] = m_leds[ledIdx].blinkR2;
	data[6u] = m_leds[ledIdx].blinkG2;
	data[7u] = m_leds[ledIdx].blinkB2;
	data[8u] = (uint8_t)(m_leds[ledIdx].blinkPeriod2 / 10u);

	*len = 9u;
}


void LED_ProcessBlink(Led_T * const p_led, const uint32_t sysTime)
{
	if (p_led->blinkCount)
	{
		if (p_led->blinkCount & 0x1u)
		{
			// odd count for off colour
			if (MS_TIMEREF_TIMEOUT(p_led->blinkTimer, sysTime, p_led->blinkPeriod2))
			{
				MS_TIMEREF_INIT(p_led->blinkTimer, sysTime);
				p_led->blinkCount --;

				if (p_led->blinkCount == 0u)
				{
					if (p_led->blinkRepeat == 0xFFu)
					{
						p_led->blinkCount = 256u;
					}
					else
					{
						LED_SetRGBLedPtr(p_led, p_led->r, p_led->g, p_led->b);
					}
				}
				else
				{
					LED_SetRGBLedPtr(p_led, p_led->blinkR1, p_led->blinkG1, p_led->blinkB1);
				}

			}
		}
		else
		{
			// even count for on colour
			if (MS_TIMEREF_TIMEOUT(p_led->blinkTimer, sysTime, p_led->blinkPeriod1))
			{
				LED_SetRGBLedPtr(p_led, p_led->blinkR2, p_led->blinkG2, p_led->blinkB2);
				MS_TIMEREF_INIT(p_led->blinkTimer, sysTime);
				p_led->blinkCount--;
			}
		}
	}
}


void LED_SetRGBLedPtr(Led_T * const p_led, const uint8_t r, const uint8_t g, const uint8_t b)
{
	*p_led->pwmDrv_r = (UINT16_MAX - pwm_table[r]);
	*p_led->pwmDrv_g = (UINT16_MAX - pwm_table[g]);
	*p_led->pwmDrv_b = (UINT16_MAX - pwm_table[b]);
}


void LED_InitFunction(const uint8_t ledIdx)
{
	Led_T * const p_led = &m_leds[ledIdx];
	uint16_t var = 0;

	EE_ReadVariable(m_ledFunctionParams[ledIdx][0u], &var);

	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		p_led->func = (LedFunction_T)(var & 0xFFu);
	}

	EE_ReadVariable(m_ledFunctionParams[ledIdx][1u], &var);

	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		p_led->paramR = (uint8_t)(var & 0xFFu);
	}

	EE_ReadVariable(m_ledFunctionParams[ledIdx][2u], &var);

	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		p_led->paramG = (uint8_t)(var & 0xFFu);
	}

	EE_ReadVariable(m_ledFunctionParams[ledIdx][3u], &var);

	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		p_led->paramB = (uint8_t)(var & 0xFFu);
	}

	if (p_led->func == LED_USER_LED)
	{
		LED_SetRGBLedPtr(p_led, p_led->paramR, p_led->paramG, p_led->paramB);
	}
	else
	{
		LED_SetRGBLedPtr(p_led, 0u, 0u, 0u);
	}
}
