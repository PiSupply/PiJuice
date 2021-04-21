/*
 * charger_bq2416x.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <string.h>

#include "main.h"

#include "system_conf.h"
#include "i2cdrv.h"
#include "iodrv.h"
#include "util.h"

#include "battery.h"
#include "charger_bq2416x.h"
#include "charger_conf.h"

#include "time_count.h"
#include "nv.h"
#include "eeprom.h"
#include "fuel_gauge_lc709203f.h"
#include "stddef.h"
#include "power_source.h"
#include "analog.h"

#include "execution.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define CHARGING_CONFIG_CHARGE_EN_bm			0x01u
#define CHARGE_ENABLED							(0u != (m_chargingConfig & CHARGING_CONFIG_CHARGE_EN_bm))
#define CHARGE_DISABLED							(0u == (m_chargingConfig & CHARGING_CONFIG_CHARGE_EN_bm))

#define CHGR_INPUTS_CONFIG_INPUT_PREFERENCE_bm	0x01u
#define CHGR_INPUTS_CONFIG_ENABLE_RPI5V_IN_bm	0x02u
#define CHGR_INPUTS_CONFIG_ENABLE_NOBATT_bm		0x04u
#define CHGR_INPUTS_CONFIG_LIMIT_IN_2pt5A_bm	0x08u
#define CHRG_CONFIG_INPUTS_WRITE_NV_bm			0x80u

#define CHGR_INPUTS_CONFIG_CHARGER_DPM_Pos		4u
#define CHGR_INPUTS_CONFIG_CHARGER_DPM_bm		(3u << CHGR_INPUTS_CONFIG_CHARGER_DPM_Pos)

#define CHGR_CONFIG_INPUTS_RPI5V_PREFERRED		(0u != (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_INPUT_PREFERENCE_bm))
#define CHGR_CONFIG_INPUTS_PWRIN_PREFERRED		(0u == (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_INPUT_PREFERENCE_bm))
#define CHGR_CONFIG_INPUTS_RPI5V_ENABLED		(0u != (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_ENABLE_RPI5V_IN_bm))
#define CHRG_CONFIG_INPUTS_NOBATT_ENABLED		(0u != (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_ENABLE_NOBATT_bm))
#define CHRG_CONFIG_INPUTS_IN_LIMITED_1pt5A		(0u == (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_LIMIT_IN_2pt5A_bm))
#define CHRG_CONFIG_INPUTS_IN_LIMITED_2pt5A		(0u != (m_chargerInputsConfig & CHGR_INPUTS_CONFIG_LIMIT_IN_2pt5A_bm))
#define CHRG_CONFIG_INPUTS_DPM					((m_chargerInputsConfig >> CHGR_INPUTS_CONFIG_CHARGER_DPM_Pos) & 0x07u)


#define CHG_READ_PERIOD_MS 						90u  // ms
#define CHARGER_WDT_TIMOUT_PERIOD_MS			30000u
#define CHARGER_WDT_RESET_PERIOD_MS 			1000u
#define CHARGER_VIN_DPM_USB						6u


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static bool CHARGER_UpdateLocalRegister(const uint8_t regAddress);
static bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value, const uint8_t writeMask);
static bool CHARGER_UpdateAllLocalRegisters(void);
static bool CHARGER_ReadDeviceRegister(const uint8_t regAddress);

static void CHARGER_UpdateSupplyPreference(void);
static void CHARGER_UpdateChgCurrentAndTermCurrent(void);
static void CHARGER_UpdateVinDPM(void);
static void CHARGER_UpdateTempRegulationControlStatus(void);
static void CHARGER_UpdateControlStatus(void);
static void CHARGER_UpdateRPi5VInLockout(void);
static void CHARGER_UpdateRegulationVoltage(void);
static void CHARGER_UpdateSettings(void);

static void CHARGER_KickDeviceWatchdog(const uint32_t sysTime);

static bool CHARGER_CheckForPoll(void);

void CHARGER_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);
void CHARGER_ReadAll_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);
void CHARGER_WDT_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);

// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

// TODO - Make these defines in the charger conf header
static uint8_t m_registersWriteMask[CHARGER_REGISTER_COUNT] =
{
		(uint8_t)(CHGR_SUPPLY_SEL_bm | CHGR_SC_TMR_RST_bm),
		(uint8_t)(CHGR_BS_OTG_LOCK_bm | CHGR_BS_EN_NOBAT_OP_bm),
		(uint8_t)(CHGR_REGISTER_ALLBITS & ~(CHGR_CTRL_RESET_bm)),
		(uint8_t)(CHGR_REGISTER_ALLBITS),
		0u,
		(uint8_t)(CHGR_REGISTER_ALLBITS),
		(uint8_t)(CHGR_VDPPM_USB_VDDPM_Msk | CHGR_VDPPM_VIN_VDDPM_Msk),
		(uint8_t)(CHGR_REGISTER_ALLBITS & ~(CHGR_ST_NTC_FAULT_Msk))
};

static uint8_t m_registersIn[CHARGER_REGISTER_COUNT] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t m_registersOut[CHARGER_REGISTER_COUNT] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// current limit code as defined in datasheet
static CHARGER_USBInCurrentLimit_T m_rpi5VCurrentLimit = CHG_IUSB_LIMIT_100MA;

static bool m_i2cSuccess;
static uint8_t m_i2cReadRegResult;

static uint8_t m_chargingConfig;
static uint8_t m_chargerInputsConfig;

static ChargerStatus_T m_chargerStatus = CHG_NA;

static uint32_t m_lastWDTResetTimeMs;
static uint32_t m_lastRegReadTimeMs;
static bool m_interrupt;
static bool m_chargerNeedPoll;
static bool m_updateBatteryProfile;


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// CALLBACK HANDLERS
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * CHARGER_I2C_Callback on a completed transfer (failed or not) the m_i2cSuccess
 * flag is set based upon the result of the transaction. If successful and a read,
 * the result is stored in the temporary read variable m_i2cReadRegResult.
 *
 * @param	p_i2cdrvDevice		const pointer to the device struct that performed
 * 								the transaction.
 * @retval	none
 *
 */
