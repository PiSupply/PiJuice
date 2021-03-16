/*
 * led.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "led.h"
#include "stm32f0xx_hal.h"
#include "nv.h"
#include "time_count.h"

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"
osThreadId_t ledTaskHandle[2];
static void StartLedTask(void *argument);
static uint8_t taskLed1;
static uint8_t taskLed2;
static const osThreadAttr_t ledTask_attributes = {
.name = "ledTask1",
.priority = (osPriority_t) osPriorityLow,
.stack_size = 512
};
/*osMutexId osMutexHandle[2];
const osMutexAttr_t ledMutex_attributes = {
	.name = "LedMutex",
	.attr_bits = osMutexRecursive | osMutexPrioInherit,//osMutexRobust,
	.cb_mem = NULL,
	.cb_size = 0
};*/
#endif

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

} Led_T;

static Led_T leds[2] = {

	{ LED_CHARGE_STATUS, 60, 60, 100},
	{ LED_USER_LED, 0, 0, 0 },
};

static const uint16_t pwm_table[] = {
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

static uint8_t mutMem[64]  __attribute__((aligned(4)));
void LedInit(void) {

	/*HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET); //LED1B
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET); //LED1G
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET); //LED1R
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4));
  //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);*/
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    //HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);

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

	uint16_t var = 0;
	EE_ReadVariable(NV_LED_FUNC_1, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[0].func = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_R_1, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[0].paramR = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_G_1, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[0].paramG = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_B_1, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[0].paramB = var&0xFF;
	}
	if (leds[0].func == LED_USER_LED) {
		LedSetRGB(LED1, leds[1].paramR, leds[1].paramG, leds[1].paramB);
	}else {
		LedSetRGB(LED1, 0, 0, 0);
	}

	var = 0;
	EE_ReadVariable(NV_LED_FUNC_2, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[1].func = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_R_2, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[1].paramR = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_G_2, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[1].paramG = var&0xFF;
	}
	EE_ReadVariable(NV_LED_PARAM_B_2, &var);
	if (((~var)&0xFF) == (var>>8)) {
		leds[1].paramB = var&0xFF;
	}
	if (leds[1].func == LED_USER_LED) {
		LedSetRGB(LED2, leds[1].paramR, leds[1].paramG, leds[1].paramB);
	}else {
		LedSetRGB(LED2, 0, 0, 0);
	}
#if defined(RTOS_FREERTOS)
	taskLed1 = LED1;
	ledTaskHandle[0] = osThreadNew(StartLedTask, &taskLed1, &ledTask_attributes);
	//osMutexHandle[0] = osMutexNew(&ledMutex_attributes);
	taskLed2 = LED2;
	ledTaskHandle[1] = osThreadNew(StartLedTask, &taskLed2, &ledTask_attributes);
	//osMutexHandle[1] = osMutexNew(&ledMutex_attributes);
	uint8_t ledBlink[] = {5, 100, 0, 80, 100, 0, 80, 0, 50};
	LedCmdSetBlink(LED1, ledBlink, 9);
	//LedCmdSetBlink(LED2, ledBlink, 9);
#endif
}

uint8_t LedGetParamR(uint8_t func) {
	if (leds[0].func == func) {
		return leds[0].paramR;
	} else if (leds[1].func == func) {
		return leds[1].paramR;
	}
}
uint8_t LedGetParamG(uint8_t func) {
	if (leds[0].func == func) {
		return leds[0].paramG;
	} else if (leds[1].func == func) {
		return leds[1].paramG;
	}
}
uint8_t LedGetParamB(uint8_t func) {
	if (leds[0].func == func) {
		return leds[0].paramB;
	} else if (leds[1].func == func) {
		return leds[1].paramB;
	}
}

#if defined(RTOS_FREERTOS)
static void StartLedTask(void *argument)
{
	portENTER_CRITICAL();
	uint8_t n = *((uint8_t*)argument); //index of led
	portEXIT_CRITICAL();

	for(;;)
	{

	  //if (osMutexWait(osMutexHandle[n], osWaitForever) == osOK)
	  {
		while(leds[n].blinkCount) {

			if ( (leds[n].blinkCount & 0x1) ) {
				// odd count for off color
				osDelay(leds[n].blinkPeriod2);
				if (leds[n].blinkCount) leds[n].blinkCount --;
				if (leds[n].blinkCount == 0) {
					if (leds[n].blinkRepeat == 0xFF) {
						leds[n].blinkCount = 256;
					} else {
						LedSetRGB(n, leds[n].r, leds[n].g, leds[n].b);
					}
				} else
					LedSetRGB(n, leds[n].blinkR1, leds[n].blinkG1, leds[n].blinkB1);
			} else {
				// even count for on color
				osDelay(leds[n].blinkPeriod1);
				if (leds[n].blinkCount) {
					leds[n].blinkCount --;
					LedSetRGB(n, leds[n].blinkR2, leds[n].blinkG2, leds[n].blinkB2);
				} else {
					LedSetRGB(n, leds[n].r, leds[n].g, leds[n].b);
				}
			}
		}
		  //osDelay(10);//
		  osThreadSuspend(ledTaskHandle[n]);//
	  }

	}
}
#else
void ProcessBlink(uint8_t n) {
	if (leds[n].blinkCount) {
		if ( (leds[n].blinkCount & 0x1) ) {
			// odd count for off color
			if (MS_TIME_COUNT(leds[n].blinkTimer) >= leds[n].blinkPeriod2) {
				MS_TIME_COUNTER_INIT(leds[n].blinkTimer);
				leds[n].blinkCount --;
				if (leds[n].blinkCount == 0) {
					if (leds[n].blinkRepeat == 0xFF) {
						leds[n].blinkCount = 256;
					} else {
						LedSetRGB(n, leds[n].r, leds[n].g, leds[n].b);
					}
				} else
					LedSetRGB(n, leds[n].blinkR1, leds[n].blinkG1, leds[n].blinkB1);
			}
		} else {
			// even count for on color
			if (MS_TIME_COUNT(leds[n].blinkTimer) >= leds[n].blinkPeriod1) {
				LedSetRGB(n, leds[n].blinkR2, leds[n].blinkG2, leds[n].blinkB2);
				MS_TIME_COUNTER_INIT(leds[n].blinkTimer);
				leds[n].blinkCount --;
			}
		}
	}
}

void LedTask(void) {
	ProcessBlink(0);
	ProcessBlink(1);
}
#endif
void LedSetRGB(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
	if (led == 1) {
		TIM15->CCR1 = 65535 - pwm_table[r];
		TIM15->CCR2 = 65535 - pwm_table[g];
		TIM17->CCR1 = 65535 - pwm_table[b];
	} else 	if (led == 0) {
		TIM3->CCR1 = 65535 - pwm_table[r];
		TIM3->CCR2 = 65535 - pwm_table[g];
		TIM3->CCR3 = 65535 - pwm_table[b];
	}
}

void LedFunctionSetRGB(LedFunction_T func, uint8_t r, uint8_t g, uint8_t b) {
	if (leds[0].func == func) {
		leds[0].r = r;
		leds[0].g = g;
		leds[0].b = b;
		leds[0].blinkRepeat = 0;
		leds[0].blinkCount = 0;
		LedSetRGB(LED1, r, g, b);
	}

	if (leds[1].func == func) {
		leds[1].r = r;
		leds[1].g = g;
		leds[1].b = b;
		leds[1].blinkRepeat = 0;
		leds[1].blinkCount = 0;
		LedSetRGB(LED2, r, g, b);
	}
}

void LedStop(void) {
	if ((htim3.Instance->CR1 & TIM_CR1_CEN) == 0) {
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);

		HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_1);
		HAL_TIM_PWM_Stop(&htim15, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop(&htim17, TIM_CHANNEL_1);

		//Configure GPIOB output pins
		/*GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
		HAL_GPIO_WritePin(GPIOB, GPIO_InitStruct.Pin, GPIO_PIN_RESET);*/
	}
}

