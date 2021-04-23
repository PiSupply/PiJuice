/*
 * power_management.c
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"

#include "nv.h"

#include "charger_bq2416x.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"
#include "power_source.h"
#include "button.h"
#include "io_control.h"
#include "led.h"
#include "iodrv.h"
#include "hostcomms.h"
#include "execution.h"
#include "rtc_ds1339_emu.h"
#include "taskman.h"
#include "util.h"

#include "power_management.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define WAKEUPONCHARGE_NV_INITIALISED	(0u != (m_wakeupOnChargeConfig & 0x80u))
#define WAKEUP_ONCHARGE_DISABLED_VAL	0xFFFFu
#define RPI_ACTLED_IO_PIN				1u
#define GPIO_POWERCONTROL				1u


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static bool POWERMAN_ResetHost(void);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint32_t m_powerMngmtTaskMsCounter;
static RunPinInstallationStatus_T m_runPinInstalled = RUN_PIN_NOT_INSTALLED;


// ----------------------------------------------------------------------------
// Variables that only have scope in this module that are persistent through reset:

static uint8_t m_wakeupOnChargeConfig __attribute__((section("no_init")));
static uint16_t m_wakeUpOnCharge __attribute__((section("no_init"))); // 0 - 1000, 0xFFFF - disabled
static bool m_delayedTurnOnFlag __attribute__((section("no_init")));
static uint32_t m_delayedTurnOnTimer __attribute__((section("no_init")));
static uint32_t m_delayedPowerOffTimeMs __attribute__((section("no_init")));
static uint16_t m_watchdogConfig __attribute__((section("no_init")));
static uint32_t m_watchdogExpirePeriod __attribute__((section("no_init"))); // 0 - disabled, 1-255 expiration time minutes
static uint32_t m_watchdogTimer __attribute__((section("no_init")));
static bool m_watchdogExpired __attribute__((section("no_init")));
static bool m_powerButtonPressedEvent __attribute__((section("no_init")));
static uint32_t m_lastWakeupTimer __attribute__((section("no_init")));


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// CALLBACK HANDLERS
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * BUTTON_PowerOnEventCb
 *
 * @param	p_button		pointer to button that initiated the event
 * @retval	none
 */
// ****************************************************************************
void BUTTON_PowerOnEventCb(const Button_T * const p_button)
{
	const uint32_t sysTime = HAL_GetTick();

	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();
	const uint32_t lastHostCommandAgeMs = HOSTCOMMS_GetLastCommandAgeMs(sysTime);

	if ( ( (false == boostConverterEnabled) && (POW_SOURCE_NOT_PRESENT == power5vIoStatus) ) ||
			( MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 12000u) && (lastHostCommandAgeMs > 11000) )
			)
	{

		if (true == POWERMAN_ResetHost())
		{
			m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
			RTC_ClearWakeEvent();
			TASKMAN_ClearIOWakeEvent();
			m_delayedPowerOffTimeMs = 0u;
		}
	}

	BUTTON_ClearEvent(p_button->index);

}


// ****************************************************************************
/*!
 * BUTTON_PowerOffEventCb
 *
 * @param	p_button		pointer to button that initiated the event
 * @retval	none
 */
// ****************************************************************************
void BUTTON_PowerOffEventCb(const Button_T * const p_button)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();

	if ( (true == boostConverterEnabled) && (POW_SOURCE_NOT_PRESENT == power5vIoStatus) )
	{
		POWERSOURCE_Set5vBoostEnable(false);
		m_powerButtonPressedEvent = true;
	}

	BUTTON_ClearEvent(p_button->index); // remove event
}


// ****************************************************************************
/*!
 * BUTTON_PowerResetEventCb
 *
 * @param	p_button		pointer to button that initiated the event
 * @retval	none
 */