// ****************************************************************************
void CHARGER_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_RX_COMPLETE)
	{
		m_i2cReadRegResult = p_i2cdrvDevice->data[2u];
		m_i2cSuccess = true;
	}
	else if (p_i2cdrvDevice->event == I2CDRV_EVENT_TX_COMPLETE)
	{
		m_i2cSuccess = true;
	}
	else
	{
		m_i2cSuccess = false;
	}
}


// ****************************************************************************
/*!
 * CHARGER_ReadAll_I2C_Callback called when all the device registers are read,
 * if the read was successful then the local copy of the registers are all updated
 * and the m_i2cSuccess flag is set. If not then m_i2cSuccess is set to false.
 *
 * @param	p_i2cdrvDevice		const pointer to the device struct that performed
 * 								the transaction.
 * @retval	none
 *
 */
// ****************************************************************************
void CHARGER_ReadAll_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_RX_COMPLETE)
	{
		memcpy(m_registersIn, &p_i2cdrvDevice->data[2u], CHARGER_REGISTER_COUNT);
		m_i2cSuccess = true;
	}
	else
	{
		m_i2cSuccess = false;
	}
}


// ****************************************************************************
/*!
 * CHARGER_WDT_I2C_Callback gets called when the WDT reset bit is attempted to
 * be written. Only the timer is initialised to tell the task routine when to
 * perform another reset.
 *
 * @param	p_i2cdrvDevice		const pointer to the device struct that performed
 * 								the transaction.
 * @retval	none
 *
 */
// ****************************************************************************
void CHARGER_WDT_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_TX_COMPLETE)
	{
		// Transact time is close enough.
		m_lastWDTResetTimeMs = p_i2cdrvDevice->transactTime;
	}
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * CHARGER_Init configures the module to a known initial state
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_Init(void)
{
	const uint32_t sysTime = HAL_GetTick();
	uint8_t i;

	// If not just powered on...
	if (EXECUTION_STATE_NORMAL != executionState)
	{
		if (NV_ReadVariable_U8(CHARGER_INPUTS_CONFIG_NV_ADDR, &m_chargerInputsConfig))
		{
			CHARGER_SetInputsConfig(m_chargerInputsConfig);
		}
		else
		{
			m_chargerInputsConfig |= CHGR_INPUTS_CONFIG_INPUT_PREFERENCE_bm;	/* Prefer RPi */
			m_chargerInputsConfig |= CHGR_INPUTS_CONFIG_ENABLE_RPI5V_IN_bm;	/* Enable RPi 5V in */
			m_chargerInputsConfig |= 0u;
			m_chargerInputsConfig |= CHGR_INPUTS_CONFIG_LIMIT_IN_2pt5A_bm;	/* 2.5A Vin limit */
			m_chargerInputsConfig |= 0u;
		}

		if (false == NV_ReadVariable_U8(CHARGING_CONFIG_NV_ADDR, &m_chargingConfig))
		{
			// Set enable charge if unprogrammed
			CHARGER_SetChargeEnableConfig(0x80 | CHARGING_CONFIG_CHARGE_EN_bm);
		}
	}


	MS_TIMEREF_INIT(m_lastRegReadTimeMs, sysTime);
	MS_TIMEREF_INIT(m_lastWDTResetTimeMs, sysTime);


	// Initialise all values from the device, most likely going to be default and a watchdog timeout
	CHARGER_UpdateAllLocalRegisters();


	// Initialise all write values
	for (i = 0; i < CHARGER_REGISTER_COUNT; i++)
	{
		m_registersOut[i] = (m_registersIn[i] & m_registersWriteMask[i]);
	}

	// Don't allow charging from RPi 5V
	m_registersOut[CHG_REG_BATTERY_STATUS] |= CHGR_BS_OTG_LOCK_bm;
	CHARGER_UpdateDeviceRegister(CHG_REG_BATTERY_STATUS, m_registersOut[CHG_REG_BATTERY_STATUS], CHGR_REGISTER_ALLBITS);


	// Disable charging and set current limit from RPi to 500mA
	// High impedance mode is not enabled or we might not get any power!
	m_registersOut[CHG_REG_CONTROL] = (CHGR_CTRL_CHG_DISABLE_bm | CHGR_CTRL_IUSB_LIMIT_500MA);
	CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL, m_registersOut[CHG_REG_CONTROL], m_registersWriteMask[CHG_REG_CONTROL]);


	DelayUs(500);


	CHARGER_UpdateRPi5VInLockout();
	CHARGER_UpdateTempRegulationControlStatus();

	CHARGER_UpdateLocalRegister(CHG_REG_SUPPLY_STATUS);
	CHARGER_UpdateLocalRegister(CHG_REG_BATTERY_STATUS);


	m_chargerStatus = (m_registersIn[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_STAT_Pos) & 0x07u;

}


