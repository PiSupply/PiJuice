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
#include "fuel_gauge.h"
#include "time_count.h"
#include "power_source.h"
#include "button.h"
#include "io_control.h"
#include "led.h"
#include "iodrv.h"

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void PowerManagementTask(void *argument);

static osThreadId_t powManTaskHandle;

static const osThreadAttr_t powManTask_attributes = {
	.name = "powManTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 128
};
#endif

#define WAKEUPONCHARGE_NV_INITIALISED	(0u != (wakeupOnChargeConfig & 0x80u))

RunPinInstallationStatus_T runPinInstallationStatus = RUN_PIN_NOT_INSTALLED;

static uint32_t powerMngmtTaskMsCounter;

//extern PowerState_T state;

uint8_t wakeupOnChargeConfig __attribute__((section("no_init")));

uint16_t wakeupOnCharge __attribute__((section("no_init"))); // 0 - 1000, 0xFFFF - disabled

extern uint32_t lastHostCommandTimer;

uint8_t delayedTurnOnFlag __attribute__((section("no_init")));
uint32_t delayedTurnOnTimer __attribute__((section("no_init")));

uint32_t lastWakeupTimer __attribute__((section("no_init")));

uint32_t delayedPowerOffCounter __attribute__((section("no_init")));

uint16_t watchdogConfig __attribute__((section("no_init")));
uint32_t watchdogExpirePeriod __attribute__((section("no_init"))); // 0 - disabled, 1-255 expiration time minutes
uint32_t watchdogTimer __attribute__((section("no_init")));
uint8_t watchdogExpiredFlag __attribute__((section("no_init")));

uint8_t rtcWakeupEventFlag __attribute__((section("no_init")));
uint8_t powerOffBtnEventFlag __attribute__((section("no_init")));

uint8_t ioWakeupEvent = 0;

extern uint8_t resetStatus;

extern uint8_t noBatteryTurnOn;

static bool m_gpioPowerControl;
static bool m_rpiActive;
static uint32_t m_rpiActiveTime;

void PowerManagementInit(void) {

	const uint32_t sysTime = HAL_GetTick();

	uint16_t var = 0u;
	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);
	if (((~var)&0xFFu) == (var>>8u)) {
		runPinInstallationStatus = (RunPinInstallationStatus_T)(var&0xFFu);
	}

	if (!resetStatus) { // on mcu power up

		wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
		wakeupOnChargeConfig = 0x7Fu;
		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (uint8_t*)&wakeupOnChargeConfig) == NV_READ_VARIABLE_SUCCESS) {
			if (wakeupOnChargeConfig<=100u) {
				/* Set last bit in config value to show EE is not just blank. */
				wakeupOnChargeConfig |= 0x80u;
			}
		}

		if (CHARGER_IS_INPUT_PRESENT()) {	/* Charger is connected */
			delayedTurnOnFlag = (noBatteryTurnOn == 1u);
		} else {							/* Charger is not connected */
			delayedTurnOnFlag = 0u;
			if (WAKEUPONCHARGE_NV_INITIALISED)
				wakeupOnCharge = (wakeupOnChargeConfig&0x7Fu) <= 100u ? (wakeupOnChargeConfig&0x7Fu) * 10u : WAKEUP_ONCHARGE_DISABLED_VAL;
		}

		if (	   NvReadVariableU8(WATCHDOG_CONFIGL_NV_ADDR, (uint8_t*)&watchdogConfig) != NV_READ_VARIABLE_SUCCESS
				|| NvReadVariableU8(WATCHDOG_CONFIGH_NV_ADDR, (uint8_t*)&watchdogConfig+1) != NV_READ_VARIABLE_SUCCESS
		 	 ) {
			watchdogConfig  = 0u;
		}

		delayedPowerOffCounter = 0u;
		watchdogExpirePeriod = 0u;
		watchdogTimer = 0u;
		watchdogExpiredFlag = 0u;
		rtcWakeupEventFlag = 0u;
		ioWakeupEvent = 0u;
		powerOffBtnEventFlag = 0u;
	}

	MS_TIMEREF_INIT(powerMngmtTaskMsCounter, sysTime);

	m_gpioPowerControl = true;
	m_rpiActive = false;
	MS_TIMEREF_INIT(m_rpiActiveTime, sysTime);

