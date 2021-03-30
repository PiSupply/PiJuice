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


extern uint8_t resetStatus;

static uint8_t m_registersIn[CHARGER_REGISTER_COUNT] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t m_registersOut[CHARGER_REGISTER_COUNT] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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


static bool CHARGER_UpdateLocalRegister(const uint8_t regAddress);
bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value, const uint8_t writeMask);
static bool CHARGER_UpdateAllLocalRegisters(void);
static bool CHARGER_ReadDeviceRegister(const uint8_t regAddress);

static void CHARGER_UpdateSupplyPreference(void);
static void CHARGER_UpdateChgCurrentAndTermCurrent(void);
static void CHARGER_UpdateVinDPM(void);
static void CHARGER_UpdateTempRegulationControlStatus(void);
static void CHARGER_UpdateControlStatus(void);
static void CHARGER_UpdateRPi5VInLockout(void);
static void CHARGER_UpdateRegulationVoltage(void);

void CHARGER_KickDeviceWatchdog(const uint32_t sysTime);

static bool CHARGER_CheckForPoll(void);

static CHARGER_USBInLockoutStatus_T m_rpi5VInputDisable = CHG_USB_IN_UNKNOWN;
static CHARGER_USBInCurrentLimit_T m_rpi5VCurrentLimit = CHG_IUSB_LIMIT_150MA; // current limit code as defined in datasheet

static bool m_i2cSuccess;
static uint8_t m_i2cReadRegResult;

static uint8_t m_chargingConfig;
static uint8_t m_chargerInputsConfig;

static ChargerStatus_T m_chargerStatus = CHG_NA;
static CHARGER_DeviceStatus_t m_chargerIcStatus;

static uint32_t m_lastWDTResetTimeMs;
static uint32_t m_lastRegReadTimeMs;
static bool m_interrupt;
static bool m_chargerNeedPoll;
static bool m_updateBatteryProfile;


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


