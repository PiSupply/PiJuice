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

#define FUEL_GAUGE_TEMP_MODE_THERMISTOR 	1
#define FUEL_GAUGE_TEMP_MODE_I2C		 	0

#define FUEL_GAUGE_TEMP_SENSE_FAULT_STATUS()	(ntcFaultFlag)
#define FUEL_GAUGE_IC_FAULT_STATUS()			( rsocMeasurementConfig==RSOC_MEASUREMENT_AUTO_DETECT && fgIcId>0 && fgIcId<0xFFFF && (fuelGaugeI2cErrorCounter <= -5 || fuelGaugeI2cErrorCounter >= 5) )

typedef enum BatteryTempSenseConfig_T {
	BAT_TEMP_SENSE_CONFIG_NOT_USED = 0,
	BAT_TEMP_SENSE_CONFIG_NTC,
	BAT_TEMP_SENSE_CONFIG_ON_BOARD,
	BAT_TEMP_SENSE_CONFIG_AUTO_DETECT,
	BAT_TEMP_SENSE_CONFIG_END
} BatteryTempSenseConfig_T;

typedef enum RsocMeasurementConfig_T {
	RSOC_MEASUREMENT_AUTO_DETECT = 0,
	RSOC_MEASUREMENT_DIRECT_DV,
	RSOC_MEASUREMENT_CONFIG_END
} RsocMeasurementConfig_T;

extern uint16_t batteryVoltage;
extern uint16_t batteryRsoc;
extern int8_t batteryTemp;
extern volatile int16_t batteryCurrent;
extern int8_t fuelGaugeI2cErrorCounter;
extern volatile uint8_t fuelGaugeTempMode;
extern BatteryTempSenseConfig_T tempSensorConfig;
extern uint16_t fgIcId;
extern int8_t ntcFaultFlag;
extern RsocMeasurementConfig_T rsocMeasurementConfig;

void FuelGaugeInit(void);
#if !defined(RTOS_FREERTOS)
void FuelGaugeTask(void);
#endif
void FuelGaugeSetBatProfile(const BatteryProfile_T *batProfile);
int8_t FuelGaugeSetConfig(uint8_t *data, uint16_t len);
void FuelGaugeGetConfig(uint8_t data[], uint16_t *len);
int8_t FuelGaugeIcPreInit(void);

#endif /* FUEL_GAUGE_LC709203F_H_ */