// ****************************************************************************
void BUTTON_PowerResetEventCb(const Button_T * const p_button)
{
	if (true == POWERMAN_ResetHost())
	{
		m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;

		RTC_ClearWakeEvent();
		TASKMAN_ClearIOWakeEvent();

		m_delayedPowerOffTimeMs = 0u;
	}

	BUTTON_ClearEvent(p_button->index);
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * POWERMAN_Init configures the module to a known initial state
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_Init(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const bool noBatteryTurnOn = CHARGER_GetNoBatteryTurnOnEnable();
	const bool chargerHasInput = CHARGER_IsChargeSourceAvailable();
	uint8_t tempU8;
	bool nvOk;

	// If not programmed will default to not installed
	NV_ReadVariable_U8(NV_RUN_PIN_CONFIG, (uint8_t*)&m_runPinInstalled);

	if (EXECUTION_STATE_NORMAL != executionState)
	{
		// on mcu power up
		m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
		m_wakeupOnChargeConfig = 0x7Fu;

		if (NV_ReadVariable_U8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (uint8_t*)&m_wakeupOnChargeConfig))
		{
			if (m_wakeupOnChargeConfig <= 100u)
			{
				/* Set last bit in config value to show EE value is valid. */
				m_wakeupOnChargeConfig |= 0x80u;
			}
		}

		if (true == chargerHasInput)
		{	/* Charger is connected */
			m_delayedTurnOnFlag = noBatteryTurnOn;
		}
		else
		{	/* Charger is not connected */
			m_delayedTurnOnFlag = false;

			if (WAKEUPONCHARGE_NV_INITIALISED)
			{
				m_wakeUpOnCharge = (m_wakeupOnChargeConfig & 0x7Fu) <= 100u ?
										(m_wakeupOnChargeConfig & 0x7Fu) * 10u :
										WAKEUP_ONCHARGE_DISABLED_VAL;
			}
		}

		nvOk = true;

		nvOk &= NV_ReadVariable_U8(WATCHDOG_CONFIGL_NV_ADDR, &tempU8);
		m_watchdogConfig = tempU8;

		nvOk &= NV_ReadVariable_U8(WATCHDOG_CONFIGH_NV_ADDR, &tempU8);

		if (true == nvOk)
		{
			m_watchdogConfig |= (uint16_t)tempU8 << 8u;
		}
		else
		{
			m_watchdogConfig = 0u;
		}


		m_delayedPowerOffTimeMs = 0u;
		m_watchdogExpirePeriod = 0u;
		m_watchdogTimer = 0u;
		m_watchdogExpired = false;

		RTC_ClearWakeEvent();
		TASKMAN_ClearIOWakeEvent();
		m_powerButtonPressedEvent = false;
	}

	MS_TIMEREF_INIT(m_powerMngmtTaskMsCounter, sysTime);
}


