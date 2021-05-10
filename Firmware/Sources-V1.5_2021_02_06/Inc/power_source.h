/*
 * power_source.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef POWER_SOURCE_H_
#define POWER_SOURCE_H_

#include "battery.h"

typedef enum
{
	RPI5V_DETECTION_STATUS_UNKNOWN = 0u,
	RPI5V_DETECTION_STATUS_UNPOWERED = 1u,
	RPI5V_DETECTION_STATUS_POWERED = 2u
} POWERSOURCE_RPi5VStatus_t;

typedef enum PowerSourceStatus_T {
	POW_SOURCE_NOT_PRESENT = 0,
	POW_SOURCE_BAD,
	POW_SOURCE_WEAK,
	POW_SOURCE_NORMAL
} PowerSourceStatus_T;

typedef enum PowerRegulatorConfig_T {
	POW_REGULATOR_MODE_POW_DET = 0u,
	POW_REGULATOR_MODE_LDO = 1u,
	POW_REGULATOR_MODE_DCDC = 2u,
	POW_REGULATOR_MODE_COUNT = 3u
} PowerRegulatorConfig_T;


void POWERSOURCE_Init(void);
void POWERSOURCE_Task(void);
void POWERSOURCE_5VIoDetection_Task(void);
void POWERSOURCE_Set5vBoostEnable(const bool enabled);
void POWERSOURCE_SetLDOEnable(const bool enabled);
bool POWERSOURCE_IsVsysEnabled(void);
bool POWERSOURCE_IsBoostConverterEnabled(void);
bool POWERSOURCE_IsLDOEnabled(void);
void POWERSOURCE_SetVSysSwitchState(uint8_t state);
uint8_t POWERSOURCE_GetVSysSwitchState(void);
void POWERSOURCE_UpdateBatteryProfile(const BatteryProfile_T* batProfile);

PowerSourceStatus_T POWERSOURCE_GetVInStatus(void);
PowerSourceStatus_T POWERSOURCE_Get5VRailStatus(void);
POWERSOURCE_RPi5VStatus_t POWERSOURCE_GetRPi5VPowerStatus(void);

void POWERSOURCE_SetRegulatorConfigData(const uint8_t * const data, const uint8_t len);
void POWERSOURCE_GetRegulatorConfigData(uint8_t * const data, uint16_t * const len);

bool POWERSOURCE_NeedPoll(void);
bool POWERSOURCE_GetForcedPowerOffStatus(void);
bool POWERSOURCE_GetForcedVSysOutputOffStatus(void);
void POWERSOURCE_ClearForcedPowerOff(void);
void POWERSOURCE_ClearForcedVSysOutputOff(void);

#endif /* POWER_SOURCE_H_ */
