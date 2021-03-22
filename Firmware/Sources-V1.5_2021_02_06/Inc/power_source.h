/*
 * power_source.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef POWER_SOURCE_H_
#define POWER_SOURCE_H_

#include "battery.h"

#define POW_5V_IN_DETECTION_STATUS_UNKNOWN		0
#define POW_5V_IN_DETECTION_STATUS_NOT_PRESENT	1
#define POW_5V_IN_DETECTION_STATUS_PRESENT	2
#define POW_5V_TURN_ON_TIMEOUT	40

#define POW_SOURCE_NEED_POLL() 			(pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT || MS_TIME_COUNT(pow5vOnTimeout) <= POW_5V_TURN_ON_TIMEOUT)

extern uint32_t pow5vOnTimeout;

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


//extern PowerSourceStatus_T powerInStatus;
//extern PowerSourceStatus_T power5vIoStatus;
extern uint8_t pow5vInDetStatus;
extern uint8_t delayedPowerOff;
extern uint8_t forcedPowerOffFlag;
extern uint8_t forcedVSysOutputOffFlag;


void PowerSourceExitLowPower(void);
void PowerSourceEnterLowPower(void);
void PowerSourceSetBatProfile(const BatteryProfile_T* batProfile);
void PowerSourceSetVSysSwitchState(uint8_t state);

void SetPowerRegulatorConfigCmd(uint8_t data[], uint8_t len);
void GetPowerRegulatorConfigCmd(uint8_t data[], uint16_t *len);

void POWERSOURCE_Init(void);
void POWERSOURCE_Task(void);
void POWERSOURCE_5VIoDetection_Task(void);
void POWERSOURCE_Set5vBoostEnable(const bool enabled);
void POWERSOURCE_SetLDOEnable(const bool enabled);
bool POWERSOURCE_IsVsysEnabled(void);
bool POWERSOURCE_IsBoostConverterEnabled(void);
void POWERSOURCE_SetVSysSwitchState(uint8_t state);
uint8_t POWERSOURCE_GetVSysSwitchState(void);

PowerSourceStatus_T POWERSOURCE_GetVInStatus(void);
PowerSourceStatus_T POWERSOURCE_Get5VRailStatus(void);

#endif /* POWER_SOURCE_H_ */
