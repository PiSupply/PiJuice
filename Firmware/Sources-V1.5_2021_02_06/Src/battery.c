/*
 * battery.c
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */

#include "main.h"

#include "system_conf.h"
#include "util.h"
#include "time_count.h"

#include "nv.h"
#include "charger_bq2416x.h"
#include "config_switch_resistor.h"
#include "fuel_gauge_lc709203f.h"
#include "power_source.h"

#include "led.h"

#include "battery.h"
#include "battery_profiles.h"


typedef enum
{
	BATTERY_CUSTOM_PROFILE_NO_WRITE = 0u,
	BATTERY_CUSTOM_PROFILE_WRITE_STANDARD = 1u,
	BATTERY_CUSTOM_PROFILE_WRITE_EXTENDED = 2u,
} BATTERY_WriteCustomProfileRequest_t;


typedef enum
{
	BATTERY_DONT_SET_PROFILE = 0x00u,
	BATTERY_REQUEST_PROFILE_MSK = 0x0Fu,
	BATTERY_SET_REQUEST_MSK = 0xF0u,
	BATTERY_SET_PROFILE = 0x10u,
} BATTERY_SetProfileStatus_t;


#define BATTERY_DEFAULT_PROFILE_ID	0xFFu

#define PACK_CAPACITY_U16(c)		((c==0xFFFFFFFF) ? 0xFFFF : (c >> ((c>=0x8000)*7)) | (c>=0x8000)*0x8000)
#define UNPACK_CAPACITY_U16(v)		((v==0xFFFF) ? 0xFFFFFFFF : (uint32_t)(v&0x7FFF) << (((v&0x8000) >> 15)*7))


// TODO - Get this properly.
extern PowerState_T state;

static BATTERY_SetProfileStatus_t m_setProfileRequest = BATTERY_DONT_SET_PROFILE;

static BATTERY_WriteCustomProfileRequest_t m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_NO_WRITE;
static BatteryProfile_T m_tempBatteryProfile;


BatteryProfile_T m_customBatteryProfile =
{
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


BATTERY_ProfileStatus_t m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;


static const BatteryProfile_T * m_p_activeBatteryProfile = NULL;
static BatteryStatus_T m_batteryStatus = BAT_STATUS_NOT_PRESENT;
static uint32_t m_lastChargeLedTaskTimeMs;
static uint32_t m_lastBatteryUpdateTimeMs;

static void BATTERY_InitProfile(uint8_t initProfileId);
static bool BATTERY_ReadEEProfileData(void);
static bool BATTERY_ReadExtendedEEProfileData(void);
static void BATTERY_WriteEEProfileData(const BatteryProfile_T *batProfile);
static void BATTERY_WriteExtendedEEProfileData(const BatteryProfile_T *batProfile);

static void BATTERY_SetNewProfile(const uint8_t newProfile);
static void BATTERY_WriteCustomProfile(const BatteryProfile_T * newProfile);
static void BATTERY_WriteCustomProfileExtended(const BatteryProfile_T * newProfile);

static void BATTERY_UpdateBatteryStatus(const uint16_t battMv,
									const ChargerStatus_T chargerStatus, const bool batteryPresent);

static void BATTERY_UpdateLeds(const uint16_t batteryRsocPt1,
						const ChargerStatus_T chargerStatus, const PowerState_T powerState);


void BATTERY_Init(void)
{
	uint16_t var;

	EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);

	if ( false == UTIL_NV_ParamInitCheck_U16(var) )
	{
		// Initialise to default
		EE_WriteVariable(BAT_PROFILE_NV_ADDR, 0x00FFu);
		BATTERY_InitProfile(BATTERY_DEFAULT_PROFILE_ID);
	}
	else
	{
		BATTERY_InitProfile((uint8_t)(var & 0xFFu));
	}
}


