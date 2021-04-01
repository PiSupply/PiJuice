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

#include "led.h"


extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim15;
extern TIM_HandleTypeDef htim17;
GPIO_InitTypeDef GPIO_InitStruct;


typedef struct
{
	LedFunction_T func;
	uint8_t paramR;
	uint8_t paramG;
	uint8_t paramB;

	uint8_t r;
	uint8_t g;
	uint8_t b;

	uint8_t blinkRepeat;

	uint8_t blinkR1;
	uint8_t blinkG1;
	uint8_t blinkB1;
	uint16_t blinkPeriod1;

	uint8_t blinkR2;
	uint8_t blinkG2;
	uint8_t blinkB2;
	uint16_t blinkPeriod2;

	uint16_t blinkCount;
	uint32_t blinkTimer;

	__IO uint32_t * pwmDrv_r;
	__IO uint32_t * pwmDrv_g;
	__IO uint32_t * pwmDrv_b;

} Led_T;


static Led_T leds[LED_COUNT] =
{
	{ LED_CHARGE_STATUS, .paramR = 60u, .paramG = 60u, .paramB = 100u, .pwmDrv_r = &TIM3->CCR1, .pwmDrv_g = &TIM3->CCR2, .pwmDrv_b = &TIM3->CCR3},
	{ LED_USER_LED, .paramR = 0u, .paramG = 0u, .paramB = 0u, .pwmDrv_r = &TIM15->CCR1, .pwmDrv_g = &TIM15->CCR2, .pwmDrv_b = &TIM17->CCR1 },
};


static const uint16_t pwm_table[256u] =
{
     65535,    65508,    65479,    65451,    65422,    65394,    65365,    65337,
     65308,    65280,    65251,    65223,    65195,    65166,    65138,    65109,
     65081,    65052,    65024,    64995,    64967,    64938,    64909,    64878,
     64847,    64815,    64781,    64747,    64711,    64675,    64637,    64599,
     64559,    64518,    64476,    64433,    64389,    64344,    64297,    64249,
     64200,    64150,    64099,    64046,    63992,    63937,    63880,    63822,
     63763,    63702,    63640,    63577,    63512,    63446,    63379,    63310,
     63239,    63167,    63094,    63019,    62943,    62865,    62785,    62704,
     62621,    62537,    62451,    62364,    62275,    62184,    62092,    61998,
     61902,    61804,    61705,    61604,    61501,    61397,    61290,    61182,
     61072,    60961,    60847,    60732,    60614,    60495,    60374,    60251,
     60126,    59999,    59870,    59739,    59606,    59471,    59334,    59195,
     59053,    58910,    58765,    58618,    58468,    58316,    58163,    58007,
     57848,    57688,    57525,    57361,    57194,    57024,    56853,    56679,
     56503,    56324,    56143,    55960,    55774,    55586,    55396,    55203,
     55008,    54810,    54610,    54408,    54203,    53995,    53785,    53572,
     53357,    53140,    52919,    52696,    52471,    52243,    52012,    51778,
     51542,    51304,    51062,    50818,    50571,    50321,    50069,    49813,
     49555,    49295,    49031,    48764,    48495,    48223,    47948,    47670,
     47389,    47105,    46818,    46529,    46236,    45940,    45641,    45340,
     45035,    44727,    44416,    44102,    43785,    43465,    43142,    42815,
     42486,    42153,    41817,    41478,    41135,    40790,    40441,    40089,
     39733,    39375,    39013,    38647,    38279,    37907,    37531,    37153,
     36770,    36385,    35996,    35603,    35207,    34808,    34405,    33999,
     33589,    33175,    32758,    32338,    31913,    31486,    31054,    30619,
     30181,    29738,    29292,    28843,    28389,    27932,    27471,    27007,
     26539,    26066,    25590,    25111,    24627,    24140,    23649,    23153,
     22654,    22152,    21645,    21134,    20619,    20101,    19578,    19051,
     18521,    17986,    17447,    16905,    16358,    15807,    15252,    14693,
     14129,    13562,    12990,    12415,    11835,    11251,    10662,    10070,
     9473,    8872,    8266,    7657,    7043,    6424,    5802,    5175,
     4543,    3908,    3267,    2623,    1974,    1320,    662,    0
};



static void LED_SetRGBLedPtr(Led_T * const p_led, const uint8_t r, const uint8_t g, const uint8_t b);
static void LED_ProcessBlink(Led_T * const p_led, const uint32_t sysTime);


