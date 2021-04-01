/*
 * power_management.c
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */

#include "main.h"
#include "system_conf.h"

#include "nv.h"
#include "power_management.h"
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


#define WAKEUPONCHARGE_NV_INITIALISED	(0u != (wakeupOnChargeConfig & 0x80u))

static RunPinInstallationStatus_T runPinInstallationStatus = RUN_PIN_NOT_INSTALLED;

static uint8_t wakeupOnChargeConfig __attribute__((section("no_init")));

static uint16_t m_wakeUpOnCharge __attribute__((section("no_init"))); // 0 - 1000, 0xFFFF - disabled

static uint8_t delayedTurnOnFlag __attribute__((section("no_init")));
static uint32_t delayedTurnOnTimer __attribute__((section("no_init")));

static uint32_t m_delayedPowerOffTimeMs __attribute__((section("no_init")));

static uint16_t watchdogConfig __attribute__((section("no_init")));
static uint32_t watchdogExpirePeriod __attribute__((section("no_init"))); // 0 - disabled, 1-255 expiration time minutes
static uint32_t watchdogTimer __attribute__((section("no_init")));

static bool m_watchdogExpired __attribute__((section("no_init")));
static bool m_powerButtonPressedEvent __attribute__((section("no_init")));


static uint32_t powerMngmtTaskMsCounter;
static uint32_t m_lastWakeupTimer __attribute__((section("no_init")));


#ifdef DETECT_RPI_PWRLED_GPIO26
static bool m_rpiActive;
static uint32_t m_rpiActiveTime;
static bool m_gpioPowerControl;
#endif


void PowerManagementInit(void)
{

	const uint32_t sysTime = HAL_GetTick();
	const bool noBatteryTurnOn = CHARGER_GetNoBatteryTurnOnEnable();
	const bool chargerHasInput = CHARGER_IsChargeSourceAvailable();

	uint16_t var = 0u;

	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);

	if (((~var)&0xFFu) == (var>>8u))
	{
		runPinInstallationStatus = (RunPinInstallationStatus_T)(var&0xFFu);
	}

	if (EXECUTION_STATE_NORMAL != executionState)
	{
		// on mcu power up

		m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
		wakeupOnChargeConfig = 0x7Fu;

		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (uint8_t*)&wakeupOnChargeConfig) == NV_READ_VARIABLE_SUCCESS)
		{
			if (wakeupOnChargeConfig<=100u) {
				/* Set last bit in config value to show EE is not just blank. */
				wakeupOnChargeConfig |= 0x80u;
			}
		}

		if (true == chargerHasInput)
		{	/* Charger is connected */
			delayedTurnOnFlag = noBatteryTurnOn;
		}
		else
		{	/* Charger is not connected */
			delayedTurnOnFlag = 0u;

			if (WAKEUPONCHARGE_NV_INITIALISED)
			{
				m_wakeUpOnCharge = (wakeupOnChargeConfig&0x7Fu) <= 100u ? (wakeupOnChargeConfig&0x7Fu) * 10u : WAKEUP_ONCHARGE_DISABLED_VAL;
			}
		}

		if ( NvReadVariableU8(WATCHDOG_CONFIGL_NV_ADDR, (uint8_t*)&watchdogConfig) != NV_READ_VARIABLE_SUCCESS ||
				NvReadVariableU8(WATCHDOG_CONFIGH_NV_ADDR, (uint8_t*)&watchdogConfig+1) != NV_READ_VARIABLE_SUCCESS
		 	 )
		{
			watchdogConfig  = 0u;
		}

		m_delayedPowerOffTimeMs = 0u;
		watchdogExpirePeriod = 0u;
		watchdogTimer = 0u;
		m_watchdogExpired = false;

		RTC_ClearWakeEvent();
		TASKMAN_ClearIOWakeEvent();
		m_powerButtonPressedEvent = false;
	}

	MS_TIMEREF_INIT(powerMngmtTaskMsCounter, sysTime);

#ifdef DETECT_RPI_PWRLED_GPIO26

	m_gpioPowerControl = true;

	m_rpiActive = false;
	MS_TIMEREF_INIT(m_rpiActiveTime, sysTime);

#endif

}

