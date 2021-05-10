/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdint.h>
#include <stdbool.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define WKUP_SW2_Pin GPIO_PIN_13
#define WKUP_SW2_GPIO_Port GPIOC
#define WKUP_SW2_EXTI_IRQn EXTI4_15_IRQn
#define CH_INT_Pin GPIO_PIN_0
#define CH_INT_GPIO_Port GPIOF
#define CH_INT_EXTI_IRQn EXTI0_1_IRQn
#define ESYSLIM_Pin GPIO_PIN_1
#define ESYSLIM_GPIO_Port GPIOF
#define CS1_Pin GPIO_PIN_0
#define CS1_GPIO_Port GPIOA
#define CS2_Pin GPIO_PIN_1
#define CS2_GPIO_Port GPIOA
#define BAT_SEN_Pin GPIO_PIN_2
#define BAT_SEN_GPIO_Port GPIOA
#define NTCR10_Pin GPIO_PIN_3
#define NTCR10_GPIO_Port GPIOA
#define POW_DET_SEN_Pin GPIO_PIN_4
#define POW_DET_SEN_GPIO_Port GPIOA
#define CHG_CUR_Pin GPIO_PIN_5
#define CHG_CUR_GPIO_Port GPIOA
#define TS_CTR1_Pin GPIO_PIN_6
#define TS_CTR1_GPIO_Port GPIOA
#define IO1_Pin GPIO_PIN_7
#define IO1_GPIO_Port GPIOA
#define LED2_B_Pin GPIO_PIN_0
#define LED2_B_GPIO_Port GPIOB
#define BGINT_Pin GPIO_PIN_1
#define BGINT_GPIO_Port GPIOB
#define SW3_Pin GPIO_PIN_2
#define SW3_GPIO_Port GPIOB
#define SW3_EXTI_IRQn EXTI2_3_IRQn
#define SW1_Pin GPIO_PIN_12
#define SW1_GPIO_Port GPIOB
#define SW1_EXTI_IRQn EXTI4_15_IRQn
#define RUN_Pin GPIO_PIN_13
#define RUN_GPIO_Port GPIOB
#define LED1_R_Pin GPIO_PIN_14
#define LED1_R_GPIO_Port GPIOB
#define LED1_G_Pin GPIO_PIN_15
#define LED1_G_GPIO_Port GPIOB
#define IO2_Pin GPIO_PIN_8
#define IO2_GPIO_Port GPIOA
#define BID_Pin GPIO_PIN_9
#define BID_GPIO_Port GPIOA
#define POW_EN_Pin GPIO_PIN_10
#define POW_EN_GPIO_Port GPIOA
#define POWDET_EN_Pin GPIO_PIN_11
#define POWDET_EN_GPIO_Port GPIOA
#define EXTVS_EN_Pin GPIO_PIN_12
#define EXTVS_EN_GPIO_Port GPIOA
#define TS_CTL2_Pin GPIO_PIN_15
#define TS_CTL2_GPIO_Port GPIOA
#define EE_A_Pin GPIO_PIN_3
#define EE_A_GPIO_Port GPIOB
#define LED2_R_Pin GPIO_PIN_4
#define LED2_R_GPIO_Port GPIOB
#define LED2_G_Pin GPIO_PIN_5
#define LED2_G_GPIO_Port GPIOB
#define EE_WP_Pin GPIO_PIN_8
#define EE_WP_GPIO_Port GPIOB
#define LED1_B_Pin GPIO_PIN_9
#define LED1_B_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
