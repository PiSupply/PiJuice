/*
 * battery.c
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */


#include "battery.h"
#include "nv.h"
#include "charger_bq2416x.h"
#include "config_switch_resistor.h"

#define BATTERY_PROFILES_COUNT() ((sizeof(batteryProfiles)/sizeof(BatteryProfile_T)))

static int16_t setProfileReq = -1;

static int8_t writeCustomProfileReq = 0;
BatteryProfile_T customBatProfileReq;

BatteryStatus_T batteryStatus = BAT_STATUS_NOT_PRESENT;

const BatteryProfile_T batteryProfiles[] = {
	{ 	// BP6X battery
		1400, // 1400mAh
		0x04, // 850mA, ~0.6C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// BP7X battery
		1820, // 1820mAh
		0x05, // 925mA, ~0.5C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// SNN5843 battery
		2300, // 2300mAh
		0x08, // 1150mA, ~0.5C
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// PKCELL LIPO8047109 battery
		5000, // 5000mAh
		0x1A, // 2500mA, ~0.5C
		0x01,//0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		1,
		2,
		45,
		50,
		0x0D34,
		1000, // 10K
	}
};

BatteryProfile_T customBatProfile = {
	 	// default battery
		1400, // 1400mAh
		0x04, // 850mA
		0x01, // 100mA
		0x1E, // 4.1V
		150, // 3V
		1,
		10,
		45,
		60,
		0xFFFF,
		0xFFFF,
};

// Resistor charge parameters config code
// code: v2 v1 v0 t1 c2 c1 c0
// v value: (code>>4 * 5 + 5) * 20mV + 3.5V, vcode = (code>>4) * 5 + 5
// c value: code&0x07 * 300mA + 550mA, ccode = code&0x07
// t value: c2t0 * 100mA + 50mA, tcode = (code&0x04>>1) | (code&0x08>>3)


//static BatteryProfile_T nvBatProfileParamAddr;

uint8_t batProfileStatus = BATTERY_INVALID_PROFILE_ID;

BatteryProfile_T const *currentBatProfile = NULL;

BatteryTempSenseConfig_T tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;

int8_t BatReadEEprofileData(void);
void BatWriteEEprofileData(BatteryProfile_T *batProfile);

void BatInitProfile(uint8_t initPofileId) {
	//uint16_t var;

	if ( initPofileId == BATTERY_CUSTOM_PROFILE_ID ) {
		if (BatReadEEprofileData() == 0) {
			currentBatProfile = &customBatProfile;
			batProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
		} else {
			currentBatProfile = NULL;
			batProfileStatus = BATTERY_INVALID_CUSTOM_PROFILE_STATUS;
		}
	} else if (initPofileId == BATTERY_DEFAULT_PROFILE_ID) {
		// Make profile data based on dip switch or resistor configuration
		if ( switchConfigCode >= 0) {
			if ( switchConfigCode < BATTERY_PROFILES_COUNT() && resistorConfig1Code7 == -1 && resistorConfig2Code4 == -1 ){
				// Use switch coded profile id
				currentBatProfile = &batteryProfiles[switchConfigCode];
				batProfileStatus = BATTERY_CONFIG_SW_PROFILE_ID | switchConfigCode;
			} else if ( switchConfigCode == 1 && resistorConfig2Code4 >= 0 && resistorConfig2Code4 < BATTERY_PROFILES_COUNT() ){
				currentBatProfile = &batteryProfiles[resistorConfig2Code4];
				batProfileStatus = BATTERY_CONFIG_RES_PROFILE_ID | resistorConfig2Code4;
			} else if ( switchConfigCode == 1 && resistorConfig1Code7 >= 0 ){
				customBatProfile.chargeCurrent = ((resistorConfig1Code7&0x07) << 2); // offset 550mA
				customBatProfile.capacity = ((int16_t)customBatProfile.chargeCurrent * 75 + 550) * 2; // suppose charge current is 0.5 capacity
				customBatProfile.terminationCurr = (resistorConfig1Code7&0x04) | ((resistorConfig1Code7&0x08)>>2);
				customBatProfile.regulationVoltage = (resistorConfig1Code7>>4) * 5 + 5;
				customBatProfile.capacity = 0xFFFF; // undefined
				customBatProfile.cutoffVoltage = 150; // 3v
				customBatProfile.ntcB = 0x0D34;
				customBatProfile.ntcResistance = 1000;
				customBatProfile.tCold = 1;
				customBatProfile.tCool = 10;
				customBatProfile.tWarm = 45;
				customBatProfile.tHot = 60;

				batProfileStatus = BATTERY_CONFIG_PROFILE_STATUS;
				currentBatProfile = &customBatProfile;
			} else {
				currentBatProfile = NULL;
				batProfileStatus = BATTERY_CONFIG_INVALID_PROFILE_STATUS;
			}
		} else {
			currentBatProfile = NULL;
			batProfileStatus = BATTERY_CONFIG_INVALID_PROFILE_STATUS;
		}
	} else if (initPofileId < BATTERY_PROFILES_COUNT()) {
		batProfileStatus = initPofileId;
		currentBatProfile = &batteryProfiles[batProfileStatus];
	} else if (initPofileId >= BATTERY_PROFILES_COUNT() && initPofileId < 15) {//32) {
		currentBatProfile = NULL;
		batProfileStatus = BATTERY_NONEXIST_PROFILE_ID | initPofileId; // non defined  profile
	} else {
		currentBatProfile = NULL;
		batProfileStatus = BATTERY_INVALID_PROFILE_ID;
	}
}

