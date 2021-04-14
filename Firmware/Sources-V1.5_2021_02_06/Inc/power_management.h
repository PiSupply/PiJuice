/*
 * power_management.h
 *
 *  Created on: 04.04.2017.
 *      Author: milan
 */

#ifndef POWER_MANAGEMENT_H_
#define POWER_MANAGEMENT_H_


#define WAKEUP_ONCHARGE_DISABLED_VAL	0xFFFFu
#define RPI_ACTLED_IO_PIN				1u
#define GPIO_POWERCONTROL				1u


typedef enum
{
	RUN_PIN_NOT_INSTALLED = 0,
	RUN_PIN_INSTALLED,
} RunPinInstallationStatus_T;



void POWERMAN_Init(void);
void POWERMAN_Task(void);

bool POWERMAN_CanShutDown(void);
uint8_t POWERMAN_GetPowerOffTime(void);
bool POWERMAN_GetWatchdogExpired(void);
void POWERMAN_ClearWatchdog(void);
bool POWERMAN_GetPowerButtonPressedStatus(void);
void POWERMAN_SetWakeupOnChargePt1(const uint16_t newValue);
void POWERMAN_SetWakeupOnChargeData(const uint8_t * const data, const uint16_t len);
void POWERMAN_GetWakeupOnChargeData(uint8_t * const data, uint16_t * const len);
void POWERMAN_ClearPowerButtonPressed(void);
void POWERMAN_SetWakeupOnChargePt1(const uint16_t newValue);
void POWERMAN_SetRunPinConfigData(const uint8_t * const p_data, const uint8_t len);
void POWERMAN_GetRunPinConfigData(uint8_t * const p_data, uint16_t * const p_len);
void POWERMAN_SetWatchdogConfigData(const uint8_t * const p_data, const uint16_t len);
void POWERMAN_GetWatchdogConfigData(uint8_t * const p_data, uint16_t * const p_len);
void POWERMAN_SchedulePowerOff(const uint8_t dalayCode);


#endif /* POWER_MANAGEMENT_H_ */
