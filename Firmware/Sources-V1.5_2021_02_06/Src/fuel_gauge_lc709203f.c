/*
 * fuel_gauge_lc709203f.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "main.h"

#include "osloop.h"
#include "system_conf.h"


#include "time_count.h"

#include "adc.h"
#include "analog.h"
#include "charger_bq2416x.h"
#include "power_source.h"
#include "execution.h"
#include "nv.h"

#include "i2cdrv.h"
#include "crc.h"
#include "util.h"

#include "fuel_gauge_lc709203f.h"
#include "fuelguage_conf.h"


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void FUELGUAGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);
bool FUELGUAGE_WriteWord(const uint8_t cmd, const uint16_t word);
bool FUELGUAGE_ReadWord(const uint8_t cmd, uint16_t *const word);
bool FUELGUAGE_IcInit(void);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint16_t m_fuelgaugeI2CReadResult;
static bool m_fuelgaugeI2CSuccess;
static FUELGUAGE_Status_t m_fuelgaugeIcStatus;
static uint16_t m_batteryMv;
static uint8_t m_icInitState;
static uint16_t m_lastSocPt1;
static uint32_t m_lastSocTimeMs;
static int16_t m_batteryTemperaturePt1;
static RsocMeasurementConfig_T m_rsocMeasurementConfig = RSOC_MEASUREMENT_AUTO_DETECT;
static BatteryTempSenseConfig_T m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
static uint8_t m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
static int16_t m_dischargeRate;
static uint16_t m_fuelguageIcId;
static bool m_thermistorGood;
static uint32_t m_lastFuelGaugeTaskTimeMs;
static bool m_updateBatteryProfile;


void FUELGUAGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_RX_COMPLETE)
	{
		crc_t crc = crc_8_init(FUELGUAGE_I2C_ADDR);
		// crc includes address, mem address, address | 0x01, data
		crc = crc_8_update(crc, p_i2cdrvDevice->data, 4u);

		if (crc == p_i2cdrvDevice->data[4u])
		{
			m_fuelgaugeI2CReadResult = (uint16_t)p_i2cdrvDevice->data[2u] | (p_i2cdrvDevice->data[3u] << 8u);

			m_fuelgaugeI2CSuccess = true;
		}
		else
		{
			m_fuelgaugeI2CSuccess = false;
		}
	}
	else if (p_i2cdrvDevice->event == I2CDRV_EVENT_TX_COMPLETE)
	{
		m_fuelgaugeI2CSuccess = true;
	}
	else
	{
		m_fuelgaugeI2CSuccess = false;
	}
}


void FUELGUAGE_Init(void)
{
	const uint32_t sysTime = HAL_GetTick();

	uint16_t tempU16;
	uint8_t config;

	if (NV_READ_VARIABLE_SUCCESS == NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config))
	{
		m_tempSensorConfig = (uint8_t)(config & 0x07u);
		m_rsocMeasurementConfig = (uint8_t)(config >> 4u) & 0x03u;
	}

	if (EXECUTION_STATE_NORMAL != executionState)
	{
		// FuelGaugeDvInit();
	}

	// Try and talk to the fuel guage ic.
	if (true == FUELGUAGE_IcInit())
	{
		m_fuelgaugeIcStatus = FUELGUAGE_STATUS_ONLINE;

		if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_ITE, &tempU16))
		{
			m_lastSocPt1 = tempU16;

			MS_TIMEREF_INIT(m_lastSocTimeMs, sysTime);
		}

		if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_CELL_MV, &tempU16))
		{
			m_batteryMv = tempU16;
		}
	}
	else
	{
		m_fuelgaugeIcStatus = FUELGUAGE_STATUS_OFFLINE;
		m_lastSocPt1 = 0u;
		m_batteryMv = 0u;
	}

	MS_TIMEREF_INIT(m_lastFuelGaugeTaskTimeMs, sysTime);

}

void SocEvaluateDirectDynVoltage(uint16_t batVolt, int32_t dt)
{
	// Dynamic voltage state of charge evaluation
}


void SocEvaluateFuelGaugeIc(const uint16_t previousRSoc, const uint16_t newRsoc, const uint32_t timeDeltaMs)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();

	int16_t socDelta = previousRSoc - newRsoc;
	int32_t deltaFactor = (int32_t)socDelta * timeDeltaMs * currentBatProfile->capacity;

	bool neg = (deltaFactor < 0u);

	uint16_t dischargeRate = UTIL_FixMul_U32_U16(FUELGUAGE_SOC_TO_I_K, abs(deltaFactor));

	m_dischargeRate = (false == neg) ? dischargeRate : -dischargeRate;

}


void FUELGUAGE_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const uint16_t battMv = ANALOG_GetBatteryMv();

	uint16_t tempU16;
	int16_t tempS16;
	uint32_t socTimeDiff;

	if (MS_TIMEREF_TIMEOUT(m_lastFuelGaugeTaskTimeMs, sysTime, FUELGUAGE_TASK_PERIOD_MS))
	{
		if (battMv < FUELGUAGE_MIN_BATT_MV)
		{
			// If the battery voltage is less than the device operating limit then don't even bother!
			m_fuelgaugeIcStatus = FUELGUAGE_STATUS_OFFLINE;
			m_lastSocPt1 = 0u;
			m_batteryMv = 0u;

			return;
		}

		if ( (FUELGUAGE_STATUS_OFFLINE == m_fuelgaugeIcStatus) || m_updateBatteryProfile )
		{
			if (true == FUELGUAGE_IcInit())
			{
				m_fuelgaugeIcStatus = FUELGUAGE_STATUS_ONLINE;
			}
		}

		if ( (FUELGUAGE_STATUS_ONLINE == m_fuelgaugeIcStatus))
		{
			if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_CELL_MV, &tempU16))
			{
				m_batteryMv = tempU16;
			}

			if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_ITE, &tempU16))
			{
				if (m_lastSocPt1 != tempU16)
				{

					socTimeDiff = MS_TIMEREF_DIFF(m_lastSocTimeMs, HAL_GetTick());

					SocEvaluateFuelGaugeIc(m_lastSocPt1, tempU16, socTimeDiff);

					m_lastSocPt1 = tempU16;

					MS_TIMEREF_INIT(m_lastSocTimeMs, sysTime);
				}
			}

			if (m_temperatureMode == FUEL_GAUGE_TEMP_MODE_THERMISTOR)
			{
				if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_CELL_TEMP, &tempU16))
				{
					tempS16 = ((int16_t)tempU16 - CELL_TEMP_OFS);

					if (tempS16 < 0)
					{
						m_batteryTemperaturePt1 = mcuTemperature * 10;
					}
					else
					{
						m_batteryTemperaturePt1 = tempS16;
					}
				}
			}
			else
			{
				// TODO - fix conversion!!
				m_batteryTemperaturePt1 = mcuTemperature * 10;
				FUELGUAGE_WriteWord(0x08, m_batteryTemperaturePt1 + CELL_TEMP_OFS);
			}
		}
	}
}


void FUELGUAGE_UpdateBatteryProfile(void)
{
	m_updateBatteryProfile = true;
}


void FUELGUAGE_SetConfig(const uint8_t * const data, const uint16_t len)
{
	const uint8_t newTempConfig = data[0u] & 0x07u;
	const uint8_t newRsocConfig = (uint8_t)((data[0u] & 0x30u) >> 4u);

	uint8_t config;

	if ( (newTempConfig >= BAT_TEMP_SENSE_CONFIG_TYPES)
			|| (newRsocConfig >= RSOC_MEASUREMENT_CONFIG_TYPES)
			)
	{
		return;
	}

	NvWriteVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, data[0u]);

	if (NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config) == NV_READ_VARIABLE_SUCCESS)
	{
		m_rsocMeasurementConfig = (RsocMeasurementConfig_T)newRsocConfig;
		m_tempSensorConfig = (BatteryTempSenseConfig_T)newTempConfig;
	}
	else
	{
		m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
		m_rsocMeasurementConfig = RSOC_MEASUREMENT_AUTO_DETECT;
	}
}


void FUELGUAGE_GetConfig(uint8_t * const data, uint16_t * const len)
{
	data[0u] = (m_tempSensorConfig & 0x07u) | ( (m_rsocMeasurementConfig & 0x03u) << 4u);
	*len = 1u;
}


uint16_t FUELGUAGE_GetIcId(void)
{
	return m_fuelguageIcId;
}


bool FUELGUAGE_IsNtcOK(void)
{
	return m_thermistorGood;
}


bool FUELGUAGE_IsOnline(void)
{
	return FUELGUAGE_STATUS_ONLINE == m_fuelgaugeIcStatus;
}


int8_t FUELGUAGE_GetBatteryTemperature(void)
{
	return m_batteryTemperaturePt1 / 10;
}


uint16_t FUELGUAGE_GetSocPt1(void)
{
	return m_lastSocPt1;
}


int16_t FUELGUAGE_GetBatteryMa(void)
{
	return m_dischargeRate;
}


uint16_t FUELGUAGE_GetBatteryMv(void)
{
	return m_batteryMv;
}


bool FUELGUAGE_IcInit(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	uint16_t tempU16;

	m_icInitState = 0u;

	// Check to see if the device is online
	if (FUELGUAGE_ReadWord(0x11, &tempU16))
	{
		m_fuelguageIcId = tempU16;

		m_icInitState++;

		// Set operational mode
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_POWER_MODE, POWER_MODE_OPERATIONAL))
		{
			return false;
		}

		m_icInitState++;

		// set APA
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_APA, 0x36u))
		{
			return false;
		}

		m_icInitState++;

		// set change of the parameter
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_PARAM_NO_SET, BATT_PROFILE_1))
		{
			return false;
		}

		m_icInitState++;

		// set APT
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_APT, 0x3000u))
		{
			return false;
		}

		m_icInitState++;

		if ( (NULL != currentBatProfile) && (0xFFFFu != currentBatProfile->ntcB) )
		{
			// Set NTC B constant
			if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_THERMB, currentBatProfile->ntcB))
			{
				return false;
			}
		}

		m_icInitState++;

		// Set NTC mode
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_THERM_TYPE, THERM_TYPE_NTC))
		{
			return false;
		}

		m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

		m_icInitState++;

		return true;
	}

	m_fuelguageIcId = 0u;

	return false;

}


BatteryTempSenseConfig_T FUELGUAGE_GetBatteryTempSensorCfg(void)
{
	return m_tempSensorConfig;
}


bool FUELGUAGE_ReadWord(const uint8_t cmd, uint16_t *const word)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;

	m_fuelgaugeI2CSuccess = false;

	transactGood = I2CDRV_Transact(FUELGUAGE_I2C_PORTNO, FUELGUAGE_I2C_ADDR, &cmd, 3u,
						I2CDRV_TRANSACTION_RX, FUELGUAGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	*word = m_fuelgaugeI2CReadResult;

	return m_fuelgaugeI2CSuccess;
}


bool FUELGUAGE_WriteWord(const uint8_t memAddress, const uint16_t value)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;
	uint8_t writeData[4u] = { memAddress, (uint8_t)(value & 0xFFu), (uint8_t)(value >> 8u), 0u};

	crc_t crc = crc_8_init(FUELGUAGE_I2C_ADDR);
	crc = crc_8_update(crc, writeData, 3u);

	writeData[3u] = (uint8_t)crc;

	m_fuelgaugeI2CSuccess = false;

	transactGood = I2CDRV_Transact(FUELGUAGE_I2C_PORTNO, FUELGUAGE_I2C_ADDR, writeData, 4u,
						I2CDRV_TRANSACTION_TX, FUELGUAGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	return m_fuelgaugeI2CSuccess;
}
