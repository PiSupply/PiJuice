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
#include "fuelgauge_conf.h"
#include "fuelgauge_tables.h"


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static void FUELGAUGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);
static bool FUELGAUGE_WriteWord(const uint8_t cmd, const uint16_t word);
static bool FUELGAUGE_ReadWord(const uint8_t cmd, uint16_t *const word);
static bool FUELGAUGE_IcInit(void);
static void FUELGAUGE_CalculateDischargeRate(const uint16_t previousRSoc,
												const uint16_t newRsoc,
												const uint32_t timeDeltaMs);

static void FUELGAUGE_CalculateSOCInit(void);
static uint32_t FUELGAUGE_GetSOCFromOCV(uint16_t batteryMv);
static void FUELGAUGE_UpdateCalculateSOC(const uint16_t batteryMv, const uint32_t dt);

// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint16_t m_fuelgaugeI2CReadResult;
static bool m_fuelgaugeI2CSuccess;
static FUELGAUGE_Status_t m_fuelgaugeIcStatus;
static uint16_t m_batteryMv;
static uint8_t m_icInitState;
static uint16_t m_lastSocPt1;
static uint32_t m_lastSocTimeMs;
static int16_t m_batteryTemperaturePt1;
static RsocMeasurementConfig_T m_rsocMeasurementConfig = RSOC_MEASUREMENT_FROM_IC;
static BatteryTempSenseConfig_T m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
static uint8_t m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
static int16_t m_dischargeRate;
static uint16_t m_fuelgaugeIcId;
static bool m_thermistorGood;
static uint32_t m_lastFuelGaugeTaskTimeMs;
static bool m_updateBatteryProfile;


// ----------------------------------------------------------------------------
// Variables that only have scope in this module that are persistent through reset:

static uint16_t m_ocvSocTbl[256u] __attribute__((section("no_init")));
static uint16_t m_rSocTbl[256u] __attribute__((section("no_init")));
static uint8_t m_rSocTempCompensateTbl[256u] __attribute__((section("no_init")));
static uint16_t m_c0 __attribute__((section("no_init")));
static uint32_t m_soc __attribute__((section("no_init")));


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// CALLBACK HANDLERS
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * FUELGAUGE_I2C_Callback interfaces with the i2cdrv module, lets this module
 * 			know when the transaction is complete and provides a pointer to the
 * 			i2cdriver internal data which will contain information regarding the
 * 			transaction.
 * @param	p_i2cdrvDevice		pointer to the i2cdrv internals
 * @retval	none
 */
// ****************************************************************************
static void FUELGAUGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_RX_COMPLETE)
	{
		crc_t crc = crc_8_init(FUELGAUGE_I2C_ADDR);
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


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * FUELGAUGE_Init configures the module to a known initial state
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void FUELGAUGE_Init(void)
{
	const uint32_t sysTime = HAL_GetTick();

	uint16_t tempU16;
	uint8_t config;

	if (NV_READ_VARIABLE_SUCCESS == NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config))
	{
		m_tempSensorConfig = (uint8_t)(config & 0x7u);
		m_rsocMeasurementConfig = (uint8_t)(config >> 4u) & 0x3u;
	}

	if (EXECUTION_STATE_NORMAL != executionState)
	{
		// FuelGaugeDvInit();
	}

	// Try and talk to the fuel gauge ic
	// Note: SOC might not be correctly evaluated if the battery is being charged or discharged
	if (true == FUELGAUGE_IcInit())
	{
		m_fuelgaugeIcStatus = FUELGAUGE_STATUS_ONLINE;

		if (true == FUELGAUGE_ReadWord(FG_MEM_ADDR_ITE, &tempU16))
		{
			m_lastSocPt1 = tempU16;

			MS_TIMEREF_INIT(m_lastSocTimeMs, sysTime);
		}

		if (true == FUELGAUGE_ReadWord(FG_MEM_ADDR_CELL_MV, &tempU16))
		{
			m_batteryMv = tempU16;
		}
	}
	else
	{
		m_fuelgaugeIcStatus = FUELGAUGE_STATUS_OFFLINE;
		m_lastSocPt1 = 0u;
		m_batteryMv = 0u;
	}

	MS_TIMEREF_INIT(m_lastFuelGaugeTaskTimeMs, sysTime);

}


