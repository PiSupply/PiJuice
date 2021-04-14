/*
 * fuel_gauge_lc709203f.h
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#ifndef FUEL_GAUGE_LC709203F_H_
#define FUEL_GAUGE_LC709203F_H_

#include "stdint.h"
#include "battery.h"

#define FUEL_GAUGE_TEMP_MODE_THERMISTOR 	1u
#define FUEL_GAUGE_TEMP_MODE_I2C		 	0u

typedef enum BatteryTempSenseConfig_T {
	BAT_TEMP_SENSE_CONFIG_NOT_USED = 0,
	BAT_TEMP_SENSE_CONFIG_NTC = 1u,
	BAT_TEMP_SENSE_CONFIG_ON_BOARD = 2u,
	BAT_TEMP_SENSE_CONFIG_AUTO_DETECT = 3u,
	BAT_TEMP_SENSE_CONFIG_TYPES = 4u
} BatteryTempSenseConfig_T;


typedef enum RsocMeasurementConfig_T {
	RSOC_MEASUREMENT_FROM_IC = 0u,
	RSOC_MEASUREMENT_DIRECT_DV = 1u,
	RSOC_MEASUREMENT_CONFIG_TYPES
} RsocMeasurementConfig_T;


typedef enum
{
	FUELGAUGE_STATUS_OFFLINE = 0u,
	FUELGAUGE_STATUS_ONLINE = 1u,
} FUELGAUGE_Status_t;


void FUELGAUGE_Init(void);
void FUELGAUGE_Task(void);

void FUELGAUGE_SetConfigData(const uint8_t * const p_data, const uint16_t len);
void FUELGAUGE_GetConfigData(uint8_t * const p_data, uint16_t * const p_len);
void FUELGAUGE_UpdateBatteryProfile(void);
int16_t FUELGAUGE_GetBatteryMaHr(void);
uint16_t FUELGAUGE_GetBatteryMv(void);
BatteryTempSenseConfig_T FUELGAUGE_GetBatteryTempSensorCfg(void);
uint16_t FUELGAUGE_GetSocPt1(void);
int8_t FUELGAUGE_GetBatteryTemperature(void);
uint16_t FUELGAUGE_GetIcId(void);
bool FUELGAUGE_IsNtcOK(void);
bool FUELGAUGE_IsOnline(void);

#endif /* FUEL_GAUGE_LC709203F_H_ */