// ****************************************************************************
/*!
 * CHARGER_Task performs periodic updates for this module
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_Task(void)
{
	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();
	const uint32_t sysTime = HAL_GetTick();
	const IODRV_Pin_t * p_pin = IODRV_GetPinInfo(2u);
	const ChargerStatus_T lastChargerStatus = m_chargerStatus;

	uint8_t tempReg;


	if (p_pin->lastPosPulseWidthTimeMs > 200u)
	{
		IORDV_ClearPinEdges(2u);

		//m_chargingConfig ^= CHARGING_CONFIG_CHARGE_EN_bm;
		m_chargerInputsConfig ^= CHGR_INPUTS_CONFIG_ENABLE_NOBATT_bm;

		m_chargerNeedPoll = true;
	}


	if (MS_TIMEREF_TIMEOUT(m_lastWDTResetTimeMs, sysTime, CHARGER_WDT_RESET_PERIOD_MS))
	{
		// Time to give the dog a poke?
		CHARGER_KickDeviceWatchdog(sysTime);

		// Timer gets updated on successful write in callback
	}


	// If there's an over voltage detection on the battery manual says to toggle HiZ bit.
	// This could occur because of battery insertion or power source insertion
	// Also check that the battery info hasn't been updated
	// Also make sure there is a battery connected!
	if ( (batteryStatus != BAT_STATUS_NOT_PRESENT) &&
			( (CHGR_BS_BATSTAT_OVP == (m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk)) ||
					m_updateBatteryProfile ) )
	{
		// Clear the battery update flag
		m_updateBatteryProfile = false;

		// Get previously written setting (ignore anything new for now)
		tempReg = (m_registersIn[CHG_REG_CONTROL] & m_registersWriteMask[CHG_REG_CONTROL]);

		// Set high-z mode to ensure safe setting of battery regulation voltage
		tempReg |= CHGR_CTRL_HZ_MODE_bm;
		CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL, tempReg, m_registersWriteMask[CHG_REG_CONTROL]);

		// Read the value back or updating regulation voltage won't know it's in high-z mode
		CHARGER_UpdateLocalRegister(CHG_REG_CONTROL);

		// Set the batvreg value
		CHARGER_UpdateRegulationVoltage();

		// High z will be changed back when the registers are updated
		m_chargerNeedPoll = true;
	}


	if (true == m_chargerNeedPoll)
	{
		CHARGER_UpdateSettings();

		// Ensure the registers get updated
		MS_TIMEREF_INIT(m_lastRegReadTimeMs, sysTime - CHG_READ_PERIOD_MS);
	}


	// Periodically read register states from charger
	if (MS_TIME_COUNT(m_lastRegReadTimeMs) >= CHG_READ_PERIOD_MS || true == m_interrupt)
	{
		m_interrupt = false;

		if (CHARGER_UpdateAllLocalRegisters())
		{
			m_chargerStatus = (m_registersIn[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_STAT_Pos) & 0x07u;

			MS_TIME_COUNTER_INIT(m_lastRegReadTimeMs);
		}

		m_chargerNeedPoll = CHARGER_CheckForPoll() || (lastChargerStatus != m_chargerStatus);
	}
}


// ****************************************************************************
/*!
 * ChargerTriggerNTCMonitor
 *
 * Not sure about this one yet
 *
 * @param	temp
 * @retval	none
 */
// ****************************************************************************
// TODO - Look at this.
void ChargerTriggerNTCMonitor(NTC_MonitorTemperature_T temp)
{
	switch (temp)
	{
	case COLD:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_SET);
		break;

	case COOL:
		break;

	case WARM:
		break;

	case HOT:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
		break;

	case NORMAL: default:
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_15, GPIO_PIN_RESET);
		break;
	}
}


// ****************************************************************************
/*!
 * CHARGER_SetRPi5vInputEnable enables or disables the RPi 5V charging source.
 * The setting will not take effect until the next module task routine has run.
 *
 * @param	enable		false = RPi GPIO 5V not allowed as charge source
 * 						true = RPi GPIO 5V allowed as charge source
 * @retval	none
 */
// ****************************************************************************
void CHARGER_SetRPi5vInputEnable(const bool enable)
{
	if (enable)
	{
		m_chargerInputsConfig |= CHGR_INPUTS_CONFIG_ENABLE_RPI5V_IN_bm;
	}
	else
	{
		m_chargerInputsConfig &= ~(CHGR_INPUTS_CONFIG_ENABLE_RPI5V_IN_bm);
	}

	m_chargerNeedPoll = true;
}


// ****************************************************************************
/*!
 * CHARGER_SetRPi5vInputEnable returns the current configuration of the RPi 5V
 * charge input source.
 *
 * Note: This is the actual device setting not the configuration.
 *
 * @param	none
 * @retval	bool		false = RPi GPIO 5V not allowed as charge source
 * 						true = RPi GPIO 5V allowed as charge source
 */
// ****************************************************************************
bool CHARGER_GetRPi5vInputEnable(void)
{
	return (CHGR_BS_USBIN_ENABLED == (m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_OTG_LOCK_bm));
}