// ****************************************************************************
/*!
 * FUELGAUGE_Task performs periodic updates for this module
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void FUELGAUGE_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const uint16_t battMv = ANALOG_GetBatteryMv();

	uint16_t tempU16;
	int16_t tempS16;
	uint32_t socTimeDiff;

	if (MS_TIMEREF_TIMEOUT(m_lastFuelGaugeTaskTimeMs, sysTime, FUELGAUGE_TASK_PERIOD_MS))
	{
		if (battMv < FUELGAUGE_MIN_BATT_MV)
		{
			// If the battery voltage is less than the device operating limit then don't even bother!
			m_fuelgaugeIcStatus = FUELGAUGE_STATUS_OFFLINE;
			m_lastSocPt1 = 0u;
			m_batteryMv = 0u;

			return;
		}


		if ( (FUELGAUGE_STATUS_OFFLINE == m_fuelgaugeIcStatus) || m_updateBatteryProfile )
		{
			m_updateBatteryProfile = false;

			if (true == FUELGAUGE_IcInit())
			{
				m_fuelgaugeIcStatus = FUELGAUGE_STATUS_ONLINE;
			}

			if (RSOC_MEASUREMENT_DIRECT_DV == m_rsocMeasurementConfig)
			{
				FUELGAUGE_CalculateSOCInit();
				FUELGAUGE_GetSOCFromOCV(battMv);
			}
		}


		if (FUELGAUGE_STATUS_ONLINE == m_fuelgaugeIcStatus)
		{
			if (true == FUELGAUGE_ReadWord(FG_MEM_ADDR_CELL_MV, &tempU16))
			{
				m_batteryMv = tempU16;
			}

			if (m_temperatureMode == FUEL_GAUGE_TEMP_MODE_THERMISTOR)
			{
				if (true == FUELGAUGE_ReadWord(FG_MEM_ADDR_CELL_TEMP, &tempU16))
				{
					tempS16 = ((int16_t)tempU16 - CELL_TEMP_OFS);

					// Check for a sane number
					if (tempS16 < (int16_t)(mcuTemperature - 10))
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
				m_batteryTemperaturePt1 = mcuTemperature * 10;
				FUELGAUGE_WriteWord(0x08, m_batteryTemperaturePt1 + CELL_TEMP_OFS);
			}

			socTimeDiff = MS_TIMEREF_DIFF(m_lastSocTimeMs, HAL_GetTick());

			if (RSOC_MEASUREMENT_DIRECT_DV == m_rsocMeasurementConfig)
			{
				// TODO - does this need to happen every 125ms?
				FUELGAUGE_UpdateCalculateSOC(battMv, socTimeDiff);

				MS_TIMEREF_INIT(m_lastSocTimeMs, sysTime);
			}
			else
			{
				if (true == FUELGAUGE_ReadWord(FG_MEM_ADDR_ITE, &tempU16))
				{
					if (m_lastSocPt1 != tempU16)
					{
						FUELGAUGE_CalculateDischargeRate(m_lastSocPt1, tempU16,
															socTimeDiff);

						m_lastSocPt1 = tempU16;

						MS_TIMEREF_INIT(m_lastSocTimeMs, sysTime);
					}
				}
			}
		}
	}
}


// ****************************************************************************
/*!
 * FUELGAUGE_UpdateBatteryProfile tells the module to update its configuration
 * based on the current battery profile. Generally called when the battery profile
 * is changed.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void FUELGAUGE_UpdateBatteryProfile(void)
{
	m_updateBatteryProfile = true;
}


// ****************************************************************************
/*!
 * FUELGAUGE_SetConfigData sets the current configuration data
 *
 * @param	p_data		pointer to buffer where the config data is held
 * @param	len			length of config data
 * @retval	none
 */
