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
