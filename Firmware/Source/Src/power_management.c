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
#include "led.h"

RunPinInstallationStatus_T runPinInstallationStatus = RUN_PIN_NOT_INSTALLED;

static uint32_t powerMngmtTaskMsCounter;

extern PowerState_T state;

//uint8_t wakeupOnChargeConfig __attribute__((section("no_init")));
uint16_t wakeupOnCharge __attribute__((section("no_init"))); // 0 - 1000, 0xFFFF - disabled

extern uint32_t lastHostCommandTimer;

uint8_t delayedTurnOnFlag __attribute__((section("no_init")));
uint32_t delayedTurnOnTimer __attribute__((section("no_init")));

uint32_t lastWakeupTimer __attribute__((section("no_init")));

uint32_t delayedPowerOffCounter __attribute__((section("no_init")));

uint32_t watchdogExpirePeriod __attribute__((section("no_init"))); // 0 - disabled, 1-255 expiration time minutes
uint32_t watchdogTimer __attribute__((section("no_init")));
uint8_t watchdogExpiredFlag __attribute__((section("no_init")));

uint8_t rtcWakeupEventFlag __attribute__((section("no_init")));
uint8_t powerOffBtnEventFlag __attribute__((section("no_init")));

extern uint8_t resetStatus;

extern uint8_t noBatteryTurnOn;

void PowerManagementInit(void) {
	uint16_t var = 0;
	EE_ReadVariable(NV_RUN_PIN_CONFIG, &var);
	if (((~var)&0xFF) == (var>>8)) {
		runPinInstallationStatus = var&0xFF;
	}

	if (!resetStatus) { // on mcu power up
		/*EE_ReadVariable(WAKEUPONCHARGE_CONFIG_NV_ADDR, &var);
		if (((~var)&0xFF) == (var>>8)) {
			wakeupOnChargeConfig = var & 0xFF;
			wakeupOnCharge = (wakeupOnChargeConfig&0x7F) <= 100 ? (wakeupOnChargeConfig&0x7F) * 10 : 0xFFFF;
		} else {
			wakeupOnChargeConfig = 0xFF;*/
			wakeupOnCharge = 0xFFFF;
		//}
		if (CHARGER_IS_INPUT_PRESENT())
			delayedTurnOnFlag = (noBatteryTurnOn == 1);
		else
			delayedTurnOnFlag = 0;
		delayedPowerOffCounter = 0;
		watchdogExpirePeriod = 0;
		watchdogTimer = 0;
		watchdogExpiredFlag = 0;
		rtcWakeupEventFlag = 0;
		powerOffBtnEventFlag = 0;
	}

	MS_TIME_COUNTER_INIT(powerMngmtTaskMsCounter);
}

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
			Turn5vBoost(1);
			MS_TIME_COUNTER_INIT(lastWakeupTimer);
			return 0;
		}
	}
	return 1;
}

int8_t WakeUpHost(void) {
	//if (/*batteryRsoc >= 50 ||*/ powerInStatus != POW_SOURCE_NOT_PRESENT || power5vIoStatus != POW_SOURCE_NOT_PRESENT ) {
	if (MS_TIME_COUNT(lastHostCommandTimer) > 15000 || (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT)) {
		return ResetHost();
	} else {
		// it is already running
		return 0;
	}
	return 1;
}

void PowerOnButtonEventCb(uint8_t b, ButtonEvent_T event) {
	//if ( event == BUTTON_EVENT_SINGLE_PRESS ) {
		if ( (!POW_5V_BOOST_EN_STATUS() && power5vIoStatus == POW_SOURCE_NOT_PRESENT) || (MS_TIME_COUNT(lastWakeupTimer) > 15000 && MS_TIME_COUNT(lastHostCommandTimer) > 11000)  ) {
			if (WakeUpHost() == 0) {
				wakeupOnCharge = 0xFFFF;
				rtcWakeupEventFlag = 0;
				delayedPowerOffCounter = 0;
			}
		}
		ButtonRemoveEvent(b);
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
		delayedPowerOffCounter = 0;
	}
	ButtonRemoveEvent(b);
}

void PowerMngmtHostPollEvent(void) {
	rtcWakeupEventFlag = 0;
	watchdogTimer = watchdogExpirePeriod;
}

