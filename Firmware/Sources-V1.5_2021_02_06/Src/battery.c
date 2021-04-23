/*
 * battery.c
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"

#include "system_conf.h"
#include "util.h"
#include "time_count.h"

#include "nv.h"
#include "charger_bq2416x.h"
#include "config_switch_resistor.h"
#include "fuel_gauge_lc709203f.h"
#include "power_source.h"

#include "taskman.h"
#include "led.h"

#include "battery.h"
#include "battery_profiles.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

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


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static void BATTERY_InitProfile(const uint8_t initProfileId);
static bool BATTERY_ReadEEProfileData(void);
static bool BATTERY_ReadExtendedEEProfileData(void);
static void BATTERY_WriteEEProfileData(const BatteryProfile_T *batProfile);
static void BATTERY_WriteExtendedEEProfileData(const BatteryProfile_T *batProfile);

static void BATTERY_SetNewProfileId(const uint8_t newProfileId);
static void BATTERY_WriteCustomProfile(const BatteryProfile_T * newProfile);
static void BATTERY_WriteCustomProfileExtended(const BatteryProfile_T * newProfile);

static void BATTERY_UpdateBatteryStatus(const uint16_t battMv,
									const ChargerStatus_T chargerStatus, const bool batteryPresent);

static void BATTERY_UpdateChargeLed(const uint16_t batteryRsocPt1,
						const ChargerStatus_T chargerStatus, const TASKMAN_RunState_t runState);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static BATTERY_SetProfileStatus_t m_setProfileRequest = BATTERY_DONT_SET_PROFILE;

static BATTERY_WriteCustomProfileRequest_t m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_NO_WRITE;
static BatteryProfile_T m_tempBatteryProfile;
static const BatteryProfile_T * m_p_activeBatteryProfile = NULL;

static BATTERY_ProfileStatus_t m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
static BatteryStatus_T m_batteryStatus = BAT_STATUS_NOT_PRESENT;
static uint32_t m_lastChargeLedTaskTimeMs;
static uint32_t m_lastBatteryUpdateTimeMs;


// Resistor charge parameters config code
// code: v2 v1 v0 t1 c2 c1 c0
// v value: (code>>4 * 5 + 5) * 20mV + 3.5V, vcode = (code>>4) * 5 + 5
// c value: code&0x07 * 300mA + 550mA, ccode = code&0x07
// t value: c2t0 * 100mA + 50mA, tcode = (code&0x04>>1) | (code&0x08>>3)




// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * BATTERY_Init configures the battery with the stored profile from NV memory,
 * if no profile is found in the NV memory then the default is used and the NV
 * memory is initialised to the default also. the profile is checked for validity
 * in the initprofile routine. This should probably be called before any other
 * module that uses the active battery configuration data.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void BATTERY_Init(void)
{
	uint8_t tempU8 = BATTERY_DEFAULT_PROFILE_ID;

	if (false == NV_ReadVariable_U8(BAT_PROFILE_NV_ADDR, &tempU8))
	{
		// Memory is blank so populate the default profile
		NV_WriteVariable_U8(BAT_PROFILE_NV_ADDR, BATTERY_DEFAULT_PROFILE_ID);
	}

	BATTERY_InitProfile(tempU8);
}


// ****************************************************************************
/*!
 * BATTERY_Task performs the periodic updates... processing the battery state,
 * updating the led indicator and writing configuration data if a change to the
 * data has occurred.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void BATTERY_Task(void)
{
	const uint16_t batteryVoltageMv = FUELGAUGE_GetBatteryMv();
	const uint16_t batteryRsocPt1 = FUELGAUGE_GetSocPt1();
	const ChargerStatus_T chargerStatus = CHARGER_GetStatus();
	const bool chargerBatteryDetected = (CHARGER_BATTERY_NOT_PRESENT != CHARGER_GetBatteryStatus());
	const TASKMAN_RunState_t runState = TASKMAN_GetRunState();


	if ((m_setProfileRequest & BATTERY_SET_REQUEST_MSK) == BATTERY_SET_PROFILE)
	{
		BATTERY_SetNewProfileId(m_setProfileRequest & BATTERY_REQUEST_PROFILE_MSK);

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

		BATTERY_UpdateChargeLed(batteryRsocPt1, chargerStatus, runState);
	}
}


// ****************************************************************************
/*!
 * BATTERY_SetProfileIdReq puts a request up for the module task to change the
 * current active battery profile
 *
 * @param	id			new battery profile id to set
 * @retval	none
 */
