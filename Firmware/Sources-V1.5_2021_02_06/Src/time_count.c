/*
 * time_count.c
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */
#include "time_count.h"
#include "stm32f0xx_hal.h"
#include "led.h"

//uint32_t ticks[TIME_COUNTERS_MAX];
//static uint8_t ticksNum = 0;
__IO uint32_t msTickCnt __attribute__((section("no_init")));

#if defined(RTOS_FREERTOS)

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef        htim6;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  This function configures the TIM6 as a time base source.
  *         The time source is configured  to have 1ms time base with a dedicated
  *         Tick interrupt priority.
  * @note   This function is called  automatically at the beginning of program after
  *         reset by HAL_Init() or at any time when clock is configured, by HAL_RCC_ClockConfig().
  * @param  TickPriority: Tick interrupt priority.
  * @retval HAL status
  */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  RCC_ClkInitTypeDef    clkconfig;
  uint32_t              uwTimclock = 0;
  uint32_t              uwPrescalerValue = 0;
  uint32_t              pFLatency;

  /*Configure the TIM6 IRQ priority */
  HAL_NVIC_SetPriority(TIM6_IRQn, TickPriority ,0);

  /* Enable the TIM6 global Interrupt */
  HAL_NVIC_EnableIRQ(TIM6_IRQn);

  /* Enable TIM6 clock */
  __HAL_RCC_TIM6_CLK_ENABLE();

  /* Get clock configuration */
  HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

  /* Compute TIM6 clock */
  uwTimclock = HAL_RCC_GetPCLK1Freq();

  /* Compute the prescaler value to have TIM6 counter clock equal to 1MHz */
  uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000) - 1);

  /* Initialize TIM6 */
  htim6.Instance = TIM6;

  /* Initialize TIMx peripheral as follow:
  + Period = [(TIM6CLK/1000) - 1]. to have a (1/1000) s time base.
  + Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
  + ClockDivision = 0
  + Counter direction = Up
  */
  htim6.Init.Period =(1000000 / 1000) - 1; // (20000000 / 1000) - 1;//
  htim6.Init.Prescaler = uwPrescalerValue;
  htim6.Init.ClockDivision = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  if(HAL_TIM_Base_Init(&htim6) == HAL_OK)
  {
    /* Start the TIM time Base generation in interrupt mode */
    return HAL_TIM_Base_Start_IT(&htim6);
  }

  /* Return function status */
  return HAL_ERROR;
}

/**
  * @brief  Suspend Tick increment.
  * @note   Disable the tick increment by disabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_SuspendTick(void)
{
  /* Disable TIM6 update Interrupt */
  __HAL_TIM_DISABLE_IT(&htim6, TIM_IT_UPDATE);
}

/**
  * @brief  Resume Tick increment.
  * @note   Enable the tick increment by Enabling TIM6 update interrupt.
  * @param  None
  * @retval None
  */
void HAL_ResumeTick(void)
{
  /* Enable TIM6 Update interrupt */
  __HAL_TIM_ENABLE_IT(&htim6, TIM_IT_UPDATE);
}
#else //if defined(RTOS_NORTOS)

#ifdef USE_20MS_TICK_PERIOD
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
  /*Configure the SysTick to have interrupt in 1ms time basis*/
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/(1000/TICK_PERIOD_MS));

  /*Configure the SysTick IRQ priority */
  HAL_NVIC_SetPriority(SysTick_IRQn, TickPriority ,1);

   /* Return function status */
  return HAL_OK;
}


void HAL_IncTick(void)
{
	msTickCnt += TICK_PERIOD_MS;
}

uint32_t HAL_GetTick(void)
{
  return msTickCnt;
}
#endif /* USE_20MS_TICK_PERIOD */
#endif
void HAL_Delay(__IO uint32_t Delay)
{
	DelayUs(Delay*1000);
}

/*int8_t AddTimeCounter() {
	if (ticksNum >= TIME_COUNTERS_MAX) return -1;
	ticks[ticksNum] = 0;
	ticksNum ++;
	return ticksNum - 1;
}*/

void TimeTickCb(uint16_t periodMs) {
	/*uint8_t i;
	for (i = 0; i < ticksNum; i++) {
		ticks[i] += periodMs;
	}*/
	msTickCnt += periodMs;
}