int8_t ResetHost(void)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();

	if ( ((true == boostConverterEnabled) || (POW_SOURCE_NOT_PRESENT != power5vIoStatus))
			&& (RUN_PIN_INSTALLED == runPinInstallationStatus)
			)
	{
		POWERSOURCE_Set5vBoostEnable(true);
		// activate RUN signal
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
		DelayUs(100);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
		return 0;
	}
	else if (power5vIoStatus == POW_SOURCE_NOT_PRESENT)
	{
		if (true == boostConverterEnabled)
		{
			// do power circle, first turn power off
			POWERSOURCE_Set5vBoostEnable(false);
			// schedule turn on after delay
			delayedTurnOnFlag = 1;
			MS_TIME_COUNTER_INIT(delayedTurnOnTimer);
			MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
			return 0;
		}
		else
		{
			POWERSOURCE_Set5vBoostEnable(true);
			MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
			return 0;
		}
	}
	else if ( (true == boostConverterEnabled) || (POW_SOURCE_NOT_PRESENT != power5vIoStatus) )
	{
		// wakeup via RPI GPIO3
	    GPIO_InitTypeDef i2c_GPIO_InitStruct;
		i2c_GPIO_InitStruct.Pin = GPIO_PIN_6;
		i2c_GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
		i2c_GPIO_InitStruct.Pull = GPIO_NOPULL;
	    HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);
	    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
		DelayUs(100);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
		i2c_GPIO_InitStruct.Pin       = GPIO_PIN_6;
		i2c_GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
		i2c_GPIO_InitStruct.Pull      = GPIO_NOPULL;
		i2c_GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
		i2c_GPIO_InitStruct.Alternate = GPIO_AF1_I2C1;
		HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);
		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);

		return 0;
	}

	return 1;
}


void BUTTON_PowerOnEventCb(const Button_T * const p_button)
{
	const uint32_t sysTime = HAL_GetTick();

	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();
	const uint32_t lastHostCommandAgeMs = HOSTCOMMS_GetLastCommandAgeMs(sysTime);

	// TODO - Check brackets are in the right places
	if ( ( (false == boostConverterEnabled) && (POW_SOURCE_NOT_PRESENT == power5vIoStatus) ) ||
			( MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 12000u) && (lastHostCommandAgeMs > 11000) )
			)
	{

		if (ResetHost() == 0)
		{
			m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
			RTC_ClearWakeEvent();
			TASKMAN_ClearIOWakeEvent();
			m_delayedPowerOffTimeMs = 0u;
		}
	}

	BUTTON_ClearEvent(p_button->index);

}


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


void BUTTON_PowerResetEventCb(const Button_T * const p_button)
{
	if (ResetHost() == 0)
	{
		m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;

		RTC_ClearWakeEvent();
		TASKMAN_ClearIOWakeEvent();

		m_delayedPowerOffTimeMs = 0u;
	}

	BUTTON_ClearEvent(p_button->index);
}

void PowerMngmtHostPollEvent(void)
{
	RTC_ClearWakeEvent();
	TASKMAN_ClearIOWakeEvent();
	watchdogTimer = watchdogExpirePeriod;
}


void PowerManagementTask(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const bool chargerHasInput = CHARGER_IsChargeSourceAvailable();
	const bool chargerHasBattery = (CHARGER_BATTERY_NOT_PRESENT != CHARGER_GetBatteryStatus());
	const uint16_t batteryRsoc = FUELGUAGE_GetSocPt1();
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();
	const uint32_t lastHostCommandAgeMs = HOSTCOMMS_GetLastCommandAgeMs(sysTime);
	const bool rtcWakeEvent = RTC_GetWakeEvent();
	const bool ioWakeEvent = TASKMAN_GetIOWakeEvent();

	bool boostConverterEnabled;
	bool isWakeupOnCharge;

	if (MS_TIMEREF_TIMEOUT(powerMngmtTaskMsCounter, sysTime, POWERMANAGE_TASK_PERIOD_MS))
	{
		MS_TIMEREF_INIT(powerMngmtTaskMsCounter, sysTime);

		isWakeupOnCharge = (batteryRsoc >= m_wakeUpOnCharge) && (true == chargerHasInput) && (true == chargerHasBattery);

		if ( ( isWakeupOnCharge || rtcWakeEvent || ioWakeEvent ) // there is wake-up trigger
				&& 	(0u != m_delayedPowerOffTimeMs) // deny wake-up during shutdown
				&& 	(false == delayedTurnOnFlag)
				&& 	( (lastHostCommandAgeMs >= 15000) && MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 30000) )
						//|| (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) //  Host is non powered
		   	   	)
		{
			if ( ResetHost() == 0u )
			{
				m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				RTC_ClearWakeEvent();
				TASKMAN_ClearIOWakeEvent();
				m_delayedPowerOffTimeMs = 0u;

				if (watchdogConfig)
				{
					// activate watchdog after wake-up if watchdog config has restore flag
					watchdogExpirePeriod = watchdogConfig * (uint32_t)60000;
					watchdogTimer = watchdogExpirePeriod;
				}
			}
		}



		if ( watchdogExpirePeriod && (lastHostCommandAgeMs > watchdogTimer) )
		{
			if ( ResetHost() == 0 )
			{
				m_wakeUpOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				m_watchdogExpired = true;
				RTC_ClearWakeEvent();
				TASKMAN_ClearIOWakeEvent();
				m_delayedPowerOffTimeMs = 0u;
			}

			watchdogTimer += watchdogExpirePeriod;
		}
	}

	if ( delayedTurnOnFlag && (MS_TIMEREF_TIMEOUT(delayedTurnOnTimer, sysTime, 100u)) )
	{
		POWERSOURCE_Set5vBoostEnable(true);
		delayedTurnOnFlag = 0;
		MS_TIME_COUNTER_INIT(m_lastWakeupTimer);
	}

	boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();

	if ( (0u != m_delayedPowerOffTimeMs) && MS_TIMEREF_TIMEOUT(m_delayedPowerOffTimeMs, sysTime, 1u) )
	{
		if ((true == boostConverterEnabled) && (POW_SOURCE_NORMAL != pow5vInDetStatus))
		{
			POWERSOURCE_Set5vBoostEnable(false);
		}

		// Disable timer
		m_delayedPowerOffTimeMs = 0u;

		/* Turn off led as it keeps flashing! */
		LED_SetRGB(LED_LED2_IDX, 0u, 0u, 0u);
	}