void BATTERY_Task(void)
{
	const uint16_t batteryVoltageMv = FUELGUAGE_GetBatteryMv();
	const uint16_t batteryRsocPt1 = FUELGUAGE_GetSocPt1();
	const ChargerStatus_T chargerStatus = CHARGER_GetStatus();
	const bool chargerBatteryDetected = CHARGER_IsBatteryPresent();
	const PowerState_T powerState = state;


	if ((m_setProfileRequest & BATTERY_SET_REQUEST_MSK) == BATTERY_SET_PROFILE)
	{
		BATTERY_SetNewProfile(m_setProfileRequest & BATTERY_REQUEST_PROFILE_MSK);

		m_setProfileRequest = BATTERY_DONT_SET_PROFILE;
	}


	if (BATTERY_CUSTOM_PROFILE_WRITE_STANDARD == m_writeCustomProfileRequest)
	{
		BATTERY_WriteCustomProfile(&m_tempBatteryProfile);
		m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_NO_WRITE;
	}
	else if (BATTERY_CUSTOM_PROFILE_WRITE_EXTENDED == m_writeCustomProfileRequest)
	{
		BATTERY_WriteCustomProfileExtended(&m_tempBatteryProfile);
		m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_NO_WRITE;
	}


	if (MS_TIME_COUNT(m_lastBatteryUpdateTimeMs) > BATTERY_UPDATE_STATUS_PERIOD_MS)
	{
		MS_TIME_COUNTER_INIT(m_lastBatteryUpdateTimeMs);

		BATTERY_UpdateBatteryStatus(batteryVoltageMv, chargerStatus, chargerBatteryDetected);
	}


	if (MS_TIME_COUNT(m_lastChargeLedTaskTimeMs) >= BATTERY_CHARGE_LED_UPDATE_PERIOD_MS)
	{
		MS_TIME_COUNTER_INIT(m_lastChargeLedTaskTimeMs);

		BATTERY_UpdateLeds(batteryRsocPt1, chargerStatus, powerState);
	}
}


void BATTERY_SetProfileReq(const uint8_t id)
{
	if ((m_batteryProfileStatus != id) && (id <= 0x0Fu))
	{
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_WRITE_BUSY;
		m_setProfileRequest = BATTERY_SET_PROFILE | id;
	}

	return;
}


void BATTERY_WriteCustomProfileData(const uint8_t * const data, const uint16_t len)
{
	m_batteryProfileStatus = BATTERY_PROFILE_STATUS_WRITE_BUSY;

	uint16_t var = (uint16_t)data[0u] | (data[1u] << 8u) ;

	m_tempBatteryProfile.capacity = UNPACK_CAPACITY_U16(var); // correction for large capacities over 32767
	m_tempBatteryProfile.chargeCurrent = data[2];
	m_tempBatteryProfile.terminationCurr = data[3];
	m_tempBatteryProfile.regulationVoltage = data[4];
	m_tempBatteryProfile.cutoffVoltage = data[5];
	m_tempBatteryProfile.tCold = data[6];
	m_tempBatteryProfile.tCool = data[7];
	m_tempBatteryProfile.tWarm = data[8];
	m_tempBatteryProfile.tHot = data[9];
	m_tempBatteryProfile.ntcB = (((uint16_t)data[11])<<8) | data[10];
	m_tempBatteryProfile.ntcResistance = (((uint16_t)data[13])<<8) | data[12];

	m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_WRITE_STANDARD;

	return;
}