// ****************************************************************************
/*!
 * POWERMAN_Task performs periodic updates for this module
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const bool chargerHasInput = CHARGER_IsChargeSourceAvailable();
	const bool chargerHasBattery = (CHARGER_BATTERY_NOT_PRESENT != CHARGER_GetBatteryStatus());
	const uint16_t batteryRsoc = FUELGAUGE_GetSocPt1();
	const POWERSOURCE_RPi5VStatus_t pow5vInDetStatus = POWERSOURCE_GetRPi5VPowerStatus();
	const uint32_t lastHostCommandAgeMs = HOSTCOMMS_GetLastCommandAgeMs(sysTime);
	const bool rtcWakeEvent = RTC_GetWakeEvent();
	const bool ioWakeEvent = TASKMAN_GetIOWakeEvent();
	const IODRV_Pin_t * p_gpio26 = IODRV_GetPinInfo(IODRV_PIN_IO1);

	bool isWakeupOnCharge;

	if (MS_TIMEREF_TIMEOUT(m_powerMngmtTaskMsCounter, sysTime, POWERMANAGE_TASK_PERIOD_MS))
	{
		MS_TIMEREF_INIT(m_powerMngmtTaskMsCounter, sysTime);

		isWakeupOnCharge = (batteryRsoc >= m_wakeUpOnCharge) && (true == chargerHasInput) && (true == chargerHasBattery);

		if ( ( isWakeupOnCharge || rtcWakeEvent || ioWakeEvent ) // there is wake-up trigger
				&& 	(0u == m_delayedPowerOffTimeMs) // deny wake-up during shutdown
				&& 	(false == m_delayedTurnOnFlag)
				&& 	( (lastHostCommandAgeMs >= 15000u) && MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 30000u) )
						//|| (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) //  Host is non powered
		   	   	)
		{
			if (true == POWERMAN_ResetHost())
			{
				m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				RTC_ClearWakeEvent();
				TASKMAN_ClearIOWakeEvent();
				m_delayedPowerOffTimeMs = 0u;

				if (0u != m_watchdogConfig)
				{
					// activate watchdog after wake-up if watchdog config has restore flag
					m_watchdogExpirePeriod = (uint32_t)m_watchdogConfig * 60000u;
					m_watchdogTimer = m_watchdogExpirePeriod;
				}
			}
		}


		if ( (0u !=  m_watchdogExpirePeriod) && (lastHostCommandAgeMs > m_watchdogTimer) )
		{
			if (true == POWERMAN_ResetHost())
			{
				m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				m_watchdogExpired = true;
				RTC_ClearWakeEvent();
				TASKMAN_ClearIOWakeEvent();
				m_delayedPowerOffTimeMs = 0u;
			}

			m_watchdogTimer += m_watchdogExpirePeriod;
		}
	}

	if ( m_delayedTurnOnFlag && (MS_TIMEREF_TIMEOUT(m_delayedTurnOnTimer, sysTime, 100u)) )
	{
		POWERSOURCE_Set5vBoostEnable(true);
		m_delayedTurnOnFlag = false;

		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
	}

	// Time is set in the future, so need to work in int32 or it'll roll over
	if (0u != m_delayedPowerOffTimeMs)
	{
		if ( ((int32_t)MS_TIMEREF_DIFF(m_delayedPowerOffTimeMs, sysTime) > 0))
		{
			if (RPI5V_DETECTION_STATUS_POWERED != pow5vInDetStatus)
			{
				POWERSOURCE_Set5vBoostEnable(false);
			}

			// Disable timer
			m_delayedPowerOffTimeMs = 0u;

			/* Turn off led as it keeps flashing! */
			LED_SetRGB(LED_LED2_IDX, 0u, 0u, 0u);
		}
	}

	if ( (false == chargerHasInput) &&
			(WAKEUP_ONCHARGE_DISABLED_VAL == m_wakeUpOnCharge) &&
			(WAKEUPONCHARGE_NV_INITIALISED)
			)
	{
		// setup wake-up on charge if charging stopped, power source removed
		m_wakeUpOnCharge = (m_wakeupOnChargeConfig & 0x7F) <= 100u ? (m_wakeupOnChargeConfig & 0x7Fu) * 10u :
																	WAKEUP_ONCHARGE_DISABLED_VAL;
	}
}


// ****************************************************************************
/*!
 * POWERMAN_SchedulePowerOff sets a timer for the module task to monitor and turn
 * off the boost converter when timed out. The timer is set to a future system
 * time to compare with the tick timer. The delay code is in seconds resolution
 *
 * @param	delayCode		seconds to wait before turning off the boost converter
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_SchedulePowerOff(const uint8_t delayCode)
{
	if (delayCode <= 250u)
	{
		MS_TIMEREF_INIT(m_delayedPowerOffTimeMs, HAL_GetTick() + (delayCode * 1000u));

		if (m_delayedPowerOffTimeMs == 0u)
		{
			m_delayedPowerOffTimeMs = 1u; // 0 is used to indicate non active counter, so avoid that value.
		}
	}
	else if (delayCode == 0xFFu)
	{
		m_delayedPowerOffTimeMs = 0u; // deactivate scheduled power off
	}
}


// ****************************************************************************
/*!
 * POWERMAN_GetPowerOffTime returns the time in seconds left before the system
 * turns off the power to the RPi.
 *
 * @param	none
 * @retval	uint8_t		time in seconds before the boost converter is disabled.
 */