void BatteryInit(void) {
	uint16_t var;

	EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);
	if ( ((var^0xFF)&0xFF) != (var>>8) ) {//if (!NV_IS_DATA_INITIALIZED) {
		EE_WriteVariable(BAT_PROFILE_NV_ADDR, 0x00FF);
	}

	EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);
	if ( ((var^0xFF)&0xFF) == (var>>8) ) {
		// if crc correct
		BatInitProfile(var&0xFF);
	}

	var = 0;
	EE_ReadVariable(BAT_TEMP_SENSE_CONFIG, &var);
	if (((~var)&0xFF) == (var>>8)) {
		tempSensorConfig = var&0xFF;
	}
}

int8_t BatReadEEprofileData(void) {
	uint8_t dataValid = 1;
	uint16_t var;
	EE_ReadVariable(BAT_CAPACITY_NV_ADDR, &var);
	customBatProfile.capacity = var;

	EE_ReadVariable(CHARGE_CURRENT_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8)); // upper byte should be complement of lower
	customBatProfile.chargeCurrent = var&0xFF;

	EE_ReadVariable(CHARGE_TERM_CURRENT_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.terminationCurr = var&0xFF;

	EE_ReadVariable(BAT_REG_VOLTAGE_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.regulationVoltage = var&0xFF;

	EE_ReadVariable(BAT_CUTOFF_VOLTAGE_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.cutoffVoltage = var&0xFF;

	EE_ReadVariable(BAT_TEMP_COLD_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.tCold = var&0xFF;

	EE_ReadVariable(BAT_TEMP_COOL_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.tCool = var&0xFF;

	EE_ReadVariable(BAT_TEMP_WARM_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.tWarm = var&0xFF;

	EE_ReadVariable(BAT_TEMP_HOT_NV_ADDR, &var);
	dataValid = dataValid && (((~var)&0xFF) == (var>>8));
	customBatProfile.tHot = var&0xFF;

	EE_ReadVariable(BAT_NTC_B_NV_ADDR, &var);
	customBatProfile.ntcB = var;
	uint16_t ntcCrc = var;

	EE_ReadVariable(BAT_NTC_RESISTANCE_NV_ADDR, &var);
	customBatProfile.ntcResistance = var;
	ntcCrc ^= var;

	EE_ReadVariable(BAT_NTC_CRC_NV_ADDR, &var);
	dataValid = dataValid &&  (var == ntcCrc);

	return !dataValid; // return 0 if valid
}

void BatWriteEEprofileData(BatteryProfile_T *batProfile) {
	EE_WriteVariable(BAT_CAPACITY_NV_ADDR, batProfile->capacity);
	NvWriteVariableU8(CHARGE_CURRENT_NV_ADDR, batProfile->chargeCurrent);//EE_WriteVariable(CHARGE_CURRENT_NV_ADDR, batProfile->chargeCurrent | ((uint16_t)~batProfile->chargeCurrent<<8));
	NvWriteVariableU8(CHARGE_TERM_CURRENT_NV_ADDR, batProfile->terminationCurr);//EE_WriteVariable(CHARGE_TERM_CURRENT_NV_ADDR, batProfile->terminationCurr | ((uint16_t)~batProfile->terminationCurr<<8));
	NvWriteVariableU8(BAT_REG_VOLTAGE_NV_ADDR, batProfile->regulationVoltage);//EE_WriteVariable(BAT_REG_VOLTAGE_NV_ADDR, batProfile->regulationVoltage | ((uint16_t)~batProfile->regulationVoltage<<8));
	NvWriteVariableU8(BAT_CUTOFF_VOLTAGE_NV_ADDR, batProfile->cutoffVoltage);//EE_WriteVariable(BAT_CUTOFF_VOLTAGE_NV_ADDR, batProfile->cutoffVoltage | ((uint16_t)~batProfile->cutoffVoltage<<8));
	NvWriteVariableU8(BAT_TEMP_COLD_NV_ADDR, batProfile->tCold);//EE_WriteVariable(BAT_TEMP_COLD_NV_ADDR, batProfile->tCold | ((uint16_t)~batProfile->tCold<<8));
	NvWriteVariableU8(BAT_TEMP_COOL_NV_ADDR, batProfile->tCool);//EE_WriteVariable(BAT_TEMP_COOL_NV_ADDR, batProfile->tCool | ((uint16_t)~batProfile->tCool<<8));
	NvWriteVariableU8(BAT_TEMP_WARM_NV_ADDR, batProfile->tWarm);//EE_WriteVariable(BAT_TEMP_WARM_NV_ADDR, batProfile->tWarm | ((uint16_t)~batProfile->tWarm<<8));
	NvWriteVariableU8(BAT_TEMP_HOT_NV_ADDR, batProfile->tHot);//EE_WriteVariable(BAT_TEMP_HOT_NV_ADDR, batProfile->tHot | ((uint16_t)~batProfile->tHot<<8));
	EE_WriteVariable(BAT_NTC_B_NV_ADDR, batProfile->ntcB);
	EE_WriteVariable(BAT_NTC_RESISTANCE_NV_ADDR, batProfile->ntcResistance);
	EE_WriteVariable(BAT_NTC_CRC_NV_ADDR, batProfile->ntcB ^ batProfile->ntcResistance);
}

void BatteryTask(void) {
	if (setProfileReq >= 0) {
		uint8_t id = setProfileReq;
		setProfileReq = -1;
		EE_WriteVariable(BAT_PROFILE_NV_ADDR, id | ((uint16_t)(~id)<<8));
		uint16_t var;
		EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);
		if ( ((var^0xFF)&0xFF) == (var>>8) ) {
			// if fcs correct
			BatInitProfile(var&0xFF);
		} else {
			currentBatProfile = NULL;
			batProfileStatus = BATTERY_INVALID_PROFILE_ID;
		}

		ChargerSetBatProfileReq(currentBatProfile);
		PowerSourceSetBatProfile(currentBatProfile);
		FuelGaugeSetBatProfile(currentBatProfile);
	}

	if (writeCustomProfileReq) {
		writeCustomProfileReq = 0;
		BatWriteEEprofileData(&customBatProfileReq);
		if (BatReadEEprofileData() == 0) {
			batProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
			currentBatProfile = &customBatProfile;
		} else {
			batProfileStatus = BATTERY_INVALID_CUSTOM_PROFILE_STATUS;
			currentBatProfile = NULL;
		}

		uint16_t var;
		EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);
		if ( ((var&0xFF) != BATTERY_CUSTOM_PROFILE_ID) || (((var^0xFF)&0xFF) != (var>>8)) ) {
			uint16_t var;
			EE_WriteVariable(BAT_PROFILE_NV_ADDR, BATTERY_CUSTOM_PROFILE_ID | ((uint16_t)~BATTERY_CUSTOM_PROFILE_ID<<8));
			EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);
			if (((var^0xFF)&0xFF) == (var>>8) && (var&0xFF) == BATTERY_CUSTOM_PROFILE_ID) {  // upper byte should be complement if data are valid
				if (currentBatProfile != NULL)
					batProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
				else
					batProfileStatus = BATTERY_INVALID_CUSTOM_PROFILE_STATUS;
			} else {
				currentBatProfile = NULL;
				batProfileStatus = BATTERY_INVALID_PROFILE_ID;
			}
		}

		ChargerSetBatProfileReq(currentBatProfile);
		PowerSourceSetBatProfile(currentBatProfile);
		FuelGaugeSetBatProfile(currentBatProfile);
	}

	if (!CHARGER_IS_BATTERY_PRESENT()) {
		batteryStatus = BAT_STATUS_NOT_PRESENT;
	} else if (chargerStatus == CHG_CHARGING_FROM_IN) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_IN;
	} else if (chargerStatus == CHG_CHARGING_FROM_USB) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_5V_IO;
	} else {
		batteryStatus = BAT_STATUS_NORMAL;
	}
}

