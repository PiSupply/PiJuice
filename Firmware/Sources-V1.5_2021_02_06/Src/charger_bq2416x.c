#include <string.h>

#include "main.h"

#include "system_conf.h"
#include "i2cdrv.h"
#include "util.h"


#include "charger_bq2416x.h"
#include "charger_conf.h"

#include "time_count.h"
#include "nv.h"
#include "eeprom.h"
#include "fuel_gauge.h"
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


#define CHG_READ_PERIOD_MS 				90  // ms
#define CHARGER_WDT_RESET_PERIOD_MS 	(30000 / 3)

#define CHARGER_VIN_DPM_USB				6//0X07

extern uint8_t resetStatus;


static uint8_t regs[8u] = {0x00, 0x00, 0x8C, 0x14, 0x40, 0x32, 0x00, 0x98};
static uint8_t regsw[8u] = {0x08, 0x08, 0x1C, 0x02, 0x00, 0x00, 0x38, 0xC0};
static uint8_t regsStatusRW[8u] = {0x00}; // 0 - no read write errors, bit0 1 - read error, bit1 1 - write error


void ChargerSetInputsConfig(uint8_t config);
bool CHARGER_UpdateLocalRegister(const uint8_t regAddress);
bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value);
bool CHARGER_UpdateAllLocalRegisters(void);
bool CHARGER_ReadDeviceRegister(const uint8_t regAddress);
int8_t CHARGER_UpdateChgCurrentAndTermCurrent(void);
int8_t CHARGER_UpdateVinDPM(void);
int8_t CHARGER_UpdateTempRegulationControlStatus(void);
int8_t CHARGER_UpdateControlStatus(void);
int8_t CHARGER_UpdateRPi5VInLockout(void);
int8_t CHARGER_UpdateRegulationVoltage(void);


static CHARGER_USBInLockoutStatus_T m_rpi5VInputDisable = CHG_USB_IN_UNKNOWN;
static CHARGER_USBInCurrentLimit_T m_rpi5VCurrentLimit = CHG_IUSB_LIMIT_150MA; // current limit code as defined in datasheet

static bool m_i2cSuccess;
static uint8_t m_i2cReadRegResult;

static uint8_t m_chargingConfig;
static uint8_t m_chargerInputsConfig;

static ChargerStatus_T m_chargerStatus = CHG_NA;

static uint32_t m_lastWDTResetTimeMs;
static uint32_t m_lastRegReadTimeMs;
static uint8_t m_nextRegReadAddr;
static bool m_interrupt;
static bool m_chargerNeedPoll;


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
		memcpy(regs, &p_i2cdrvDevice->data[2u], CHARGER_REGISTER_COUNT);
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


int8_t CHARGER_UpdateRegulationVoltage(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint8_t batteryTemperature = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();

	int8_t newRegVol;

	// Clear everything except the d+/d- detection.
	regsw[CHG_REG_CONTROL_BATTERY] &= CHGR_CB_DPDN_EN_bm;

	// Set input current limit
	regsw[CHG_REG_CONTROL_BATTERY] |= (false == CHRG_CONFIG_INPUTS_IN_LIMITED_1pt5A) ?
										CHGR_CB_IN_LIMIT_2pt5A :
										CHGR_CB_IN_LIMIT_1pt5A;

	if (currentBatProfile != NULL)
	{
		if ( (batteryTemperature > currentBatProfile->tWarm) && (BAT_TEMP_SENSE_CONFIG_NOT_USED != tempSensorConfig) )
		{
			newRegVol = ((int8_t)currentBatProfile->regulationVoltage) - (140/20);
			newRegVol = newRegVol < 0 ? 0 : newRegVol;
		}
		else
		{
			newRegVol = currentBatProfile->regulationVoltage;
		}

		regsw[CHG_REG_CONTROL_BATTERY] |= newRegVol << CHGR_CB_BATT_REGV_Pos;
	}
	else
	{
		if (0u != regsStatusRW[CHG_REG_CONTROL_BATTERY])
		{
			return 0;
		}

	}

	// if there were errors in previous transfers read state from register
	if (regsStatusRW[CHG_REG_CONTROL_BATTERY] )
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_CONTROL_BATTERY))
		{
			return 1;
		}
	}


	// If update required
	if ( (regsw[CHG_REG_CONTROL_BATTERY] & 0xFFu) != (regs[CHG_REG_CONTROL_BATTERY] & 0xFFu) )
	{
		// write regulation voltage register
		if (false == CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL_BATTERY, regsw[CHG_REG_CONTROL_BATTERY]))
		{
			return 2;
		}

	}

	return 0;
}





