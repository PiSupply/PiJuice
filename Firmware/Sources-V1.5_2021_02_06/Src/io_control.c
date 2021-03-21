/*
 * io_control.c
 *
 *  Created on: 23.10.2017.
 *      Author: milan
 */
#include "main.h"

#include "system_conf.h"
#include "analog.h"
#include "nv.h"

#include "iodrv.h"
#include "io_control.h"

void HAL_TIM_MspPostInit(TIM_HandleTypeDef* htim);

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim14;

TIM_OC_InitTypeDef sConfigOC;

uint8_t ioConfig[2] __attribute__((section("no_init")));
uint16_t ioParam1[2] __attribute__((section("no_init")));
uint16_t ioParam2[2] __attribute__((section("no_init")));
uint16_t pwmLevel[2] __attribute__((section("no_init")));

extern uint8_t resetStatus;

/* TIM1 init function */
void MX_TIM1_Init(void)
{

  TIM_ClockConfigTypeDef sClockSourceConfig;
  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 7;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 665;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 100;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim1);

  // Start channel 1
  /*if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    // PWM Generation Error
    Error_Handler();
  }*/

}

/* TIM14 init function */
void MX_TIM14_Init(void)
{

  TIM_OC_InitTypeDef sConfigOC;

  htim14.Instance = TIM14;
  htim14.Init.Prescaler = 7;
  htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim14.Init.Period = 100;
  htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Init(&htim14) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 50;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim14, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim14);

  // Start channel 1
  /*if (HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1) != HAL_OK)
  {
    // PWM Generation Error
    Error_Handler();
  }*/

}

void IoConfigure(uint8_t extIOPinIdx)
{
	uint8_t iodrvPinIdx;
	const uint8_t ioPinIdx = extIOPinIdx - 1u;
	TIM_HandleTypeDef *htim;

	if (extIOPinIdx == 1u)
	{
		iodrvPinIdx = IODRV_PIN_IO1;
		htim = &htim14;
	}
	else if (extIOPinIdx == 2u)
	{
		iodrvPinIdx = IODRV_PIN_IO2;
		htim = &htim1;
	}
	else
	{
		return;
	}

	const uint8_t pulldir = (ioConfig[ioPinIdx] >> 4u) & 0x3u;
	const uint8_t newPinType = (ioConfig[ioPinIdx] & 0x0Fu);

	if (0u != (htim->Instance->BDTR & TIM_BDTR_MOE))
	{
		HAL_TIM_PWM_Stop(htim, TIM_CHANNEL_1);
	}

	IODRV_SetPinType(iodrvPinIdx, newPinType);
	IODRV_SetPinPullDir(iodrvPinIdx, pulldir);

	if ( (newPinType == IOTYPE_PWM_PUSHPULL) || (newPinType == IOTYPE_PWM_OPENDRAIN) )
	{
		// Setup pwm timer.
		htim->Instance->PSC = ioParam1[ioPinIdx] < 4096u ? 0u : (((uint32_t)ioParam1[ioPinIdx]) / 4096u);
		htim->Instance->ARR = ((uint32_t)ioParam1[ioPinIdx] + 1u) * 16u / (htim->Instance->PSC + 1u) - 1u;
		htim->Instance->CCR1 = ioParam2[ioPinIdx] == 65535u ? 65535u : (uint32_t)htim->Instance->ARR * ioParam2[ioPinIdx] / 65534u;

		pwmLevel[ioPinIdx] = ioParam2[ioPinIdx];

		HAL_TIM_PWM_Start(htim, TIM_CHANNEL_1); // Start channel 1
	}
}

void IoNvReadConfig(uint8_t pin) {
	uint16_t var;
	EE_ReadVariable(IO_CONFIG1_NV_ADDR+(pin-1)*3, &var);
	if (((~var)&0xFF) != (var>>8)) {
		ioConfig[pin-1] = 0x80; // in case of nv write error, set to non configured
		return;
	} else {
		ioConfig[pin-1] = var & 0xFF;
	}
	EE_ReadVariable(IO_CONFIG1_PARAM1_NV_ADDR+(pin-1)*3, &ioParam1[pin-1]);
	EE_ReadVariable(IO_CONFIG1_PARAM2_NV_ADDR+(pin-1)*3, &ioParam2[pin-1]);
}

void IoControlInit() {
	if (!resetStatus) { // on mcu power up
		IoNvReadConfig(1);
		IoNvReadConfig(2);
	}
	IoConfigure(1);
	IoConfigure(2);
}

