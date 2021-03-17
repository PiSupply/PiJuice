/*
 * power_management.h
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_

#define WAKEUP_ONCHARGE_DISABLED_VAL	0xFFFFu

typedef enum RunPinInstallationStatus_T {
	RUN_PIN_NOT_INSTALLED = 0,
	RUN_PIN_INSTALLED,
} RunPinInstallationStatus_T;

extern RunPinInstallationStatus_T runPinInstallationStatus;
extern uint8_t watchdogExpiredFlag;
extern uint8_t rtcWakeupEventFlag;
extern uint8_t ioWakeupEvent;

void PowerManagementInit(void);
#if !defined(RTOS_FREERTOS)
void PowerManagementTask(void);
#endif
void RunPinInstallationStatusSetConfigCmd(uint8_t data[], uint8_t len);
void RunPinInstallationStatusGetConfigCmd(uint8_t data[], uint16_t *len);
void PowerMngmtSchedulePowerOff(uint8_t dalayCode);
uint8_t PowerMngmtGetPowerOffCounter(void);
void PowerMngmtConfigureWatchdogCmd(uint8_t data[], uint16_t len);
void PowerMngmtGetWatchdogConfigurationCmd(uint8_t data[], uint16_t *len);
void PowerMngmtHostPollEvent(void);
//int8_t WakeUpHost(void);
void PowerMngmtSetWakeupOnChargeCmd(uint8_t data[], uint16_t len);
void PowerMngmtGetWakeupOnChargeCmd(uint8_t data[], uint16_t *len);

#endif /* POWER_MANAGEMENT_H_ */
