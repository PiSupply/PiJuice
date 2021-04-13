/*
 * led.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"

#include "nv.h"
#include "time_count.h"
#include "util.h"
#include "iodrv.h"

#include "led.h"
#include "led_pwm_table.h"

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define LED_PARAM_IDX_FNCT_TYPE			0u
#define LED_PARAM_IDX_FNCT_RED_VALUE	1u
#define LED_PARAM_IDX_FNCT_GREEN_VALUE	2u
#define LED_PARAM_IDX_FNCT_BLUE_VALUE	3u

// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static void LED_SetRGBLedPtr(Led_T * const p_led, const uint8_t r, const uint8_t g, const uint8_t b);
static void LED_ProcessBlink(Led_T * const p_led, const uint32_t sysTime);
static void LED_InitFunction(const uint8_t ledIdx);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

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


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim15;
extern TIM_HandleTypeDef htim17;


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * LED_Init configures the module to a known initial state
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * LED_Shutdown prepares the module for a low power stop mode.
 * @param	none
 * @retval	none
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * LED_Service performs periodic updates for this module
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void LED_Service(const uint32_t sysTime)
{
	LED_ProcessBlink(&m_leds[LED_LED1_IDX], sysTime);
	LED_ProcessBlink(&m_leds[LED_LED2_IDX], sysTime);
}


// ****************************************************************************
/*!
 * LED_GetParamR returns the red set point for the type of function passed to the
 * routine. Rather than send an index the routine searches the led array for the
 * first one that matches the function type and returns the associated value for
 * that function.
 *
 * @param	func		function type to find
 * @retval	uint8_t		red set point level for the associated function
 */
// ****************************************************************************
uint8_t LED_GetParamR(uint8_t func)
{
	if (m_leds[0u].func == func)
	{
		return m_leds[0u].paramR;
	}
	else if (m_leds[1u].func == func)
	{
		return m_leds[1u].paramR;
	}
	else
	{
		return 0u;
	}
}


// ****************************************************************************
/*!
 * LED_GetParamG returns the green set point for the type of function passed to the
 * routine. Rather than send an index the routine searches the led array for the
 * first one that matches the function type and returns the associated value for
 * that function.
 *
 * @param	func		function type to find
 * @retval	uint8_t		green set point level for the associated function
 */
// ****************************************************************************
uint8_t LED_GetParamG(const uint8_t func)
{
	if (m_leds[0u].func == func)
	{
		return m_leds[0u].paramG;
	}
	else if (m_leds[1u].func == func)
	{
		return m_leds[1u].paramG;
	}
	else
	{
		return 0u;
	}
}


// ****************************************************************************
/*!
 * LED_GetParamB returns the blue set point for the type of function passed to the
 * routine. Rather than send an index the routine searches the led array for the
 * first one that matches the function type and returns the associated value for
 * that function.
 *
 * @param	func		function type to find
 * @retval	uint8_t		blue set point level for the associated function
 */
// ****************************************************************************
uint8_t LED_GetParamB(uint8_t func)
{
	if (m_leds[0u].func == func)
	{
		return m_leds[0u].paramB;
	}
	else if (m_leds[1u].func == func)
	{
		return m_leds[1u].paramB;
	}
	else
	{
		return 0u;
	}
}


// ****************************************************************************
/*!
 * LED_SetRGB directly addresses an led in the led array to directly set the red
 * green and blue components
 *
 * @param	ledIdx		addressed led
 * @param	r			red set point for addressed led
 * @param	g			green set point for addressed led
 * @param	b			blue set point for addressed led
 * @retval	none
 */
// ****************************************************************************
void LED_SetRGB(const uint8_t ledIdx, const uint8_t r, const uint8_t g, const uint8_t b)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	LED_SetRGBLedPtr(&m_leds[ledIdx], r, g, b);
}


// ****************************************************************************
/*!
 * LED_FunctionSetRGB finds an led in the led array by function to directly set
 * the red, green and blue components
 *
 * @param	func		function type to match led with
 * @param	r			red set point for led
 * @param	g			green set point for led
 * @param	b			blue set point for led
 * @retval	none
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * LED_FunctionSetRGB finds an led in the led array by function and returns a
 * const pointer to it.
 *
 * @param	func		function type to match led with
 * @retval	none
 *  */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * LED_SetConfigurationData sets the function configuration data for the addressed
 * led. Data is commited to non volatile memory and will be persistent through
 * power cycles.
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to configuration data
 * @param	len			length of configuration data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_SetConfigurationData(const uint8_t ledIdx, const uint8_t * const p_data, const uint8_t len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_TYPE], p_data[0u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_RED_VALUE], p_data[1u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_GREEN_VALUE], p_data[2u]);
	NV_WriteVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_BLUE_VALUE], p_data[3u]);

	LED_InitFunction(ledIdx);
}