void BATTERY_ReadActiveProfileData(uint8_t * const data, uint16_t * const len)
{
	uint8_t i;

	if (m_p_activeBatteryProfile == NULL)
	{
		for (i = 0u; i < 14u; i++)
		{
			data[i] = 0u;
		}
	}
	else
	{
		volatile uint16_t var = PACK_CAPACITY_U16(m_p_activeBatteryProfile->capacity); // correction for large capacities over 32767
		data[0] = var; //currentBatProfile->capacity;
		data[1] = var>>8; //currentBatProfile->capacity>>8;
		data[2] = m_p_activeBatteryProfile->chargeCurrent;
		data[3] = m_p_activeBatteryProfile->terminationCurr;
		data[4] = m_p_activeBatteryProfile->regulationVoltage;
		data[5] = m_p_activeBatteryProfile->cutoffVoltage;
		data[6] = m_p_activeBatteryProfile->tCold;
		data[7] = m_p_activeBatteryProfile->tCool;
		data[8] = m_p_activeBatteryProfile->tWarm;
		data[9] = m_p_activeBatteryProfile->tHot;
		data[10] = m_p_activeBatteryProfile->ntcB;
		data[11] = m_p_activeBatteryProfile->ntcB>>8;
		data[12] = m_p_activeBatteryProfile->ntcResistance;
		data[13] = m_p_activeBatteryProfile->ntcResistance>>8;

	}

	*len = 14u;
}


void BATTERY_WriteCustomProfileExtendedData(const uint8_t * const data, const uint16_t len)
{
	//batProfileStatus = BATTERY_PROFILE_WRITE_BUSY_STATUS;

	m_tempBatteryProfile.chemistry = data[0];
	m_tempBatteryProfile.ocv10 = *(uint16_t*)&data[1];
	m_tempBatteryProfile.ocv50 = *(uint16_t*)&data[3];
	m_tempBatteryProfile.ocv90 = *(uint16_t*)&data[5];
	m_tempBatteryProfile.r10 = *(uint16_t*)&data[7];
	m_tempBatteryProfile.r50 = *(uint16_t*)&data[9];
	m_tempBatteryProfile.r90 = *(uint16_t*)&data[11];

	m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_WRITE_EXTENDED;
}


void BATTERY_ReadActiveProfileExtendedData(uint8_t * const data, uint16_t * const len)
{

	data[0] = m_p_activeBatteryProfile->chemistry;
	data[1] = m_p_activeBatteryProfile->ocv10;
	data[2] = m_p_activeBatteryProfile->ocv10>>8;
	data[3] = m_p_activeBatteryProfile->ocv50;
	data[4] = m_p_activeBatteryProfile->ocv50>>8;
	data[5] = m_p_activeBatteryProfile->ocv90;
	data[6] = m_p_activeBatteryProfile->ocv90>>8;
	data[7] = m_p_activeBatteryProfile->r10;
	data[8] = m_p_activeBatteryProfile->r10>>8;
	data[9] = m_p_activeBatteryProfile->r50;
	data[10] = m_p_activeBatteryProfile->r50>>8;
	data[11] = m_p_activeBatteryProfile->r90;
	data[12] = m_p_activeBatteryProfile->r90>>8;
	data[13] = 0xFF; // reserved for future use
	data[14] = 0xFF; // reserved for future use
	data[15] = 0xFF; // reserved for future use
	data[16] = 0xFF; // reserved for future use

	*len = 17u;
}


void BATTERY_ReadProfileStatus(uint8_t * const data, uint16_t * const len)
{
	data[0u] = m_batteryProfileStatus;
	*len = 1u;
}


const BatteryProfile_T * BATTERY_GetActiveProfileHandle(void)
{
	return m_p_activeBatteryProfile;
}