void CHARGER_Init(void)
{
	const uint32_t sysTime = HAL_GetTick();
	uint16_t var = 0;

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
			m_chargingConfig = CHARGING_CONFIG_CHARGE_EN_bm;
		}
	}

	MS_TIMEREF_INIT(m_lastRegReadTimeMs, sysTime);
	MS_TIMEREF_INIT(m_lastWDTResetTimeMs, sysTime);

	regsw[CHG_REG_BATTERY_STATUS] |= CHGR_BS_OTG_LOCK_bm;
	CHARGER_UpdateDeviceRegister(CHG_REG_BATTERY_STATUS, regsw[CHG_REG_BATTERY_STATUS]);

	// NOTE: do not place in high impedance mode, it will disable VSys mosfet, and no power to mcu
	regsw[2] |= 0x02; // set control register, disable charging initially
	regsw[2] |= 0x20; // Set USB limit 500mA

	regsw[CHG_REG_CONTROL] |= (CHGR_CTRL_CHG_DISABLE | CHGR_CTRL_IUSB_LIMIT_500MA);
	CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL, regsw[CHG_REG_CONTROL]);

	DelayUs(500);

	// Initialise all values from the device
	CHARGER_UpdateAllLocalRegisters();

	CHARGER_UpdateRPi5VInLockout();
	CHARGER_UpdateTempRegulationControlStatus();

	CHARGER_UpdateLocalRegister(CHG_REG_SUPPLY_STATUS);
	CHARGER_UpdateLocalRegister(CHG_REG_BATTERY_STATUS);

	m_chargerStatus = (regs[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_STAT_Pos) & 0x07u;

	m_nextRegReadAddr = 0u;

}


__weak void InputSourcePresenceChangeCb(uint8_t event) {
	UNUSED(event);
}