void LedStart(void) {
	if ((htim3.Instance->CR1 & TIM_CR1_CEN) == TIM_CR1_CEN)
		//Configure GPIOB PWM pins
		/*GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_4|GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);*/
		LedInit();
}

void LedSetConfiguarion(uint8_t led, uint8_t data[], uint8_t len) {
	if (led > 1) return;
	uint16_t var = 0;
	if (led == 0) {
		NvWriteVariableU8(NV_LED_FUNC_1, data[0]);
		NvWriteVariableU8(NV_LED_PARAM_R_1, data[1]);
		NvWriteVariableU8(NV_LED_PARAM_G_1, data[2]);
		NvWriteVariableU8(NV_LED_PARAM_B_1, data[3]);

		EE_ReadVariable(NV_LED_FUNC_1, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].func = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_R_1, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramR = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_G_1, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramG = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_B_1, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramB = var&0xFF;
		}
		if (leds[led].func == LED_USER_LED) {
			LedSetRGB(LED1, leds[led].paramR, leds[led].paramG, leds[led].paramB);
		}else {
			LedSetRGB(LED1, 0, 0, 0);
		}
	}
	if (led == 1) {
		NvWriteVariableU8(NV_LED_FUNC_2, data[0]);
		NvWriteVariableU8(NV_LED_PARAM_R_2, data[1]);
		NvWriteVariableU8(NV_LED_PARAM_G_2, data[2]);
		NvWriteVariableU8(NV_LED_PARAM_B_2, data[3]);

		EE_ReadVariable(NV_LED_FUNC_2, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].func = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_R_2, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramR = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_G_2, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramG = var&0xFF;
		}
		EE_ReadVariable(NV_LED_PARAM_B_2, &var);
		if (((~var)&0xFF) == (var>>8)) {
			leds[led].paramB = var&0xFF;
		}
		if (leds[led].func == LED_USER_LED) {
			LedSetRGB(LED2, leds[led].paramR, leds[led].paramG, leds[led].paramB);
		}else {
			LedSetRGB(LED2, 0, 0, 0);
		}
	}

}