#ifdef DETECT_RPI_PWRLED_GPIO26
	if ( true == m_rpiActive )
	{
		if ( (true == m_gpioPowerControl) && MS_TIMEREF_TIMEOUT(m_delayedPowerOffTimeMs, sysTime, 2000u) )
		{
			uint8_t pinData[2u];
			uint16_t len;

			IoRead(RPI_ACTLED_IO_PIN, pinData, &len);

			if (0u == pinData[0u])
			{
				/* RPi power has gone off, make sure */
				if (MS_TIMEREF_TIMEOUT(m_rpiActiveTime, sysTime, 100u))
				{
					/* Turn led to red */
					LedSetRGB(LED2, 100u, 0u, 0u);

					/* Adjust power off to 2 seconds */
					MS_TIMEREF_INIT(m_delayedPowerOffTimeMs, sysTime + 2000u);
				}
			}
			else
			{
				/* RPi power is still on */
				MS_TIMEREF_INIT(m_rpiActiveTime, sysTime);
			}
		}
	}
	else
	{
		uint8_t pinData[2u];
		uint16_t len;

		IoRead(RPI_ACTLED_IO_PIN, pinData, &len);

		if (0u == pinData[0u])
		{
			/* RPi is not on yet */
			MS_TIMEREF_INIT(m_rpiActiveTime, sysTime);
		}
		else if (MS_TIMEREF_TIMEOUT(m_rpiActiveTime, sysTime, 5000u))
		{
			/* RPi has been on for 5 seconds */
			m_rpiActive = true;
			LedSetRGB(LED2, 0u, 100u, 0u);
		}
	}
#endif

	if ( (false == chargerHasInput) &&
			(WAKEUP_ONCHARGE_DISABLED_VAL == m_wakeUpOnCharge) &&
			(WAKEUPONCHARGE_NV_INITIALISED)
			)
	{
		// setup wake-up on charge if charging stopped, power source removed
		m_wakeUpOnCharge = (wakeupOnChargeConfig & 0x7F) <= 100u ? (wakeupOnChargeConfig & 0x7Fu) * 10u :
																	WAKEUP_ONCHARGE_DISABLED_VAL;
	}
}


void InputSourcePresenceChangeCb(uint8_t event)
{

}