void CHARGER_Task(void)
{
	uint32_t sysTime;

	m_chargerNeedPoll = false;

	if (CHARGER_UpdateRPi5VInLockout() != 0)
	{
		m_chargerNeedPoll = true;
		return;
	}

	if (true == m_interrupt)
	{
		// update status on interrupt
		CHARGER_UpdateLocalRegister(CHG_REG_SUPPLY_STATUS);
		m_interrupt = false;
		m_chargerNeedPoll = true;
	}

	if (CHARGER_UpdateControlStatus() != 0)
	{
		m_chargerNeedPoll = true;
		return;
	}

	if (CHARGER_UpdateRegulationVoltage() != 0)
	{
		m_chargerNeedPoll = true;
		return;
	}

	if (CHARGER_UpdateTempRegulationControlStatus() != 0)
	{
		m_chargerNeedPoll = true;
		return;
	}

	if (0u != CHARGER_UpdateChgCurrentAndTermCurrent())
	{
		m_chargerNeedPoll = true;
		return;
	}

	if (0u != CHARGER_UpdateVinDPM())
	{
		m_chargerNeedPoll = true;
		return;
	}

	sysTime = HAL_GetTick();

	if (MS_TIMEREF_TIMEOUT(m_lastWDTResetTimeMs, sysTime, CHARGER_WDT_RESET_PERIOD_MS))
	{
		// reset timer
		// NOTE: reset bit must be 0 in write register image to prevent resets for other write access
		regsw[CHG_REG_SUPPLY_STATUS] = (true == CHGR_CONFIG_INPUTS_RPI5V_PREFERRED) ? CHGR_SC_FLT_SUPPLY_PREF_USB : 0u;

		uint8_t writeData[2u] = { CHG_REG_SUPPLY_STATUS, regsw[CHG_REG_SUPPLY_STATUS] | CHGR_SC_TMR_RST_bm };

		I2CDRV_Transact(CHARGER_I2C_PORTNO, CHARGER_I2C_ADDR, writeData, 2u,
								I2CDRV_TRANSACTION_TX, CHARGER_WDT_I2C_Callback,
								1000u, sysTime);

		while (false == I2CDRV_IsReady(CHARGER_I2C_PORTNO))
		{
			// Wait for transaction to complete
		}

	}

	// Periodically read register states from charger
	if (MS_TIME_COUNT(m_lastRegReadTimeMs) >= CHG_READ_PERIOD_MS)
	{
		if (CHARGER_UpdateLocalRegister(m_nextRegReadAddr))
		{
			m_nextRegReadAddr++;
			if (m_nextRegReadAddr == CHARGER_REGISTER_COUNT)
			{
				m_nextRegReadAddr = 0u;
			}
		}

		m_chargerStatus = (regs[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_STAT_Pos) & 0x07u;

		MS_TIME_COUNTER_INIT(m_lastRegReadTimeMs);
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

	m_chargerNeedPoll = (0u != CHARGER_UpdateRPi5VInLockout()) ?
							true :
							m_chargerNeedPoll;
}


bool CHARGER_GetRPi5vInputEnable()
{
	// TODO - This should probably be the actual setting from the device registers
	return CHGR_CONFIG_INPUTS_RPI5V_ENABLED;
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

	regs[regAddress] = tempReg;

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


bool CHARGER_UpdateDeviceRegister(const uint8_t regAddress, const uint8_t value)
{
	const uint32_t sysTime = HAL_GetTick();
	uint8_t writeData[2u] = {regAddress, value};
	bool transactGood;

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

	// Read written value back
	if (false == CHARGER_ReadDeviceRegister(regAddress))
	{
		return false;
	}

	// Check match of data
	if (value != m_i2cReadRegResult)
	{
		// This could mean the charger data is now invalid
		return false;
	}

	return true;
}


int8_t CHARGER_UpdateChgCurrentAndTermCurrent(void)
{

	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();

	if (currentBatProfile!=NULL)
	{
		regsw[CHG_REG_TERMI_FASTCHARGEI] =
				(((currentBatProfile->chargeCurrent > 26u) ? 26u : currentBatProfile->chargeCurrent & 0x1F) << 3u)
				| (currentBatProfile->terminationCurr & 0x07u);
	}
	else
	{
		regsw[CHG_REG_TERMI_FASTCHARGEI] = 0u;

		if (0u != regsStatusRW[CHG_REG_TERMI_FASTCHARGEI])
		{
			return 0u;
		}
	}

	// if there were errors in previous transfers, or first time update, read state from register
	if (0u != regsStatusRW[CHG_REG_TERMI_FASTCHARGEI])
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_TERMI_FASTCHARGEI))
		{
			return 1;
		}
	}

	if ( regsw[CHG_REG_TERMI_FASTCHARGEI] != regs[CHG_REG_TERMI_FASTCHARGEI] )
	{
		// write new value to register
		if (false == CHARGER_UpdateDeviceRegister(CHG_REG_TERMI_FASTCHARGEI, regsw[CHG_REG_TERMI_FASTCHARGEI]))
		{
			return 2;
		}
	}

	return 0;
}


int8_t CHARGER_UpdateVinDPM(void)
{
	// if there were errors in previous transfers read state from register
	if (regsStatusRW[CHG_REG_DPPM_STATUS])
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_DPPM_STATUS))
		{
			return 1;
		}
	}

	// Take vale for Vin as set by the inputs config, RPi 5V set to 480mV
	regsw[CHG_REG_DPPM_STATUS] = CHRG_CONFIG_INPUTS_DPM | (CHARGER_VIN_DPM_USB << 3u);

	if ( regsw[CHG_REG_DPPM_STATUS] != (regs[CHG_REG_DPPM_STATUS]&0x3F) )
	{
		// write new value to register
		if (CHARGER_UpdateDeviceRegister(CHG_REG_DPPM_STATUS, regsw[CHG_REG_DPPM_STATUS]))
		{
			return 2;
		}
	}

	return 0;
}


