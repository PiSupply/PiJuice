/**
  ******************************************************************************
  * @file    stm32f0xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"
#include "stm32f0xx.h"
#include "stm32f0xx_it.h"

/* USER CODE BEGIN 0 */
extern void SysTickCb();
/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern ADC_HandleTypeDef hadc;
extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;
//extern SMBUS_HandleTypeDef hsmbus;
extern void I2C_EV_IRQHandler(I2C_HandleTypeDef *hi2c);

extern RTC_HandleTypeDef hrtc;
extern ADC_HandleTypeDef hadc;
//extern WWDG_HandleTypeDef hwwdg;
extern TIM_HandleTypeDef htim6;
/******************************************************************************/
/*            Cortex-M0 Processor Interruption and Exception Handlers         */
/******************************************************************************/

/**
* @brief This function handles Non maskable interrupt.
*/
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
* @brief This function handles Hard fault interrupt.
*/
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
  }
  /* USER CODE BEGIN HardFault_IRQn 1 */

  /* USER CODE END HardFault_IRQn 1 */
}
#if defined(RTOS_FREERTOS)
/**
  * @brief This function handles TIM6 global interrupt.
  */
void TIM6_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_IRQn 0 */

  /* USER CODE END TIM6_IRQn 0 */
  HAL_IncTick();
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_IRQn 1 */

  /* USER CODE END TIM6_IRQn 1 */
}
#else
/**
* @brief This function handles System service call via SWI instruction.
*/

void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
* @brief This function handles Pendable request for system service.
*/
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
* @brief This function handles System tick timer.
*/
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
  /* USER CODE BEGIN SysTick_IRQn 1 */
  //SysTickCb();
  /* USER CODE END SysTick_IRQn 1 */
}

#endif
/******************************************************************************/
/* STM32F0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f0xx.s).                    */
/******************************************************************************/
extern uint8_t i2cTrfBuffer[];
extern uint8_t newSmbusTransferFlag;
/* USER CODE BEGIN 1 */
/**
  * @brief  This function handles I2C event and error interrupt request.  
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to I2C data transmission     
  */
void I2C1_IRQHandler(void)
{
  /*if ((hsmbus.Instance->ISR & 0x40) && newSmbusTransferFlag) {
	HAL_SMBUS_Slave_Receive_IT(&hsmbus, (uint8_t *)i2cTrfBuffer, 255, SMBUS_FIRST_AND_LAST_FRAME_NO_PEC);
	newSmbusTransferFlag = 0;
  }*/
  //HAL_SMBUS_EV_IRQHandler(&hsmbus);
  //HAL_SMBUS_ER_IRQHandler(&hsmbus);
  //I2C_EV_IRQHandler(&hi2c1);
  HAL_I2C_EV_IRQHandler(&hi2c1);
  HAL_I2C_ER_IRQHandler(&hi2c1);
  //hi2c1.Instance->ICR = (uint32_t)0xFFFDF;//0x3FD0F;
  //hi2c1.Instance->CR1 &= (uint32_t)0x7F;//0x3FD0F;
 // if (hi2c1.Instance->ISR & 0x02) hi2c1.Instance->TXDR = 1;
}

void I2C2_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&hi2c2);
  HAL_I2C_ER_IRQHandler(&hi2c2);
}

/**
  * @brief  This function handles DMA interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to DMA Channel
  *         used for I2C data transmission
  */
void DMA1_Channel2_3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hi2c1.hdmarx);
  HAL_DMA_IRQHandler(hi2c1.hdmatx);
}

void DMA1_Channel4_5_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hi2c2.hdmarx);
  HAL_DMA_IRQHandler(hi2c2.hdmatx);
}

/**
* @brief  This function handles DMA1 Channel1 interrupt request.
* @param  None
* @retval None
*/
void DMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(hadc.DMA_Handle);
}

/**
  * @brief  This function handles RTC Alarm interrupt request.
  * @param  None
  * @retval None
  */
void RTC_IRQHandler(void)
{
  HAL_RTC_AlarmIRQHandler(&hrtc);
  HAL_RTCEx_WakeUpTimerIRQHandler(&hrtc);
}

void ADC1_IRQHandler(void)
{
	HAL_ADC_IRQHandler(&hadc);
}

/**
  * @brief  This function handles external line 0 interrupt request.
  * @param  None
  * @retval None
  */
void EXTI4_15_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_7);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_12);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_13);
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_8); // IO2
}

void EXTI0_1_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_0);
}

void EXTI2_3_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}

/*void WWDG_IRQHandler(void)
{
	HAL_WWDG_IRQHandler(&hwwdg);
}*/


/* USER CODE END 1 */
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