// ****************************************************************************
uint8_t POWERMAN_GetPowerOffTime(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const int32_t timeLeft = -MS_TIMEREF_DIFF(m_delayedPowerOffTimeMs, sysTime);

	if (0u != m_delayedPowerOffTimeMs)
	{
		if ( (timeLeft > 0) && (timeLeft < (UINT8_MAX * 1000u)) )
		{
			return (uint8_t)(timeLeft / 1000u);
		}
		else
		{
			return 0u;
		}
	}
	else
	{
		return 0xFFu;
	}
}


// ****************************************************************************
/*!
 * POWERMAN_GetPowerButtonPressedStatus returns true if the power button callback
 * was run though the power off button event callback.
 * Will remain true until it is cleared.
 *
 * @param	none
 * @retval	bool		false = power off event has not been initiated
 * 						true = power off event has been initiated.
 */
// ****************************************************************************
bool POWERMAN_GetPowerButtonPressedStatus(void)
{
	return m_powerButtonPressedEvent;
}


// ****************************************************************************
/*!
 * POWERMAN_GetWatchdogExpired returns true if the watchdog time has expired.
 * Will remain true until cleared.
 *
 * @param	none
 * @retval	bool		false = watchdog timer has not expired
 * 						true = watchdog timer has expired
 */
// ****************************************************************************
bool POWERMAN_GetWatchdogExpired(void)
{
	return m_watchdogExpired;
}