int8_t CHARGER_UpdateTempRegulationControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint8_t batteryTemp = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();

	//Timer slowed by 2x when in thermal regulation, 10 � 9 hour fast charge, TS function disabled
	regsw[CHG_REG_SAFETY_NTC] = CHGR_ST_NTC_2XTMR_EN_bm | CHGR_ST_NTC_SFTMR_9HOUR;

	if (currentBatProfile != NULL)
	{
		if ( (batteryTemp < currentBatProfile->tCool) && (tempSensorConfig != BAT_TEMP_SENSE_CONFIG_NOT_USED) )
		{
			// Reduce charge current to half
			regsw[CHG_REG_SAFETY_NTC] |= CHGR_ST_NTC_LOW_CHARGE_bm;
		}
		else
		{
			// Allow full set charge current
			regsw[CHG_REG_SAFETY_NTC] &= ~(CHGR_ST_NTC_LOW_CHARGE_bm);
		}
	}
	else
	{
		// Allow full set charge current
		regsw[CHG_REG_SAFETY_NTC] &= ~(CHGR_ST_NTC_LOW_CHARGE_bm);

		if (0u != regsStatusRW[CHG_REG_SAFETY_NTC])
		{
			return 0;
		}
	}

	// if there were errors in previous transfers read state from register
	if (regsStatusRW[CHG_REG_SAFETY_NTC])
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_SAFETY_NTC))
		{
			return 1;
		}
	}

	// See if anything has changed
	if ( (regsw[CHG_REG_SAFETY_NTC] & 0xE9u) != (regs[CHG_REG_SAFETY_NTC] & 0xE9u) )
	{
		// write new value to register
		if (false == CHARGER_UpdateDeviceRegister(CHG_REG_SAFETY_NTC, regsw[CHG_REG_SAFETY_NTC]))
		{
			return 2;
		}
	}

	return 0;
}


int8_t CHARGER_UpdateControlStatus(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint8_t batteryTemp = FUELGUAGE_GetBatteryTemperature();
	const BatteryTempSenseConfig_T tempSensorConfig = FUELGUAGE_GetBatteryTempSensorCfg();

	// usb in current limit code, Enable STAT output, Enable charge current termination
	regsw[CHG_REG_CONTROL] = ((m_rpi5VCurrentLimit << CHGR_CTRL_IUSB_LIMIT_Pos) & CHGR_CTRL_IUSB_LIMIT_Msk)
									| CHGR_CTRL_EN_STAT
									| CHGR_CTRL_TE;

	if (currentBatProfile != NULL)
	{
		if ( CHARGE_DISABLED
			|| ( (tempSensorConfig != BAT_TEMP_SENSE_CONFIG_NOT_USED)
						&& ((batteryTemp >= currentBatProfile->tHot) || (batteryTemp <= currentBatProfile->tCold))
					)
			)
		{
			// disable charging
			regsw[CHG_REG_CONTROL] |= CHGR_CTRL_CHG_DISABLE;
			regsw[CHG_REG_CONTROL] &= ~(CHGR_CTRL_HZ_MODE); // clear high impedance mode
		}
		else
		{
			// enable charging
			regsw[CHG_REG_CONTROL] &= ~(CHGR_CTRL_CHG_DISABLE);
			regsw[CHG_REG_CONTROL] &= ~(CHGR_CTRL_HZ_MODE); // clear high impedance mode
		}
	}
	else
	{
		// disable charging
		regsw[CHG_REG_CONTROL] |= CHGR_CTRL_CHG_DISABLE;
		regsw[CHG_REG_CONTROL] &= ~(CHGR_CTRL_HZ_MODE); // clear high impedance mode
	}


	// if there were errors in previous transfers, or first time update, read state from register
	if (regsStatusRW[CHG_REG_CONTROL])
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_CONTROL))
		{
			return 1;
		}
	}

	if ( (regsw[CHG_REG_CONTROL]&0x7F) != (regs[CHG_REG_CONTROL]&0x7F) )
	{
		// write new value to register
		if (false == CHARGER_UpdateDeviceRegister(CHG_REG_CONTROL, regsw[CHG_REG_CONTROL]))
		{
			return 1;
		}
	}

	return 0;
}


