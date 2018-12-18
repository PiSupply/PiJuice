/*
 * fuel_gauge_lc709203f.h
 *
 *  Created on: 06.12.2016.
 *      Author: milAN
 */

#ifndef FUEL_GAUGE_LC709203F_H_
#define FUEL_GAUGE_LC709203F_H_

#include "stdint.h"
#include "battery.h"

#define FUEL_GAUGE_TEMP_MODE_THERMISTOR 	1
#define FUEL_GAUGE_TEMP_MODE_I2C		 	0

#define FUEL_GAUGE_TEMP_SENSE_FAULT_STATUS()	((fuelGaugeTempMode != FUEL_GAUGE_TEMP_MODE_THERMISTOR))

extern uint16_t batteryVoltage;
extern uint16_t batteryRsoc;
extern int8_t batteryTemp;
extern int16_t batteryCurrent;
extern uint8_t fuelGaugeI2cErrorCounter;
extern volatile uint8_t fuelGaugeTempMode;

void FuelGaugeInit(void);
void FuelGaugeTask(void);
void FuelGaugeSetBatProfile(const BatteryProfile_T *batProfile);

#endif /* FUEL_GAUGE_LC709203F_H_ */