// ****************************************************************************
void BATTERY_SetProfileIdReq(const uint8_t id)
{
	if ((m_batteryProfileStatus != id) && (id <= 0x0Fu))
	{
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_WRITE_BUSY;
		m_setProfileRequest = BATTERY_SET_PROFILE | id;
	}
}


// ****************************************************************************
/*!
 * BATTERY_WriteCustomProfileData writes new custom profile standard data to the
 * intermediate storage and sets the write request for the module task to perform
 * the write. No check is performed to ensure a previous write has completed so
 * could be inconsistent if time is not allowed for the previous request to be
 * satisfied.
 *
 * @param	p_data		pointer to buffer that contains the standard profile data
 * @param	len			length of data in the buffer
 * @retval	none
 */
// ****************************************************************************
void BATTERY_WriteCustomProfileData(const uint8_t * const p_data, const uint16_t len)
{
	m_batteryProfileStatus = BATTERY_PROFILE_STATUS_WRITE_BUSY;

	uint16_t var = (uint16_t)p_data[0u] | (p_data[1u] << 8u);

	m_tempBatteryProfile.capacity = UNPACK_CAPACITY_U16(var); // correction for large capacities over 32767
	m_tempBatteryProfile.chargeCurrent = p_data[2u];
	m_tempBatteryProfile.terminationCurr = p_data[3u];
	m_tempBatteryProfile.regulationVoltage = p_data[4u];
	m_tempBatteryProfile.cutoffVoltage = p_data[5u];
	m_tempBatteryProfile.tCold = p_data[6u];
	m_tempBatteryProfile.tCool = p_data[7u];
	m_tempBatteryProfile.tWarm = p_data[8u];
	m_tempBatteryProfile.tHot = p_data[9u];
	m_tempBatteryProfile.ntcB = (uint16_t)p_data[10u] | (p_data[11u] << 8u);
	m_tempBatteryProfile.ntcResistance = (uint16_t)p_data[12u] | (p_data[13u] << 8u);

	m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_WRITE_STANDARD;

	return;
}


// ****************************************************************************
/*!
 * BATTERY_ReadActiveProfileData populates a buffer with the active profile
 * standard data
 *
 * @param	p_data	pointer to buffer where the standard profile data is to be placed
 * @param	p_len	pointer to where to place the length of standard profile data
 * @retval	none
 */
// ****************************************************************************
void BATTERY_ReadActiveProfileData(uint8_t * const p_data, uint16_t * const p_len)
{
	uint8_t i;
	uint16_t var;

	if (m_p_activeBatteryProfile == NULL)
	{
		for (i = 0u; i < 14u; i++)
		{
			p_data[i] = 0u;
		}
	}
	else
	{
		var = PACK_CAPACITY_U16(m_p_activeBatteryProfile->capacity); // correction for large capacities over 32767
		p_data[0u] = var;
		p_data[1u] = (var >> 8u);
		p_data[2u] = m_p_activeBatteryProfile->chargeCurrent;
		p_data[3u] = m_p_activeBatteryProfile->terminationCurr;
		p_data[4u] = m_p_activeBatteryProfile->regulationVoltage;
		p_data[5u] = m_p_activeBatteryProfile->cutoffVoltage;
		p_data[6u] = m_p_activeBatteryProfile->tCold;
		p_data[7u] = m_p_activeBatteryProfile->tCool;
		p_data[8u] = m_p_activeBatteryProfile->tWarm;
		p_data[9u] = m_p_activeBatteryProfile->tHot;
		p_data[10u] = m_p_activeBatteryProfile->ntcB;
		p_data[11u] = (m_p_activeBatteryProfile->ntcB >> 8u);
		p_data[12u] = m_p_activeBatteryProfile->ntcResistance;
		p_data[13u] = (m_p_activeBatteryProfile->ntcResistance >> 8u);

	}

	*p_len = 14u;
}


// ****************************************************************************
/*!
 * BATTERY_WriteCustomProfileExtendedData writes new custom profile data to the
 * intermediate storage and sets the write request for the module task to perform
 * the write. No check is performed to ensure a previous write has completed so
 * could be inconsistent if time is not allowed for the previous request to be
 * satisfied.
 *
 * @param	p_data		pointer to buffer that contains the extended profile data
 * @param	len			length of data in the buffer
 * @retval	none
 */