int8_t BatterySetProfileReq(uint8_t id) {
	if (batProfileStatus != id) {
		batProfileStatus = BATTERY_PROFILE_WRITE_BUSY_STATUS;
		setProfileReq = id;
	}
	/*if (id < BATTERY_PROFILES_COUNT() || id == BATTERY_CUSTOM_PROFILE_ID || id == BATTERY_DEFAULT_PROFILE_ID) {
		setProfileReq = id;
		return 0;
	} else {
		return -1;
	}*/
	return 0;
}

int8_t BatteryWriteCustomProfileReq(uint8_t *data, uint16_t len) {
	batProfileStatus = BATTERY_PROFILE_WRITE_BUSY_STATUS;

	customBatProfileReq.capacity = (((uint16_t)data[1])<<8) | data[0];
	customBatProfileReq.chargeCurrent = data[2];
	customBatProfileReq.terminationCurr = data[3];
	customBatProfileReq.regulationVoltage = data[4];
	customBatProfileReq.cutoffVoltage = data[5];
	customBatProfileReq.tCold = data[6];
	customBatProfileReq.tCool = data[7];
	customBatProfileReq.tWarm = data[8];
	customBatProfileReq.tHot = data[9];
	customBatProfileReq.ntcB = (((uint16_t)data[11])<<8) | data[10];
	customBatProfileReq.ntcResistance = (((uint16_t)data[13])<<8) | data[12];

	writeCustomProfileReq = 1;
	return 0;
}

