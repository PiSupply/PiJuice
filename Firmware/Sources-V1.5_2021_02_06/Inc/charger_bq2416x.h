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
/*
#define BAT_REG_VOLTAGE	(regs[3] >> 2)
#define CHARGE_CURRENT	((((int16_t)(regs[5] >> 3) * 75 + 550) / 5 + 1) >> 1)
#define CHARGE_TERMINATION_CURRENT	((((int16_t)(regs[5] && 0x07) * 50 + 50) / 5 + 1) >> 1)
#define CHARGER_IS_BATTERY_PRESENT()	(!((regs[1] & 0x06) == 0x04))//((regs[1] & 0x06) == 0x00 || (regs[1] & 0x06) == 0x02)
#define CHARGER_INSTAT()	((regs[1] >> 6) & 0x03)
#define CHARGER_USBSTAT()	((regs[1] >> 4) & 0x03)
#define CHARGER_IS_DPM_MODE_ACTIVE() 	(regs[6]&0x40)
#define CHARGER_IS_USBIN_LOCKED() 	(regs[1]&0x08)
#define CHRGER_TS_FAULT_STATUS() 	((regs[7u]&0x06u)>>1u)
#define CHARGER_IS_INPUT_PRESENT() ((regs[0]&0x70)&&((regs[0]&0x70)<(6*16)))
#define CHARGER_FAULT_STATUS() (regs[0]&0x07)
*/

typedef enum
{
	CHG_NO_VALID_SOURCE = 0u,
	CHG_IN_READY = 1u,
	CHG_USB_READY = 2u,
	CHG_CHARGING_FROM_IN = 3u,
	CHG_CHARGING_FROM_USB = 4u,
	CHG_CHARGING_FROM_RPI = 4u,
	CHG_CHARGE_DONE = 5u,
	CHG_NA = 6u,
	CHG_FAULT = 7u
} ChargerStatus_T;


typedef enum
{
	CHG_FAULT_NORMAL = 0u,
	CHG_FAULT_THERMAL_SHUTDOWN = 1u,
	CHG_FAULT_BATTERY_TEMPERATURE_FAULT = 2u,
	CHG_FAULT_WATCHDOG_TIMER_EXPIRED = 3u,
	CHG_FAULT_SAFETY_TIMER_EXPIRED = 4u,
	CHG_FAULT_IN_SUPPLY_FAULT = 5u,
	CHG_FAULT_USB_SUPPLY_FAULT = 6u,
	CHG_FAULT_BATTERY_FAULT = 7u
} ChargerFaultStatus_T;


typedef enum NTC_MonitorTemperature_T
{
	COLD,
	COOL,
	NORMAL,
	WARM,
	HOT,
	UNKNOWN
} NTC_MonitorTemperature_T;


typedef enum ChargerUSBInLockoutStatus_T
{
	CHG_USB_IN_UNKNOWN,
	CHG_USB_IN_LOCK,
	CHG_USB_IN_UNLOCK
} CHARGER_USBInLockoutStatus_T;


typedef enum ChargerUsbInCurrentLimit_T
{
	CHG_IUSB_LIMIT_100MA = 0,
	CHG_IUSB_LIMIT_150MA,
	CHG_IUSB_LIMIT_500MA,
	CHG_IUSB_LIMIT_800MA,
	CHG_IUSB_LIMIT_900MA,
	CHG_IUSB_LIMIT_1500MA,
} CHARGER_USBInCurrentLimit_T;


typedef enum
{
	CHARGER_INPUT_NORMAL = 0u,
	CHARGER_INPUT_OVP = 1u,
	CHARGER_INPUT_WEAK = 2u,
	CHARGER_INPUT_UVP = 3u
} CHARGER_InputStatus_t;


typedef enum
{
	CHARGER_BATTERY_NORMAL = 0u,
	CHARGER_BATTERY_OVP = 1u,
	CHARGER_BATTERY_NOT_PRESENT = 2u
} CHARGER_BatteryStatus_t;


typedef enum
{
	CHARGER_STATUS_OFFLINE = 0u,
	CHARGER_STATUS_ONLINE = 1u,
} CHARGER_DeviceStatus_t;


void CHARGER_Init(void);
void CHARGER_Task(void);

void CHARGER_SetInputsConfig(const uint8_t config);
uint8_t CHARGER_GetInputsConfig(void);
void CHARGER_SetChargeEnableConfig(const uint8_t config);
uint8_t CHARGER_GetChargeEnableConfig(void);

ChargerStatus_T CHARGER_GetStatus(void);
void CHARGER_SetInterrupt(void);

void CHARGER_SetRPi5vInputEnable(bool enable);
bool CHARGER_GetRPi5vInputEnable(void);

void CHARGER_RPi5vInCurrentLimitSetMin(void);
void CHARGER_RPi5vInCurrentLimitStepDown(void);
void CHARGER_RPi5vInCurrentLimitStepUp(void);
uint8_t CHARGER_GetRPiChargeInputLevel(void);

void CHARGER_RefreshSettings(void);
bool CHARGER_RequirePoll(void);
bool CHARGER_GetNoBatteryTurnOnEnable(void);

bool CHARGER_IsChargeSourceAvailable(void);
bool CHARGER_IsCharging(void);

bool CHARGER_IsBatteryPresent(void);
CHARGER_BatteryStatus_t CHARGER_GetBatteryStatus(void);

bool CHARGER_HasTempSensorFault(void);
uint8_t CHARGER_GetTempFault(void);
CHARGER_InputStatus_t CHARGER_GetInputStatus(uint8_t channel);
bool CHARGER_IsDPMActive(void);
uint8_t CHARGER_GetFaultStatus(void);
void CHARGER_UpdateBatteryProfile(void);

#endif /* CHARGER_BQ2416X_H_ */