// ****************************************************************************
void BATTERY_WriteCustomProfileExtendedData(const uint8_t * const p_data,
												const uint16_t len)
{
	//batProfileStatus = BATTERY_PROFILE_WRITE_BUSY_STATUS;

	m_tempBatteryProfile.chemistry = (BatteryChemistry_T)p_data[0u];
	m_tempBatteryProfile.ocv10 = UTIL_FromBytes_U16(&p_data[1u]);
	m_tempBatteryProfile.ocv50 = UTIL_FromBytes_U16(&p_data[3u]);
	m_tempBatteryProfile.ocv90 = UTIL_FromBytes_U16(&p_data[5u]);
	m_tempBatteryProfile.r10 = UTIL_FromBytes_U16(&p_data[7u]);
	m_tempBatteryProfile.r50 = UTIL_FromBytes_U16(&p_data[9u]);
	m_tempBatteryProfile.r90 = UTIL_FromBytes_U16(&p_data[11u]);

	m_writeCustomProfileRequest = BATTERY_CUSTOM_PROFILE_WRITE_EXTENDED;
}


// ****************************************************************************
/*!
 * BATTERY_ReadActiveProfileExtendedData populates a buffer with the active profile
 * extended data
 *
 * @param	p_data		pointer to buffer where the extended profile data is to be placed
 * @param	p_len		pointer to where to place the length of length of extended profile data
 * @retval	none
 */
// ****************************************************************************
void BATTERY_ReadActiveProfileExtendedData(uint8_t * const p_data,
											uint16_t * const p_len)
{
	p_data[0u] = (uint8_t)m_p_activeBatteryProfile->chemistry;

	UTIL_ToBytes_U16(m_p_activeBatteryProfile->ocv10, &p_data[1u]);
	UTIL_ToBytes_U16(m_p_activeBatteryProfile->ocv50, &p_data[3u]);
	UTIL_ToBytes_U16(m_p_activeBatteryProfile->ocv90, &p_data[5u]);
	UTIL_ToBytes_U16(m_p_activeBatteryProfile->r10, &p_data[7u]);
	UTIL_ToBytes_U16(m_p_activeBatteryProfile->r50, &p_data[9u]);
	UTIL_ToBytes_U16(m_p_activeBatteryProfile->r90, &p_data[11u]);

	p_data[13u] = 0xFFu; // reserved for future use
	p_data[14u] = 0xFFu; // reserved for future use
	p_data[15u] = 0xFFu; // reserved for future use
	p_data[16u] = 0xFFu; // reserved for future use

	*p_len = 17u;
}


// ****************************************************************************
/*!
 * BATTERY_ReadProfileStatus populates a buffer with the current battery status
 *
 * @param	p_data		pointer to buffer where status is to be placed
 * @param	p_len		length of status data
 * @retval	none
 */
// ****************************************************************************
void BATTERY_ReadProfileStatusData(uint8_t * const p_data, uint16_t * const p_len)
{
	p_data[0u] = m_batteryProfileStatus;
	*p_len = 1u;
}


// ****************************************************************************
/*!
 * BATTERY_GetActiveProfileHandle returns a const pointer to the currently active
 * battery profile
 *
 * @param	none
 * @retval	BatteryProfile_T *		pointer to the active battery profile
 */
// ****************************************************************************
const BatteryProfile_T * BATTERY_GetActiveProfileHandle(void)
{
	return m_p_activeBatteryProfile;
}


// ****************************************************************************
/*!
 * BATTERY_GetStatus returns the current battery status
 *
 * @param	none
 * @retval	BatteryStatus_T		current battery status
 */
