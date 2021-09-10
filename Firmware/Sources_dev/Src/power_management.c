/*
 * power_management.c
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */

#include "nv.h"
#include "power_management.h"
#include "charger_bq2416x.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"
#include "power_source.h"
#include "button.h"
//#include "led.h"
#include "logging.h"

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

void PowerManagementInit(void) {
	uint16_t var = 0;
	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);
	if (((~var)&0xFF) == (var>>8)) {
		runPinInstallationStatus = var&0xFF;
	}

	if (!resetStatus) { // on mcu power up

		wakeupOnCharge = 0xFFFF;
		wakeupOnChargeConfig = 0x7F;
		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (uint8_t*)&wakeupOnChargeConfig) == NV_READ_VARIABLE_SUCCESS) {
			if (wakeupOnChargeConfig<=100) {
				wakeupOnChargeConfig |= 0x80;
			}
		}

		if (CHARGER_IS_INPUT_PRESENT()) {
			delayedTurnOnFlag = (noBatteryTurnOn == 1);
		} else {
			delayedTurnOnFlag = 0;
		}

		if (wakeupOnChargeConfig&0x80)
			wakeupOnCharge = (wakeupOnChargeConfig&0x7F) <= 100 ? (wakeupOnChargeConfig&0x7F) * 10 : 0xFFFF;

		if (	   NvReadVariableU8(WATCHDOG_CONFIGL_NV_ADDR, (uint8_t*)&watchdogConfig) != NV_READ_VARIABLE_SUCCESS
				|| NvReadVariableU8(WATCHDOG_CONFIGH_NV_ADDR, (uint8_t*)&watchdogConfig+1) != NV_READ_VARIABLE_SUCCESS
		 	 ) {
			watchdogConfig  = 0;
		}

		delayedPowerOffCounter = 0;
		watchdogExpirePeriod = 0;
		watchdogTimer = 0;
		watchdogExpiredFlag = 0;
		rtcWakeupEventFlag = 0;
		ioWakeupEvent = 0;
		powerOffBtnEventFlag = 0;
	}

	MS_TIME_COUNTER_INIT(powerMngmtTaskMsCounter);

#if defined(RTOS_FREERTOS)
	powManTaskHandle = osThreadNew(PowerManagementTask, (void*)NULL, &powManTask_attributes);
#endif
}
#if defined LOGGING
__STATIC_INLINE void LOG_PM_WAKEUP_EVENT(uint8_t triggers) {
	uint8_t *buf = LoggingInitMessage(WAKEUP_EVT);
	if (buf == NULL) 	return;
	buf[0] = triggers;

	buf[1] = 0;
	buf[1] |= (batteryStatus << 2);
	buf[1] |= (powerInStatus << 4);
	buf[1] |= (power5vIoStatus << 6);

	buf[2] = POW_5V_BOOST_EN_STATUS();
	buf[2] |= POW_VSYS_OUTPUT_EN_STATUS()<<2;
	buf[2] |= (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_SET)<<3;

	buf[3] = wakeupOnCharge;
	buf[4] = wakeupOnCharge>>8;

	buf[5] = batteryRsoc>>2;
	buf[6] = batteryTemp;
	buf[7] = batteryVoltage;
	buf[8] = batteryVoltage >> 8;
	uint16_t ioVolt = Get5vIoVoltage();
	buf[9] = ioVolt;
	buf[10] = ioVolt >> 8;
	int16_t curr = GetLoadCurrent(); // compress average current to one byte
	buf[11] = curr;
	buf[12] = curr>>8;
}
#else
#define LOG_PM_WAKEUP_EVENT(triggers)
#endif