void CHARGER_WDT_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	const uint32_t sysTime = HAL_GetTick();

	if (p_i2cdrvDevice->event == I2CDRV_EVENT_TX_COMPLETE)
	{
		m_lastWDTResetTimeMs = sysTime;
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

	uint16_t var = 0;
	uint8_t i;


	// If not just powered on...
	if (!resetStatus)
	{
		EE_ReadVariable(CHARGER_INPUTS_CONFIG_NV_ADDR, &var);

		if (true == UTIL_NV_ParamInitCheck_U16(var))
		{
			m_chargerInputsConfig = (uint8_t)(var & 0xFFu);

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

		EE_ReadVariable(CHARGING_CONFIG_NV_ADDR, &var);

		// Check to see if the parameter is programmed
		if (true == UTIL_NV_ParamInitCheck_U16(var))
		{
			m_chargingConfig = (uint8_t)(var & 0xFFu);
		}
		else
		{
			// Set enable charge if unprogrammed
			m_chargingConfig = CHARGING_CONFIG_CHARGE_EN_bm;
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


__weak void InputSourcePresenceChangeCb(uint8_t event) {
	UNUSED(event);
}


void CHARGER_UpdateSettings(void)
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
	uint8_t tempReg;
	const IODRV_Pin_t * p_pin = IODRV_GetPinInfo(2u);


	if (p_pin->lastPosPulseWidthTimeMs > 200u)
	{
		IORDV_ClearPinEdges(2u);

		m_chargingConfig ^= CHARGING_CONFIG_CHARGE_EN_bm;
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

		// TODO - Not sure about this yet, does fix the watchdog reset issue
		m_chargerNeedPoll = CHARGER_CheckForPoll();
	}
}


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


void CHARGER_SetRPi5vInputEnable(bool enable)
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


bool CHARGER_GetRPi5vInputEnable()
{
	// TODO - This should probably be the actual setting from the device registers
	return CHGR_CONFIG_INPUTS_RPI5V_ENABLED;
}


CHARGER_USBInLockoutStatus_T CHARGER_GetRPi5vInLockStatus(void)
{
	return m_rpi5VInputDisable;
}



void CHARGER_RPi5vInCurrentLimitStepUp(void)
{
	if (m_rpi5VCurrentLimit < CHG_IUSB_LIMIT_1500MA)
	{
		m_rpi5VCurrentLimit ++;
	}
}


void CHARGER_RPi5vInCurrentLimitStepDown(void)
{
	if (m_rpi5VCurrentLimit > CHG_IUSB_LIMIT_150MA)
	{
		m_rpi5VCurrentLimit --;
	}
}


void CHARGER_RPi5vInCurrentLimitSetMin(void)
{
	m_rpi5VCurrentLimit = CHG_IUSB_LIMIT_150MA;
}


uint8_t CHARGER_GetRPiChargeInputLevel(void)
{
	// TODO - read actual register?
	return (uint8_t)m_rpi5VCurrentLimit;
}


void CHARGER_SetInputsConfig(const uint8_t config)
{
	m_chargerInputsConfig = (config & ~(CHRG_CONFIG_INPUTS_WRITE_NV_bm));

	// If write to
	if (0u != (config & CHRG_CONFIG_INPUTS_WRITE_NV_bm))
	{
		NvWriteVariableU8(CHARGER_INPUTS_CONFIG_NV_ADDR, config);
	}
}


uint8_t CHARGER_GetInputsConfig(void)
{
	uint16_t var = 0;

	EE_ReadVariable(CHARGER_INPUTS_CONFIG_NV_ADDR, &var);

	if ( UTIL_NV_ParamInitCheck_U16(var)
			&& ((m_chargerInputsConfig & 0x7Fu) == (var & 0x7Fu))
			)
	{
		return (uint8_t)(var & 0xFFu);
	}

	return m_chargerInputsConfig;

}


void CHARGER_SetChargeEnableConfig(const uint8_t config)
{
	m_chargingConfig = config;

	// See if caller wants to make this permanent
	if (0u != (config & 0x80u))
	{
		NvWriteVariableU8(CHARGING_CONFIG_NV_ADDR, config);
	}
}


uint8_t CHARGER_GetChargeEnableConfig(void)
{
	uint16_t var = 0;

	EE_ReadVariable(CHARGING_CONFIG_NV_ADDR, &var);

	// Check if the parameter has been programmed and matches current setting
	// Note: This does not seem right... why not just return charging config?
	if ( UTIL_NV_ParamInitCheck_U16(var)
			&& ((m_chargingConfig & 0x7Fu) == (var & 0x7Fu))
			)
	{
		return (uint8_t)(var & 0xFFu);
	}

	return m_chargingConfig;
}


void CHARGER_SetInterrupt(void)
{
	// Flag went up
	m_interrupt = true;
}


void CHARGER_UpdateBatteryProfile(void)
{
	m_updateBatteryProfile = true;
}


bool CHARGER_RequirePoll(void)
{
	return true == m_chargerNeedPoll || true == m_interrupt;
}


ChargerStatus_T CHARGER_GetStatus(void)
{
	return m_chargerStatus;
}


bool CHARGER_GetNoBatteryTurnOnEnable(void)
{
	return CHRG_CONFIG_INPUTS_NOBATT_ENABLED;
}


bool CHARGER_IsChargeSourceAvailable(void)
{
	const uint8_t instat = (m_registersIn[CHG_REG_SUPPLY_STATUS] & CHGR_SC_STAT_Msk);

	// TODO- Supply status register might be better for this.
	return (instat > CHGR_SC_STAT_NO_SOURCE) && (instat <= CHGR_SC_STAT_CHARGE_DONE);
}


bool CHARGER_IsBatteryPresent(void)
{
	return (m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk) == CHGR_BS_BATSTAT_NORMAL;
}


CHARGER_BatteryStatus_t CHARGER_GetBatteryStatus(void)
{
	return ((m_registersIn[CHG_REG_BATTERY_STATUS] >> CHGR_BS_BATSTAT_Pos) & 0x3u);
}


bool CHARGER_HasTempSensorFault(void)
{
	return (m_registersIn[CHG_REG_SAFETY_NTC] & CHGR_ST_NTC_FAULT_Msk) != CHGR_ST_NTC_TS_FAULT_NONE;
}


uint8_t CHARGER_GetTempFault(void)
{
	return (m_registersIn[CHG_REG_SAFETY_NTC] >> CHGR_ST_NTC_FAULT_Pos);
}


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


bool CHARGER_IsDPMActive(void)
{
	return (0u != (m_registersIn[CHG_REG_DPPM_STATUS] & CHGR_VDPPM_DPM_ACTIVE));
}


uint8_t CHARGER_GetFaultStatus(void)
{
	return (m_registersIn[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_FLT_Pos) & 0x7u;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
bool CHARGER_UpdateLocalRegister(const uint8_t regAddress)
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


bool CHARGER_ReadDeviceRegister(const uint8_t regAddress)
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

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	// m_i2cReadRegResult has the data!
	return m_i2cSuccess;
}


bool CHARGER_UpdateAllLocalRegisters(void)
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

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	// m_i2cReadRegResult has the data!
	return m_i2cSuccess;
}


bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value, const uint8_t writeMask)
{
	const uint32_t sysTime = HAL_GetTick();
	uint8_t writeData[2u] = {regAddress, value};
	bool transactGood;

	// Check register address is in bounds
	if (regAddress > CHARGER_LAST_REGISTER)
	{
		return false;
	}


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


void CHARGER_KickDeviceWatchdog(const uint32_t sysTime)
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


void CHARGER_UpdateSupplyPreference(void)
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


void CHARGER_UpdateRPi5VInLockout(void)
{
	// TODO - Check for circular operation, also do we want this module to be
	// making these kind of decisions?
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();

	m_registersOut[CHG_REG_BATTERY_STATUS] = (true == CHRG_CONFIG_INPUTS_NOBATT_ENABLED) ? CHGR_BS_EN_NOBAT_OP : 0u;

	// If RPi is powered by itself and host allows charging from RPi 5v and the battery checks out ok...
	if ( (true == CHGR_CONFIG_INPUTS_RPI5V_ENABLED)
			&& (pow5vInDetStatus == POW_SOURCE_NORMAL)
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


void CHARGER_UpdateControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint8_t batteryTemp = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();
	const bool inputSourcePresent =
			((CHGR_BS_USBSTAT_UVP | CHGR_BS_INSTAT_UVP) !=
					(m_registersIn[CHG_REG_BATTERY_STATUS] & (CHGR_BS_INSTAT_Msk | CHGR_BS_USBSTAT_Msk)));


	if ((m_registersIn[CHG_REG_BATTERY_STATUS] & CHGR_BS_INSTAT_Msk) > CHGR_BS_INSTAT_OVP)
	{
		m_registersOut[CHG_REG_CONTROL] = 0u;
	}
	else
	{
		m_registersOut[CHG_REG_CONTROL] = (m_rpi5VCurrentLimit << CHGR_CTRL_IUSB_LIMIT_Pos) & CHGR_CTRL_IUSB_LIMIT_Msk;
	}


	// Enable STAT output, Enable charge current termination
	m_registersOut[CHG_REG_CONTROL] |= (CHGR_CTRL_EN_STAT_bm);// | CHGR_CTRL_TE);


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


void CHARGER_UpdateRegulationVoltage(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const int8_t batteryTemperature = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();
	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();

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

		// Check to make sure its a positive value
		if (newRegVol < 0u)
		{
			newRegVol = 0u;
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


void CHARGER_UpdateChgCurrentAndTermCurrent(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();

	if (currentBatProfile!=NULL)
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


void CHARGER_UpdateVinDPM(void)
{
	// Take value for Vin as set by the inputs config, RPi 5V set to 480mV
	m_registersOut[CHG_REG_DPPM_STATUS] = CHRG_CONFIG_INPUTS_DPM | (CHARGER_VIN_DPM_USB << 3u);

	CHARGER_UpdateDeviceRegister(CHG_REG_DPPM_STATUS, m_registersOut[CHG_REG_DPPM_STATUS],
									m_registersWriteMask[CHG_REG_DPPM_STATUS]);
}


void CHARGER_UpdateTempRegulationControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint8_t batteryTemp = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();

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


static volatile uint8_t lastChgUpdate;


bool CHARGER_CheckForPoll(void)
{
	uint8_t i = CHARGER_REGISTER_COUNT;

	while (i-- > 0u)
	{
		if ((m_registersIn[i] & m_registersWriteMask[i]) != m_registersOut[i])
		{
			lastChgUpdate = i;
			return true;
		}
	}

	lastChgUpdate = 0xFFu;

	return false;
}