// ****************************************************************************
BatteryStatus_T BATTERY_GetStatus(void)
{
	return m_batteryStatus;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * BATTERY_InitProfile configures the current battery profile using the profile
 * id, if this is set to default then the battery profile is set by the dip switch
 * settings in combination with the
 *
 * @param	initProfileId		id of profile to initialise module with
 * @retval	none
 */
// ****************************************************************************
static void BATTERY_InitProfile(const uint8_t initProfileId)
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
		if (switchConfigCode >= 0)
		{
			if ( (switchConfigCode < BATTERY_PROFILES_COUNT) &&
					(resistorConfig1Code7 == -1) &&
					(resistorConfig2Code4 == -1)
					)
			{
				// Use switch coded profile id
				m_p_activeBatteryProfile = &m_batteryProfiles[switchConfigCode];
				m_batteryProfileStatus = BATTERY_CONFIG_SW_PROFILE_ID | switchConfigCode;
			}
			else if ( (switchConfigCode == 1) &&
					(resistorConfig2Code4 >= 0) &&
					(resistorConfig2Code4 < BATTERY_PROFILES_COUNT)
					)
			{
				m_p_activeBatteryProfile = &m_batteryProfiles[resistorConfig2Code4];
				m_batteryProfileStatus = BATTERY_CONFIG_RES_PROFILE_ID | resistorConfig2Code4;
			}
			else if ( (switchConfigCode) == 1 && (resistorConfig1Code7 >= 0) )
			{
				m_customBatteryProfile.chargeCurrent = ((resistorConfig1Code7 & 0x07u) << 2u); // offset 550mA
				m_customBatteryProfile.capacity = ((int16_t)m_customBatteryProfile.chargeCurrent * 75u + 550u) * 2; // suppose charge current is 0.5 capacity
				m_customBatteryProfile.terminationCurr = (resistorConfig1Code7 & 0x04u) | ((resistorConfig1Code7 & 0x08u) >> 2u);
				m_customBatteryProfile.regulationVoltage = (resistorConfig1Code7 >> 4u) * 5u + 5u;
				m_customBatteryProfile.capacity = 0xFFFFFFFFu; // undefined
				m_customBatteryProfile.cutoffVoltage = 150u; // 3v
				m_customBatteryProfile.ntcB = 0x0D34u;
				m_customBatteryProfile.ntcResistance = 1000u;
				m_customBatteryProfile.tCold = 1u;
				m_customBatteryProfile.tCool = 10u;
				m_customBatteryProfile.tWarm = 45u;
				m_customBatteryProfile.tHot = 60u;

				// Extended data
				m_customBatteryProfile.chemistry=0xFFu;
				m_customBatteryProfile.ocv10 = 0xFFFFu;
				m_customBatteryProfile.ocv50 = 0xFFFFu;
				m_customBatteryProfile.ocv90 = 0xFFFFu;
				m_customBatteryProfile.r10 = 0xFFFFu;
				m_customBatteryProfile.r50 = 0xFFFFu;
				m_customBatteryProfile.r90 = 0xFFFFu;

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
	else if (initProfileId < BATTERY_PROFILES_COUNT)
	{
		m_batteryProfileStatus = initProfileId;
		m_p_activeBatteryProfile = &m_batteryProfiles[initProfileId];
	}
	else if ( (initProfileId >= BATTERY_PROFILES_COUNT) && (initProfileId < 15u) )
	{
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID | initProfileId; // non defined  profile
	}
	else
	{
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
	}

	// Make sure the charger knows about the profile
	CHARGER_UpdateBatteryProfile();
}


// ****************************************************************************
/*!
 * BATTERY_ReadEEProfileData reads the NV memory for the custom profile
 * standard data and stores it to the custom profile. On success the routine returns
 * true.
 *
 * @param	none
 * @retval	bool		false = profile data invalid
 * 						true = profile data valid
 */
// ****************************************************************************
static bool BATTERY_ReadEEProfileData(void)
{
	bool dataValid = true;
	uint16_t tempU16;
	uint16_t ntcCrc;

	EE_ReadVariable(BAT_CAPACITY_NV_ADDR, &tempU16);
	m_customBatteryProfile.capacity = UNPACK_CAPACITY_U16(tempU16); // correction for large capacities over 32767

	dataValid &= NV_ReadVariable_U8(CHARGE_CURRENT_NV_ADDR, &m_customBatteryProfile.chargeCurrent);
	dataValid &= NV_ReadVariable_U8(CHARGE_TERM_CURRENT_NV_ADDR, &m_customBatteryProfile.terminationCurr);
	dataValid &= NV_ReadVariable_U8(BAT_REG_VOLTAGE_NV_ADDR, &m_customBatteryProfile.regulationVoltage);
	dataValid &= NV_ReadVariable_U8(BAT_CUTOFF_VOLTAGE_NV_ADDR, &m_customBatteryProfile.cutoffVoltage);
	dataValid &= NV_ReadVariable_U8(BAT_TEMP_COLD_NV_ADDR, (uint8_t*)&m_customBatteryProfile.tCold);
	dataValid &= NV_ReadVariable_U8(BAT_TEMP_COOL_NV_ADDR, (uint8_t*)&m_customBatteryProfile.tCool);
	dataValid &= NV_ReadVariable_U8(BAT_TEMP_WARM_NV_ADDR, (uint8_t*)&m_customBatteryProfile.tWarm);
	dataValid &= NV_ReadVariable_U8(BAT_TEMP_HOT_NV_ADDR, (uint8_t*)&m_customBatteryProfile.tHot);

	EE_ReadVariable(BAT_NTC_B_NV_ADDR, &tempU16);
	m_customBatteryProfile.ntcB = tempU16;
	ntcCrc = tempU16;

	EE_ReadVariable(BAT_NTC_RESISTANCE_NV_ADDR, &tempU16);
	m_customBatteryProfile.ntcResistance = tempU16;
	ntcCrc ^= tempU16;

	EE_ReadVariable(BAT_NTC_CRC_NV_ADDR, &tempU16);
	dataValid &= (tempU16 == ntcCrc);

	return dataValid;
}


// ****************************************************************************
/*!
 * BATTERY_ReadExtendedEEProfileData reads the NV memory for the custom profile
 * extended data and stores it to the custom profile. On success the routine returns
 * true. If any of the data reads are unsuccessful the extended data is wiped.
 *
 * @param	none
 * @retval	bool		false = profile data invalid
 * 						true = profile data valid
 */
// ****************************************************************************
static bool BATTERY_ReadExtendedEEProfileData(void)
{
	uint8_t tempU8;
	bool dataValid = true;

	dataValid &= NV_ReadVariable_U8(BAT_CHEMISTRY_NV_ADDR, (uint8_t*)&m_customBatteryProfile.chemistry);
	dataValid &= NV_ReadVariable_U8(BAT_OCV10L_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv10 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_OCV10H_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv10 |= (tempU8 << 8u);


	dataValid &= NV_ReadVariable_U8(BAT_OCV50L_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv50 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_OCV50H_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv50 |= (tempU8 << 8u);


	dataValid &= NV_ReadVariable_U8(BAT_OCV90L_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv90 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_OCV90H_NV_ADDR, &tempU8);
	m_customBatteryProfile.ocv90 |= (tempU8 << 8u);


	dataValid &= NV_ReadVariable_U8(BAT_R10L_NV_ADDR, &tempU8);
	m_customBatteryProfile.r10 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_R10H_NV_ADDR, &tempU8);
	m_customBatteryProfile.r10 |= (tempU8 << 8u);


	dataValid &= NV_ReadVariable_U8(BAT_R50L_NV_ADDR, &tempU8);
	m_customBatteryProfile.r50 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_R50H_NV_ADDR, &tempU8);
	m_customBatteryProfile.r50 |= (tempU8 << 8u);


	dataValid &= NV_ReadVariable_U8(BAT_R90L_NV_ADDR, &tempU8);
	m_customBatteryProfile.r90 = tempU8;

	dataValid &= NV_ReadVariable_U8(BAT_R90H_NV_ADDR, &tempU8);
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


// ****************************************************************************
/*!
 * BATTERY_WriteEEProfileData commits battery profile data to non volatile memory
 *
 * @param	p_batProfile		pointer to battery profile data
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_WriteEEProfileData(const BatteryProfile_T * const p_batProfile)
{
	uint16_t var = PACK_CAPACITY_U16(p_batProfile->capacity); // correction for large capacities over 32767

	EE_WriteVariable(BAT_CAPACITY_NV_ADDR, var);

	NV_WriteVariable_U8(CHARGE_CURRENT_NV_ADDR, p_batProfile->chargeCurrent);
	NV_WriteVariable_U8(CHARGE_TERM_CURRENT_NV_ADDR, p_batProfile->terminationCurr);
	NV_WriteVariable_U8(BAT_REG_VOLTAGE_NV_ADDR, p_batProfile->regulationVoltage);
	NV_WriteVariable_U8(BAT_CUTOFF_VOLTAGE_NV_ADDR, p_batProfile->cutoffVoltage);
	NV_WriteVariable_U8(BAT_TEMP_COLD_NV_ADDR, p_batProfile->tCold);
	NV_WriteVariable_U8(BAT_TEMP_COOL_NV_ADDR, p_batProfile->tCool);
	NV_WriteVariable_U8(BAT_TEMP_WARM_NV_ADDR, p_batProfile->tWarm);
	NV_WriteVariable_U8(BAT_TEMP_HOT_NV_ADDR, p_batProfile->tHot);
	EE_WriteVariable(BAT_NTC_B_NV_ADDR, p_batProfile->ntcB);
	EE_WriteVariable(BAT_NTC_RESISTANCE_NV_ADDR, p_batProfile->ntcResistance);
	EE_WriteVariable(BAT_NTC_CRC_NV_ADDR, p_batProfile->ntcB ^ p_batProfile->ntcResistance);
}


// ****************************************************************************
/*!
 * BATTERY_WriteExtendedEEProfileData commits battery profile extended data to
 * non volatile memory
 *
 * @param	p_batProfile		pointer to battery profile data
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_WriteExtendedEEProfileData(const BatteryProfile_T * const p_batProfile)
{
	NV_WriteVariable_U8(BAT_CHEMISTRY_NV_ADDR, (uint8_t)(p_batProfile->chemistry));
	NV_WriteVariable_U8(BAT_OCV10L_NV_ADDR, (uint8_t)(p_batProfile->ocv10 & 0xFFu));
	NV_WriteVariable_U8(BAT_OCV10H_NV_ADDR, (uint8_t)((p_batProfile->ocv10) >> 8u) & 0xFFu);
	NV_WriteVariable_U8(BAT_OCV50L_NV_ADDR, (uint8_t)(p_batProfile->ocv50) & 0xFFu);
	NV_WriteVariable_U8(BAT_OCV50H_NV_ADDR, (uint8_t)((p_batProfile->ocv50) >> 8u) & 0xFFu);
	NV_WriteVariable_U8(BAT_OCV90L_NV_ADDR, (uint8_t)(p_batProfile->ocv90) & 0xFFu);
	NV_WriteVariable_U8(BAT_OCV90H_NV_ADDR, (uint8_t)((p_batProfile->ocv90) >> 8u) & 0xFFu);
	NV_WriteVariable_U8(BAT_R10L_NV_ADDR, (uint8_t)(p_batProfile->r10) & 0xFFu);
	NV_WriteVariable_U8(BAT_R10H_NV_ADDR, (uint8_t)((p_batProfile->r10) >> 8u) & 0xFFu);
	NV_WriteVariable_U8(BAT_R50L_NV_ADDR, (uint8_t)(p_batProfile->r50) & 0xFFu);
	NV_WriteVariable_U8(BAT_R50H_NV_ADDR, (uint8_t)((p_batProfile->r50) >> 8u) & 0xFFu);
	NV_WriteVariable_U8(BAT_R90L_NV_ADDR, (uint8_t)(p_batProfile->r90) & 0xFFu);
	NV_WriteVariable_U8(BAT_R90H_NV_ADDR, (uint8_t)((p_batProfile->r90) >> 8u) & 0xFFu);
}


// ****************************************************************************
/*!
 * BATTERY_SetNewProfileId tells the module to use a new profile from the preset
 * profiles or the custom one. The dependent modules are notified for a change
 * in battery profile. The new profile is is stored to NV memory for persistence
 * over power cycles.
 *
 * @param	newProfileId		id of profile to use
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_SetNewProfileId(const uint8_t newProfileId)
{
	uint8_t tempU8;

	// Check to see if new profile is preset and isn't already active
	if ( (newProfileId < BATTERY_CUSTOM_PROFILE_ID) &&
			(m_p_activeBatteryProfile == &m_batteryProfiles[newProfileId])
			)
	{
		return;
	}

	// Commit new profile id to NV
	NV_WriteVariable_U8(BAT_PROFILE_NV_ADDR, newProfileId);

	if (false == NV_ReadVariable_U8(BAT_PROFILE_NV_ADDR, &tempU8))
	{
		// Something went wrong with the write
		m_p_activeBatteryProfile = NULL;
		m_batteryProfileStatus = BATTERY_PROFILE_STATUS_STORED_PROFILE_ID_INVALID;
	}
	else
	{
		// Initialise new profile
		BATTERY_InitProfile(tempU8);
	}

	// Notify dependent modules
	POWERSOURCE_UpdateBatteryProfile(m_p_activeBatteryProfile);
	FUELGAUGE_UpdateBatteryProfile();
	CHARGER_UpdateBatteryProfile();
}


// ****************************************************************************
/*!
 * BATTERY_WriteCustomProfile writes the new profile data to NV memory and then
 * reads it back to ensure correctness. Battery profile Id is updated if not already
 * set to custom profile.
 *
 * @param	p_newProfile		new profile to write to NV memory
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_WriteCustomProfile(const BatteryProfile_T * p_newProfile)
{
	BATTERY_WriteEEProfileData(p_newProfile);

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

	// Set profile ID to custom battery and initialise battery data
	BATTERY_SetNewProfileId(BATTERY_CUSTOM_PROFILE_ID);
}


// ****************************************************************************
/*!
 * BATTERY_WriteCustomProfileExtended writes the extended data for a new custom
 * profile to non volatile memory for persistence over power cycles.
 *
 * @param	p_newProfile		new profile to write to NV memory
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_WriteCustomProfileExtended(const BatteryProfile_T * p_newProfile)
{
	BATTERY_WriteExtendedEEProfileData(p_newProfile);

	BATTERY_ReadExtendedEEProfileData();

	if (m_batteryProfileStatus == BATTERY_CUSTOM_PROFILE_ID)
	{
		// Fuel guage is the only module that uses the extended data in the profile
		FUELGAUGE_UpdateBatteryProfile();
	}
}


// ****************************************************************************
/*!
 * BATTERY_UpdateBatteryStatus processes the battery status from the battery voltage
 * and charger input status.
 *
 * @param	battMv				current battery voltage in millivolts
 * @param	chargerStatus		current status of all charger inputs
 * @param	batteryPresent		true if battery is present, false if not present
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_UpdateBatteryStatus(const uint16_t battMv,
									const ChargerStatus_T chargerStatus, const bool batteryPresent)
{
	// if the battery has just been inserted then get the fuel gauge to update it's soc.
	if ( (true == batteryPresent) & (BAT_STATUS_NOT_PRESENT == m_batteryStatus) )
	{
		FUELGAUGE_InitBatterySOC();
	}

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


// ****************************************************************************
/*!
 * BATTERY_UpdateChargeLed changes the charge led based on current charge level
 * and if the battery is charging. If the system is in low power mode then the
 * light level of the charge led is reduced.
 *
 * @param	batteryRsocPt1		current SOC of battery in 0.1% steps
 * @param	chargerStatus		current status of all charge inputs
 * @param	runState			current run state of the task manager
 * @retval	none
 *
 */
// ****************************************************************************
static void BATTERY_UpdateChargeLed(const uint16_t batteryRsocPt1,
						const ChargerStatus_T chargerStatus, const TASKMAN_RunState_t runState)
{
	const Led_T * p_chargeLed = LED_FindHandleByFunction(LED_CHARGE_STATUS);
	uint8_t r, g, b, paramB;

	if (batteryRsocPt1 > 500u)
	{
		r = 0u;
		g = p_chargeLed->paramG;
	}
	else if (batteryRsocPt1 > 150u)
	{
		r = p_chargeLed->paramR;
		g = p_chargeLed->paramG;
	}
	else
	{
		r = p_chargeLed->paramR;
		g = 0u;
	}

	paramB = p_chargeLed->paramB;

	if ( (m_batteryStatus == BAT_STATUS_CHARGING_FROM_IN) || (m_batteryStatus == BAT_STATUS_CHARGING_FROM_5V_IO) )
	{
		// Alternate flash blue to show charging
		b = p_chargeLed->b == 0 ? paramB : 0u;
	}
	else if (chargerStatus == CHG_CHARGE_DONE)
	{
		// Constant blue to show charge done
		b = paramB;
	}
	else
	{
		b = 0u;
	}

	if (TASKMAN_RUNSTATE_LOW_POWER == runState)
	{
		LED_FunctionSetRGB(LED_CHARGE_STATUS, r / 4u, g / 4u, b);
	}
	else
	{
		LED_FunctionSetRGB(LED_CHARGE_STATUS, r, g, b);
	}
}