void IoSetConfiguarion(uint8_t pin, uint8_t data[], uint8_t len) {

	ioConfig[pin-1] = data[0];
	ioParam1[pin-1] = data[2];
	ioParam1[pin-1] <<= 8;
	ioParam1[pin-1] |= data[1];
	ioParam2[pin-1] = data[4];
	ioParam2[pin-1] <<= 8;
	ioParam2[pin-1] |= data[3];

	if (data[0]&0x80) {
		NvWriteVariableU8(IO_CONFIG1_NV_ADDR+(pin-1)*3, data[0]);
		EE_WriteVariable(IO_CONFIG1_PARAM1_NV_ADDR+(pin-1)*3, ioParam1[pin-1]);
		EE_WriteVariable(IO_CONFIG1_PARAM2_NV_ADDR+(pin-1)*3, ioParam2[pin-1]);

		IoNvReadConfig(pin);
	}

	IoConfigure(pin);
}

void IoGetConfiguarion(uint8_t pin, uint8_t data[], uint16_t *len) {
	data[0] = ioConfig[pin-1];
	data[1] = ioParam1[pin-1] & 0xFF;
	data[2] = (ioParam1[pin-1] >> 8) & 0xFF;
	data[3] = ioParam2[pin-1] & 0xFF;
	data[4] = (ioParam2[pin-1] >> 8) & 0xFF;
	*len = 5;
}


void IoWrite(const uint8_t extIOPinIdx, const uint8_t * const data, const uint16_t len)
{
	uint8_t iodrvPinIdx;
	const uint8_t ioPinIdx = extIOPinIdx - 1u;
	const uint8_t pinType = (ioConfig[ioPinIdx] & 0x0Fu);
	const uint16_t outVal = (uint16_t)data[0u] | (data[1] << 8u);

	TIM_HandleTypeDef *htim;

	if (len != 2u)
	{
		return;
	}

	if (extIOPinIdx == 1u)
	{
		iodrvPinIdx = IODRV_PIN_IO1;
		htim = &htim14;
	}
	else if (extIOPinIdx == 2u)
	{
		iodrvPinIdx = IODRV_PIN_IO2;
		htim = &htim1;
	}
	else
	{
		return;
	}

	switch (pinType)
	{
	case 3:
	case 4:
		IODRV_WritePin(iodrvPinIdx, (0u != (data[1u] & 0x01u)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
		break;

	case 5:
	case 6:
		htim->Instance->CCR1 = (outVal == 65535) ? 65535 : (uint32_t)htim->Instance->ARR * outVal / 65534u;
		pwmLevel[ioPinIdx] = outVal;

	default:
		break;
	}
}

void IoRead(const uint8_t extIOPinIdx, uint8_t * const data, uint16_t * const len)
{
	const uint8_t ioPinIdx = extIOPinIdx - 1u;
	const uint8_t pinType = (ioConfig[ioPinIdx] & 0x0Fu);
	uint8_t iodrvPinIdx;

	if (extIOPinIdx == 1u)
	{
		iodrvPinIdx = IODRV_PIN_IO1;
	}
	else if (extIOPinIdx == 2u)
	{
		iodrvPinIdx = IODRV_PIN_IO2;
	}
	else
	{
		return;
	}

	const uint16_t pinValue = IODRV_ReadPinValue(iodrvPinIdx);

	switch (pinType)
	{
	case IOTYPE_ANALOG:
		data[0u] = (uint8_t)(pinValue & 0xFFu);
		data[1u] = (uint8_t)((pinValue >> 8u) & 0xFFu);
		break;

	case IOTYPE_DIGIN:
		data[0u] = (uint8_t)pinValue;
		break;

	case IOTYPE_DIGOUT_PUSHPULL:
	case IOTYPE_DIGOUT_OPENDRAIN:
		data[0u] = (uint8_t)pinValue;
		data[1u] = IODRV_ReadPinOutputState(iodrvPinIdx);
		break;

	case IOTYPE_PWM_PUSHPULL:
	case IOTYPE_PWM_OPENDRAIN:
		data[0u] = (uint8_t)(pwmLevel[ioPinIdx] & 0xFFu);
		data[1u] = (uint8_t)((pwmLevel[ioPinIdx] >> 8u) & 0xFFu);
		break;

	default:
		break;
	}

	*len = 2u;
}
