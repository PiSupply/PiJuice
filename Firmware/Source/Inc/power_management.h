/*
 * power_management.h
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

typedef enum RunPinInstallationStatus_T {
	RUN_PIN_NOT_INSTALLED = 0,
	RUN_PIN_INSTALLED,
} RunPinInstallationStatus_T;

typedef enum {
	STATE_INIT = 0,
	STATE_NORMAL,
	STATE_RUN,
	STATE_LOWPOWER
} PowerState_T;

extern RunPinInstallationStatus_T runPinInstallationStatus;
extern uint8_t watchdogExpiredFlag;
extern uint8_t rtcWakeupEventFlag;

void PowerManagementInit(void);
void PowerManagementTask(void);
void RunPinInstallationStatusSetConfigCmd(uint8_t data[], uint8_t len);
void RunPinInstallationStatusGetConfigCmd(uint8_t data[], uint16_t *len);
void PowerMngmtSchedulePowerOff(uint8_t dalayCode);
uint8_t PowerMngmtGetPowerOffCounter(void);
void PowerMngmtConfigureWatchdogCmd(uint8_t data[], uint16_t len);
void PowerMngmtGetWatchdogConfigurationCmd(uint8_t data[], uint16_t *len);
void PowerMngmtHostPollEvent(void);
//int8_t WakeUpHost(void);

#endif /* POWER_MANAGEMENT_H_ */
