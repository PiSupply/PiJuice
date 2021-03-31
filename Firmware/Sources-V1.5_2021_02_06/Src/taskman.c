#include "main.h"
#include "eeprom.h"

#include "system_conf.h"
#include "iodrv.h"
#include "crc.h"

#include "execution.h"

#include "charger_bq2416x.h"
#include "fuel_gauge_lc709203f.h"
#include "power_source.h"
#include "command_server.h"
#include "led.h"
#include "button.h"
#include "analog.h"
#include "time_count.h"
#include "load_current_sense.h"
#include "rtc_ds1339_emu.h"
#include "power_management.h"
#include "io_control.h"
#include "execution.h"
#include "e2.h"
#include "osloop.h"
#include "taskman.h"
#include "adc.h"
#include "i2cdrv.h"
#include "util.h"
#include "hostcomms.h"


typedef enum
{
	EXTI_EVENT_NONE = 0u,
	EXTI_EVENT_CHARGER = 1u,
	EXTI_EVENT_I2C = 2u,
	EXTI_EVENT_USER = 3u,
	EXTI_EVENT_IO2 = 4u,
} EXTI_EventStatus_t;


static uint32_t m_lastTaskloopOpTimeUs = 0u;
static uint32_t m_lastSleepTimeMs = 0u;
static uint32_t m_lastTaskRunTimeMs;
static uint32_t m_lowPowerDelayTimer;

static TASKMAN_RunState_t m_runState;


static EXTI_EventStatus_t m_extiEvent = 0;
static bool m_ioWakeupEvent = false;


extern RTC_HandleTypeDef hrtc;


void TASKMAN_WaitInterrupt(void);


void TASKMAN_Init(void)
{
	LoadCurrentSenseInit();

	BATTERY_Init();
	BUTTON_Init();
	CHARGER_Init();
	POWERSOURCE_Init();
	FUELGUAGE_Init();

	PowerManagementInit();
	LedInit();

	BUTTON_Init();

	RtcInit();
	IoControlInit();

	NvSetDataInitialized();

	E2_Init();

	MS_TIME_COUNTER_INIT(m_lastTaskRunTimeMs);
	MS_TIME_COUNTER_INIT(m_lowPowerDelayTimer);

	HOSTCOMMS_Init();

	m_runState = TASKMAN_RUNSTATE_NORMAL;
}


void TASKMAN_Run(void)
{
	bool powerManagerCanShutdown;
	bool hostCommandActive;
	uint32_t lastHostCommandAge;
	bool needEventPoll;
	uint32_t sysTime;

	// Just starts the listener
	HOSTCOMMS_Task();

	while(true)
	{
		sysTime = HAL_GetTick();

		powerManagerCanShutdown = POWERMAN_CanShutDown();
		hostCommandActive = HOSTCOMMS_IsCommandActive();
		lastHostCommandAge = HOSTCOMMS_GetLastCommandAgeMs(sysTime);

		needEventPoll = CHARGER_RequirePoll()
							|| (EXTI_EVENT_NONE != m_extiEvent)
							|| rtcWakeupEventFlag
							|| hostCommandActive
							|| POWERSOURCE_NeedPoll()
							|| RTC_GetAlarmState();

		if (false == needEventPoll)
		{
			if ( /*(
					(ANALOG_Get5VRailMa() <= 50) ||
					( (ANALOG_Get5VRailMv() < 4600u) && IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN)) ) &&*/
					(lastHostCommandAge > 5000u) &&
					MS_TIMEREF_TIMEOUT(m_lowPowerDelayTimer, sysTime, 22u) &&
					(true == powerManagerCanShutdown) &&
					(CHG_NO_VALID_SOURCE == CHARGER_GetStatus()) &&
					(false == BUTTON_IsButtonActive())
					)
			{

				m_runState = TASKMAN_RUNSTATE_LOW_POWER;
			}
			else
			{
				m_runState = TASKMAN_RUNSTATE_SLEEP;
			}

			TASKMAN_WaitInterrupt();

			m_runState = TASKMAN_RUNSTATE_NORMAL;
		}


		if (EXTI_EVENT_I2C == m_extiEvent)
		{
			HOSTCOMMS_SetInterrupt();
		}

		m_extiEvent = EXTI_EVENT_NONE;


		// Do not disturb i2c transfer if this is i2c interrupt wakeup
		if (MS_TIME_COUNT(m_lastTaskRunTimeMs) >= TICK_PERIOD_MS || needEventPoll)
		{
			MS_TIME_COUNTER_INIT(m_lastTaskRunTimeMs);

			const uint32_t loopStartTIme = TIMER_OSLOOP->CNT;

			CHARGER_Task();
			FUELGUAGE_Task();
			BATTERY_Task();
			POWERSOURCE_Task();

			RTC_EvaluateAlarm();

			LedTask();

			BUTTON_Task();
			LoadCurrentSenseTask();
			PowerManagementTask();

			CHARGER_Task();

			m_lastTaskloopOpTimeUs = MS_TIMEREF_DIFF(loopStartTIme,TIMER_OSLOOP->CNT);
		}
	}


}