int8_t CHARGER_UpdateRPi5VInLockout(void)
{
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();

	regsw[CHG_REG_BATTERY_STATUS] = (true == CHRG_CONFIG_INPUTS_NOBATT_ENABLED) ? CHGR_BS_EN_NOBAT_OP : 0u;

	// If RPi is powered by itself and host allows charging from RPi 5v and the battery checks out ok...
	if ( (true == CHGR_CONFIG_INPUTS_RPI5V_ENABLED)
			&& (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT)
			&& ((regs[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk) == CHGR_BS_BATSTAT_NORMAL)
			)
	{
		// Allow RPi 5v to charge battery
		regsw[CHG_REG_BATTERY_STATUS] &= ~(CHGR_BS_OTG_LOCK_bm);
	}
	else
	{
		// Don't allow RPi 5v to charge battery
		regsw[CHG_REG_BATTERY_STATUS] |= CHGR_BS_OTG_LOCK_bm;
	}


	// if there were errors in previous transfers, or first time update, read state from register
	if (0u != regsStatusRW[CHG_REG_BATTERY_STATUS])
	{
		if (false == CHARGER_UpdateLocalRegister(CHG_REG_BATTERY_STATUS))
		{
			return 1;
		}
	}

	if ( (regsw[CHG_REG_BATTERY_STATUS] & 0x09u) != (regs[CHG_REG_BATTERY_STATUS] & 0x09u) )
	{
		// write new value to register
		if (true == CHARGER_UpdateDeviceRegister(CHG_REG_BATTERY_STATUS, regsw[CHG_REG_BATTERY_STATUS]))
		{
			return 2;
		}
	}

	return 0;
}


CHARGER_USBInLockoutStatus_T CHARGER_GetRPi5vInLockStatus(void)
{
	return m_rpi5VInputDisable;
}


void CHARGER_SetInterrupt(void)
{
	m_interrupt = true;
}


bool CHARGER_RequirePoll(void)
{
	return m_chargerNeedPoll;
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
	const uint8_t instat = (regs[CHG_REG_SUPPLY_STATUS] & CHGR_SC_STAT_Msk);

	// TODO- Supply status register might be better for this.
	return (instat > CHGR_SC_STAT_NO_SOURCE) && (instat <= CHGR_SC_STAT_CHARGE_DONE);
}


bool CHARGER_IsBatteryPresent(void)
{
	return (regs[CHG_REG_BATTERY_STATUS] & CHGR_BS_BATSTAT_Msk) == CHGR_BS_BATSTAT_NORMAL;
}


bool CHARGER_HasTempSensorFault(void)
{
	return (regs[CHG_REG_SAFETY_NTC] & CHGR_ST_NTC_FAULT_Msk) != CHGR_ST_NTC_TS_FAULT_NONE;
}


uint8_t CHARGER_GetTempFault(void)
{
	return (regs[CHG_REG_SAFETY_NTC] >> CHGR_ST_NTC_FAULT_Pos);
}


CHARGER_InputStatus_t CHARGER_GetInputStatus(uint8_t inputChannel)
{
	uint8_t channelPos;

	if (inputChannel < CHARGER_INPUT_CHANNELS)
	{
		channelPos = (CHARGER_INPUT_RPI == inputChannel) ? CHGR_BS_USBSTAT_Pos : CHGR_BS_INSTAT_Pos;

		return (regs[CHG_REG_BATTERY_STATUS] >> channelPos) & 0x03u;
	}

	return CHARGER_INPUT_UVP;
}


bool CHARGER_IsDPMActive(void)
{
	return (0u != (regs[CHG_REG_DPPM_STATUS] & CHGR_VDPPM_DPM_ACTIVE));
}


uint8_t CHARGER_GetFaultStatus(void)
{
	return (regs[CHG_REG_SUPPLY_STATUS] >> CHGR_SC_FLT_Pos) & 0x7u;
}
