/*
 * power_source.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "main.h"

#include "power_source.h"
#include "charger_bq2416x.h"
#include "nv.h"
#include "adc.h"
#include "analog.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"
#include "load_current_sense.h"
#include "execution.h"

#include "system_conf.h"
#include "iodrv.h"

#include "util.h"


#define POW_5V_IO_DET_ADC_THRESHOLD		2950u
#define VBAT_TURNOFF_ADC_THRESHOLD		0u // mV unit
#define POWER_SOURCE_PRESENT			( (m_powerInStatus == POW_SOURCE_NORMAL) \
											|| (m_powerInStatus == POW_SOURCE_WEAK) \
											|| (m_power5vIoStatus == POW_SOURCE_NORMAL) \
											|| (m_power5vIoStatus == POW_SOURCE_WEAK) )


uint8_t forcedPowerOffFlag __attribute__((section("no_init")));
uint8_t forcedVSysOutputOffFlag __attribute__((section("no_init")));

extern uint8_t resetStatus;
extern uint16_t wakeupOnCharge;

volatile int32_t adcDmaPos = -1;

static uint16_t m_vbatPowerOffThreshold;

//uint8_t pow5VChgLoadMaximumReached = 0;

uint8_t m_rpiVLowCount;

static uint32_t m_boostOnTimeMs;
static uint32_t m_lastRPiPowerDetectTimeMs;
static uint32_t m_rpiDetTimeMs;
static uint32_t m_lastPowerProcessTime;

static uint32_t forcedPowerOffCounter __attribute__((section("no_init")));
static uint8_t m_vsysSwitchLimit __attribute__((section("no_init")));

static bool m_boostConverterEnabled;
static bool m_vsysEnabled;
static bool m_ldoEnabled;

static PowerSourceStatus_T m_powerInStatus = POW_SOURCE_NOT_PRESENT;
static PowerSourceStatus_T m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
static POWERSOURCE_RPi5VStatus_t m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_UNKNOWN;

static void POWERSOURCE_ProcessVINStatus(void);
static void POWERSOURCE_Process5VRailStatus(void);
static void POWERSOURCE_CheckPowerValid(void);
static void POWERSOURCE_RPi5vDetect(void);
static void POWERSOURCE_Process5VRailPower(void);

static PowerRegulatorConfig_T m_powerRegulatorConfig = POW_REGULATOR_MODE_POW_DET;


// TODO - Check what this does
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc)
{
	adcDmaPos = __HAL_DMA_GET_COUNTER(hadc->DMA_Handle);
}


void POWERSOURCE_Init(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint32_t sysTime = HAL_GetTick();
	const bool chargerHasPowerIn = CHARGER_IsChargeSourceAvailable();

	uint8_t tempU8;

	m_boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);
	m_vsysEnabled = IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN);

	uint16_t vBatMv;
	uint16_t var = 0u;

	// initialize global variables after power-up
	if (EXECUTION_STATE_NORMAL != executionState)
	{
		forcedPowerOffFlag = 0u;
		forcedVSysOutputOffFlag = 0u;
		forcedPowerOffCounter = 0u;
		m_boostOnTimeMs = UINT32_MAX;
	}

	// Setup module timers
	MS_TIMEREF_INIT(m_lastPowerProcessTime, sysTime);
	MS_TIMEREF_INIT(m_lastRPiPowerDetectTimeMs, sysTime);

	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);

	if ( UTIL_NV_ParamInitCheck_U16(var) )
	{
		tempU8 = (uint8_t)(var & 0xFFu);

		if (tempU8 < POW_REGULATOR_MODE_COUNT)
		{
			m_powerRegulatorConfig = (PowerRegulatorConfig_T)tempU8;
		}
	}

	// Set battery cut off threshold
	m_vbatPowerOffThreshold = (NULL != currentBatProfile) ?
				currentBatProfile->cutoffVoltage * (20u + VBAT_TURNOFF_ADC_THRESHOLD) :
				3000u + VBAT_TURNOFF_ADC_THRESHOLD;

	AnalogAdcWDGConfig(ANALOG_CHANNEL_VBAT,  m_vbatPowerOffThreshold);

	DelayUs(100u);

	vBatMv = ANALOG_GetBatteryMv();

	// powerEnableState
	// maintain regulator state before reset
	if ( true == m_boostConverterEnabled )
	{
		// if there is mcu power-on, but reg was on, it can be power lost fault condition, check sources
		if ( (executionState == EXECUTION_STATE_POWER_RESET)
				&& (vBatMv < m_vbatPowerOffThreshold)
				&& (false == chargerHasPowerIn)
				)
		{
			POWERSOURCE_Set5vBoostEnable(false);
			forcedPowerOffFlag = 1u;
			wakeupOnCharge = 5u; // schedule wake up when there is enough energy
		}
		else
		{
			POWERSOURCE_Set5vBoostEnable(true);
		}
	}
	else
	{
		POWERSOURCE_SetLDOEnable(false);
		POWERSOURCE_Set5vBoostEnable(false);
	}

	if ( (executionState == EXECUTION_STATE_POWER_ON) || (executionState == EXECUTION_STATE_POWER_RESET) )
	{
		// after power-up state is 500mA limit
		IODRV_SetPin(IODRV_PIN_ESYSLIM, true);
		m_vsysSwitchLimit = 5;
	}
	else
	{
		if (m_vsysSwitchLimit == 21 )
		{
			IODRV_SetPin(IODRV_PIN_ESYSLIM, false);
		}
		else
		{
			IODRV_SetPin(IODRV_PIN_ESYSLIM, true);
		}
	}

	// Restore previous Vsys enable
	if ( (true == m_vsysEnabled) && (EXECUTION_STATE_POWER_ON != executionState) )
	{
		if ( (EXECUTION_STATE_POWER_RESET == executionState)  && (vBatMv < m_vbatPowerOffThreshold) && (chargerHasPowerIn) )
		{
			// Disable vsys
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
			m_vsysEnabled = false;

			forcedVSysOutputOffFlag = 1u;
		}
		else
		{
			// Enable vsys
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, true);
		}
	}
	else
	{
		// Disable vsys
		IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
	}

	IODRV_SetPinType(IODRV_PIN_POW_EN, IOTYPE_DIGOUT_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_EXTVS_EN, IOTYPE_DIGOUT_PUSHPULL);
}


void POWERSOURCE_Task(void)
{
	POWERSOURCE_RPi5vDetect();

	POWERSOURCE_ProcessVINStatus();
	POWERSOURCE_Process5VRailStatus();

	POWERSOURCE_Process5VRailPower();

	POWERSOURCE_CheckPowerValid();
}


void POWERSOURCE_UpdateBatteryProfile(const BatteryProfile_T * const batProfile)
{
	m_vbatPowerOffThreshold = (NULL == batProfile) ?
								3000u + VBAT_TURNOFF_ADC_THRESHOLD	:
								(batProfile->cutoffVoltage * 20u) + VBAT_TURNOFF_ADC_THRESHOLD;

	AnalogAdcWDGConfig(ANALOG_CHANNEL_VBAT,  m_vbatPowerOffThreshold);
}


// 21 = on & 2.1A limit, 5 - on and 0.5A limit, anything else is off.
void POWERSOURCE_SetVSysSwitchState(uint8_t switchState)
{
	const uint16_t vbatAdcVal = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);
	const uint16_t wdgThreshold = GetAdcWDGThreshold();

	if (5u == switchState)
	{
		// Set VSys I limit pin
		IODRV_SetPin(IODRV_PIN_ESYSLIM, true);

		if ( (vbatAdcVal > wdgThreshold) || (true == POWER_SOURCE_PRESENT) )
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, true);
			m_vsysEnabled = true;
		}

		m_vsysSwitchLimit = switchState;
	}
	else if (21u == switchState)
	{
		// Reset VSys I limit pin
		IODRV_SetPin(IODRV_PIN_ESYSLIM, false);

		if ( (vbatAdcVal > wdgThreshold) || (true == POWER_SOURCE_PRESENT) )
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, true);
			m_vsysEnabled = true;
		}

		m_vsysSwitchLimit = switchState;
	}
	else
	{
		IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
		m_vsysEnabled = false;
	}

	forcedVSysOutputOffFlag = 0u;
}


uint8_t POWERSOURCE_GetVSysSwitchState(void)
{
	if (true == m_vsysEnabled)
	{
		return m_vsysSwitchLimit;
	}
	else
	{
		return 0u;
	}
}


void POWERSOURCE_SetRegulatorConfig(const uint8_t * const data, const uint8_t len)
{
	uint8_t temp;
	uint16_t var = 0u;

	if ( (data[0u] >= (uint8_t)POW_REGULATOR_MODE_COUNT) || (len < 1u) )
	{
		return;
	}

	EE_WriteVariable(POWER_REGULATOR_CONFIG_NV_ADDR, data[0u] | ((uint16_t)(~data[0u]) << 8u));

	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);
	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		temp = (uint8_t)(var & 0xFFu);

		if (temp < POW_REGULATOR_MODE_COUNT)
		{
			m_powerRegulatorConfig = temp;
		}
	}
}


void POWERSOURCE_GetRegulatorConfig(uint8_t * const data, uint16_t * const len)
{
	data[0u] = m_powerRegulatorConfig;
	*len = 1u;
}


void POWERSOURCE_Set5vBoostEnable(const bool enabled)
{
	uint16_t vBattMv = ANALOG_GetBatteryMv();
	uint8_t retrys = 2u;

	if (m_boostConverterEnabled == enabled)
	{
		// Already there.
		return;
	}

	if (true == enabled)
	{
		if ( (vBattMv > m_vbatPowerOffThreshold) || (true == POWER_SOURCE_PRESENT) )
		{
			// Turn off LDO
			POWERSOURCE_SetLDOEnable(false);

			// Switch off the analog watchdog
			AnalogAdcWDGEnable(DISABLE);

			// Wait for it to happen
			DelayUs(5u);

			// Turn on boost converter
			IODRV_SetPin(IODRV_PIN_POW_EN, true);

			// If running on battery
			if (false == POWER_SOURCE_PRESENT)
			{
				// Check to make sure the battery is low impedance
				while (retrys > 0u)
				{
					// Wait before checking
					DelayUs(200u);

					vBattMv = ANALOG_GetBatteryMv();

					if (vBattMv > m_vbatPowerOffThreshold)
					{
						// Happy days
						break;
					}

					retrys--;
				}

				if (0u == retrys)
				{
					// Battery is not in a good enough state to power the system
					IODRV_SetPin(IODRV_PIN_POW_EN, false);

					return;
				}
			}

			// Original code pulsed power off-5us-on
			m_boostConverterEnabled = true;

			// Stamp time boost converter is enabled
			MS_TIME_COUNTER_INIT(m_boostOnTimeMs);

			// Turn on the analog watchdog
			AnalogAdcWDGEnable(ENABLE);

			return;
		}
		else
		{
			return;
		}
	}
	else
	{
		// Turn off LDO
		POWERSOURCE_SetLDOEnable(false);

		AnalogAdcWDGEnable(DISABLE);

		// Turn off the boost converter
		IODRV_SetPin(IODRV_PIN_POW_EN, false);

		// TODO - Need to find out what this does
		MS_TIME_COUNTER_INIT(m_lastRPiPowerDetectTimeMs);

		m_boostConverterEnabled = false;

		return;
	}
}


void POWERSOURCE_SetLDOEnable(const bool enabled)
{
	if (m_powerRegulatorConfig == POW_REGULATOR_MODE_DCDC)
	{
		m_ldoEnabled = false;
	}
	else if (m_powerRegulatorConfig == POW_REGULATOR_MODE_LDO)
	{
		if (true == m_boostConverterEnabled)
		{
			// Turn off the LDO is the boost is enabled
			m_ldoEnabled = false;
		}
		else
		{
			m_ldoEnabled = true;
		}
	}
	else // not configured
	{
		m_ldoEnabled = enabled;
	}

	IODRV_SetPin(IODRV_PIN_POWDET_EN, m_ldoEnabled);
}


bool POWERSOURCE_IsVsysEnabled(void)
{
	return m_vsysEnabled;
}


bool POWERSOURCE_IsBoostConverterEnabled(void)
{
	return m_boostConverterEnabled;
}


PowerSourceStatus_T POWERSOURCE_GetVInStatus(void)
{
	return m_powerInStatus;
}


PowerSourceStatus_T POWERSOURCE_Get5VRailStatus(void)
{
	return m_power5vIoStatus;
}


POWERSOURCE_RPi5VStatus_t POWERSOURCE_GetRPi5VPowerStatus(void)
{
	return m_rpi5VInDetStatus;
}


bool POWERSOURCE_NeedPoll(void)
{
	return ((m_rpi5VInDetStatus == RPI5V_DETECTION_STATUS_POWERED) || (MS_TIME_COUNT(m_boostOnTimeMs) < POWERSOURCE_STABLISE_TIME_MS));
}


void POWERSOURCE_CheckPowerValid(void)
{
	const uint16_t v5RailMv = ANALOG_Get5VRailMv();
	const uint16_t aVddMv = ANALOG_GetAVDDMv();
	const uint16_t vBattMv = ANALOG_GetBatteryMv();
	const uint16_t batteryADCValue = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);
	const bool chargerHasPowerIn = CHARGER_IsChargeSourceAvailable();
	const uint32_t sysTime = HAL_GetTick();

	// 47day rollover issue will stop the routine being run but it'll only be for a brief time!
	if (false == MS_TIMEREF_TIMEOUT(m_boostOnTimeMs, sysTime, POWERSOURCE_STABLISE_TIME_MS))
	{
		return;
	}

	if ( true == m_boostConverterEnabled )
	{
		if ( (v5RailMv < 2000u) && (aVddMv > 2500u) )
		{
			//5V DCDC is in fault overcurrent state, turn it off to prevent draining battery
			POWERSOURCE_Set5vBoostEnable(false);
			forcedPowerOffFlag = 1u;;

			return;
		}

		// if no sources connected, turn off 5V regulator and system switch when battery voltage drops below minimum
		if ( (batteryADCValue < GetAdcWDGThreshold())
				&& (m_rpi5VInDetStatus != RPI5V_DETECTION_STATUS_POWERED)
				&& (true == chargerHasPowerIn)
				)
		{
			if (true == m_vsysEnabled)
			{
				// Turn off Vsys and set forced off flag
				IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
				m_vsysEnabled = false;
				forcedVSysOutputOffFlag = 1u;

				// Leave 2 ms for switch to react
				MS_TIME_COUNTER_INIT(forcedPowerOffCounter);

				return;
			}
			else if ( MS_TIME_COUNT(forcedPowerOffCounter) >= 2u)
			{

				POWERSOURCE_SetLDOEnable(false);
				POWERSOURCE_Set5vBoostEnable(false);

				forcedPowerOffFlag = 1u;
				wakeupOnCharge = 5u; // schedule wake up when power is applied

				return;
			}
		}
	}
	else
	{
		// If running on battery turn off Vsys when it gets too low
		if ( (false == POWER_SOURCE_PRESENT)
				&& (vBattMv < m_vbatPowerOffThreshold)
				&& (true == m_vsysEnabled)
				)
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
			forcedVSysOutputOffFlag = 1u;

			return;
		}
	}
}


// Adjust the charge rate if the rail is low
static void POWERSOURCE_Process5VRailPower(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();
	const uint16_t vBattMv = ANALOG_GetBatteryMv();
	const ChargerStatus_T chargerStatus = CHARGER_GetStatus();

	if (false == MS_TIMEREF_TIMEOUT(m_lastPowerProcessTime, sysTime, POWERSOURCE_5VRAIL_PROCESS_PERIOD_MS))
	{
		return;
	}

	MS_TIMEREF_INIT(m_lastPowerProcessTime, sysTime);

	// If running on battery, test to make sure there is enough power to supply the system
	if ( (false == POWER_SOURCE_PRESENT) )
	{
		// If battery is low terminal voltage,
		// Note: if there is no power source and no battery, what does that mean?!
		if ( (BAT_STATUS_NOT_PRESENT != batteryStatus) && (vBattMv < m_vbatPowerOffThreshold) )
		{
			POWERSOURCE_Set5vBoostEnable(false);
			forcedPowerOffFlag = 1;
		}
	}


	if (m_rpi5VInDetStatus != RPI5V_DETECTION_STATUS_POWERED)
	{
		return;
	}


	if (CHG_CHARGING_FROM_RPI == chargerStatus)
	{
		if (m_rpiVLowCount > 0u)
		{
			CHARGER_RPi5vInCurrentLimitStepDown();
		}
		else
		{
			CHARGER_RPi5vInCurrentLimitStepUp();
		}
	}
}


// Try and determine if the 5v comes from the RPi
void POWERSOURCE_RPi5vDetect(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const uint16_t v5RailMv = ANALOG_Get5VRailMv();

	// Check to see if the boost converter has just switched on.
	if (false == MS_TIMEREF_TIMEOUT(m_boostOnTimeMs, sysTime, POWERSOURCE_STABLISE_TIME_MS))
	{
		// Wait for 5V to become stable after turn on timeout
		return;
	}


	// Check to see if its time to have a look at the RPi power - probably should do this all the time!
	if (false == MS_TIMEREF_TIMEOUT(m_lastRPiPowerDetectTimeMs, sysTime, POWERSROUCE_RPI5V_DETECT_PERIOD_MS))
	{
		return;
	}


	// If the boost converter is enabled
	if ( (true == m_boostConverterEnabled) )
	{
		if (false == m_ldoEnabled)
		{
			// Can't tell if the RPi is powered without the LDO!
			m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_UNKNOWN;
			MS_TIMEREF_INIT(m_rpiDetTimeMs, sysTime);
		}
		else
		{
			if (v5RailMv > POWERSOURCE_LDO_MV)
			{
				if ( (RPI5V_DETECTION_STATUS_POWERED != m_rpi5VInDetStatus)
						&& (true == MS_TIMEREF_TIMEOUT(m_rpiDetTimeMs, sysTime, POWERSOURCE_RPI5V_POWERED_MS)) )
				{
					// If the voltage on the rail is more than the LDO can supply then the RPi is probably powered
					m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_POWERED;
				}
			}
			else
			{
				m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_UNPOWERED;

				MS_TIMEREF_INIT(m_rpiDetTimeMs, sysTime);
			}
		}

		return;
	}
	else if (v5RailMv > POWERSOURCE_RPI_UNDER_MV)
	{
		if ( (RPI5V_DETECTION_STATUS_POWERED != m_rpi5VInDetStatus)
				&& (true == MS_TIMEREF_TIMEOUT(m_rpiDetTimeMs, sysTime, POWERSOURCE_RPI5V_POWERED_MS)) )
		{
			// If the voltage on the rail is more than the LDO can supply then the RPi is probably powered
			m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_POWERED;
		}

		if (v5RailMv <= POWERSOURCE_RPI_LOW_MV)
		{
			if (m_rpiVLowCount < POWERSOURCE_RPI_VLOW_MAX_COUNT)
			{
				m_rpiVLowCount++;
			}
		}
		else
		{
			m_rpiVLowCount = 0u;
		}
	}
	else
	{
		MS_TIMEREF_INIT(m_rpiDetTimeMs, sysTime);

		m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_UNPOWERED;
	}
}


void POWERSOURCE_ProcessVINStatus(void)
{
	const CHARGER_InputStatus_t vinStatus = CHARGER_GetInputStatus(CHARGER_INPUT_VIN);
	const uint8_t chargerDPMActive = CHARGER_IsDPMActive();

	if (vinStatus == CHARGER_INPUT_UVP)
	{
		m_powerInStatus = POW_SOURCE_NOT_PRESENT;
	}
	else if ( (vinStatus == CHARGER_INPUT_OVP) || (vinStatus == CHARGER_INPUT_WEAK) )
	{
		m_powerInStatus = POW_SOURCE_BAD;
	}
	else if (true == chargerDPMActive)
	{
		m_powerInStatus = POW_SOURCE_WEAK;
	}
	else
	{
		m_powerInStatus = POW_SOURCE_NORMAL;
	}
}


void POWERSOURCE_Process5VRailStatus(void)
{
	const bool rpi5vChargeEnable = CHARGER_GetRPi5vInputEnable();
	const CHARGER_InputStatus_t rpi5vChargeStatus = CHARGER_GetInputStatus(CHARGER_INPUT_RPI);


	if (RPI5V_DETECTION_STATUS_POWERED != m_rpi5VInDetStatus)
	{
		m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;

		CHARGER_SetRPi5vInputEnable(false);
	}
	else
	{
		m_power5vIoStatus = POW_SOURCE_NORMAL;

		if (true == rpi5vChargeEnable)
		{
			if (rpi5vChargeStatus == CHARGER_INPUT_UVP)
			{
				m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
			}
			else if ( rpi5vChargeStatus == CHARGER_INPUT_OVP || rpi5vChargeStatus == CHARGER_INPUT_WEAK)
			{
				m_power5vIoStatus = POW_SOURCE_BAD;
			}
			else if (0u != m_rpiVLowCount)
			{
				m_power5vIoStatus = POW_SOURCE_WEAK;
			}
			else
			{
				m_power5vIoStatus = POW_SOURCE_NORMAL;
			}
		}
		else
		{
			CHARGER_SetRPi5vInputEnable(true);
		}
	}
}