void PowerManagementTask(void) {
	static uint8_t b = 0;//rsoc;

	if (MS_TIME_COUNT(powerMngmtTaskMsCounter) >= 900/*(state == STATE_LOWPOWER?2000:500)*/) {
		MS_TIME_COUNTER_INIT(powerMngmtTaskMsCounter);
	    //uint8_t inc = batteryRsoc >> 2;
		//rsoc = rsoc < batteryRsoc ? rsoc + inc : 0;
		uint8_t r,g;
		if (batteryRsoc > 500) {
			r = 0;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else if (batteryRsoc > 150) {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = 0;
		}
		uint8_t paramB = LedGetParamB(LED_CHARGE_STATUS);
		//uint8_t r = (100 - (batteryRsoc>>4)) * 1;
		//uint8_t g = 170 - r;//250 - (rsoc <= 50 ? (50 - rsoc) * 5 : (rsoc - 50) * 5);
		if (batteryStatus == BAT_STATUS_CHARGING_FROM_IN || batteryStatus == BAT_STATUS_CHARGING_FROM_5V_IO) {//if (chargerStatus == CHG_CHARGING_FROM_IN || chargerStatus == CHG_CHARGING_FROM_USB) {
			b = b?0:paramB;//200 - r;
		} else if (chargerStatus == CHG_CHARGE_DONE) {
			b = paramB;
		} else {
			b = 0;
			//r = LedGetParamR(LED_CHARGE_STATUS);//50;
			//g = 0;
		}

		if (state == STATE_LOWPOWER)
			LedFunctionSetRGB(LED_CHARGE_STATUS, r>>2, g>>2, b);
		else
			LedFunctionSetRGB(LED_CHARGE_STATUS, r, g, b);

		if ( ((batteryRsoc >= wakeupOnCharge && CHARGER_IS_INPUT_PRESENT() && CHARGER_IS_BATTERY_PRESENT()) || rtcWakeupEventFlag) && MS_TIME_COUNT(lastHostCommandTimer) > 15000 && MS_TIME_COUNT(lastWakeupTimer) > 30000 ) {
			if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = 0xFFFF;
				rtcWakeupEventFlag = 0;
				delayedPowerOffCounter = 0;
			}
		}

		if (watchdogExpirePeriod && MS_TIME_COUNT(lastHostCommandTimer) > watchdogTimer) {
			if ( WakeUpHost() == 0 ) {
				wakeupOnCharge = 0xFFFF;
				watchdogExpiredFlag = 1;
				rtcWakeupEventFlag = 0;
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
		if (POW_5V_BOOST_EN_STATUS()) {
			Turn5vBoost(0);
		}
		delayedPowerOffCounter = 0;
	}
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
	uint16_t d = data[1];
	d <<= 8;
	d |= data[0];
	watchdogExpirePeriod = d * (uint32_t)60000;
	watchdogTimer = watchdogExpirePeriod;
}

void PowerMngmtGetWatchdogConfigurationCmd(uint8_t data[], uint16_t *len) {
	 uint16_t d = watchdogExpirePeriod / 60000;
	 data[0] = d;
	 data[1] = d >> 8;
	 *len = 2;
}

void PowerMngmtSetWakeupOnChargeCmd(uint8_t data[], uint16_t len) {
	//wakeupOnChargeConfig = data[0];
	wakeupOnCharge = (data[0]&0x7F) <= 100 ? (data[0]&0x7F) * 10 : 0xFFFF;
	/*if (wakeupOnChargeConfig & 0x80) {
		NvWriteVariableU8(WAKEUPONCHARGE_CONFIG_NV_ADDR, wakeupOnChargeConfig);
	}*/
}

void PowerMngmtGetWakeupOnChargeCmd(uint8_t data[], uint16_t *len)  {
	/*uint16_t var = 0;
	EE_ReadVariable(WAKEUPONCHARGE_CONFIG_NV_ADDR, &var);
	if (((~var)&0xFF) == (var>>8)) {
		if ((wakeupOnChargeConfig & 0x7F) == (var & 0x7F))
			data[0] = var & 0xFF;
		else
			data[0] = wakeupOnChargeConfig;
	} else {*/
		data[0] = wakeupOnCharge <= 1000 ? wakeupOnCharge / 10 : 0x7F;
	//}
	*len = 1;
}