TASKMAN_RunState_t TASKMAN_GetRunState(void)
{
	return m_runState;
}


#ifdef DEBUG
static volatile bool wakeupEvent;

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
	wakeupEvent = true;
}

#endif


void TASKMAN_WaitInterrupt(void)
{
	extern __IO uint32_t uwTick;
	static GPIO_InitTypeDef i2c_GPIO_InitStruct;

	RTC_TimeTypeDef sleepTime_rtc, wakeTime_rtc;
	uint32_t sleepTime, wakeTime;
	uint8_t tempU8;

	if (TASKMAN_RUNSTATE_LOW_POWER == m_runState)
	{
		HAL_SuspendTick();

		// Stop the background tasks
		OSLOOP_Shutdown();

		// Stop the led module
		LedStop();

		// Enable wake up from host
		i2c_GPIO_InitStruct.Pin = GPIO_PIN_7;
		i2c_GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
		i2c_GPIO_InitStruct.Pull = GPIO_NOPULL;
	    HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);

	    HAL_RTC_GetTime(&hrtc, &sleepTime_rtc, RTC_FORMAT_BIN);
	    (void)RTC->DR;

	    wakeupEvent = false;

	    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 8192ul, RTC_WAKEUPCLOCK_RTCCLK_DIV16);

#ifdef DEBUG

	    while( (false == wakeupEvent) && (EXTI_EVENT_NONE == m_extiEvent) )
	    {
	    	// Wait for the rtc to do its thing
	    }

#else
	    // Shutdown
		HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
#endif

		HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

	    HAL_RTC_GetTime(&hrtc, &wakeTime_rtc, RTC_FORMAT_BIN);
	    (void)RTC->DR;

	    sleepTime = (sleepTime_rtc.Hours * 3600) + (sleepTime_rtc.Minutes * 60) + (sleepTime_rtc.Seconds);
	    wakeTime = (wakeTime_rtc.Hours * 3600) + (wakeTime_rtc.Minutes * 60) + (wakeTime_rtc.Seconds);
	    m_lastSleepTimeMs = (wakeTime - sleepTime) * 1000u;

	    tempU8 = (uint8_t)(wakeTime_rtc.SubSeconds & 0xFFu);
	    tempU8 -= (uint8_t)(sleepTime_rtc.SubSeconds & 0xFFu);

	    // Add on the ms from the subseconds timer
	    m_lastSleepTimeMs += UTIL_FixMul_U32_U16(257003ul, tempU8);

	    uwTick += m_lastSleepTimeMs;

	    HAL_ResumeTick();

		// Revert GPIOB.7 back to i2c mode
		i2c_GPIO_InitStruct.Pin       = GPIO_PIN_7;
		i2c_GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
		i2c_GPIO_InitStruct.Pull      = GPIO_NOPULL;//GPIO_PULLUP;
		i2c_GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
		i2c_GPIO_InitStruct.Alternate = GPIO_AF1_I2C1;
		HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);


		// Restart background tasks
		OSLOOP_Init();

		while(false == ADC_GetFilterReady())
		{
			// Wait for ADC to become ready
		}

		// Restart LED module
		LedStart();

		MS_TIME_COUNTER_INIT(m_lowPowerDelayTimer);

	}
	else if (TASKMAN_RUNSTATE_SLEEP == m_runState)
	{
		HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	}
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_0)
	{
		// CH_INT
		CHARGER_SetInterrupt();
		m_extiEvent = EXTI_EVENT_CHARGER;
	}
	else if (GPIO_Pin == GPIO_PIN_7)
	{
		// I2C SDA
		m_extiEvent = EXTI_EVENT_I2C;
	}
	else if (GPIO_Pin == GPIO_PIN_8)
	{
		m_extiEvent = EXTI_EVENT_IO2;
		m_ioWakeupEvent = true;
	}
	else
	{
		// SW1, SW2, SW3
		m_extiEvent = EXTI_EVENT_USER;
	}
}