// ****************************************************************************
void FUELGAUGE_SetConfigData(const uint8_t * const p_data, const uint16_t len)
{
	const uint8_t newTempConfig = p_data[0u] & 0x07u;
	const uint8_t newRsocConfig = (uint8_t)((p_data[0u] & 0x30u) >> 4u);

	uint8_t config;

	if ( (newTempConfig >= BAT_TEMP_SENSE_CONFIG_TYPES)
			|| (newRsocConfig >= RSOC_MEASUREMENT_CONFIG_TYPES)
			)
	{
		return;
	}

	NV_WriteVariable_U8(FUEL_GAUGE_CONFIG_NV_ADDR, p_data[0u]);

	if (NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config) == NV_READ_VARIABLE_SUCCESS)
	{
		m_rsocMeasurementConfig = (RsocMeasurementConfig_T)newRsocConfig;
		m_tempSensorConfig = (BatteryTempSenseConfig_T)newTempConfig;
	}
	else
	{
		m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
		m_rsocMeasurementConfig = RSOC_MEASUREMENT_FROM_IC;
	}
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetConfigData populates a buffer with the current fuel gauge configuration
 *
 * @param	p_data		pointer to buffer where the config data is to be placed
 * @param	p_len		length of status data
 * @retval	none
 */
// ****************************************************************************
void FUELGAUGE_GetConfigData(uint8_t * const p_data, uint16_t * const p_len)
{
	p_data[0u] = (m_tempSensorConfig & 0x07u) | ( (m_rsocMeasurementConfig & 0x03u) << 4u);
	*p_len = 1u;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetIcId returns the id of the fuel gauge IC. Think this is just its
 * address!
 *
 * @param	none
 * @retval	uint16_t		id of the fuel gauge IC
 */
// ****************************************************************************
uint16_t FUELGAUGE_GetIcId(void)
{
	return m_fuelgaugeIcId;
}


// ****************************************************************************
/*!
 * FUELGAUGE_IsNtcOK
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
bool FUELGAUGE_IsNtcOK(void)
{
	return m_thermistorGood;
}


// ****************************************************************************
/*!
 * FUELGAUGE_IsOnline
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
bool FUELGAUGE_IsOnline(void)
{
	return FUELGAUGE_STATUS_ONLINE == m_fuelgaugeIcStatus;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetBatteryTemperature
 *
 * @param	none
 * @retval	int8_t
 */
// ****************************************************************************
int8_t FUELGAUGE_GetBatteryTemperature(void)
{
	return m_batteryTemperaturePt1 / 10;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetSocPt1
 *
 * @param	none
 * @retval	uint16_t
 */
// ****************************************************************************
uint16_t FUELGAUGE_GetSocPt1(void)
{
	return m_lastSocPt1;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetBatteryMaHr
 *
 * @param	none
 * @retval	int16_t
 */
// ****************************************************************************
int16_t FUELGAUGE_GetBatteryMaHr(void)
{
	return m_dischargeRate;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetBatteryMv
 *
 * @param	none
 * @retval	uint16_t		Battery voltage in mV
 */
// ****************************************************************************
uint16_t FUELGAUGE_GetBatteryMv(void)
{
	return m_batteryMv;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetBatteryTempSensorCfg
 *
 * @param	none
 * @retval	BatteryTempSenseConfig_T
 */
// ****************************************************************************
BatteryTempSenseConfig_T FUELGAUGE_GetBatteryTempSensorCfg(void)
{
	return m_tempSensorConfig;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * FUELGAUGE_IcInit
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
static bool FUELGAUGE_IcInit(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	uint16_t tempU16;

	m_icInitState = 0u;

	// Check to see if the device is online
	if (FUELGAUGE_ReadWord(0x11, &tempU16))
	{
		m_fuelgaugeIcId = tempU16;

		m_icInitState++;

		// Set operational mode
		if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_POWER_MODE, POWER_MODE_OPERATIONAL))
		{
			return false;
		}

		m_icInitState++;

		// set APA
		if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_APA, 0x36u))
		{
			return false;
		}

		m_icInitState++;

		// set change of the parameter
		if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_PARAM_NO_SET,
				(currentBatProfile->chemistry == BAT_CHEMISTRY_LIPO_GRAPHENE) ?
						BATT_PROFILE_0 :
						BATT_PROFILE_1)
				)
		{
			return false;
		}

		m_icInitState++;

		// set APT
		if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_APT, 0x3000u))
		{
			return false;
		}

		m_icInitState++;

		if ( (NULL != currentBatProfile) && (0xFFFFu != currentBatProfile->ntcB) )
		{
			// Set NTC B constant
			if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_THERMB, currentBatProfile->ntcB))
			{
				return false;
			}
		}

		m_icInitState++;

		// Set NTC mode
		if (false == FUELGAUGE_WriteWord(FG_MEM_ADDR_THERM_TYPE, THERM_TYPE_NTC))
		{
			return false;
		}

		m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

		// IC only calculates for LIPO chemistry, override the setting
		if ( (currentBatProfile->chemistry != BAT_CHEMISTRY_LIPO) && (currentBatProfile->chemistry != BAT_CHEMISTRY_LIPO_GRAPHENE) )
		{
			m_rsocMeasurementConfig = RSOC_MEASUREMENT_DIRECT_DV;
		}

		m_icInitState++;

		return true;
	}

	m_fuelgaugeIcId = 0u;

	return false;

}