int8_t BatteryReadCurrentProfile(uint8_t *data, uint16_t *len) {
	if (writeCustomProfileReq || setProfileReq >= 0 || currentBatProfile == NULL) {
		int i = 0;
		for (i = 0; i < 14; i++) data[i] = 0;
	} else {
		data[0] = currentBatProfile->capacity;
		data[1] = currentBatProfile->capacity>>8;
		data[2] = currentBatProfile->chargeCurrent;
		data[3] = currentBatProfile->terminationCurr;
		data[4] = currentBatProfile->regulationVoltage;
		data[5] = currentBatProfile->cutoffVoltage;
		data[6] = currentBatProfile->tCold;
		data[7] = currentBatProfile->tCool;
		data[8] = currentBatProfile->tWarm;
		data[9] = currentBatProfile->tHot;
		data[10] = currentBatProfile->ntcB;
		data[11] = currentBatProfile->ntcB>>8;
		data[12] = currentBatProfile->ntcResistance;
		data[13] = currentBatProfile->ntcResistance>>8;

	}
	*len = 14;
	return 0;
}

const BatteryProfile_T *BatteryGetProfile(void) {
	/*if (batProfileStatus < BATTERY_PROFILES_COUNT) {
		return &(batteryProfiles[batProfileStatus]);
	} else {
		return &currentBatProfile;
	}*/
	return currentBatProfile;
}

int8_t BatteryReadProfileStatus(uint8_t *data, uint16_t *len) {
	data[0] = batProfileStatus;
	*len = 1;
	return 0;
}

int8_t BatterySetTempSenseConfig(uint8_t *data, uint16_t len) {
	if (data[0] >= BAT_TEMP_SENSE_CONFIG_END) return 1;

	EE_WriteVariable(BAT_TEMP_SENSE_CONFIG, data[0] | ((uint16_t)(~data[0])<<8));

	uint16_t var = 0;
	EE_ReadVariable(BAT_TEMP_SENSE_CONFIG, &var);
	if (((~var)&0xFF) == (var>>8)) {
		tempSensorConfig = var&0xFF;
	}
	return 0;
}

void BatteryGetTempSenseConfig(uint8_t data[], uint16_t *len) {
	data[0] = tempSensorConfig;
	*len = 1;
}
