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
#include "fuel_gauge_lc709203f.h"
#include "power_source.h"
#include "time_count.h"
#include "led.h"

#define BATTERY_PROFILES_COUNT() ((sizeof(batteryProfiles)/sizeof(BatteryProfile_T)))
#define PACK_CAPACITY_U16(c) 	((c==0xFFFFFFFF) ? 0xFFFF : (c >> ((c>=0x8000)*7)) | (c>=0x8000)*0x8000)
#define UNPACK_CAPACITY_U16(v) 	((v==0xFFFF) ? 0xFFFFFFFF : (uint32_t)(v&0x7FFF) << (((v&0x8000) >> 15)*7))

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void BatteryTask(void *argument);

static osThreadId_t batteryTaskHandle;

static const osThreadAttr_t batteryTask_attributes = {
	.name = "batteryTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 256
};
#endif

extern PowerState_T state;

static int16_t setProfileReq = -1;

static int8_t writeCustomProfileReq = 0;
BatteryProfile_T customBatProfileReq;

BatteryStatus_T batteryStatus = BAT_STATUS_NOT_PRESENT;

const BatteryProfile_T batteryProfiles[] = {
	{ 	// PiJuice Zero 1000mAh battery
		BAT_CHEMISTRY_LIPO,
		1000, // 1000mAh
		0x01, // 6250mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3743, 3933, 4057,
		13500, 13300, 13300,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// BP7X battery
		BAT_CHEMISTRY_LIPO,
		1820, // 1820mAh
		0x05, // 925mA, ~0.5C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3649, 3800, 4077,
		20900, 20500, 20200,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// SNN5843 battery
		BAT_CHEMISTRY_LIPO,
		2300, // 2300mAh
		0x08, // 1150mA, ~0.5C
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		3650, 3800, 4079,
		15300, 14900, 14820,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 12000mAh battery
		BAT_CHEMISTRY_LIPO,
		12000, // 12000mAh
		0x1A, // 2500mA
		0x06, // 350mA
		0x22, // 4.18V
		150, // 3V
		3488, 3824, 4061,
		11200, 10800, 10800,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 5000mAh battery
		BAT_CHEMISTRY_LIPO,
		5000, // 5000mAh
		0x1A, // 2500mA
		0x04, // 250mA
		0x22, // 4.18V
		150, // 3V
		3506, 3870, 4056,
		11100, 10500, 10700,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice BP7X 1600mAh battery
		BAT_CHEMISTRY_LIPO,
		1600, // 1600mAh
		0x05, // 925mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3672, 3811, 4094,
		22200, 20800, 20500,
		0,
		2,
		50,
		70,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice SNN5843 1300mAh battery
		BAT_CHEMISTRY_LIPO,
		1300, // 1300mAh
		0x03, // 775mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3675, 3818, 4105,
		15600, 15100, 15100,
		0,
		2,
		50,
		70,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 1200mAh battery
		BAT_CHEMISTRY_LIPO,
		1200, // 1200mAh
		0x02, // 700mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3514, 3859, 4045,
		19400, 17300, 16900,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// BP6X battery
		BAT_CHEMISTRY_LIPO,
		1400, // 1400mAh
		0x04, // 850mA, ~0.6C
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3649, 3800, 4077,
		20670, 20319, 20215,
		1,
		10,
		45,
		59,
		0x0D34,
		1000, // 10K
	},
	{ 	// PiJuice 600mAh battery
		BAT_CHEMISTRY_LIPO,
		600, // 600mAh
		0x00, // 550mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3659, 3816, 4087,
		37200, 22100, 22300,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 500mAh battery
		BAT_CHEMISTRY_LIPO,
		500, // 500mAh
		0x00, // 550mA
		0x00, // 50mA
		0x22, // 4.18V
		150, // 3V
		3659, 3914, 4060,
		16600, 15600, 15600,
		0,
		2,
		49,
		65,
		3450,
		1000, // 10K
	},
	{ 	// PiJuice 2500mAh battery
		BAT_CHEMISTRY_LIPO,
		2500, // 2500mAh
		0x0a, // 1300mA, ~0.5C
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		3650, 3800, 4079,
		15300, 14900, 14820,
		1,
		10,
		45,
		59,
		3450,
		1000, // 10K
	}
};