#if defined(RTOS_FREERTOS)
	powManTaskHandle = osThreadNew(PowerManagementTask, (void*)NULL, &powManTask_attributes);
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
		MS_TIME_COUNTER_INIT(lastWakeupTimer);
		return 0;
	}
	else if (power5vIoStatus == POW_SOURCE_NOT_PRESENT) {
		if (true == boostConverterEnabled) {
			// do power circle, first turn power off
			POWERSOURCE_Set5vBoostEnable(false);
			// schedule turn on after delay
			delayedTurnOnFlag = 1;
			MS_TIME_COUNTER_INIT(delayedTurnOnTimer);
			MS_TIME_COUNTER_INIT(lastWakeupTimer);
			return 0;
		}
		else
		{
			POWERSOURCE_Set5vBoostEnable(true);
			MS_TIME_COUNTER_INIT(lastWakeupTimer);
			return 0;
		}
	} else if ( (true == boostConverterEnabled) || (POW_SOURCE_NOT_PRESENT != power5vIoStatus) )
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
		MS_TIME_COUNTER_INIT(lastWakeupTimer);

		return 0;
	}

	return 1;
}

/*static int8_t WakeUpHost(void) {
	if (	MS_TIME_COUNT(lastHostCommandTimer) > 15000
			|| (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) //  Host is non powered
		) {
		return ResetHost();
	} else {
		// it is already running
		return 0;
	}
	return 1;
}*/

void BUTTON_PowerOnEventCb(const Button_T * const p_button)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const PowerSourceStatus_T power5vIoStatus = POWERSOURCE_Get5VRailStatus();

	// TODO - Should be another bracket set there somewhere!
	if ( ((false == boostConverterEnabled) && (POW_SOURCE_NOT_PRESENT == power5vIoStatus))
			|| (MS_TIME_COUNT(lastWakeupTimer) > 12000/*15000*/
			&& MS_TIME_COUNT(lastHostCommandTimer) > 11000)
			)
	{

		if (ResetHost() == 0)
		{//if (ResetHost() == 0) {
			wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
			rtcWakeupEventFlag = 0;
			ioWakeupEvent = 0;
			delayedPowerOffCounter = 0;
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
		powerOffBtnEventFlag = 1u;
	}

	BUTTON_ClearEvent(p_button->index); // remove event
}


void BUTTON_PowerResetEventCb(const Button_T * const p_button)
{
	if (ResetHost() == 0)
	{
		wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
		rtcWakeupEventFlag = 0;
		ioWakeupEvent = 0;
		delayedPowerOffCounter = 0;
	}

	BUTTON_ClearEvent(p_button->index);
}

void PowerMngmtHostPollEvent(void) {
	rtcWakeupEventFlag = 0;
	ioWakeupEvent = 0;
	watchdogTimer = watchdogExpirePeriod;
}