// ****************************************************************************
/*!
 * CHARGER_RPi5vInCurrentLimitStepUp steps up the maximum allowed charging
 * current step level. If already at maximum then it does not change.
 *
 * Step levels:
 * 0 = 100mA, 1 = 150mA, 2 = 500mA, 3 = 800mA, 4 = 900mA, 5 = 1.5A
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_RPi5vInCurrentLimitStepUp(void)
{
	if (m_rpi5VCurrentLimit < CHG_IUSB_LIMIT_1500MA)
	{
		m_rpi5VCurrentLimit++;
	}
}


// ****************************************************************************
/*!
 * CHARGER_RPi5vInCurrentLimitStepDown steps down the maximum allowed charging
 * current step level. If already at minimum then it does not change.
 *
 * Step levels:
 * 0 = 100mA, 1 = 150mA, 2 = 500mA, 3 = 800mA, 4 = 900mA, 5 = 1.5A
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_RPi5vInCurrentLimitStepDown(void)
{
	if (m_rpi5VCurrentLimit > CHG_IUSB_LIMIT_100MA)
	{
		m_rpi5VCurrentLimit --;
	}
}


// ****************************************************************************
/*!
 * CHARGER_RPi5vInCurrentLimitSetMin sets the maximum allowed charging current
 * to the lowest step level, 100mA.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_RPi5vInCurrentLimitSetMin(void)
{
	m_rpi5VCurrentLimit = CHG_IUSB_LIMIT_100MA;
}


// ****************************************************************************
/*!
 * CHARGER_GetRPiChargeInputLevel gets the current step level (not actual value)
 * of the current limit set for charging from the RPi power source. The higher
 * the level value the higher the current limit.
 *
 * 0 = 100mA, 1 = 150mA, 2 = 500mA, 3 = 800mA, 4 = 900mA, 5 = 1.5A
 *
 * @param	none
 * @retval	uint8_t		step level of RPi GPIO charging current limit
 */
// ****************************************************************************
uint8_t CHARGER_GetRPiChargeInputLevel(void)
{
	// TODO - read actual register?
	return (uint8_t)m_rpi5VCurrentLimit;
}


// ****************************************************************************
/*!
 * CHARGER_SetInputsConfig sets the input source configuration, if the caller wants
 * the change to be persistent through power cycles then setting the 7th bit
 * (or'd with 0x80u) will commit that change to NV memory. The settings will be
 * updated next time the task routine is run.
 *
 * @param	config		new input source configuration
 * @retval	none
 */
// ****************************************************************************
void CHARGER_SetInputsConfig(const uint8_t config)
{
	m_chargerInputsConfig = (config & ~(CHRG_CONFIG_INPUTS_WRITE_NV_bm));

	if (0u != (config & CHRG_CONFIG_INPUTS_WRITE_NV_bm))
	{
		NV_WriteVariable_U8(CHARGER_INPUTS_CONFIG_NV_ADDR, config);
	}

	m_chargerNeedPoll = true;
}


// ****************************************************************************
/*!
 * CHARGER_GetInputsConfig returns the input source configuration. If the
 * value in NV memory matches the current configuration then the one from NV is
 * sent.
 *
 * @param	none
 * @retval	uint8_t		input source configuration either current or stored
 */
// ****************************************************************************
uint8_t CHARGER_GetInputsConfig(void)
{
	uint8_t tempU8 = m_chargerInputsConfig;

	if (NV_ReadVariable_U8(CHARGER_INPUTS_CONFIG_NV_ADDR, &tempU8))
	{
		if ((m_chargerInputsConfig & 0x7Fu) != (tempU8 & 0x7Fu))
		{
			tempU8 = m_chargerInputsConfig;
		}
	}

	return tempU8;
}


// ****************************************************************************
/*!
 * CHARGER_SetChargeEnableConfig sets the charge configuration, if the caller wants
 * the change to be persistent through power cycles then setting the 7th bit
 * (or'd with 0x80u) will commit that change to NV memory. The settings will be
 * updated next time the task routine is run.
 *
 * @param	config		new charge configuration
 * @retval	none
 */
// ****************************************************************************
void CHARGER_SetChargeEnableConfig(const uint8_t config)
{
	m_chargingConfig = config;

	// See if caller wants to make this permanent
	if (0u != (config & 0x80u))
	{
		NV_WriteVariable_U8(CHARGING_CONFIG_NV_ADDR, config);
	}

	m_chargerNeedPoll = true;
}


// ****************************************************************************
/*!
 * CHARGER_GetChargeEnableConfig returns the charge enable configuration. If the
 * value in NV memory matches the current configuration then the one from NV is
 * sent.
 *
 * @param	none
 * @retval	uint8_t		charge configuration either current or stored
 */
// ****************************************************************************
uint8_t CHARGER_GetChargeEnableConfig(void)
{
	uint8_t tempU8 = m_chargingConfig;

	if (NV_ReadVariable_U8(CHARGING_CONFIG_NV_ADDR, &tempU8))
	{
		if ( (tempU8 & 0x7Fu) != (m_chargingConfig & 0x7Fu) )
		{
			tempU8 = m_chargingConfig;
		}
	}

	return tempU8;
}


// ****************************************************************************
/*!
 * CHARGER_SetInterrupt tells the module that the charging IC interrupt just
 * happened and action may need to take place.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_SetInterrupt(void)
{
	// Flag went up
	m_interrupt = true;
}


// ****************************************************************************
/*!
 * CHARGER_RefreshSettings tells the module to update its configuration as a
 * configuration change has occurred.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_RefreshSettings(void)
{
	m_chargerNeedPoll = true;
}


// ****************************************************************************
/*!
 * CHARGER_UpdateBatteryProfile tells the module to update its configuration based
 * on the current battery profile. Generally called when the battery profile is
 * changed.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void CHARGER_UpdateBatteryProfile(void)
{
	m_updateBatteryProfile = true;
	m_chargerNeedPoll = true;
}


// ****************************************************************************
/*!
 * CHARGER_RequirePoll returns true if the charger IC module needs to run it's
 * task.
 *
 * @param	none
 * @retval	bool		false = charging ic task does not need to run
 * 						true = charging ic task needs to run
 */
