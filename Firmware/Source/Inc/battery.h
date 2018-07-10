/*
 * battery.h
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */

#ifndef BATTERY_H_
#define BATTERY_H_

#include "stdint.h"
/*
#define BATTERY_PROFILE (batteryProfiles[batProfileStatus])
#define BATTERY_NONEXIST_PROFILE_ID 0xE0 // ored with stored profile id
#define BATTERY_CUSTOM_PROFILE_ID 0x80
#define BATTERY_INVALID_CUSTOM_PROFILE_STATUS 0x8F
#define BATTERY_DEFAULT_PROFILE_ID 0xFF
#define BATTERY_CONFIG_RES_PROFILE_ID 0x40 // profile id is selected with resistor, ored with id
#define BATTERY_CONFIG_SW_PROFILE_ID 0x20 // profile id is selected with dip switch, ored with id
#define BATTERY_CONFIG_PROFILE_STATUS 0x60 // current profile data are configured with resistor
#define BATTERY_CONFIG_INVALID_PROFILE_STATUS 0x6F //dip switch/resistor configuration is invalid
#define BATTERY_INVALID_PROFILE_ID 0xEF // stored profile id in eeprom is invalid
#define BATTERY_PROFILE_WRITE_BUSY_STATUS 0xF0 // profile id/data write not completed */
// bit 0-3: 0 - 0xE profile id, 0xF - custom profile; bit 4-5: source, 0-host or stored,1-dip switch, 2-resistor; bit 6: validity, 0-valid,1-invalid
// special value 0xFF: default profile id, defined by dip switch or resistor if present
#define BATTERY_PROFILE (batteryProfiles[batProfileStatus])
#define BATTERY_DEFAULT_PROFILE_ID 0xFF
#define BATTERY_NONEXIST_PROFILE_ID ((0x0<<4)|(0x1<<6))//0xE0 // ored with stored profile id
#define BATTERY_CUSTOM_PROFILE_ID 0x0F//((0x00<<4)|(0x00<<6)|0x0F)//0x80
#define BATTERY_INVALID_CUSTOM_PROFILE_STATUS ((0x0<<4)|(0x1<<6)|0x0F)//0x8F
#define BATTERY_CONFIG_RES_PROFILE_ID ((0x2<<4)|(0x0<<6))//0x40 // profile id is selected with resistor, ored with id
#define BATTERY_CONFIG_SW_PROFILE_ID ((0x1<<4)|(0x0<<6))//0x20 // profile id is selected with dip switch, ored with id
#define BATTERY_CONFIG_PROFILE_STATUS ((0x2<<4)|(0x0<<6)|0x0F)//0x60 // current profile data are configured with resistor
#define BATTERY_CONFIG_INVALID_PROFILE_STATUS ((0x2<<4)|(0x1<<6)|0x0F)//0x6F //dip switch/resistor configuration is invalid
#define BATTERY_INVALID_PROFILE_ID ((0x0<<4)|(0x1<<6))//0xEF // stored profile id in eeprom is invalid
#define BATTERY_PROFILE_WRITE_BUSY_STATUS 0xF0 // profile id/data write not completed

typedef struct
{
	uint16_t capacity; // mAh
	uint8_t chargeCurrent; // bq24160 register 5 bit code, 75mA resolution, 550mA offset
	uint8_t terminationCurr; // bq24160 register 3 bit code, 50mA resolution, 50mA offset
	uint8_t regulationVoltage; // bq24160 register 6 bit code, 20mV resolution, 3.5V offset
	uint8_t cutoffVoltage; // discharge cut-off voltage, 20mV resolution
	int8_t tCold; // cold temperature point
	int8_t tCool;
	int8_t tWarm;
	int8_t tHot;
	uint16_t ntcB; // B constant
	uint16_t ntcResistance; // 10ohm unit, 0xFFFF undefined
} BatteryProfile_T;

typedef enum BatteryStatus_T {
	BAT_STATUS_NORMAL = 0,
	BAT_STATUS_CHARGING_FROM_IN,
	BAT_STATUS_CHARGING_FROM_5V_IO,
	BAT_STATUS_NOT_PRESENT
} BatteryStatus_T;

typedef enum BatteryTempSenseConfig_T {
	BAT_TEMP_SENSE_CONFIG_NOT_USED = 0,
	BAT_TEMP_SENSE_CONFIG_NTC,
	BAT_TEMP_SENSE_CONFIG_ON_BOARD,
	BAT_TEMP_SENSE_CONFIG_AUTO_DETECT,
	BAT_TEMP_SENSE_CONFIG_END
} BatteryTempSenseConfig_T;

//extern const BatteryProfile_T batteryProfiles[];
extern uint8_t batProfileStatus;
extern BatteryProfile_T const *currentBatProfile;
extern BatteryStatus_T batteryStatus;
extern BatteryTempSenseConfig_T tempSensorConfig;

void BatteryInit(void);
void BatteryTask(void);
int8_t BatterySetProfileReq(uint8_t id);
const BatteryProfile_T *BatteryGetProfile(void);
int8_t BatteryReadCurrentProfile(uint8_t *data, uint16_t *len);
int8_t BatteryReadProfileStatus(uint8_t *data, uint16_t *len);
int8_t BatteryWriteCustomProfileReq(uint8_t *data, uint16_t len);
int8_t BatterySetTempSenseConfig(uint8_t *data, uint16_t len);
void BatteryGetTempSenseConfig(uint8_t data[], uint16_t *len);

#endif /* BATTERY_H_ */