// ****************************************************************************
/*!
 * POWERMAN_ClearWatchdog clears the watchdog expired flag if set.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_ClearWatchdog(void)
{
	m_watchdogExpired = false;
}


// ****************************************************************************
/*!
 * POWERMAN_SetRunPinConfigData writes the run pin installation config to NV memory
 * and sets current configuration
 *
 * @param	p_data		pointer to config data
 * @param	len			length of config data
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_SetRunPinConfigData(const uint8_t * const p_data, const uint8_t len)
{
	if (p_data[0u] > 1u)
	{
		return;
	}

	NV_WriteVariable_U8(NV_RUN_PIN_CONFIG, p_data[0u]);

	if (false == NV_ReadVariable_U8(NV_RUN_PIN_CONFIG, (uint8_t*)&m_runPinInstalled))
	{
		m_runPinInstalled = RUN_PIN_NOT_INSTALLED;
	}
}


// ****************************************************************************
/*!
 * POWERMAN_GetRunPinConfigData populates a data buffer with the run pin installation
 * configuration.
 *
 * @param	p_data		pointer to destination buffer
 * @param	p_len		length of data written to the buffer
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_GetRunPinConfigData(uint8_t * const p_data, uint16_t * const p_len)
{
	p_data[0u] = m_runPinInstalled;
	*p_len = 1u;
}


// ****************************************************************************
/*!
 * POWERMAN_SetWatchdogConfigData writes the watchdog timer configuration.
 * if the watchdog timer is 16384 seconds or more then the resolution is adjusted
 * to 4 times the value written. To make the setting persistent though power cycles
 * set bit 15 (or'd with 0x8000u) else the value passed is used to only set the
 * current timeout period.
 *
 * @param	p_data		pointer to config data
 * @param	len			length of config data
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_SetWatchdogConfigData(const uint8_t * const p_data, const uint16_t len)
{
	uint32_t cfg;
	uint8_t tempU8;
	bool nvOk;

	if (len < 2u)
	{
		return;
	}

	cfg = UTIL_FromBytes_U16(&p_data[0u]) & 0x3FFFu;

	// Drop resolution if time is >= 16384 seconds
	if (0u != (p_data[1u] & 0x40u))
	{
		cfg >>= 2u;
		cfg |= 0x4000u;
	}

	if (p_data[1u] & 0x80)
	{
		m_watchdogConfig = cfg;
		NV_WriteVariable_U8(WATCHDOG_CONFIGL_NV_ADDR, (uint8_t)(m_watchdogConfig & 0xFFu));
		NV_WriteVariable_U8(WATCHDOG_CONFIGH_NV_ADDR, (uint8_t)((m_watchdogConfig >> 8u) & 0xFFu));

		nvOk = true;

		nvOk &= NV_ReadVariable_U8(WATCHDOG_CONFIGL_NV_ADDR, &tempU8);
		m_watchdogConfig = tempU8;

		nvOk &= NV_ReadVariable_U8(WATCHDOG_CONFIGH_NV_ADDR, &tempU8);

		if (false == nvOk)
		{
			m_watchdogConfig = 0u;
		}

		if (0u == m_watchdogConfig)
		{
			m_watchdogExpirePeriod = 0u;
			m_watchdogTimer = 0u;
		}
	}
	else
	{
		m_watchdogExpirePeriod = cfg * 60000u;
		m_watchdogTimer = m_watchdogExpirePeriod;
	}

}


// ****************************************************************************
/*!
 * POWERMAN_GetWatchdogConfigData populates a buffer with the watchdog configuration
 * if it has been written to NV memory or the watchdog expiry period as previously
 * set. If the value is greater than 16384 seconds then the resolution is dropped
 * and the value is four times that set.
 *
 * @param	p_data		pointer to destination buffer
 * @param	p_len		length of data written to the buffer
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_GetWatchdogConfigData(uint8_t * const p_data, uint16_t * const p_len)
{
	uint16_t d;

	if (0u != m_watchdogConfig)
	{
		d = m_watchdogConfig;

		if (0u != (d & 0x4000u))
		{
			d = (d >> 2u) | 0x4000u;
		}

		d |= 0x8000u;
	}
	else
	{
		d = m_watchdogExpirePeriod / 60000u;

		if (0u != (d & 0x4000u))
		{
			 d = (d >> 2u) | 0x4000u;
		}
	}

	UTIL_ToBytes_U16(d, &p_data[0u]);

	*p_len = 2;
}


// ****************************************************************************
/*!
 * POWERMAN_SetWakeupOnChargeData
 *
 * @param	p_data		pointer to config data
 * @param	len			length of config data
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_SetWakeupOnChargeData(const uint8_t * const p_data, const uint16_t len)
{
	uint16_t newWakeupVal = (p_data[0u] & 0x7Fu) * 10u;

	if (newWakeupVal > 1000u)
	{
		newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
	}

	if (p_data[0u] & 0x80u)
	{
		NV_WriteVariable_U8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (p_data[0u] & 0x7Fu) <= 100u ? p_data[0u] : 0x7Fu);

		if (false == NV_ReadVariable_U8(WAKEUPONCHARGE_CONFIG_NV_ADDR, &m_wakeupOnChargeConfig))
		{
			/* If writing to NV failed then disable wakeup on charge */
			m_wakeupOnChargeConfig = 0x7Fu;
		}

		if (0x7Fu == m_wakeupOnChargeConfig)
		{
			newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
		}
	}

	m_wakeUpOnCharge = newWakeupVal;
}


// ****************************************************************************
/*!
 * POWERMAN_GetWakeupOnChargeData
 *
 * @param	p_data		pointer to destination buffer
 * @param	p_len		length of data written to the buffer
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_GetWakeupOnChargeData(uint8_t * const p_data, uint16_t * const p_len)
{
	if (WAKEUPONCHARGE_NV_INITIALISED)
	{
		p_data[0u] = m_wakeupOnChargeConfig;
	}
	else
	{
		p_data[0u] = (m_wakeUpOnCharge <= 1000) ? m_wakeUpOnCharge / 10u : 100u;
	}

	*p_len = 1u;
}


// ****************************************************************************
/*!
 * POWERMAN_CanShutDown tells the caller if there are tasks being performed by
 * the module and effectively not to put the processor in STOP mode.
 *
 * @param	none
 * @retval	bool		false = module busy and not convenient to stop CPU
 * 						true = can stop CPU
 */