void PowerMngmtSchedulePowerOff(uint8_t delayCode)
{
	if (delayCode <= 250u)
	{
		MS_TIMEREF_INIT(m_delayedPowerOffTimeMs, HAL_GetTick() + (delayCode * 1024u));

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


uint8_t POWERMAN_GetPowerOffTime(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const uint32_t timeLeft = MS_TIMEREF_DIFF(m_delayedPowerOffTimeMs, sysTime);

	if (0u != m_delayedPowerOffTimeMs)
	{
		if (timeLeft < (UINT8_MAX * 1024u))
		{
			return (uint8_t)(timeLeft / 1024u);
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


bool POWERMAN_GetPowerButtonPressedStatus(void)
{
	return m_powerButtonPressedEvent;
}


bool POWERMAN_GetWatchdogExpired(void)
{
	return m_watchdogExpired;
}


void POWERMAN_ClearWatchdog(void)
{
	m_watchdogExpired = false;
}


void RunPinInstallationStatusSetConfigCmd(uint8_t data[], uint8_t len)
{
	uint16_t var = 0;

	if (data[0] > 1)
	{
		return;
	}

	EE_WriteVariable(NV_RUN_PIN_CONFIG, data[0] | ((uint16_t)(~data[0])<<8));
	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);

	if (((~var)&0xFF) == (var>>8))
	{
		runPinInstallationStatus = var&0xFF;
	}
	else
	{
		runPinInstallationStatus = RUN_PIN_NOT_INSTALLED;
	}
}

void RunPinInstallationStatusGetConfigCmd(uint8_t data[], uint16_t *len)
{
	data[0] = runPinInstallationStatus;
	*len = 1;
}

void PowerMngmtConfigureWatchdogCmd(uint8_t data[], uint16_t len)
{
	if (len < 2) return;
	volatile uint16_t d, cfg;

	cfg = data[1];
	cfg <<= 8;
	cfg |= data[0];

	d = cfg & 0x3FFF;
	d <<= ((cfg&0x4000) >> 13); // correct resolution to 4 minutes for range 16384-65536

	if (data[1]&0x80) {
		watchdogConfig = d;
		NvWriteVariableU8(WATCHDOG_CONFIGL_NV_ADDR, watchdogConfig);
		NvWriteVariableU8(WATCHDOG_CONFIGH_NV_ADDR, watchdogConfig>>8);

		if (NvReadVariableU8(WATCHDOG_CONFIGL_NV_ADDR, (uint8_t*)&watchdogConfig) != NV_READ_VARIABLE_SUCCESS
		 || NvReadVariableU8(WATCHDOG_CONFIGH_NV_ADDR, (uint8_t*)&watchdogConfig+1) != NV_READ_VARIABLE_SUCCESS
		 ) {
			watchdogConfig = 0;
		}

		if (watchdogConfig == 0) {
			watchdogExpirePeriod = 0;
			watchdogTimer = 0;
		}
	} else {
		watchdogExpirePeriod = d * (uint32_t)60000;
		watchdogTimer = watchdogExpirePeriod;
	}

}

void PowerMngmtGetWatchdogConfigurationCmd(uint8_t data[], uint16_t *len) {
	if (watchdogConfig) {
		uint16_t d = watchdogConfig;
		if (d >= 0x4000) d = (d >> 2) | 0x4000;
		data[0] = d;
		data[1] = (d >> 8) | 0x80;
	} else {
		 uint16_t d = watchdogExpirePeriod / 60000;
		 if (d >= 0x4000) d = (d >> 2) | 0x4000;
		 data[0] = d;
		 data[1] = d >> 8;
	}

	 *len = 2;
}


void POWERMAN_SetWakeupOnChargeData(const uint8_t * const data, const uint16_t len)
{
	uint16_t newWakeupVal = (data[0u] & 0x7Fu) * 10u;

	if (newWakeupVal > 1000u)
	{
		newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
	}

	if (data[0u] & 0x80u)
	{
		NvWriteVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (data[0u]&0x7Fu) <= 100u ? data[0u] : 0x7Fu);

		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, &wakeupOnChargeConfig) != NV_READ_VARIABLE_SUCCESS )
		{
			/* If writing to NV failed then disable wakeup on charge */
			wakeupOnChargeConfig = 0x7Fu;
			newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
		}
		else
		{
			wakeupOnChargeConfig |= 0x80u;
		}
	}

	m_wakeUpOnCharge = newWakeupVal;
}


void POWERMAN_GetWakeupOnChargeData(uint8_t * const data, uint16_t * const len)
{
	if (WAKEUPONCHARGE_NV_INITIALISED)
	{
		data[0u] = wakeupOnChargeConfig;
	}
	else
	{
		data[0u] = (m_wakeUpOnCharge <= 1000) ? m_wakeUpOnCharge / 10u : 100u;
	}

	*len = 1u;
}


bool POWERMAN_CanShutDown(void)
{
	const uint32_t sysTime = HAL_GetTick();

	return MS_TIMEREF_TIMEOUT(m_lastWakeupTimer, sysTime, 5000u);
}


void POWERMAN_ClearPowerButtonPressed(void)
{
	m_powerButtonPressedEvent = false;
}


void POWERMAN_SetWakeupOnChargePt1(const uint16_t newValue)
{
	m_wakeUpOnCharge = newValue;
}