// ****************************************************************************
bool CHARGER_RequirePoll(void)
{
	return true == m_chargerNeedPoll || true == m_interrupt;
}


// ****************************************************************************
/*!
 * CHARGER_GetStatus returns the status of the charging IC.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	ChargerStatus_T		status of charging IC
 */
// ****************************************************************************
ChargerStatus_T CHARGER_GetStatus(void)
{
	return m_chargerStatus;
}


// ****************************************************************************
/*!
 * CHARGER_GetNoBatteryTurnOnEnable returns the configuration status of the no
 * battery turn on.
 *
 * Note: this is the local configuration not the actual device register.
 *
 * @param	none
 * @retval	bool	false = no battery turn on disabled in configuration
 * 					true = no battery turn on enabled in configuration
 */
// ****************************************************************************
bool CHARGER_GetNoBatteryTurnOnEnable(void)
{
	return CHRG_CONFIG_INPUTS_NOBATT_ENABLED;
}


// ****************************************************************************
/*!
 * CHARGER_IsChargeSourceAvailable returns true if there is a valid input source
 * capable of charging the battery.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	bool	false = charging source not present
 * 					true = charging source present
 */
// ****************************************************************************
bool CHARGER_IsChargeSourceAvailable(void)
{
	const uint8_t instat = (m_registersIn[CHG_REG_SUPPLY_STATUS] & CHGR_SC_STAT_Msk);

	return (instat > CHGR_SC_STAT_NO_SOURCE) && (instat <= CHGR_SC_STAT_CHARGE_DONE);
}


// ****************************************************************************
/*!
 * CHARGER_IsCharging returns true if the battery is being charged by one of the
 * input sources.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	bool	false = battery not charging
 * 					true = battery charging
 */
// ****************************************************************************
bool CHARGER_IsCharging(void)
{
	const uint8_t instat = (m_registersIn[CHG_REG_SUPPLY_STATUS] & CHGR_SC_STAT_Msk);

	return (instat == CHGR_SC_STAT_IN_CHARGING) || (instat == CHGR_SC_STAT_USB_CHARGING);
}


// ****************************************************************************
/*!
 * CHARGER_IsBatteryPresent returns true if a battery has been detected, further
 * status information can be gained from CHARGER_GetBatteryStatus.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	bool	false = battery not present
 * 					true = battery present
 */
// ****************************************************************************
bool CHARGER_IsBatteryPresent(void)
{
	return (m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk) == CHGR_BS_BATSTAT_NORMAL;
}


// ****************************************************************************
/*!
 * CHARGER_GetBatteryStatus returns the status of the battery according to the
 * charging IC.
 *
 * @param	none
 * @retval	CHARGER_BatteryStatus_t		battery status detected by the charging IC
 */
// ****************************************************************************
CHARGER_BatteryStatus_t CHARGER_GetBatteryStatus(void)
{
	return ((m_registersIn[CHG_REG_BATTERY_STATUS] >> CHGR_BS_BATSTAT_Pos) & 0x3u);
}


// ****************************************************************************
/*!
 * CHARGER_HasTempSensorFault tells the caller if there is a temperature related
 * fault present within the charging IC.
 *
 * @param	none
 * @retval	bool		false = no temperature related fault present
 * 						true = temperature related fault present
 */
// ****************************************************************************
bool CHARGER_HasTempSensorFault(void)
{
	return (m_registersIn[CHG_REG_SAFETY_NTC] & CHGR_ST_NTC_FAULT_Msk) != CHGR_ST_NTC_TS_FAULT_NONE;
}


// ****************************************************************************
/*!
 * CHARGER_GetTempFault returns the temperature fault code from the charging IC.
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	uint8_t		fault status code of temperature fault
 */
// ****************************************************************************
uint8_t CHARGER_GetTempFault(void)
{
	return (m_registersIn[CHG_REG_SAFETY_NTC] >> CHGR_ST_NTC_FAULT_Pos);
}


// ****************************************************************************
/*!
 * CHARGER_GetInputStatus returns the status of an indexed input. If the RPi 5v
 * (USB in) is requested its return value is dependent on whether that input
 * has been locked out and may not reflect the actual condition. If the index is
 * out of bounds then the routine shall return CHARGER_INPUT_UVP.
 *
 * @param	inputChannel			index of the input channel 0 = VIn, 1 = USB
 * @retval	CHARGER_InputStatus_t	status of indexed input channel
 */
// ****************************************************************************
CHARGER_InputStatus_t CHARGER_GetInputStatus(uint8_t inputChannel)
{
	uint8_t channelPos;

	if (inputChannel < CHARGER_INPUT_CHANNELS)
	{
		channelPos = (CHARGER_INPUT_RPI == inputChannel) ? CHGR_BS_USBSTAT_Pos : CHGR_BS_INSTAT_Pos;

		return (m_registersIn[CHG_REG_BATTERY_STATUS] >> channelPos) & 0x03u;
	}

	return CHARGER_INPUT_UVP;
}


// ****************************************************************************
/*!
 * CHARGER_IsDPMActive returns the DPM status of the charging IC. Refer
 * to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	bool			true = DPM active
 * 							false = DPM not active
 */
