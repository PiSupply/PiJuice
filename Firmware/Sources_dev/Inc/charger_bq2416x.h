/*
 * charger_bq2416x.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef CHARGER_BQ2416X_H_
#define CHARGER_BQ2416X_H_

#include <battery.h>
#include "stdint.h"

#define BAT_REG_VOLTAGE	(regs[3] >> 2)
#define CHARGE_CURRENT	((((int16_t)(regs[5] >> 3) * 75 + 550) / 5 + 1) >> 1)
#define CHARGE_TERMINATION_CURRENT	((((int16_t)(regs[5] && 0x07) * 50 + 50) / 5 + 1) >> 1)
#define CHARGER_IS_BATTERY_PRESENT()	(!((regs[1] & 0x06) == 0x04))//((regs[1] & 0x06) == 0x00 || (regs[1] & 0x06) == 0x02)
#define CHARGER_INSTAT()	((regs[1] >> 6) & 0x03)
#define CHARGER_USBSTAT()	((regs[1] >> 4) & 0x03)
#define CHARGER_IS_DPM_MODE_ACTIVE() 	(regs[6]&0x40)
#define CHARGER_IS_USBIN_LOCKED() 	(regs[1]&0x08)
#define CHRGER_TS_FAULT_STATUS() 	(regs[7]&0x06>>1)
#define CHARGER_IS_INPUT_PRESENT() ((regs[0]&0x70)&&((regs[0]&0x70)<(6*16)))
#define CHARGER_FAULT_STATUS() (regs[0]&0x07)

typedef enum ChargerStatus_T {
	CHG_NO_VALID_SOURCE = 0,
	CHG_IN_READY,
	CHG_USB_READY,
	CHG_CHARGING_FROM_IN,
	CHG_CHARGING_FROM_USB,
	CHG_CHARGE_DONE,
	CHG_NA,
	CHG_FAULT
} ChargerStatus_T;

typedef enum NTC_MonitorTemperature_T {
	COLD,
	COOL,
	NORMAL,
	WARM,
	HOT,
	UNKNOWN
} NTC_MonitorTemperature_T;

typedef enum ChargerUSBInLockoutStatus_T {
	CHG_USB_IN_UNKNOWN,
	CHG_USB_IN_LOCK,
	CHG_USB_IN_UNLOCK
} ChargerUSBInLockoutStatus_T;

typedef enum ChargerUsbInCurrentLimit_T {
	CHG_IUSB_LIMIT_100MA = 0,
	CHG_IUSB_LIMIT_150MA,
	CHG_IUSB_LIMIT_500MA,
	CHG_IUSB_LIMIT_800MA,
	CHG_IUSB_LIMIT_900MA,
	CHG_IUSB_LIMIT_1500MA,
} ChargerUsbInCurrentLimit_T;

typedef enum ChargerFaultStatus_T {
	CHG_FAULT_NORMAL = 0,
	CHG_FAULT_THERMAL_SHUTDOWN,
	CHG_FAULT_BATTERY_TEMPERATURE_FAULT,
	CHG_FAULT_WATCHDOG_TIMER_EXPIRED,
	CHG_FAULT_SAFETY_TIMER_EXPIRED,
	CHG_FAULT_IN_SUPPLY_FAULT,
	CHG_FAULT_USB_SUPPLY_FAULT,
	CHG_FAULT_BATTERY_FAULT,
	CHG_FAULT_UNKNOWN
} ChargerFaultStatus_T;

extern uint8_t regs[8];

extern ChargerStatus_T chargerStatus;
extern uint8_t chargerNeedPoll;
extern ChargerUSBInLockoutStatus_T usbInLockoutStatus;
extern uint8_t chargerI2cErrorCounter;
extern uint8_t chargerInterruptFlag;

void ChargerInit();
#if !defined(RTOS_FREERTOS)
void ChargerTask(void);
#endif
void ChargerTriggerNTCMonitor(NTC_MonitorTemperature_T temp);
void ChargerSetUSBLockout(ChargerUSBInLockoutStatus_T status);
void SetChargeCurrentReq(uint8_t current);
void SetChargeTerminationCurrentReq(uint8_t current);
void SetBatRegulationVoltageReq(uint8_t voltageCode);
void ChargerSetBatProfileReq(const BatteryProfile_T* batProfile);
void ChargerUsbInCurrentLimitStepUp(void);
void ChargerUsbInCurrentLimitSetMin(void);
void ChargerUsbInCurrentLimitStepDown(void);
void ChargerWriteInputsConfig(uint8_t config);
uint8_t ChargerReadInputsConfig(void);
void ChargerWriteChargingConfig(uint8_t config);
uint8_t ChargerReadChargingConfig(void);

#endif /* CHARGER_BQ2416X_H_ */