BatteryProfile_T customBatProfile = {
	 	// default battery
		BAT_CHEMISTRY_LIPO,
		1400, // 1400mAh
		0x04, // 850mA
		0x01, // 100mA
		0x22, // 4.18V
		150, // 3V
		3649, 3800, 4077,
		15900, 15630, 15550,
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

int8_t BatReadEEprofileData(void);
int8_t BatReadExtendedEEprofileData(void);
void BatWriteEEprofileData(BatteryProfile_T *batProfile);

void BatInitProfile(uint8_t initPofileId) {
	//uint16_t var;

	if ( initPofileId == BATTERY_CUSTOM_PROFILE_ID ) {
		if (BatReadEEprofileData() == 0) {
			BatReadExtendedEEprofileData();
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
				customBatProfile.capacity = 0xFFFFFFFF; // undefined
				customBatProfile.cutoffVoltage = 150; // 3v
				customBatProfile.ntcB = 0x0D34;
				customBatProfile.ntcResistance = 1000;
				customBatProfile.tCold = 1;
				customBatProfile.tCool = 10;
				customBatProfile.tWarm = 45;
				customBatProfile.tHot = 60;
				customBatProfile.chemistry=0xFF;
				customBatProfile.ocv10 = 0xFFFF;
				customBatProfile.ocv50 = 0xFFFF;
				customBatProfile.ocv90 = 0xFFFF;
				customBatProfile.r10 = 0xFFFF;
				customBatProfile.r50 = 0xFFFF;
				customBatProfile.r90 = 0xFFFF;

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

#if defined(RTOS_FREERTOS)
	batteryTaskHandle = osThreadNew(BatteryTask, (void*)NULL, &batteryTask_attributes);
#endif
}

int8_t BatReadExtendedEEprofileData(void) {
	uint8_t dataValid = 1;

	customBatProfile.chemistry=0xFF;
	if (NvReadVariableU8(BAT_CHEMISTRY_NV_ADDR, (uint8_t*)&(customBatProfile.chemistry)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.ocv10=0xFFFF;
	if (NvReadVariableU8(BAT_OCV10L_NV_ADDR, (uint8_t*)&(customBatProfile.ocv10)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_OCV10H_NV_ADDR, (uint8_t*)&(customBatProfile.ocv10)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.ocv50=0xFFFF;
	if (NvReadVariableU8(BAT_OCV50L_NV_ADDR, (uint8_t*)&(customBatProfile.ocv50)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_OCV50H_NV_ADDR, (uint8_t*)&(customBatProfile.ocv50)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.ocv90=0xFFFF;
	if (NvReadVariableU8(BAT_OCV90L_NV_ADDR, (uint8_t*)&(customBatProfile.ocv90)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_OCV90H_NV_ADDR, (uint8_t*)&(customBatProfile.ocv90)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.r10=0xFFFF;
	if (NvReadVariableU8(BAT_R10L_NV_ADDR, (uint8_t*)&(customBatProfile.r10)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_R10H_NV_ADDR, (uint8_t*)&(customBatProfile.r10)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.r50=0xFFFF;
	if (NvReadVariableU8(BAT_R50L_NV_ADDR, (uint8_t*)&(customBatProfile.r50)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_R50H_NV_ADDR, (uint8_t*)&(customBatProfile.r50)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	customBatProfile.r90=0xFFFF;
	if (NvReadVariableU8(BAT_R90L_NV_ADDR, (uint8_t*)&(customBatProfile.r90)) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;
	if (NvReadVariableU8(BAT_R90H_NV_ADDR, (uint8_t*)&(customBatProfile.r90)+1) != NV_READ_VARIABLE_SUCCESS) dataValid = 0;

	return !dataValid; // return 0 if valid
}

int8_t BatReadEEprofileData(void) {
	uint8_t dataValid = 1;
	uint16_t var;
	EE_ReadVariable(BAT_CAPACITY_NV_ADDR, &var);
	customBatProfile.capacity = UNPACK_CAPACITY_U16(var); // correction for large capacities over 32767

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
	uint16_t var = PACK_CAPACITY_U16(batProfile->capacity); // correction for large capacities over 32767
	EE_WriteVariable(BAT_CAPACITY_NV_ADDR, var);
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

void BatWriteExtendedEEprofileData(BatteryProfile_T *batProfile) {
	NvWriteVariableU8(BAT_CHEMISTRY_NV_ADDR, (uint8_t)(batProfile->chemistry));
	NvWriteVariableU8(BAT_OCV10L_NV_ADDR, batProfile->ocv10);
	NvWriteVariableU8(BAT_OCV10H_NV_ADDR, (batProfile->ocv10)>>8);
	NvWriteVariableU8(BAT_OCV50L_NV_ADDR, batProfile->ocv50);
	NvWriteVariableU8(BAT_OCV50H_NV_ADDR, (batProfile->ocv50)>>8);
	NvWriteVariableU8(BAT_OCV90L_NV_ADDR, batProfile->ocv90);
	NvWriteVariableU8(BAT_OCV90H_NV_ADDR, (batProfile->ocv90)>>8);
	NvWriteVariableU8(BAT_R10L_NV_ADDR, batProfile->r10);
	NvWriteVariableU8(BAT_R10H_NV_ADDR, (batProfile->r10)>>8);
	NvWriteVariableU8(BAT_R50L_NV_ADDR, batProfile->r50);
	NvWriteVariableU8(BAT_R50H_NV_ADDR, (batProfile->r50)>>8);
	NvWriteVariableU8(BAT_R90L_NV_ADDR, batProfile->r90);
	NvWriteVariableU8(BAT_R90H_NV_ADDR, (batProfile->r90)>>8);
}

#if defined(RTOS_FREERTOS)
static void BatteryTask(void *argument) {
  uint8_t chargingBlink[] = {0xFF, 0, 0, 0, 90, 0, 0, 0, 90};
  BatteryStatus_T prevBatteryStatus = BAT_STATUS_NORMAL;
  uint16_t prevBatteryRsoc = 0xFFFF;

  for(;;)
  {
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

	if (writeCustomProfileReq==1) {
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
			BatReadExtendedEEprofileData();
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
	} else if (writeCustomProfileReq==2) {
		writeCustomProfileReq = 0;
		BatWriteExtendedEEprofileData(&customBatProfileReq);
		BatReadExtendedEEprofileData();
		if (batProfileStatus == BATTERY_CUSTOM_PROFILE_ID) {
			FuelGaugeSetBatProfile(currentBatProfile);
		}
	}

	if (!CHARGER_IS_BATTERY_PRESENT() || batteryVoltage < 2500) {
		batteryStatus = BAT_STATUS_NOT_PRESENT;
	} else if (chargerStatus == CHG_CHARGING_FROM_IN) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_IN;
	} else if (chargerStatus == CHG_CHARGING_FROM_USB) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_5V_IO;
	} else {
		batteryStatus = BAT_STATUS_NORMAL;
	}

	uint8_t r=0,g=0,b=0;
	if (prevBatteryRsoc != batteryRsoc || prevBatteryStatus != batteryStatus)  {
		if (batteryRsoc > 500) {
			r = 0;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else if (batteryRsoc > 150) {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = 0;
		}
		b = LedGetParamB(LED_CHARGE_STATUS);
	}

	if (batteryStatus == BAT_STATUS_CHARGING_FROM_IN || batteryStatus == BAT_STATUS_CHARGING_FROM_5V_IO) {
		if (prevBatteryStatus != batteryStatus) {
			chargingBlink[1] = r;
			chargingBlink[2] = g;
			chargingBlink[5] = r;
			chargingBlink[6] = g;
			chargingBlink[7] = b;
			chargingBlink[0] = 0xFF;
			LedCmdSetBlink(LED_CHARGE_STATUS, chargingBlink, 0xFF);
		}
	} else if (chargerStatus == CHG_CHARGE_DONE) {
		if (prevBatteryStatus != batteryStatus) {
			LedFunctionSetRGB(LED_CHARGE_STATUS, r, g, b);
		}
	} else {
		if (prevBatteryRsoc != batteryRsoc || prevBatteryStatus != batteryStatus)  {
			if (state == STATE_LOWPOWER)
				LedFunctionSetRGB(LED_CHARGE_STATUS, r>>2, g>>2, 0);
			else
				LedFunctionSetRGB(LED_CHARGE_STATUS, r, g, 0);
		}
	}

	prevBatteryStatus = batteryStatus;
	prevBatteryRsoc = batteryRsoc;

	osDelay(20);
  }
}
#else
static uint32_t chargeLedTaskMsCounter;

void BatteryTask(void) {
	static uint8_t b = 0;

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

	if (writeCustomProfileReq==1) {
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
			BatReadExtendedEEprofileData();
			uint16_t var;
			EE_WriteVariable(BAT_PROFILE_NV_ADDR, BATTERY_CUSTOM_PROFILE_ID | (uint16_t)((~BATTERY_CUSTOM_PROFILE_ID)<<8));
			//EE_WriteVariable(BAT_PROFILE_NV_ADDR, BATTERY_CUSTOM_PROFILE_ID | ((uint16_t)(0x0F)<< 8));
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
	} else if (writeCustomProfileReq==2) {
		writeCustomProfileReq = 0;
		BatWriteExtendedEEprofileData(&customBatProfileReq);
		BatReadExtendedEEprofileData();
		if (batProfileStatus == BATTERY_CUSTOM_PROFILE_ID) {
			FuelGaugeSetBatProfile(currentBatProfile);
		}
	}

	if (!CHARGER_IS_BATTERY_PRESENT() || batteryVoltage < 2500) {
		batteryStatus = BAT_STATUS_NOT_PRESENT;
	} else if (chargerStatus == CHG_CHARGING_FROM_IN) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_IN;
	} else if (chargerStatus == CHG_CHARGING_FROM_USB) {
		batteryStatus = BAT_STATUS_CHARGING_FROM_5V_IO;
	} else {
		batteryStatus = BAT_STATUS_NORMAL;
	}

	if (MS_TIME_COUNT(chargeLedTaskMsCounter) >= 900/*(state == STATE_LOWPOWER?2000:500)*/) {
		MS_TIME_COUNTER_INIT(chargeLedTaskMsCounter);

		uint8_t r,g;
		if (batteryRsoc > 500) {
			r = 0;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else if (batteryRsoc > 150) {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = LedGetParamG(LED_CHARGE_STATUS);//60;
		} else {
			r = LedGetParamR(LED_CHARGE_STATUS);//50;
			g = 0;
		}
		uint8_t paramB = LedGetParamB(LED_CHARGE_STATUS);

		if (batteryStatus == BAT_STATUS_CHARGING_FROM_IN || batteryStatus == BAT_STATUS_CHARGING_FROM_5V_IO) {//if (chargerStatus == CHG_CHARGING_FROM_IN || chargerStatus == CHG_CHARGING_FROM_USB) {
			b = b?0:paramB;//200 - r;
		} else if (chargerStatus == CHG_CHARGE_DONE) {
			b = paramB;
		} else {
			b = 0;
		}

		if (state == STATE_LOWPOWER)
			LedFunctionSetRGB(LED_CHARGE_STATUS, r>>2, g>>2, b);
		else
			LedFunctionSetRGB(LED_CHARGE_STATUS, r, g, b);
	}
}
#endif

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

	volatile uint16_t var = (((uint16_t)data[1])<<8) | data[0];
	customBatProfileReq.capacity = UNPACK_CAPACITY_U16(var); // correction for large capacities over 32767
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
		volatile uint16_t var = PACK_CAPACITY_U16(currentBatProfile->capacity); // correction for large capacities over 32767
		data[0] = var; //currentBatProfile->capacity;
		data[1] = var>>8; //currentBatProfile->capacity>>8;
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

int8_t BatteryWriteCustomExtendedProfileReq(uint8_t data[], uint16_t len) {
	//batProfileStatus = BATTERY_PROFILE_WRITE_BUSY_STATUS;

	customBatProfileReq.chemistry = data[0];
	customBatProfileReq.ocv10 = *(uint16_t*)&data[1];
	customBatProfileReq.ocv50 = *(uint16_t*)&data[3];
	customBatProfileReq.ocv90 = *(uint16_t*)&data[5];
	customBatProfileReq.r10 = *(uint16_t*)&data[7];
	customBatProfileReq.r50 = *(uint16_t*)&data[9];
	customBatProfileReq.r90 = *(uint16_t*)&data[11];

	writeCustomProfileReq = 2;
	return 0;
}

int8_t BatteryReadCurrentExtendedProfile(uint8_t *data, uint16_t *len) {
	data[0] = currentBatProfile->chemistry;
	data[1] = currentBatProfile->ocv10;
	data[2] = currentBatProfile->ocv10>>8;
	data[3] = currentBatProfile->ocv50;
	data[4] = currentBatProfile->ocv50>>8;
	data[5] = currentBatProfile->ocv90;
	data[6] = currentBatProfile->ocv90>>8;
	data[7] = currentBatProfile->r10;
	data[8] = currentBatProfile->r10>>8;
	data[9] = currentBatProfile->r50;
	data[10] = currentBatProfile->r50>>8;
	data[11] = currentBatProfile->r90;
	data[12] = currentBatProfile->r90>>8;
	data[13] = 0xFF; // reserved for future use
	data[14] = 0xFF; // reserved for future use
	data[15] = 0xFF; // reserved for future use
	data[16] = 0xFF; // reserved for future use

	*len = 17;
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
