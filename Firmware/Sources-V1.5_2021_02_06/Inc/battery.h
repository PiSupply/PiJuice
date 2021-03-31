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
//#define BATTERY_PROFILE (batteryProfiles[batProfileStatus])
//#define BATTERY_DEFAULT_PROFILE_ID 0xFF
//#define BATTERY_NONEXIST_PROFILE_ID ((0x0<<4)|(0x1<<6))//0xE0 // ored with stored profile id
//#define BATTERY_CUSTOM_PROFILE_ID ((uint8_t)0x0F)//((0x00<<4)|(0x00<<6)|0x0F)//0x80
//#define BATTERY_INVALID_CUSTOM_PROFILE_STATUS ((0x0<<4)|(0x1<<6)|0x0F)//0x8F
//#define BATTERY_CONFIG_RES_PROFILE_ID ((0x2<<4)|(0x0<<6))//0x40 // profile id is selected with resistor, ored with id
//#define BATTERY_CONFIG_SW_PROFILE_ID ((0x1<<4)|(0x0<<6))//0x20 // profile id is selected with dip switch, ored with id
//#define BATTERY_CONFIG_PROFILE_STATUS ((0x2<<4)|(0x0<<6)|0x0F)//0x60 // current profile data are configured with resistor
//#define BATTERY_CONFIG_INVALID_PROFILE_STATUS ((0x2<<4)|(0x1<<6)|0x0F)//0x6F //dip switch/resistor configuration is invalid
//#define BATTERY_INVALID_PROFILE_ID ((0x0<<4)|(0x1<<6))//0xEF // stored profile id in eeprom is invalid
//#define BATTERY_NONEXIST_PROFILE_ID	((0x0<<4)|(0x1<<6))


typedef enum
{
	BATTERY_CUSTOM_PROFILE_ID = 0x0Fu,
	BATTERY_CONFIG_SW_PROFILE_ID = 0x10u,
	BATTERY_CONFIG_RES_PROFILE_ID = 0x20u,
	BATTERY_CONFIG_PROFILE_STATUS = 0x2Fu,
	BATTERY_CONFIG_INVALID_PROFILE_STATUS = 0x7Fu,
	BATTERY_PROFILE_STATUS_CUSTOM_PROFILE_INVALID = 0x4Fu,
	BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID = 0x40u,
	BATTERY_PROFILE_STATUS_WRITE_BUSY = 0xF0u,
} BATTERY_ProfileStatus_t;


typedef enum
{
	BAT_CHEMISTRY_LIPO = 0u,
	BAT_CHEMISTRY_LIFEPO4 = 1u,
	BAT_CHEMISTRY_LIPO_GRAPHENE = 2u,
	BATTERY_CHEMISTRY_NOT_SET = 0xFFu
} BatteryChemistry_T;


typedef struct
{
	BatteryChemistry_T chemistry;
	uint32_t capacity; // mAh
	uint8_t chargeCurrent; // bq24160 register 5 bit code, 75mA resolution, 550mA offset
	uint8_t terminationCurr; // bq24160 register 3 bit code, 50mA resolution, 50mA offset
	uint8_t regulationVoltage; // bq24160 register 6 bit code, 20mV resolution, 3.5V offset
	uint8_t cutoffVoltage; // discharge cut-off voltage, 20mV resolution
	uint16_t ocv10, ocv50, ocv90;
	uint16_t r10, r50, r90;
	int8_t tCold; // cold temperature point
	int8_t tCool;
	int8_t tWarm;
	int8_t tHot;
	uint16_t ntcB; // B constant
	uint16_t ntcResistance; // 10ohm unit, 0xFFFF undefined
} BatteryProfile_T;


typedef enum BatteryStatus_T {
	BAT_STATUS_NORMAL = 0u,
	BAT_STATUS_CHARGING_FROM_IN = 1u,
	BAT_STATUS_CHARGING_FROM_5V_IO = 2u,
	BAT_STATUS_NOT_PRESENT = 3u
} BatteryStatus_T;


void BATTERY_Init(void);
void BATTERY_Task(void);

BatteryStatus_T BATTERY_GetStatus(void);
const BatteryProfile_T * BATTERY_GetActiveProfileHandle(void);

void BATTERY_ReadActiveProfileData(uint8_t * const data, uint16_t * const len);
void BATTERY_ReadActiveProfileExtendedData(uint8_t * const data, uint16_t * const len);

void BATTERY_WriteCustomProfileData(const uint8_t * const data, const uint16_t len);
void BATTERY_WriteCustomProfileExtendedData(const uint8_t * const data, const uint16_t len);

void BATTERY_SetProfileReq(const uint8_t id);
void BATTERY_ReadProfileStatus(uint8_t * const data, uint16_t * const len);

#endif /* BATTERY_H_ */
