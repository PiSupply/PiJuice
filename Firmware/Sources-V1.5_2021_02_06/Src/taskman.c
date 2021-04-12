// ----------------------------------------------------------------------------
/*!
 * @file		taskman.c
 * @author    	John Steggall, milan
 * @date       	19 March 2021
 * @details     Handles the application side of the system that isn't too dependent
 * 				on close synchronous actions. The power mode is switched here
 * 				where possible stopping the main process waiting for an external
 * 				event or RTC alarm. The stop mode is woken up after 4 seconds regardless
 * 				of no event occurring.
 *
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:
#include "main.h"

#include "nv.h"
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


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define TASKMAN_LOOP_TRACKER_COUNT		16u


typedef enum
{
	EXTI_EVENT_NONE = 0u,
	EXTI_EVENT_CHARGER = 1u,
	EXTI_EVENT_I2C = 2u,
	EXTI_EVENT_USER = 3u,
	EXTI_EVENT_IO2 = 4u,
} EXTI_EventStatus_t;


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void TASKMAN_WaitInterrupt(void);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint32_t m_taskmanLoopTimeTrack[TASKMAN_LOOP_TRACKER_COUNT];
static uint32_t m_taskloopTrackIdx = 0u;

static uint32_t m_lastSleepTimeMs = 0u;
static uint32_t m_lastTaskRunTimeMs;
static uint32_t m_lowPowerDelayTimer;

static TASKMAN_RunState_t m_runState;

static EXTI_EventStatus_t m_extiEvent = 0;
static bool m_ioWakeupEvent = false;


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern RTC_HandleTypeDef hrtc;


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * TASKMAN_Init calls the modules init routines
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void TASKMAN_Init(void)
{

	ISENSE_Init();
	BATTERY_Init();
	BUTTON_Init();
	CHARGER_Init();
	POWERSOURCE_Init();
	FUELGUAGE_Init();

	PowerManagementInit();

	BUTTON_Init();

	RtcInit();
	IoControlInit();

	// TODO - Correct spelling!
	NvSetDataInitialized();

	E2_Init();

	MS_TIME_COUNTER_INIT(m_lastTaskRunTimeMs);
	MS_TIME_COUNTER_INIT(m_lowPowerDelayTimer);

	m_runState = TASKMAN_RUNSTATE_NORMAL;
}


// ****************************************************************************
/*!
 * TASKMAN_Run is a non exiting loop, calling the task routines of the application
 * modules. unlike the osloop service routines, the task routines can block and
 * take as much time as they like without upsetting the background service routines.
 * The power mode is switched here, allowing the system to sleep or stop.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void TASKMAN_Run(void)
{
	bool powerManagerCanShutdown;
	bool rtcWakeEvent;
	uint32_t lastHostCommandAge;
	bool needEventPoll;
	uint32_t sysTime;
	uint32_t loopStartTIme;

	while(true)
	{
		sysTime = HAL_GetTick();

		powerManagerCanShutdown = POWERMAN_CanShutDown();
		lastHostCommandAge = HOSTCOMMS_GetLastCommandAgeMs(sysTime);
		rtcWakeEvent = RTC_GetWakeEvent();

		needEventPoll = CHARGER_RequirePoll()
							|| (EXTI_EVENT_NONE != m_extiEvent)
							|| lastHostCommandAge < 10u
							|| rtcWakeEvent
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
		if ( (MS_TIME_COUNT(m_lastTaskRunTimeMs) >= TASKMAN_TASK_PERIOD_MS) || needEventPoll )
		{
			MS_TIME_COUNTER_INIT(m_lastTaskRunTimeMs);

			loopStartTIme = TIMER_OSLOOP->CNT;

			CHARGER_Task();
			FUELGUAGE_Task();
			BATTERY_Task();
			POWERSOURCE_Task();

			RTC_EvaluateAlarm();

			BUTTON_Task();
			PowerManagementTask();

			HOSTCOMMS_Task();

			CHARGER_Task();

			m_taskmanLoopTimeTrack[m_taskloopTrackIdx] = (loopStartTIme - TIMER_OSLOOP->CNT);
		}
	}
}


// ****************************************************************************
/*!
 * TASKMAN_GetRunState gets the current running state of the system.
 *
 * @param	none
 * @retval	m_runState		current running state of the system
 */
// ****************************************************************************
TASKMAN_RunState_t TASKMAN_GetRunState(void)
{
	return m_runState;
}


// ****************************************************************************
/*!
 * TASKMAN_GetIOWakeEvent gets the state of the user configured IO wake event
 *
 * @param	none
 * @retval	bool			false = no user configured IO wake event occurred
 * 							true = user configured IO wake event occurred
 */
// ****************************************************************************
bool TASKMAN_GetIOWakeEvent(void)
{
	return m_ioWakeupEvent;
}


// ****************************************************************************
/*!
 * TASKMAN_ClearIOWakeEvent clears any user configured IO wake event
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void TASKMAN_ClearIOWakeEvent(void)
{
	m_ioWakeupEvent = false;
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

#ifdef LOWPOWER_NO_STOP
static volatile bool wakeupEvent;

void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
	wakeupEvent = true;
}

#endif


// ****************************************************************************
/*!
 * TASKMAN_WaitInterrupt determines the method of sleep mode to put the cpu in
 * and then waits for a configured interrupt event. If in low power mode the
 * processor will enter STOP mode for it's lowest power consumption. Configured
 * peripherals can wake up the device at any point or the RTC peripheral will
 * set a timer to wake up the device every 4 seconds by default, the osloop
 * and taskman will run their routines for a period defined or until any task
 * has been satisfied. On wake from STOP the system tick timer is adjusted for
 * the amount of time it has been suspended.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void TASKMAN_WaitInterrupt(void)
{
	// TODO - BAD!
	extern __IO uint32_t uwTick;

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

	    HAL_RTC_GetTime(&hrtc, &sleepTime_rtc, RTC_FORMAT_BIN);
	    (void)RTC->DR;

	    HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, TASKMAN_SLEEP_SETTING, RTC_WAKEUPCLOCK_RTCCLK_DIV16);

#ifdef LOWPOWER_NO_STOP

	    wakeupEvent = false;

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

		// Restart background tasks
	    OSLOOP_Restart();

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


// ****************************************************************************
/*!
 * HAL_GPIO_EXTI_Callback deals with the EXTI events, the HAL driver is fairly
 * low overhead, simply collating all exti events into one callback. Little point
 * in making it more efficient by hooking on the interrupt directly.
 *
 * @param	GPIO_Pin		Pin that caused the EXTI event.
 * @retval	none
 *
 * @note	The pin that gets passed does not have a defined GPIO even though
 * 			the events could be from random ports. STM have routed the EXTI events
 * 			such that they do not overlap on ports and are always be unique by
 * 			pin reference.
 */
// ****************************************************************************
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