// ****************************************************************************
bool CHARGER_IsDPMActive(void)
{
	return (0u != (m_registersIn[CHG_REG_DPPM_STATUS] & CHGR_VDPPM_DPM_ACTIVE));
}


// ****************************************************************************
/*!
 * CHARGER_GetFaultStatus returns the status as reported by the charging IC. Refer
 * to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	uint8_t			fault status of charging IC
 */
// ****************************************************************************
uint8_t CHARGER_GetFaultStatus(void)
{
	return (m_registersIn[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_FLT_Pos) & 0x7u;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * CHARGER_UpdateLocalRegister updates a single local register with the contents
 * of that device register. The device register is read twice to check consistency
 * of the data as no crc is passed in the data packet.
 *
 * @param	regAddress		device register address
 * @retval	bool			false = read transaction failed
 * 							true = read transaction succeeded
 */
// ****************************************************************************
static bool CHARGER_UpdateLocalRegister(const uint8_t regAddress)
{
	uint8_t tempReg;

	if (regAddress > CHARGER_LAST_REGISTER)
	{
		return false;
	}

	if (false == CHARGER_ReadDeviceRegister(regAddress))
	{
		return false;
	}

	tempReg = m_i2cReadRegResult;

	if (false == CHARGER_ReadDeviceRegister(regAddress))
	{
		return false;
	}

	if (m_i2cReadRegResult != tempReg)
	{
		return false;
	}

	m_registersIn[regAddress] = tempReg;

	return true;
}


// ****************************************************************************
/*!
 * CHARGER_ReadDeviceRegister sends a request for one single device register, on
 * completion the callback is called and populates the m_i2cReadRegResult upon
 * a successful transaction. The routine waits until the transaction is complete.
 *
 * @param	regAddress		device register address
 * @retval	bool			false = read transaction failed
 * 							true = read transaction succeeded
 */
// ****************************************************************************
static bool CHARGER_ReadDeviceRegister(const uint8_t regAddress)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;

	m_i2cSuccess = false;

	transactGood = I2CDRV_Transact(CHARGER_I2C_PORTNO, CHARGER_I2C_ADDR, &regAddress, 1u,
						I2CDRV_TRANSACTION_RX, CHARGER_I2C_Callback,
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

	// m_i2cReadRegResult has the data!
	return m_i2cSuccess;
}


// ****************************************************************************
/*!
 * CHARGER_UpdateAllLocalRegisters reads all the registers from the device and
 * populates the local copy. If the transaction could not send or failed during
 * then the routine returns false. If successful the routine returns true and
 * the local copy of registers is valid.
 *
 * @param	none
 * @retval	bool		false = read transaction failed
 * 						true = read transaction succeeded
 */
// ****************************************************************************
static bool CHARGER_UpdateAllLocalRegisters(void)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;
	uint8_t regAddress = 0u;

	m_i2cSuccess = false;

	transactGood = I2CDRV_Transact(CHARGER_I2C_PORTNO, CHARGER_I2C_ADDR, &regAddress, CHARGER_REGISTER_COUNT,
						I2CDRV_TRANSACTION_RX, CHARGER_ReadAll_I2C_Callback,
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

	// m_i2cReadRegResult has the data!
	return m_i2cSuccess;
}


// ****************************************************************************
/*!
 * CHARGER_UpdateDeviceRegister sends the configuration data to a single device
 * address. A check is made that the write value isn't already valid in the device
 * by comparing the last register read using a mask to filter the relevant bits.
 * If a write is required then the routine will wait until the transaction is
 * complete.
 *
 * @param	regAddress		device register address
 * @param	value			value to write to the device register
 * @param	writeMask		mask for bits of interest (1 = compare)
 * @retval	none
 */
// ****************************************************************************
static bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value, const uint8_t writeMask)
{
	const uint32_t sysTime = HAL_GetTick();
	uint8_t writeData[2u] = {regAddress, value};
	bool transactGood;

	// Check if the register actually needs an update
	if (value == (m_registersIn[regAddress] & writeMask))
	{
		return true;
	}

	m_i2cSuccess = false;

	transactGood = I2CDRV_Transact(CHARGER_I2C_PORTNO, CHARGER_I2C_ADDR, writeData, 2u,
						I2CDRV_TRANSACTION_TX, CHARGER_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(CHARGER_I2C_PORTNO))
	{
		// Wait for transfer
	}

	return true;
}


// ****************************************************************************
/*!
 * CHARGER_KickDeviceWatchdog sets the bit in the supply status register that tells
 * the device not to reset its configuration on elapsed time. The routine wil wait
 * until the transaction is completed
 *
 * @param	sysTime		current value of the system tick timer
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_KickDeviceWatchdog(const uint32_t sysTime)
{
	uint8_t writeData[2u] = {CHG_REG_SUPPLY_STATUS, m_registersOut[CHG_REG_SUPPLY_STATUS]};
	// Reset charger watchdog timer, the timeout is reset in the callback
	writeData[1u] |= CHGR_SC_TMR_RST_bm;

	I2CDRV_Transact(CHARGER_I2C_PORTNO, CHARGER_I2C_ADDR, writeData, 2u,
							I2CDRV_TRANSACTION_TX, CHARGER_WDT_I2C_Callback,
							1000u, sysTime
							);

	while(false == I2CDRV_IsReady(CHARGER_I2C_PORTNO))
	{
		// Wait for transfer
	}
}


// ****************************************************************************
/*!
 * CHARGER_UpdateSupplyPreference configures the device register
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateSupplyPreference(void)
{

	if (true == CHGR_CONFIG_INPUTS_RPI5V_PREFERRED)
	{
		m_registersOut[CHG_REG_SUPPLY_STATUS] |= CHGR_SC_FLT_SUPPLY_PREF_USB;
	}
	else
	{
		m_registersOut[CHG_REG_SUPPLY_STATUS] &= ~(CHGR_SUPPLY_SEL_bm);
	}

	CHARGER_UpdateDeviceRegister(CHG_REG_SUPPLY_STATUS, m_registersOut[CHG_REG_SUPPLY_STATUS],
									m_registersWriteMask[CHG_REG_SUPPLY_STATUS]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateRPi5VInLockout configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateRPi5VInLockout(void)
{
	const POWERSOURCE_RPi5VStatus_t pow5vInDetStatus = POWERSOURCE_GetRPi5VPowerStatus();

	m_registersOut[CHG_REG_BATTERY_STATUS] = (true == CHRG_CONFIG_INPUTS_NOBATT_ENABLED) ?
												CHGR_BS_EN_NOBAT_OP :
												0u;

	// If RPi is powered by itself and host allows charging from RPi 5v and the battery checks out ok...
	if ( (true == CHGR_CONFIG_INPUTS_RPI5V_ENABLED)
			&& (pow5vInDetStatus == RPI5V_DETECTION_STATUS_POWERED)
			&& ((m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk) == CHGR_BS_BATSTAT_NORMAL)
			)
	{
		// Allow RPi 5v to charge battery
		m_registersOut[CHG_REG_BATTERY_STATUS] &= ~(CHGR_BS_OTG_LOCK_bm);
	}
	else
	{
		// Don't allow RPi 5v to charge battery
		m_registersOut[CHG_REG_BATTERY_STATUS] |= CHGR_BS_OTG_LOCK_bm;
	}

	CHARGER_UpdateDeviceRegister(CHG_REG_BATTERY_STATUS, m_registersOut[CHG_REG_BATTERY_STATUS],
									m_registersWriteMask[CHG_REG_BATTERY_STATUS]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateControlStatus configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint8_t batteryTemp = FUELGAUGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGAUGE_GetBatteryTempSensorCfg();
	const bool inputSourcePresent =
			((CHGR_BS_USBSTAT_UVP | CHGR_BS_INSTAT_UVP) !=
					(m_registersIn[CHG_REG_BATTERY_STATUS] & (CHGR_BS_INSTAT_Msk | CHGR_BS_USBSTAT_Msk)));

	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();

	if ((m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_INSTAT_Msk) > CHGR_BS_INSTAT_OVP)
	{
		m_registersOut[CHG_REG_CONTROL] = 0u;
	}
	else
	{
		m_registersOut[CHG_REG_CONTROL] = (m_rpi5VCurrentLimit << CHGR_CTRL_IUSB_LIMIT_Pos) & CHGR_CTRL_IUSB_LIMIT_Msk;
	}


	// Enable STAT output.
	m_registersOut[CHG_REG_CONTROL] |= (CHGR_CTRL_EN_STAT_bm);// | CHGR_CTRL_TE);

	if ( (BAT_STATUS_NOT_PRESENT != batteryStatus) )
	{
		m_registersOut[CHG_REG_CONTROL] |= CHGR_CTRL_TE_bm;
	}


	// Disable charging if configured not, battery profile is not set or temperature is out of working range
	if ( CHARGE_DISABLED || (currentBatProfile == NULL) || (false == inputSourcePresent) ||
			( (tempSensorConfig != BAT_TEMP_SENSE_CONFIG_NOT_USED)
					&& ((batteryTemp >= currentBatProfile->tHot) || (batteryTemp <= currentBatProfile->tCold))
				)
		)
	{
		// disable charging
		m_registersOut[CHG_REG_CONTROL] |= CHGR_CTRL_CHG_DISABLE_bm;
	}


	// Enable high-z mode if no input source is present
	if (false == inputSourcePresent)
	{
		m_registersOut[CHG_REG_CONTROL] |= CHGR_CTRL_HZ_MODE_bm;
	}


	CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL, m_registersOut[CHG_REG_CONTROL],
									m_registersWriteMask[CHG_REG_CONTROL]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateRegulationVoltage configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateRegulationVoltage(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const int8_t batteryTemperature = FUELGAUGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGAUGE_GetBatteryTempSensorCfg();
	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();
	const uint8_t minRegVol = ((FUELGAUGE_GetBatteryMv() - CHGR_CB_BATT_REGV_BASE_MV) / CHGR_CB_BATT_REGV_RESOLUTION) + 1u;

	int8_t newRegVol;

	m_registersOut[CHG_REG_CONTROL_BATTERY] = 0u;

	// Set input current limit
	m_registersOut[CHG_REG_CONTROL_BATTERY] |= (false == CHRG_CONFIG_INPUTS_IN_LIMITED_1pt5A) ?
										CHGR_CB_IN_LIMIT_2pt5A :
										CHGR_CB_IN_LIMIT_1pt5A;


	// Check for valid battery profile, a battery present and the device set to high-z mode
	if ( (currentBatProfile != NULL) &&
			(BAT_STATUS_NOT_PRESENT != batteryStatus) &&
			(CHGR_CTRL_HZ_MODE_bm == (m_registersIn[CHG_REG_CONTROL] & CHGR_CTRL_HZ_MODE_bm))
			)
	{
		if ( (batteryTemperature > currentBatProfile->tWarm) && (BAT_TEMP_SENSE_CONFIG_NOT_USED != tempSensorConfig) )
		{
			newRegVol = ((int8_t)currentBatProfile->regulationVoltage) - (140/20);
		}
		else
		{
			newRegVol = currentBatProfile->regulationVoltage;
		}

		// Check to make sure its at least the current battery voltage or bad stuff happens
		if (newRegVol < (int8_t)minRegVol)
		{
			newRegVol = (int8_t)minRegVol;
		}

		// Check to make sure the value doesn't exceed the maximum allowed set
		if (newRegVol > CHGR_CB_BATT_REGV_MAX_SET)
		{
			newRegVol = CHGR_CB_BATT_REGV_MAX_SET;
		}

		m_registersOut[CHG_REG_CONTROL_BATTERY] |= ((uint8_t)newRegVol << CHGR_CB_BATT_REGV_Pos);
	}
	else
	{
		// Keep to whatever the charging IC has overriden the value to
		m_registersOut[CHG_REG_CONTROL_BATTERY] |= (m_registersIn[CHG_REG_CONTROL_BATTERY] & CHGR_CB_BATT_REGV_Msk);
	}


	CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL_BATTERY, m_registersOut[CHG_REG_CONTROL_BATTERY],
									m_registersWriteMask[CHG_REG_CONTROL_BATTERY]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateChgCurrentAndTermCurrent configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateChgCurrentAndTermCurrent(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();

	if (currentBatProfile != NULL)
	{
		m_registersOut[CHG_REG_TERMI_FASTCHARGEI] =
				(((currentBatProfile->chargeCurrent > 26u) ? 26u : currentBatProfile->chargeCurrent & 0x1F) << 3u)
				| (currentBatProfile->terminationCurr & 0x07u);
	}
	else
	{
		m_registersOut[CHG_REG_TERMI_FASTCHARGEI] = 0u;
	}

	CHARGER_UpdateDeviceRegister(CHG_REG_TERMI_FASTCHARGEI, m_registersOut[CHG_REG_TERMI_FASTCHARGEI],
									m_registersWriteMask[CHG_REG_TERMI_FASTCHARGEI]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateVinDPM configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateVinDPM(void)
{
	// Take value for Vin as set by the inputs config, RPi 5V set to 480mV
	m_registersOut[CHG_REG_DPPM_STATUS] = CHRG_CONFIG_INPUTS_DPM | (CHARGER_VIN_DPM_USB << 3u);

	CHARGER_UpdateDeviceRegister(CHG_REG_DPPM_STATUS, m_registersOut[CHG_REG_DPPM_STATUS],
									m_registersWriteMask[CHG_REG_DPPM_STATUS]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateTempRegulationControlStatus configures the device register.
 *
 * Refer to BQ2416x datasheet for more information.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateTempRegulationControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint8_t batteryTemp = FUELGAUGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGAUGE_GetBatteryTempSensorCfg();

	//Timer slowed by 2x when in thermal regulation, 10 � 9 hour fast charge, TS function disabled
	m_registersOut[CHG_REG_SAFETY_NTC] = CHGR_ST_NTC_2XTMR_EN_bm | CHGR_ST_NTC_SFTMR_9HOUR;

	// Allow full set charge current
	m_registersOut[CHG_REG_SAFETY_NTC] &= ~(CHGR_ST_NTC_LOW_CHARGE_bm);

	// If Battery has a profile set and the temperature sensor is used and the battery is cold
	if ( (currentBatProfile != NULL)
			&& (batteryTemp < currentBatProfile->tCool) && (tempSensorConfig != BAT_TEMP_SENSE_CONFIG_NOT_USED)
			)
	{
		// Limit the charge current until its warmed up a bit
		m_registersOut[CHG_REG_SAFETY_NTC] |= CHGR_ST_NTC_LOW_CHARGE_bm;
	}

	CHARGER_UpdateDeviceRegister(CHG_REG_SAFETY_NTC, m_registersOut[CHG_REG_SAFETY_NTC],
									m_registersWriteMask[CHG_REG_SAFETY_NTC]);
}


// ****************************************************************************
/*!
 * CHARGER_UpdateSettings runs though the configuration for each of the registers
 * and sets accordingly. The device registers are written also going along if they
 * need updating.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void CHARGER_UpdateSettings(void)
{
	// Setting was changed, process the lot!
	CHARGER_UpdateSupplyPreference();
	CHARGER_UpdateRPi5VInLockout();
	CHARGER_UpdateControlStatus();
	CHARGER_UpdateRegulationVoltage();

	CHARGER_UpdateTempRegulationControlStatus();
	CHARGER_UpdateChgCurrentAndTermCurrent();
	CHARGER_UpdateVinDPM();

	m_chargerNeedPoll = false;
}


// ****************************************************************************
/*!
 * CHARGER_CheckForPoll runs though the device registers to check if they match
 * what should be set. If any register does not match then the routines returns
 * true
 *
 * @param	none
 * @retval	bool		false = all device registers match current configuration
 * 						true = one or more registers require configuration
 */
// ****************************************************************************
static bool CHARGER_CheckForPoll(void)
{
	uint8_t i = CHARGER_REGISTER_COUNT;

	while (i-- > 0u)
	{
		if ((m_registersIn[i] & m_registersWriteMask[i]) != m_registersOut[i])
		{
			return true;
		}
	}

	return false;
}