// ****************************************************************************
/*!
 * FUELGAUGE_ReadWord reads a single register word from the fuel gauge IC, the
 * routine returns true if the read was successful and the value is valid. The
 * crc of the returned value is checked in the callback to ensure correctness.
 *
 * @param	cmd			address of device register to read
 * @param	p_word		pointer to value destination
 * @retval	bool		false = read unsuccessful
 * 						true = read successful
 */
// ****************************************************************************
static bool FUELGAUGE_ReadWord(const uint8_t cmd, uint16_t * const p_word)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;

	m_fuelgaugeI2CSuccess = false;

	transactGood = I2CDRV_Transact(FUELGAUGE_I2C_PORTNO, FUELGAUGE_I2C_ADDR, &cmd, 3u,
						I2CDRV_TRANSACTION_RX, FUELGAUGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGAUGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	*p_word = m_fuelgaugeI2CReadResult;

	return m_fuelgaugeI2CSuccess;
}


// ****************************************************************************
/*!
 * FUELGAUGE_WriteWord writes to a word register within the fuel gauge device.
 * A transfer buffer is created and the crc is calculated and appended to the
 * end of the buffer. The routine will wait until the transfer is complete and
 * upon a successful write will return true.
 *
 * @param	memAddress		device memory address to write to
 * @param	value			uint16 value to write to the memory address
 * @retval	bool			false = write unsuccessful
 * 							true = write successful
 */
// ****************************************************************************
static bool FUELGAUGE_WriteWord(const uint8_t memAddress, const uint16_t value)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;
	uint8_t writeData[4u] = { memAddress, (uint8_t)(value & 0xFFu), (uint8_t)(value >> 8u), 0u};

	crc_t crc = crc_8_init(FUELGAUGE_I2C_ADDR);
	crc = crc_8_update(crc, writeData, 3u);

	writeData[3u] = (uint8_t)crc;

	m_fuelgaugeI2CSuccess = false;

	transactGood = I2CDRV_Transact(FUELGAUGE_I2C_PORTNO, FUELGAUGE_I2C_ADDR, writeData, 4u,
						I2CDRV_TRANSACTION_TX, FUELGAUGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGAUGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	return m_fuelgaugeI2CSuccess;
}


// ****************************************************************************
/*!
 * FUELGAUGE_GetSocFromOCV finds the SOC from the index of the calculated SOC
 * voltage table.
 *
 * @param	batteryMv	battery voltage in millivolts
 * @retval	uint32_t	index of SOC table * 2^23
 */
// ****************************************************************************
static uint32_t FUELGAUGE_GetSOCFromOCV(uint16_t batteryMv)
{
	uint8_t i = 0u;

	while ( (m_ocvSocTbl[i] < batteryMv) && (i < 255u) )
	{
		i++;
	}

	return i * (1u << 23u);
}


