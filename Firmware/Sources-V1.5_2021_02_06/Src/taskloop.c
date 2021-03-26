#include "main.h"

#include "time_count.h"

static uint32_t m_lastTaskloopOpTime = 0u;

void TASKLOOP_Init(void)
{

}


void TASKLOOP_Run(void)
{
	const uint32_t loopStartTIme = HAL_GetTick();



	m_lastTaskloopOpTime = MS_TIMEREF_DIFF(loopStartTIme, HAL_GetTick());
}