BatteryStatus_T BATTERY_GetStatus(void)
{
	return m_batteryStatus;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void BATTERY_InitProfile(uint8_t initProfileId)
{
	if (initProfileId == BATTERY_CUSTOM_PROFILE_ID)
	{
		if (true == BATTERY_ReadEEProfileData())
		{
			BATTERY_ReadExtendedEEProfileData();

			m_p_activeBatteryProfile = &m_customBatteryProfile;
			m_batteryProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
		}
		else
		{
			m_p_activeBatteryProfile = NULL;
			m_batteryProfileStatus = BATTERY_PROFILE_STATUS_CUSTOM_PROFILE_INVALID;
		}
	}
	else if (initProfileId == BATTERY_DEFAULT_PROFILE_ID)
	{
		// Make profile data based on dip switch or resistor configuration
		if ( switchConfigCode >= 0)
		{
			if ( switchConfigCode < BATTERY_PROFILES_COUNT() && resistorConfig1Code7 == -1 && resistorConfig2Code4 == -1 )
			{
				// Use switch coded profile id
				m_p_activeBatteryProfile = &m_batteryProfiles[switchConfigCode];
				m_batteryProfileStatus = BATTERY_CONFIG_SW_PROFILE_ID | switchConfigCode;
			}
			else if ( switchConfigCode == 1 && resistorConfig2Code4 >= 0 && resistorConfig2Code4 < BATTERY_PROFILES_COUNT() )
			{
				m_p_activeBatteryProfile = &m_batteryProfiles[resistorConfig2Code4];
				m_batteryProfileStatus = BATTERY_CONFIG_RES_PROFILE_ID | resistorConfig2Code4;
			}
			else if ( switchConfigCode == 1 && resistorConfig1Code7 >= 0 )
			{
				m_customBatteryProfile.chargeCurrent = ((resistorConfig1Code7&0x07) << 2); // offset 550mA
				m_customBatteryProfile.capacity = ((int16_t)m_customBatteryProfile.chargeCurrent * 75 + 550) * 2; // suppose charge current is 0.5 capacity
				m_customBatteryProfile.terminationCurr = (resistorConfig1Code7&0x04) | ((resistorConfig1Code7&0x08)>>2);
				m_customBatteryProfile.regulationVoltage = (resistorConfig1Code7>>4) * 5 + 5;
				m_customBatteryProfile.capacity = 0xFFFFFFFF; // undefined
				m_customBatteryProfile.cutoffVoltage = 150; // 3v
				m_customBatteryProfile.ntcB = 0x0D34;
				m_customBatteryProfile.ntcResistance = 1000;
				m_customBatteryProfile.tCold = 1;
				m_customBatteryProfile.tCool = 10;
				m_customBatteryProfile.tWarm = 45;
				m_customBatteryProfile.tHot = 60;
				// Extended data
				m_customBatteryProfile.chemistry=0xFF;
				m_customBatteryProfile.ocv10 = 0xFFFF;
				m_customBatteryProfile.ocv50 = 0xFFFF;
				m_customBatteryProfile.ocv90 = 0xFFFF;
				m_customBatteryProfile.r10 = 0xFFFF;
				m_customBatteryProfile.r50 = 0xFFFF;
				m_customBatteryProfile.r90 = 0xFFFF;

				m_batteryProfileStatus = BATTERY_CONFIG_PROFILE_STATUS;
				m_p_activeBatteryProfile = &m_customBatteryProfile;
			}
			else
			{
				m_p_activeBatteryProfile = NULL;
				m_batteryProfileStatus = BATTERY_CONFIG_INVALID_PROFILE_STATUS;
			}
		}
		else
		{
			m_p_activeBatteryProfile = NULL;
			m_batteryProfileStatus = BATTERY_CONFIG_INVALID_PROFILE_STATUS;
		}
	}
	else if (initProfileId < BATTERY_PROFILES_COUNT())
	{
		m_batteryProfileStatus = initProfileId;
		m_p_activeBatteryProfile = &m_batteryProfiles[initProfileId];
	}
	else if (initProfileId >= BATTERY_PROFILES_COUNT() && initProfileId < 15)
	{
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID | initProfileId; // non defined  profile
	}
	else
	{
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
	}
}


bool BATTERY_ReadEEProfileData(void)
{
	uint8_t dataValid = 1;
	uint16_t var;

	EE_ReadVariable(BAT_CAPACITY_NV_ADDR, &var);
	m_customBatteryProfile.capacity = UNPACK_CAPACITY_U16(var); // correction for large capacities over 32767

	EE_ReadVariable(CHARGE_CURRENT_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.chargeCurrent = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(CHARGE_TERM_CURRENT_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.terminationCurr = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_REG_VOLTAGE_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.regulationVoltage = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_CUTOFF_VOLTAGE_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.cutoffVoltage = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_TEMP_COLD_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.tCold = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_TEMP_COOL_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.tCool = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_TEMP_WARM_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.tWarm = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_TEMP_HOT_NV_ADDR, &var);
	dataValid = (dataValid && UTIL_NV_ParamInitCheck_U16(var));
	m_customBatteryProfile.tHot = (uint8_t)(var & 0xFFu);

	EE_ReadVariable(BAT_NTC_B_NV_ADDR, &var);
	m_customBatteryProfile.ntcB = var;
	uint16_t ntcCrc = var;

	EE_ReadVariable(BAT_NTC_RESISTANCE_NV_ADDR, &var);
	m_customBatteryProfile.ntcResistance = var;
	ntcCrc ^= var;

	EE_ReadVariable(BAT_NTC_CRC_NV_ADDR, &var);
	dataValid = dataValid &&  (var == ntcCrc);

	return dataValid;
}


bool BATTERY_ReadExtendedEEProfileData(void)
{
	uint8_t tempU8;
	bool dataValid = true;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_CHEMISTRY_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.chemistry = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV10L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv10 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV10H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv10 |= (tempU8 << 8u);

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV50L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv50 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV50H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv50 |= (tempU8 << 8u);

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV90L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv90 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_OCV90H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.ocv90 |= (tempU8 << 8u);

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R10L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r10 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R10H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r10 |= (tempU8 << 8u);

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R50L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r50 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R50H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r50 |= (tempU8 << 8u);

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R90L_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r90 = tempU8;

	if (NV_READ_VARIABLE_SUCCESS != NvReadVariableU8(BAT_R90H_NV_ADDR, &tempU8))
	{
		dataValid = false;
	}

	m_customBatteryProfile.r90 |= (tempU8 << 8u);


	if (false == dataValid)
	{
		m_customBatteryProfile.chemistry = BATTERY_CHEMISTRY_NOT_SET;
		m_customBatteryProfile.ocv10 = 0xFFFFu;
		m_customBatteryProfile.ocv50 = 0xFFFFu;
		m_customBatteryProfile.ocv90 = 0xFFFFu;
		m_customBatteryProfile.r10 = 0xFFFFu;
		m_customBatteryProfile.r50 = 0xFFFFu;
		m_customBatteryProfile.r90 = 0xFFFFu;
	}


	return dataValid;
}


void BATTERY_WriteEEProfileData(const BatteryProfile_T *batProfile)
{
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


void BATTERY_WriteExtendedEEProfileData(const BatteryProfile_T *batProfile)
{
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


void BATTERY_SetNewProfile(const uint8_t newProfile)
{
	uint16_t var;

	EE_WriteVariable(BAT_PROFILE_NV_ADDR, newProfile | ((uint16_t)(~newProfile) << 8u));

	EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);

	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		// if fcs correct
		BATTERY_InitProfile((uint8_t)(var * 0xFFu));
	}
	else
	{
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
	}

	POWERSOURCE_UpdateBatteryProfile(m_p_activeBatteryProfile);
	FUELGUAGE_SetBatteryProfile(m_p_activeBatteryProfile);
}


void BATTERY_WriteCustomProfile(const BatteryProfile_T * newProfile)
{
	uint16_t var;

	BATTERY_WriteEEProfileData(newProfile);

	if (true == BATTERY_ReadEEProfileData())
	{
		m_batteryProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
		m_p_activeBatteryProfile = &m_customBatteryProfile;
	}
	else
	{
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_CUSTOM_PROFILE_INVALID;
		m_p_activeBatteryProfile = NULL;
	}

	EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);

	if ( (BATTERY_CUSTOM_PROFILE_ID != (uint8_t)(var & 0xFFu)) || (false == UTIL_NV_ParamInitCheck_U16(var)) )
	{
		BATTERY_ReadExtendedEEProfileData();

		EE_WriteVariable(BAT_PROFILE_NV_ADDR, BATTERY_CUSTOM_PROFILE_ID | (uint16_t)((~BATTERY_CUSTOM_PROFILE_ID) << 8u));
		EE_ReadVariable(BAT_PROFILE_NV_ADDR, &var);

		if ( (BATTERY_CUSTOM_PROFILE_ID == (uint8_t)(var & 0xFFu)) && (false == UTIL_NV_ParamInitCheck_U16(var)) )
		{
			if (m_p_activeBatteryProfile != NULL)
			{
				m_batteryProfileStatus = BATTERY_CUSTOM_PROFILE_ID;
			}
			else
			{
				m_batteryProfileStatus = BATTERY_PROFILE_STATUS_CUSTOM_PROFILE_INVALID;
			}
		}
		else
		{
			m_p_activeBatteryProfile = NULL;
			m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
		}
	}

	POWERSOURCE_UpdateBatteryProfile(m_p_activeBatteryProfile);
	FUELGUAGE_SetBatteryProfile(m_p_activeBatteryProfile);
}


void BATTERY_WriteCustomProfileExtended(const BatteryProfile_T * newProfile)
{
	BATTERY_WriteExtendedEEProfileData(newProfile);

	BATTERY_ReadExtendedEEProfileData();

	if (m_batteryProfileStatus == BATTERY_CUSTOM_PROFILE_ID)
	{
		FUELGUAGE_SetBatteryProfile(m_p_activeBatteryProfile);
	}
}


void BATTERY_UpdateBatteryStatus(const uint16_t battMv,
									const ChargerStatus_T chargerStatus, const bool batteryPresent)
{
	if ( (false == batteryPresent) || (battMv < 2500u) )
	{
		m_batteryStatus = BAT_STATUS_NOT_PRESENT;
	}
	else if (chargerStatus == CHG_CHARGING_FROM_IN)
	{
		m_batteryStatus = BAT_STATUS_CHARGING_FROM_IN;
	}
	else if (chargerStatus == CHG_CHARGING_FROM_USB)
	{
		m_batteryStatus = BAT_STATUS_CHARGING_FROM_5V_IO;
	}
	else
	{
		m_batteryStatus = BAT_STATUS_NORMAL;
	}
}


void BATTERY_UpdateLeds(const uint16_t batteryRsocPt1,
						const ChargerStatus_T chargerStatus, const PowerState_T powerState)
{
	uint8_t r, g, b, paramB;

	if (batteryRsocPt1 > 500u)
	{
		r = 0u;
		g = LedGetParamG(LED_CHARGE_STATUS);//60;
	}
	else if (batteryRsocPt1 > 150u)
	{
		r = LedGetParamR(LED_CHARGE_STATUS);//50;
		g = LedGetParamG(LED_CHARGE_STATUS);//60;
	}
	else
	{
		r = LedGetParamR(LED_CHARGE_STATUS);//50;
		g = 0u;
	}

	paramB = LedGetParamB(LED_CHARGE_STATUS);

	if ( (m_batteryStatus == BAT_STATUS_CHARGING_FROM_IN) || (m_batteryStatus == BAT_STATUS_CHARGING_FROM_5V_IO) )
	{
		// b did = 0!
		//b = b ? 0 : paramB;
		b = paramB;
	}
	else if (chargerStatus == CHG_CHARGE_DONE)
	{
		b = paramB;
	}
	else
	{
		b = 0;
	}

	if (state == STATE_LOWPOWER)
		LedFunctionSetRGB(LED_CHARGE_STATUS, r>>2, g>>2, b);
	else
		LedFunctionSetRGB(LED_CHARGE_STATUS, r, g, b);
}