// ****************************************************************************
/*!
 * FUELGAUGE_CalculateSOCInit initialises the battery SOC calculation tables from
 * using the extended parameter settings for the battery internal DC resistance
 * characteristics and the preset voltage curves. The parameters originate from
 * the python script in the pijuice software folder.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void FUELGAUGE_CalculateSOCInit(void)
{
	const BatteryProfile_T * const p_currentBatProfile = BATTERY_GetActiveProfileHandle();
	const int16_t * ocvSocTableRef = ocvSocTableNormLipo;

	int16_t i;
	uint16_t ocv50 = 3800u;
	uint16_t ocv10 = 3649u;
	uint16_t ocv90 = 4077u;

	uint16_t r50 = 1.82 * 156u;
	uint16_t r10 = 1.82 * 160u;
	uint16_t r90 = 1.82 * 155u;

	uint32_t ocvRef50 = 3791u;
	uint32_t ocvRef10 = 3652u;
	uint32_t ocvRef90 = 4070u;

	int32_t k1;
	uint32_t OCVdSoc50;
	int32_t k2;
	int32_t d1;
	int32_t d2;

	m_c0 = 1820u;

	if (NULL == p_currentBatProfile)
	{
		return;
	}

	if (BAT_CHEMISTRY_LIFEPO4 == p_currentBatProfile->chemistry)
	{
		ocvSocTableRef = ocvSocTableNormLifepo4;
		ocvRef50 = 3243;
		ocvRef10 = 3111;
		ocvRef90 = 3283;
	}
	else
	{
		ocvSocTableRef = ocvSocTableNormLipo;
		ocvRef50 = 3791;
		ocvRef10 = 3652;
		ocvRef90 = 4070;
	}


	m_c0 = p_currentBatProfile->capacity;


	ocv50 = (0xFFFF != p_currentBatProfile->ocv50) ?
				p_currentBatProfile->ocv50 :
				((uint16_t)p_currentBatProfile->regulationVoltage + 175u + p_currentBatProfile->cutoffVoltage) * 10u;


	ocv10 = (0xFFFF != p_currentBatProfile->ocv10) ?
				p_currentBatProfile->ocv10 :
				0.96322f * ocv50;


	ocv90 = (0xFFFF != p_currentBatProfile->ocv90) ?
				p_currentBatProfile->ocv90 :
				1.0735f * ocv50;


	if (0xFFFFu != p_currentBatProfile->r50)
	{
		r50 = ((uint32_t)m_c0 * p_currentBatProfile->r50) / 100000u;
	}


	r10 = (0xFFFFu != p_currentBatProfile->r10) ?
			((uint32_t)m_c0 * p_currentBatProfile->r10) / 100000u :
			r50;


	r90 = (0xFFFFu != p_currentBatProfile->r90) ?
			((uint32_t)m_c0 * p_currentBatProfile->r90) / 100000u :
			r50;


	// TODO - sanity check on parameters


	k1 = ((ocvRef90 - ocvRef50) * (230 - 26) * (ocv50 - ocv10)) / 1024u;

	for (i = 0; i < 26u; i++)
	{
		m_ocvSocTbl[i] = ocv50 - ((k1 * ocvSocTableRef[i]) / 65536); //ocv50 - (((((int32_t)(4070-3791)*(230-26.0)*dOCV10)>>8)*ocvSocTableNorm[i])>>10);
		m_rSocTbl[i] = 65535 / (r50 + (r50 - r10) * (i - 128.0f) / (128 - 26));
	}

	OCVdSoc50 = (((uint32_t)ocv50) * 8196u) / (230u - 26u);

	k1 = ((ocvRef90 - ocvRef50) * (ocv50 - ocv10)) / 16;
	k2 = ((ocvRef50 - ocvRef10) * (ocv90 - ocv50)) / 16;

	for (i = 26; i < 128; i++)
	{
		d1= (OCVdSoc50 - ((k1 * ocvSocTableRef[i]) / 512)) * (230.0f - i); //ocv50/(230-26.0) - (((((int32_t)(4070-3791)*dOCV10))*ocvSocTableNorm[i])>>18);
		d2= (OCVdSoc50 - ((k2 * ocvSocTableRef[i]) / 512)) * (i - 26); //ocv50/(230-26.0) + (((((int32_t)(3652-3791)*dOCV90))*ocvSocTableNorm[i])>>18);
		m_ocvSocTbl[i] = (d2 + d1) / 8192;
		m_rSocTbl[i] = 65535 / (r50 + (r50 - r10) * (i - 128.0f) / (128 - 26));
	}

	k2 = ((ocvRef50 - ocvRef10) * (230 - 26) * (ocv90 - ocv50)) / 1024;

	// TODO - int converts to uint!
	for (i = 128; i < 256; i++)
	{
		m_ocvSocTbl[i] = ocv50 - ((k2 * ocvSocTableRef[i]) / 65536); //ocv50 + (((((int32_t)(3652-3791)*(230-26.0)*dOCV90)>>8)*ocvSocTableNorm[i])>>10);
		m_rSocTbl[i] = 65535 / (r50 + (r50 - r90) * (i - 128.0f) / (128 - 230));
	}

	// TODO - this hurts my head.
	for (i = -127; i < 129; i++)
	{
		m_rSocTempCompensateTbl[(uint8_t)i] = (i < 21) ? 255ul * 32 / (32 + 2 * (20 - i)) : 255; // 1 + 2*(20-batteryTemp)/(20-(-12)), krtemp ~ 3, temperature=i
	}

	//batteryCurrent = 0;
}


// ****************************************************************************
/*!
 * FUELGAUGE_UpdateCalculateSOC attempts to determine the change in SOC from the
 * battery voltage using the extended parameter settings for the battery internal
 * DC resistance characteristics.
 *
 * @param	batteryMv		current voltage of battery in mV
 * @param	dt				period in mS since the last update
 * @retval	none
 */