#if defined(RTOS_FREERTOS)
static void PowerManagementTask(void *argument) {

  for (;;) {
	if (MS_TIME_COUNT(powerMngmtTaskMsCounter) >= 900/*(state == STATE_LOWPOWER?2000:500)*/) {
		MS_TIME_COUNTER_INIT(powerMngmtTaskMsCounter);

		if ( ( (batteryRsoc >= wakeupOnCharge && CHARGER_IS_INPUT_PRESENT() && CHARGER_IS_BATTERY_PRESENT()) || rtcWakeupEventFlag || ioWakeupEvent)
				&& !delayedPowerOffCounter
				&& MS_TIME_COUNT(lastHostCommandTimer) > 15000
				&& MS_TIME_COUNT(lastWakeupTimer) > 30000 ) {
			if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				rtcWakeupEventFlag = 0;
				delayedPowerOffCounter = 0;
				ioWakeupEvent = 0;
			}
		}

		if (watchdogExpirePeriod && MS_TIME_COUNT(lastHostCommandTimer) > watchdogTimer) {
			if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				watchdogExpiredFlag = 1;
				rtcWakeupEventFlag = 0;
				ioWakeupEvent = 0;
				delayedPowerOffCounter = 0;
			}
			watchdogTimer += watchdogExpirePeriod;
		}
	}

	if ( delayedTurnOnFlag && MS_TIME_COUNT(delayedTurnOnTimer) >= 100 ) {
		Turn5vBoost(1);
		delayedTurnOnFlag = 0;
	}

	if ( delayedPowerOffCounter && delayedPowerOffCounter <= HAL_GetTick() ) {
		if (POW_5V_BOOST_EN_STATUS() && (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT)) {
			Turn5vBoost(0);
		}
		delayedPowerOffCounter = 0;
	}

	osDelay(20);
  }
}
#else
void PowerManagementTask(void)
{
	const bool chargerPresent = CHARGER_IS_INPUT_PRESENT();
	const uint32_t sysTime = HAL_GetTick();
	const uint16_t batteryRsoc = FUELGUAGE_GetSocPt1();

	bool boostConverterEnabled;
	bool isWakeupOnCharge;

	if (MS_TIMEREF_TIMEOUT(powerMngmtTaskMsCounter, sysTime, POWERMANAGE_TASK_PERIOD_MS))
	{
		MS_TIMEREF_INIT(powerMngmtTaskMsCounter, sysTime);

		isWakeupOnCharge = (batteryRsoc >= wakeupOnCharge) && (chargerPresent) && (CHARGER_IS_BATTERY_PRESENT());

		if ( ( isWakeupOnCharge || rtcWakeupEventFlag || ioWakeupEvent ) // there is wake-up trigger
				&& 	!delayedPowerOffCounter // deny wake-up during shutdown
				&& 	!delayedTurnOnFlag
				&& 	( (MS_TIMEREF_TIMEOUT(lastHostCommandTimer, sysTime, 15000) && MS_TIMEREF_TIMEOUT(lastWakeupTimer, sysTime, 30000))
						//|| (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) //  Host is non powered
		   ) )
		{
			if ( ResetHost() == 0u )
			{ //if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				rtcWakeupEventFlag = 0;
				ioWakeupEvent = 0;
				delayedPowerOffCounter = 0;

				if (watchdogConfig)
				{
					// activate watchdog after wake-up if watchdog config has restore flag
					watchdogExpirePeriod = watchdogConfig * (uint32_t)60000;
					watchdogTimer = watchdogExpirePeriod;
				}
			}
		}

		if (watchdogExpirePeriod && MS_TIMEREF_DIFF(lastHostCommandTimer, sysTime) > watchdogTimer) {
			if ( ResetHost() == 0 ) {
				wakeupOnCharge = WAKEUP_ONCHARGE_DISABLED_VAL;
				watchdogExpiredFlag = 1;
				rtcWakeupEventFlag = 0;
				ioWakeupEvent = 0;
				delayedPowerOffCounter = 0;
			}
			watchdogTimer += watchdogExpirePeriod;
		}
	}

	if ( delayedTurnOnFlag && (MS_TIMEREF_TIMEOUT(delayedTurnOnTimer, sysTime, 100u)) ) {
		POWERSOURCE_Set5vBoostEnable(true);
		delayedTurnOnFlag = 0;
		MS_TIME_COUNTER_INIT(lastWakeupTimer);
	}

	boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();

	if ( (0u != delayedPowerOffCounter) && (delayedPowerOffCounter <= sysTime) )
	{
		if ((true == boostConverterEnabled) && (POW_5V_IN_DETECTION_STATUS_PRESENT != pow5vInDetStatus))
		{
			POWERSOURCE_Set5vBoostEnable(false);
		}

		delayedPowerOffCounter = 0u;

		/* Turn off led as it keeps flashing! */
		LedSetRGB(LED2, 0u, 0u, 0u);
	}

	if ( true == m_rpiActive )
	{
		if ( (true == m_gpioPowerControl) && (delayedPowerOffCounter > 2000u) )
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
					delayedPowerOffCounter = 2000u;
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

	if ( (false == chargerPresent) && (WAKEUP_ONCHARGE_DISABLED_VAL == wakeupOnCharge) && (WAKEUPONCHARGE_NV_INITIALISED) ) {
		// setup wake-up on charge if charging stopped, power source removed
		wakeupOnCharge = (wakeupOnChargeConfig&0x7F) <= 100 ? (wakeupOnChargeConfig&0x7F) * 10 : WAKEUP_ONCHARGE_DISABLED_VAL;
	}
}
#endif

void InputSourcePresenceChangeCb(uint8_t event) {

}

void PowerMngmtSchedulePowerOff(uint8_t delayCode) {
	if (delayCode <= 250u) {
		MS_TIMEREF_INIT(delayedPowerOffCounter, HAL_GetTick() + (delayCode * 1024u));
		if (delayedPowerOffCounter == 0u) delayedPowerOffCounter = 1u; // 0 is used to indicate non active counter, so avoid that value
	} else if (delayCode == 0xFFu) {
		delayedPowerOffCounter = 0u; // deactivate scheduled power off
	}
}

uint8_t PowerMngmtGetPowerOffCounter(void) {
	if ( delayedPowerOffCounter ) {
		if (delayedPowerOffCounter > HAL_GetTick()) {
			return (delayedPowerOffCounter-HAL_GetTick()) >> 10u;
		} else {
			return 0u;
		}
	} else {
		return 0xFFu;
	}
}

void RunPinInstallationStatusSetConfigCmd(uint8_t data[], uint8_t len) {
	if (data[0] > 1) return;

	EE_WriteVariable(NV_RUN_PIN_CONFIG, data[0] | ((uint16_t)(~data[0])<<8));

	uint16_t var = 0;
	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);
	if (((~var)&0xFF) == (var>>8)) {
		runPinInstallationStatus = var&0xFF;
	} else {
		runPinInstallationStatus = RUN_PIN_NOT_INSTALLED;
	}
}