int8_t ResetHost(void) {
	if ( (POW_5V_BOOST_EN_STATUS() || power5vIoStatus != POW_SOURCE_NOT_PRESENT) && runPinInstallationStatus == RUN_PIN_INSTALLED ) {
		Turn5vBoost(1);
		// activate RUN signal
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
		DelayUs(100);
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
		MS_TIME_COUNTER_INIT(lastWakeupTimer);
		return 0;
	} else if (power5vIoStatus == POW_SOURCE_NOT_PRESENT) {
		if (POW_5V_BOOST_EN_STATUS()) {
			// do power circle, first turn power off
			Turn5vBoost(0);
			// schedule turn on after delay
			delayedTurnOnFlag = 1;
			MS_TIME_COUNTER_INIT(delayedTurnOnTimer);
			MS_TIME_COUNTER_INIT(lastWakeupTimer);
			return 0;
		} else {
			int ret = Turn5vBoost(1);
			MS_TIME_COUNTER_INIT(lastWakeupTimer);
			return 0;
		}
	} else if ( (POW_5V_BOOST_EN_STATUS() || power5vIoStatus != POW_SOURCE_NOT_PRESENT) ) {
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

void PowerOnButtonEventCb(uint8_t b, ButtonEvent_T event) {
	//if ( event == BUTTON_EVENT_SINGLE_PRESS ) {
		if ( (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT)
				|| (MS_TIME_COUNT(lastWakeupTimer) > 12000/*15000*/ && MS_TIME_COUNT(lastHostCommandTimer) > 11000)  ) {
			LOG_PM_WAKEUP_EVENT(0x10);
			if (ResetHost() == 0) {//if (ResetHost() == 0) {
				wakeupOnCharge = 0xFFFF;
				rtcWakeupEventFlag = 0;
				ioWakeupEvent = 0;
				delayedPowerOffCounter = 0;
			}
		}
		ButtonRemoveEvent(b);
		//LogPut(LOG_5VREG_ON);
	//}
}

void PowerOffButtonEventCb(uint8_t b, ButtonEvent_T event) {
	//if ( event == BUTTON_EVENT_LONG_PRESS2 ) {
		// cut power to rpi
		if ( POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT ) {
				Turn5vBoost(0);
				powerOffBtnEventFlag = 1;
		}
		ButtonRemoveEvent(b); // remove event
	//}
}

void ButtonEventFuncPowerResetCb(uint8_t b, ButtonEvent_T event) {
	if (ResetHost() == 0) {
		wakeupOnCharge = 0xFFFF;
		rtcWakeupEventFlag = 0;
		ioWakeupEvent = 0;
		delayedPowerOffCounter = 0;
	}
	ButtonRemoveEvent(b);
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
				wakeupOnCharge = 0xFFFF;
				rtcWakeupEventFlag = 0;
				delayedPowerOffCounter = 0;
				ioWakeupEvent = 0;
			}
		}

		if (watchdogExpirePeriod && MS_TIME_COUNT(lastHostCommandTimer) > watchdogTimer) {
			if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = 0xFFFF;
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
void PowerManagementTask(void) {

	if (MS_TIME_COUNT(powerMngmtTaskMsCounter) >= 500) {
		//LogPut(LOG_5VREG_ON);
		MS_TIME_COUNTER_INIT(powerMngmtTaskMsCounter);

		volatile int isWakeupOnCharge = batteryRsoc >= wakeupOnCharge && CHARGER_IS_INPUT_PRESENT() && CHARGER_IS_BATTERY_PRESENT();
		if ( 		( isWakeupOnCharge || rtcWakeupEventFlag || ioWakeupEvent) // there is wake-up trigger
				&& 	!delayedPowerOffCounter // deny wake-up during shutdown
				&& 	!delayedTurnOnFlag
				&& 	( (MS_TIME_COUNT(lastHostCommandTimer) > 15000 && MS_TIME_COUNT(lastWakeupTimer) > 30000)
						//|| (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) //  Host is non powered
		   ) ) {

			LOG_PM_WAKEUP_EVENT((isWakeupOnCharge&0x01) | ((rtcWakeupEventFlag<<1)&0x02) | ((ioWakeupEvent<<2)&0x04));

			if ( ResetHost() == 0 ) { //if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = 0xFFFF;
				rtcWakeupEventFlag = 0;
				ioWakeupEvent = 0;
				delayedPowerOffCounter = 0;

				if (watchdogConfig) {
					// activate watchdog after wake-up if watchdog config has restore flag
					watchdogExpirePeriod = watchdogConfig * (uint32_t)60000;
				    watchdogTimer = MS_TIME_COUNT(lastHostCommandTimer); //lastHostCommandTimer;
				    watchdogTimer += watchdogExpirePeriod;
				}
			}

		}

		if (watchdogExpirePeriod && MS_TIME_COUNT(lastHostCommandTimer) > watchdogTimer) {
			LOG_PM_WAKEUP_EVENT(0x08);

			if ( ResetHost() == 0 ) {
				wakeupOnCharge = 0xFFFF;
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
		MS_TIME_COUNTER_INIT(lastWakeupTimer);
	}

	if ( delayedPowerOffCounter && delayedPowerOffCounter <= HAL_GetTick() ) {
		if (POW_5V_BOOST_EN_STATUS() && (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT)) {
			Turn5vBoost(0);
		}
		delayedPowerOffCounter = 0;
	}

	if ( wakeupOnCharge == 0xFFFF && !CHARGER_IS_INPUT_PRESENT() && (wakeupOnChargeConfig&0x80)) {
		// setup wake-up on charge if charging stopped, power source removed
		wakeupOnCharge = (wakeupOnChargeConfig&0x7F) <= 100 ? (wakeupOnChargeConfig&0x7F) * 10 : 0xFFFF;
	}
}
#endif

void InputSourcePresenceChangeCb(uint8_t event) {

}

void PowerMngmtSchedulePowerOff(uint8_t dalayCode) {
	if (dalayCode <= 250) {
		delayedPowerOffCounter = HAL_GetTick() + dalayCode * 1024;
		if (delayedPowerOffCounter == 0) delayedPowerOffCounter ++; // 0 is used to indicate non active counter, so avoid that value
	} else if (dalayCode == 0xFF) {
		delayedPowerOffCounter = 0; // deactivate scheduled power off
	}
}

uint8_t PowerMngmtGetPowerOffCounter(void) {
	if ( delayedPowerOffCounter ) {
		if (delayedPowerOffCounter > HAL_GetTick()) {
			return (delayedPowerOffCounter-HAL_GetTick()) >> 10;
		} else {
			return 0;
		}
	} else {
		return 0xFF;
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
		watchdogTimer = MS_TIME_COUNT(lastHostCommandTimer);
		watchdogTimer += watchdogExpirePeriod;
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

	if (data[0]&0x80) {
		wakeupOnChargeConfig = (data[0]&0x7F) <= 100 ? data[0] : 0x7F;
		NvWriteVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, wakeupOnChargeConfig);
		if (NvReadVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, (uint8_t*)&wakeupOnChargeConfig) != NV_READ_VARIABLE_SUCCESS ) {
			wakeupOnChargeConfig = 0x7F;
		}

		if (wakeupOnChargeConfig == 0x7F) {
			wakeupOnCharge = 0xFFFF;
		}
	} else {
		wakeupOnCharge = (data[0]&0x7F) <= 100 ? (data[0]&0x7F) * 10 : 0xFFFF;
	}
}

void PowerMngmtGetWakeupOnChargeCmd(uint8_t data[], uint16_t *len)  {
	if (wakeupOnChargeConfig&0x80)
		data[0] = wakeupOnChargeConfig;
	else
		data[0] = wakeupOnCharge <= 1000 ? wakeupOnCharge / 10 : 0x7F;

	*len = 1;
}