// ****************************************************************************
bool POWERMAN_CanShutDown(void)
{
	const uint32_t sysTime = HAL_GetTick();

	// Make sure pijuice does not sleep if starting up or just about to shutdown
	return (MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 5000u)) && (m_delayedPowerOffTimeMs == 0u);
}


// ****************************************************************************
/*!
 * POWERMAN_ClearPowerButtonPressed resets the power button pressed flag
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_ClearPowerButtonPressed(void)
{
	m_powerButtonPressedEvent = false;
}


// ****************************************************************************
/*!
 * POWERMAN_SetWakeupOnChargePt1 sets the wake up on charge charge trigger level,
 * if the RPi has powered down then setting this level will power it back up again
 * once the level has been met or exceeded. Setting to minimum will simply wake
 * it back up when the external VIn charge source is connected.
 *
 * @param	chargeTriggerPcntPt1	charge percentage in 0.1% steps to power up
 * 									the RPi.
 * @retval	none
 */
// ****************************************************************************
void POWERMAN_SetWakeupOnChargePcntPt1(const uint16_t chargeTriggerPcntPt1)
{
	m_wakeUpOnCharge = chargeTriggerPcntPt1;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * POWERMAN_ResetHost attempts to wake up the RPi though either the run pin, 5v
 * power cycle or GPIO3 line toggle. If the boost converter is already on then
 * the power is turned off and the delay timer will re-enable it in the module
 * task routine.
 *
 * @param	none
 * @retval	bool		false = reset not performed
 * 						true = reset performed
 */
// ****************************************************************************
static bool POWERMAN_ResetHost(void)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();

	bool result = false;

	if ( ((true == boostConverterEnabled) || (POW_SOURCE_NOT_PRESENT != power5vIoStatus))
			&& (RUN_PIN_INSTALLED == m_runPinInstalled)
			)
	{
		POWERSOURCE_Set5vBoostEnable(true);

		// activate RUN signal
		IODRV_SetPin(IODRV_PIN_RUN,GPIO_PIN_RESET);
		DelayUs(100);
		IODRV_SetPin(IODRV_PIN_RUN,GPIO_PIN_SET);

		// Note wakeup time
		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
		result = true;
	}
	else if (power5vIoStatus == POW_SOURCE_NOT_PRESENT)
	{
		if (true == boostConverterEnabled)
		{
			// if power is already on then first turn power off
			POWERSOURCE_Set5vBoostEnable(false);

			// schedule turn on after delay
			m_delayedTurnOnFlag = true;

			// wake up time will be set on delay power up
			MS_TIME_COUNTER_INIT(m_delayedTurnOnTimer);

			result = true;
		}
		else
		{
			// Enable the power
			POWERSOURCE_Set5vBoostEnable(true);

			// Note wakeup time
			MS_TIME_COUNTER_INIT(m_lastWakeupTimer);

			result = true;
		}
	}
	else if ( (true == boostConverterEnabled) || (POW_SOURCE_NOT_PRESENT != power5vIoStatus) )
	{
		// Change SCL pin to output open drain
		IODRV_SetPinType(IODRV_PIN_RPI_GPIO3, IOTYPE_DIGOUT_OPENDRAIN);

		// Pull down the pin
		IODRV_SetPin(IODRV_PIN_RPI_GPIO3,GPIO_PIN_RESET);
		DelayUs(100);

		// Let the pin go
		IODRV_SetPin(IODRV_PIN_RPI_GPIO3,GPIO_PIN_SET);

		// PWM open drain is the same as I2C SCL
		IODRV_SetPinType(IODRV_PIN_RPI_GPIO3, IOTYPE_PWM_OPENDRAIN);

		// Note wakeup time
		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);

		result = true;
	}

	return result;
}