void RunPinInstallationStatusGetConfigCmd(uint8_t data[], uint16_t *len) {
	data[0] = runPinInstallationStatus;
	*len = 1;
}

void PowerMngmtConfigureWatchdogCmd(uint8_t data[], uint16_t len) {
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

void PowerMngmtSetWakeupOnChargeCmd(uint8_t data[], uint16_t len) {

	uint16_t newWakeupVal = (data[0u]&0x7Fu) * 10u;

	if (newWakeupVal > 1000u) {	/* Check out of range and set disabled */
		newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
	}

	if (data[0u]&0x80u) {		/* Host wants to commit this value to NV */
		NvWriteVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (data[0u]&0x7Fu) <= 100u ? data[0u] : 0x7Fu);
		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, &wakeupOnChargeConfig) != NV_READ_VARIABLE_SUCCESS ) {
			/* If writing to NV failed then disable wakeup on charge */
			wakeupOnChargeConfig = 0x7Fu;
			newWakeupVal = WAKEUP_ONCHARGE_DISABLED_VAL;
		} else {
			wakeupOnChargeConfig |= 0x80u;
		}
	}

	wakeupOnCharge = newWakeupVal;
}

void PowerMngmtGetWakeupOnChargeCmd(uint8_t data[], uint16_t *len)  {
	if (WAKEUPONCHARGE_NV_INITIALISED)
		data[0] = wakeupOnChargeConfig;
	else
		data[0] = wakeupOnCharge <= 1000 ? wakeupOnCharge / 10 : 0x7F;

	*len = 1;
}