// ****************************************************************************
/*!
 * LED_GetConfigurationData gets the function configuration data for the addressed
 * led
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to destination for configuration data
 * @param	p_len		pointer to destination for length of configuration data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_GetConfigurationData(const uint8_t ledIdx, uint8_t * const p_data, uint16_t * const p_len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*p_len = 0u;
		return;
	}

	p_data[0u] = m_leds[ledIdx].func;
	p_data[1u] = m_leds[ledIdx].paramR;
	p_data[2u] = m_leds[ledIdx].paramG;
	p_data[3u] = m_leds[ledIdx].paramB;

	*p_len = 4u;
}


// ****************************************************************************
/*!
 * LED_SetStateData sets the current levels of rgb data for the addressed led
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to state data
 * @param	len			length of state data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_SetStateData(const uint8_t ledIdx, const uint8_t * const p_data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (m_leds[ledIdx].func != LED_USER_LED) )
	{
		return;
	}

	m_leds[ledIdx].r = p_data[0u];
	m_leds[ledIdx].g = p_data[1u];
	m_leds[ledIdx].b = p_data[2u];

	LED_SetRGBLedPtr(&m_leds[ledIdx], p_data[0u], p_data[1u], p_data[2u]);
}


// ****************************************************************************
/*!
 * LED_GetStateData gets the current state of the led rgb data from the addressed
 * led
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to destination for state data
 * @param	p_len		pointer to destination for length of state data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_GetStateData(const uint8_t ledIdx, uint8_t * const p_data, uint16_t * const p_len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*p_len = 0u;
		return;
	}

	p_data[0u] = m_leds[ledIdx].r;
	p_data[1u] = m_leds[ledIdx].g;
	p_data[2u] = m_leds[ledIdx].b;

	*p_len = 3u;
}


// ****************************************************************************
/*!
 * LED_SetBlinkData sets the data for process blink to operate on
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to blink data
 * @param	len			length of blink data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_SetBlinkData(const uint8_t ledIdx, const uint8_t * const p_data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (m_leds[ledIdx].func == LED_NOT_USED) )
	{
		return;
	}

	m_leds[ledIdx].blinkRepeat = p_data[0u];
	m_leds[ledIdx].blinkCount = (uint16_t)(p_data[0u] * 2u);
	m_leds[ledIdx].blinkR1 = p_data[1u];
	m_leds[ledIdx].blinkG1 = p_data[2u];
	m_leds[ledIdx].blinkB1 = p_data[3u];
	m_leds[ledIdx].blinkPeriod1 = (uint16_t)(p_data[4u] * 10u);
	m_leds[ledIdx].blinkR2 = p_data[5u];
	m_leds[ledIdx].blinkG2 = p_data[6u];
	m_leds[ledIdx].blinkB2 = p_data[7u];
	m_leds[ledIdx].blinkPeriod2 = (uint16_t)(p_data[8u] * 10u);

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


// ****************************************************************************
/*!
 * LED_GetBlinkData gets the data that process blink operates on
 *
 * @param	ledIdx		addressed led
 * @param	p_data		pointer to destination for blink data
 * @param	p_len		pointer to destination for length of blink data
 * @retval	none
 *
 */
// ****************************************************************************
void LED_GetBlinkData(const uint8_t ledIdx, uint8_t * const p_data, uint16_t * const p_len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*p_len = 0u;
		return;
	}

	p_data[0u] = (m_leds[ledIdx].blinkRepeat < 255u) ? (m_leds[ledIdx].blinkCount / 2u) : 255u;
	p_data[1u] = m_leds[ledIdx].blinkR1;
	p_data[2u] = m_leds[ledIdx].blinkG1;
	p_data[3u] = m_leds[ledIdx].blinkB1;
	p_data[4u] = (uint8_t)(m_leds[ledIdx].blinkPeriod1 / 10u);
	p_data[5u] = m_leds[ledIdx].blinkR2;
	p_data[6u] = m_leds[ledIdx].blinkG2;
	p_data[7u] = m_leds[ledIdx].blinkB2;
	p_data[8u] = (uint8_t)(m_leds[ledIdx].blinkPeriod2 / 10u);

	*p_len = 9u;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * LED_ProcessBlink
 *
 * @param	p_led		pointer to led
 * @param	sysTime		current value of the system tick timer
 * @retval	none
 *
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * LED_SetRGBLedPtr sets the red, green and blue values for the led that gets
 * passed through.
 *
 * @param	p_led		pointer to led to set the levels for
 * @param	r			red set point for led
 * @param	g			green set point for led
 * @param	b			blue set point for led
 * @retval	none
 */
// ****************************************************************************
void LED_SetRGBLedPtr(Led_T * const p_led, const uint8_t r, const uint8_t g, const uint8_t b)
{
	*p_led->pwmDrv_r = (UINT16_MAX - pwm_table[r]);
	*p_led->pwmDrv_g = (UINT16_MAX - pwm_table[g]);
	*p_led->pwmDrv_b = (UINT16_MAX - pwm_table[b]);
}


// ****************************************************************************
/*!
 * LED_InitFunction reads the function and associated red, green and blue levels
 * from non volatile storage and configures the addressed led.
 *
 * @param	ledIdx		addressed led
 * @retval	none
 */
// ****************************************************************************
void LED_InitFunction(const uint8_t ledIdx)
{
	Led_T * const p_led = &m_leds[ledIdx];

	NV_ReadVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_TYPE], (uint8_t*)&p_led->func);
	NV_ReadVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_RED_VALUE], &p_led->paramR);
	NV_ReadVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_GREEN_VALUE], &p_led->paramG);
	NV_ReadVariable_U8(m_ledFunctionParams[ledIdx][LED_PARAM_IDX_FNCT_BLUE_VALUE], &p_led->paramB);

	if (p_led->func == LED_USER_LED)
	{
		LED_SetRGBLedPtr(p_led, p_led->paramR, p_led->paramG, p_led->paramB);
	}
	else
	{
		LED_SetRGBLedPtr(p_led, 0u, 0u, 0u);
	}
}
