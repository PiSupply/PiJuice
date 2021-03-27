#include "main.h"

#include "charger_bq2416x.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"

static uint32_t m_lastTaskloopOpTime = 0u;

void TASKLOOP_Init(void)
{
	CHARGER_Init();
	FUELGUAGE_Init();
	BATTERY_Init();
}


void TASKLOOP_Run(void)
{
	const uint32_t loopStartTIme = HAL_GetTick();

	CHARGER_Task();
	FUELGUAGE_Task();

	m_lastTaskloopOpTime = MS_TIMEREF_DIFF(loopStartTIme, HAL_GetTick());
}