void LED_Init(const uint32_t sysTime)
{
	uint16_t var = 0;
	Led_T * p_led;

	// Start channel 1 RED2
	if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	// Start channel 2 Green2
	if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	// Start channel 3 Blue2
	if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	// LED2 C1
	if (HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_1) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	// LED2 C2
	if (HAL_TIM_PWM_Start(&htim15, TIM_CHANNEL_2) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	// LED2 C3
	if (HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1) != HAL_OK)
	{
	// PWM generation Error
	Error_Handler();
	}

	p_led = &leds[LED_LED1_IDX];

	EE_ReadVariable(NV_LED_FUNC_1, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->func = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_R_1, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramR = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_G_1, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramG = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_B_1, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramB = var&0xFF;
	}

	if (p_led->func == LED_USER_LED)
	{
		LED_SetRGB(LED_LED1_IDX, p_led->paramR, p_led->paramG, p_led->paramB);
	}
	else
	{
		LED_SetRGB(LED_LED1_IDX, 0u, 0u, 0u);
	}

	p_led = &leds[LED_LED2_IDX];
	var = 0;

	EE_ReadVariable(NV_LED_FUNC_2, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->func = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_R_2, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramR = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_G_2, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramG = var&0xFF;
	}

	EE_ReadVariable(NV_LED_PARAM_B_2, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		p_led->paramB = var&0xFF;
	}
}


uint8_t LED_GetParamR(uint8_t func)
{
	if (leds[0].func == func)
	{
		return leds[0].paramR;
	}
	else if (leds[1].func == func)
	{
		return leds[1].paramR;
	}
	else
	{
		return 0u;
	}
}


uint8_t LED_GetParamG(const uint8_t func)
{
	if (leds[0].func == func)
	{
		return leds[0].paramG;
	}
	else if (leds[1].func == func)
	{
		return leds[1].paramG;
	}
	else
	{
		return 0u;
	}
}


uint8_t LED_GetParamB(uint8_t func)
{
	if (leds[0].func == func)
	{
		return leds[0].paramB;
	}
	else if (leds[1].func == func)
	{
		return leds[1].paramB;
	}
	else
	{
		return 0u;
	}
}





void LED_Service(const uint32_t sysTime)
{
	LED_ProcessBlink(&leds[LED_LED1_IDX], sysTime);
	LED_ProcessBlink(&leds[LED_LED2_IDX], sysTime);
}


void LED_SetRGB(const uint8_t ledIdx, const uint8_t r, const uint8_t g, const uint8_t b)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	LED_SetRGBLedPtr(&leds[ledIdx], r, g, b);
}


// Finds LED by function and sets rgb
void LED_FunctionSetRGB(LedFunction_T func, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t i = 0u;

	while( (i < LED_COUNT) && (leds[i].func != func) )
	{
		i++;
	}

	if (i > LED_LAST_LED_IDX)
	{
		return;
	}

	Led_T * p_led = &leds[i];

	p_led->r = r;
	p_led->g = g;
	p_led->b = b;
	p_led->blinkCount = 0u;
	p_led->blinkRepeat = 0u;

	LED_SetRGBLedPtr(p_led, r, g, b);
}


void LedStop(void)
{
	if ((htim3.Instance->CR1 & TIM_CR1_CEN) == 0)
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
	uint16_t var = 0;

	if (ledIdx > LED_LAST_LED_IDX)
	{
		return;
	}

	if (ledIdx == 0u)
	{
		NvWriteVariableU8(NV_LED_FUNC_1, data[0]);
		NvWriteVariableU8(NV_LED_PARAM_R_1, data[1]);
		NvWriteVariableU8(NV_LED_PARAM_G_1, data[2]);
		NvWriteVariableU8(NV_LED_PARAM_B_1, data[3]);

		EE_ReadVariable(NV_LED_FUNC_1, &var);

		if (((~var)&0xFF) == (var>>8)) {
			leds[ledIdx].func = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_R_1, &var);

		if (((~var)&0xFF) == (var>>8)) {
			leds[ledIdx].paramR = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_G_1, &var);

		if (((~var)&0xFF) == (var>>8)) {
			leds[ledIdx].paramG = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_B_1, &var);

		if (((~var)&0xFF) == (var>>8)) {
			leds[ledIdx].paramB = var&0xFF;
		}

		if (leds[ledIdx].func == LED_USER_LED)
		{
			LED_SetRGBLedPtr(&leds[ledIdx], leds[ledIdx].paramR, leds[ledIdx].paramG, leds[ledIdx].paramB);
		}
		else
		{
			LED_SetRGB(ledIdx, 0u, 0u, 0u);
		}
	}

	if (ledIdx == 1u)
	{
		NvWriteVariableU8(NV_LED_FUNC_2, data[0]);
		NvWriteVariableU8(NV_LED_PARAM_R_2, data[1]);
		NvWriteVariableU8(NV_LED_PARAM_G_2, data[2]);
		NvWriteVariableU8(NV_LED_PARAM_B_2, data[3]);

		EE_ReadVariable(NV_LED_FUNC_2, &var);

		if (((~var)&0xFF) == (var>>8))
		{
			leds[ledIdx].func = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_R_2, &var);

		if (((~var)&0xFF) == (var>>8))
		{
			leds[ledIdx].paramR = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_G_2, &var);

		if (((~var)&0xFF) == (var>>8))
		{
			leds[ledIdx].paramG = var&0xFF;
		}

		EE_ReadVariable(NV_LED_PARAM_B_2, &var);

		if (((~var)&0xFF) == (var>>8))
		{
			leds[ledIdx].paramB = var&0xFF;
		}

		if (leds[ledIdx].func == LED_USER_LED)
		{
			LED_SetRGBLedPtr(&leds[ledIdx], leds[ledIdx].paramR, leds[ledIdx].paramG, leds[ledIdx].paramB);
		}
		else
		{
			LED_SetRGBLedPtr(&leds[ledIdx], 0, 0, 0);
		}
	}
}