void LedGetConfiguarion(uint8_t led, uint8_t data[], uint16_t *len) {
	if (led > 1) return;
	data[0] = leds[led].func;
	data[1] = leds[led].paramR;
	data[2] = leds[led].paramG;
	data[3] = leds[led].paramB;
	*len = 4;
}

void LedCmdSetState(uint8_t led, uint8_t data[], uint8_t len) {
	if (led > 1 || leds[led].func != LED_USER_LED) return;
	leds[led].r = data[0];
	leds[led].g = data[1];
	leds[led].b = data[2];
	LedSetRGB(led, data[0], data[1], data[2]);
}

void LedCmdGetState(uint8_t led, uint8_t data[], uint16_t *len) {
	if (led > 1) return;
	data[0] = leds[led].r;
	data[1] = leds[led].g;
	data[2] = leds[led].b;
	*len = 3;
}

void LedCmdSetBlink(uint8_t led, uint8_t data[], uint8_t len) {
	if (led > 1 || leds[led].func == LED_NOT_USED) return;

	leds[led].blinkRepeat = data[0];
	leds[led].blinkCount = data[0];
	leds[led].blinkCount <<= 1; // *=2
	leds[led].blinkR1 = data[1];
	leds[led].blinkG1 = data[2];
	leds[led].blinkB1 = data[3];
	leds[led].blinkPeriod1 = data[4];
	leds[led].blinkPeriod1 *= 10;
	leds[led].blinkR2 = data[5];
	leds[led].blinkG2 = data[6];
	leds[led].blinkB2 = data[7];
	leds[led].blinkPeriod2 = data[8];
	leds[led].blinkPeriod2 *= 10;


#if defined(RTOS_FREERTOS)
	if (leds[led].blinkRepeat) {
		LedSetRGB(led, leds[led].blinkR1, leds[led].blinkG1, leds[led].blinkB1);
		osThreadResume(ledTaskHandle[led]);
		//osMutexRelease(osMutexHandle[led]);
	} else {
		LedSetRGB(led, leds[led].r, leds[led].g, leds[led].b);
	}
#else
	if (leds[led].blinkRepeat) {
		LedSetRGB(led, leds[led].blinkR1, leds[led].blinkG1, leds[led].blinkB1);
		MS_TIME_COUNTER_INIT(leds[led].blinkTimer);
	} else {
		LedSetRGB(led, leds[led].r, leds[led].g, leds[led].b);
	}
#endif

}

void LedCmdGetBlink(uint8_t led, uint8_t data[], uint16_t *len) {
	if (led > 1) return;

	data[0] = leds[led].blinkRepeat < 255 ? leds[led].blinkCount >> 1 : 255;
	data[1] = leds[led].blinkR1;
	data[2] = leds[led].blinkG1;
	data[3] = leds[led].blinkB1;
	data[4] = leds[led].blinkPeriod1 / 10;
	data[5] = leds[led].blinkR2;
	data[6] = leds[led].blinkG2;
	data[7] = leds[led].blinkB2;
	data[8] = leds[led].blinkPeriod2 / 10;
	*len = 9;
}