// ****************************************************************************
static void FUELGAUGE_UpdateCalculateSOC(const uint16_t batteryMv, const uint32_t dt)
{
	const uint32_t ind = m_soc >> 23u;
	const int32_t dif = m_ocvSocTbl[ind] - batteryMv;
	const uint16_t rsoc = ((uint32_t)m_rSocTbl[ind] * m_rSocTempCompensateTbl[(uint8_t)m_batteryTemperaturePt1]) >> 8;
	const int32_t dSoC = ((dif * dt * ((596l * rsoc) / 256))) / 256; //dif*596*dt/res;

	m_dischargeRate = (dif * (((uint32_t)m_c0 * rsoc) / 64)) / 1024;

	m_soc -= dSoC;
	m_soc = m_soc <= 2139095040 ? m_soc : 2139095040;

	m_soc = (m_soc > 0) ? m_soc : 0;

	// Divide SOC by 2147483.648 to get batterySOC
	m_lastSocPt1 = ((m_soc >> 7) * 125) >> 21;
}


// ****************************************************************************
/*!
 * FUELGAUGE_CalculateDischargeRate looks at the change in SOC and works out how
 * much capacity has been taken over the amount of time the change occurred and
 * converts that value into mA per hour. If no change in SOC has occurred or time
 * delta is 0 then no action is taken.
 *
 * @param	previousRSoc		soc reading at start of time period
 * @param	newRsoc				soc reading at end of time period
 * @param	timeDeltaMs			time elapsed between readings
 * @retval	none
 */
// ****************************************************************************
// TODO - check this calculation against original code
static void FUELGAUGE_CalculateDischargeRate(const uint16_t previousRSoc,
												const uint16_t newRsoc,
												const uint32_t timeDeltaMs)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const int16_t socDelta = previousRSoc - newRsoc;
	const int32_t deltaFactor = (int32_t)socDelta * timeDeltaMs * currentBatProfile->capacity;
	uint16_t dischargeRate;

	// Check to make sure there is a change in both time and SOC
	if (deltaFactor != 0)
	{
		dischargeRate = UTIL_FixMul_U32_U16(FUELGAUGE_SOC_TO_I_K, abs(deltaFactor));
		m_dischargeRate = (deltaFactor > 0) ? dischargeRate : -dischargeRate;
	}
}