void LED_GetConfigurationData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*len = 0u;
		return;
	}

	data[0u] = leds[ledIdx].func;
	data[1u] = leds[ledIdx].paramR;
	data[2u] = leds[ledIdx].paramG;
	data[3u] = leds[ledIdx].paramB;

	*len = 4u;
}


void LED_SetStateData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (leds[ledIdx].func != LED_USER_LED) )
	{
		return;
	}

	leds[ledIdx].r = data[0u];
	leds[ledIdx].g = data[1u];
	leds[ledIdx].b = data[2u];

	LED_SetRGBLedPtr(&leds[ledIdx], data[0u], data[1u], data[2u]);
}


void LED_GetStateData(const uint8_t ledIdx, uint8_t * const data, uint16_t * const len)
{
	if (ledIdx > LED_LAST_LED_IDX)
	{
		*len = 0u;
		return;
	}

	data[0u] = leds[ledIdx].r;
	data[1u] = leds[ledIdx].g;
	data[2u] = leds[ledIdx].b;

	*len = 3u;
}


void LED_SetBlinkData(const uint8_t ledIdx, const uint8_t * const data, const uint8_t len)
{
	if ( (ledIdx > LED_LAST_LED_IDX) || (leds[ledIdx].func == LED_NOT_USED) )
	{
		return;
	}

	leds[ledIdx].blinkRepeat = data[0u];
	leds[ledIdx].blinkCount = (uint16_t)(data[0u] * 2u);
	leds[ledIdx].blinkR1 = data[1u];
	leds[ledIdx].blinkG1 = data[2u];
	leds[ledIdx].blinkB1 = data[3u];
	leds[ledIdx].blinkPeriod1 = (uint16_t)(data[4u] * 10u);
	leds[ledIdx].blinkR2 = data[5u];
	leds[ledIdx].blinkG2 = data[6u];
	leds[ledIdx].blinkB2 = data[7u];
	leds[ledIdx].blinkPeriod2 = (uint16_t)(data[8u] * 10u);

	if (0u != leds[ledIdx].blinkRepeat)
	{
		LED_SetRGBLedPtr(&leds[ledIdx], leds[ledIdx].blinkR1, leds[ledIdx].blinkG1, leds[ledIdx].blinkB1);
		MS_TIME_COUNTER_INIT(leds[ledIdx].blinkTimer);
	}
	else
	{
		LED_SetRGBLedPtr(&leds[ledIdx], leds[ledIdx].r, leds[ledIdx].g, leds[ledIdx].b);
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

	data[0u] = (leds[ledIdx].blinkRepeat < 255u) ? (leds[ledIdx].blinkCount / 2u) : 255u;
	data[1u] = leds[ledIdx].blinkR1;
	data[2u] = leds[ledIdx].blinkG1;
	data[3u] = leds[ledIdx].blinkB1;
	data[4u] = (uint8_t)(leds[ledIdx].blinkPeriod1 / 10u);
	data[5u] = leds[ledIdx].blinkR2;
	data[6u] = leds[ledIdx].blinkG2;
	data[7u] = leds[ledIdx].blinkB2;
	data[8u] = (uint8_t)(leds[ledIdx].blinkPeriod2 / 10u);

	*len = 9u;
}


void LED_ProcessBlink(Led_T * const p_led, const uint32_t sysTime)
{
	if (p_led->blinkCount)
	{
		if (p_led->blinkCount & 0x1u)
		{
			// odd count for off colour
			if (MS_TIME_COUNT(p_led->blinkTimer) >= p_led->blinkPeriod2)
			{
				MS_TIME_COUNTER_INIT(p_led->blinkTimer);
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
			if (MS_TIME_COUNT(p_led->blinkTimer) >= p_led->blinkPeriod1)
			{
				LED_SetRGBLedPtr(p_led, p_led->blinkR2, p_led->blinkG2, p_led->blinkB2);
				MS_TIME_COUNTER_INIT(p_led->blinkTimer);
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
