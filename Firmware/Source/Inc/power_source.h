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

#define POW_5V_BOOST_EN_STATUS()	 	(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET)
#define POW_SOURCE_NEED_POLL() 	(pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT || MS_TIME_COUNT(pow5vOnTimeout) <= POW_5V_TURN_ON_TIMEOUT)
#define POW_VSYS_OUTPUT_EN_STATUS()	 	(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET)

#define REGULATOR_5V_SWITCHING_STATUS_SUCCESS		0
#define REGULATOR_5V_SWITCHING_STATUS_NO_ENERGY		1

extern uint32_t pow5vOnTimeout;

typedef enum PowerSourceStatus_T {
	POW_SOURCE_NOT_PRESENT = 0,
	POW_SOURCE_BAD,
	POW_SOURCE_WEAK,
	POW_SOURCE_NORMAL
} PowerSourceStatus_T;

typedef enum PowerRegulatorConfig_T {
	POW_REGULATOR_MODE_POW_DET = 0,
	POW_REGULATOR_MODE_LDO,
	POW_REGULATOR_MODE_DCDC,
	POW_REGULATOR_MODE_END
} PowerRegulatorConfig_T;

extern PowerSourceStatus_T powerInStatus;
extern PowerSourceStatus_T power5vIoStatus;
extern uint8_t pow5vInDetStatus;
extern uint8_t delayedPowerOff;
extern uint8_t forcedPowerOffFlag;
extern uint8_t forcedVSysOutputOffFlag;

void PowerSourceInit(void);
#if !defined(RTOS_FREERTOS)
void PowerSourceTask(void);
void PowerSource5vIoDetectionTask(void);
#endif
void PowerSourceExitLowPower(void);
void PowerSourceEnterLowPower(void);
void PowerSourceSetBatProfile(const BatteryProfile_T* batProfile);
void PowerSourceSetVSysSwitchState(uint8_t state);
uint8_t PowerSourceGetVSysSwitchState();
int8_t Turn5vBoost(uint8_t onOff);
void Power5VSetModeLDO(void);
void SetPowerRegulatorConfigCmd(uint8_t data[], uint8_t len);
void GetPowerRegulatorConfigCmd(uint8_t data[], uint16_t *len);

#endif /* POWER_SOURCE_H_ */
