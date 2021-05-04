/*
 * power_source.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include "main.h"
#include "system_conf.h"

#include "power_management.h"
#include "charger_bq2416x.h"
#include "nv.h"
#include "adc.h"
#include "analog.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"
#include "load_current_sense.h"
#include "execution.h"

#include "iodrv.h"

#include "util.h"

#include "power_source.h"


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define POW_5V_IO_DET_ADC_THRESHOLD		2950u
#define VBAT_TURNOFF_ADC_THRESHOLD		0u // mV unit
#define POWER_SOURCE_PRESENT			( (m_powerInStatus == POW_SOURCE_NORMAL) \
											|| (m_powerInStatus == POW_SOURCE_WEAK) \
											|| (m_power5vIoStatus == POW_SOURCE_NORMAL) \
											|| (m_power5vIoStatus == POW_SOURCE_WEAK) )


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static void POWERSOURCE_ProcessVINStatus(void);
static void POWERSOURCE_Process5VRailStatus(void);
static void POWERSOURCE_CheckPowerValid(void);
static void POWERSOURCE_RPi5vDetect(void);
static void POWERSOURCE_Process5VRailPower(void);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint16_t m_vbatPowerOffThreshold;
static uint8_t m_rpiVLowCount;
static uint32_t m_boostOnTimeMs;
static uint32_t m_lastRPiPowerDetectTimeMs;
static uint32_t m_rpiDetTimeMs;
static uint32_t m_lastPowerProcessTime;
static bool m_ldoEnabled;
static PowerSourceStatus_T m_powerInStatus = POW_SOURCE_NOT_PRESENT;
static PowerSourceStatus_T m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
static POWERSOURCE_RPi5VStatus_t m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_UNKNOWN;
static PowerRegulatorConfig_T m_powerRegulatorConfig = POW_REGULATOR_MODE_POW_DET;


// ----------------------------------------------------------------------------
// Variables that only have scope in this module that are persistent through reset:

static bool m_forcedPowerOff __attribute__((section("no_init")));
static bool m_forcedVSysOutputOff __attribute__((section("no_init")));
static uint32_t forcedPowerOffCounter __attribute__((section("no_init")));
static bool m_boostConverterEnabled __attribute__((section("no_init")));
static bool m_vsysEnabled __attribute__((section("no_init")));
static uint8_t m_vsysSwitchLimit __attribute__((section("no_init")));


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * POWERSOURCE_Init configures the module to a known initial state and tries to
 * determine in the event of a non-power cycle reset if the boost converter and
 * VSys output were previously on and the previous current limit for VSys.
 * The LDO output will follow as configured.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_Init(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfileHandle();
	const uint32_t sysTime = HAL_GetTick();
	const bool chargerHasVIn = (CHARGER_INPUT_NORMAL == CHARGER_GetInputStatus(CHARGER_INPUT_VIN));
	const uint16_t vBatMv = ANALOG_GetBatteryMv();

	uint8_t tempU8;


	// initialise global variables in the no-init section after power-up
	if (EXECUTION_STATE_POWER_RESET == executionState)
	{
		m_forcedPowerOff = 0u;
		m_forcedVSysOutputOff = 0u;
		forcedPowerOffCounter = 0u;

		m_boostConverterEnabled = false;
		m_vsysEnabled = false;
		m_vsysSwitchLimit = 5u;
	}

	// Setup module timers
	MS_TIMEREF_INIT(m_lastPowerProcessTime, sysTime);
	MS_TIMEREF_INIT(m_lastRPiPowerDetectTimeMs, sysTime);


	if (NV_ReadVariable_U8(POWER_REGULATOR_CONFIG_NV_ADDR, &tempU8))
	{
		if (tempU8 < POW_REGULATOR_MODE_COUNT)
		{
			m_powerRegulatorConfig = (PowerRegulatorConfig_T)tempU8;
		}
	}


	// Setup battery low voltage threshold
	POWERSOURCE_UpdateBatteryProfile(currentBatProfile);


	// maintain regulator state before reset
	if ( true == m_boostConverterEnabled )
	{
		// if there is mcu power-on, but reg was on, it can be power lost fault condition, check sources
		if ( (executionState == EXECUTION_STATE_POWER_RESET)
				&& (vBatMv < m_vbatPowerOffThreshold)
				&& (false == chargerHasVIn)
				)
		{
			POWERSOURCE_Set5vBoostEnable(false);
			m_forcedPowerOff = true;
			POWERMAN_SetWakeupOnChargePcntPt1(5u); // schedule wake up when there is enough energy
		}
		else
		{
			// Need to rest the boost enabled flag or it'll just think its already on!
			m_boostConverterEnabled = false;
			POWERSOURCE_Set5vBoostEnable(true);
		}
	}


	// Check if reset cycle was because of a loss of power (boost en on but power on reset)
	if (EXECUTION_STATE_POWER_ON == executionState)
	{
		m_vsysSwitchLimit = 5u;
	}

	// Set the VSys output current limit
	IODRV_SetPin(IODRV_PIN_ESYSLIM, 21u != m_vsysSwitchLimit);

	// Restore previous Vsys enable
	if ( (true == m_vsysEnabled) && (EXECUTION_STATE_POWER_ON != executionState) )
	{
		if ( (EXECUTION_STATE_POWER_RESET == executionState)  &&
				(vBatMv < m_vbatPowerOffThreshold) &&
				(false == chargerHasVIn)
				)
		{
			// Disable vsys
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
			m_vsysEnabled = false;

			m_forcedVSysOutputOff = true;
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


	// Switch the pin types to output
	IODRV_SetPinType(IODRV_PIN_POW_EN, IOTYPE_DIGOUT_PUSHPULL);
	IODRV_SetPinType(IODRV_PIN_EXTVS_EN, IOTYPE_DIGOUT_PUSHPULL);
}


// ****************************************************************************
/*!
 * POWERSOURCE_Task performs the periodic tasks within the module
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_Task(void)
{
	POWERSOURCE_RPi5vDetect();

	POWERSOURCE_ProcessVINStatus();
	POWERSOURCE_Process5VRailStatus();

	POWERSOURCE_Process5VRailPower();

	POWERSOURCE_CheckPowerValid();
}


// ****************************************************************************
/*!
 * POWERSOURCE_UpdateBatteryProfile should be called when the battery profile
 * is changed, this updates the threshold for which the battery is deemed to be
 * out of power
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_UpdateBatteryProfile(const BatteryProfile_T * const batProfile)
{
	m_vbatPowerOffThreshold = (NULL == batProfile) ?
								3000u + VBAT_TURNOFF_ADC_THRESHOLD	:
								(batProfile->cutoffVoltage * 20u) + VBAT_TURNOFF_ADC_THRESHOLD;
}


// ****************************************************************************
/*!
 * POWERSOURCE_SetVSysSwitchState sets the VSys output current limit, there are
 * two valid set points in 0.1A resolution, 500mA and 2100mA. Anything other than
 * these values will turn off the VSys output but will not change the current limit
 * setting. If running on battery power only the voltage level will be checked to
 * make sure it is above the minimum threshold set by the battery profile before
 * enabling the output.
 *
 * @param	switchState		5 = 500mA, 21 = 2100mA, anything else VSys is off.
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_SetVSysSwitchState(const uint8_t switchState)
{
	const uint16_t vbatAdcVal = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);


	if (5u == switchState)
	{
		// Set VSys I limit pin
		IODRV_SetPin(IODRV_PIN_ESYSLIM, true);

		if ( (vbatAdcVal > m_vbatPowerOffThreshold) || (true == POWER_SOURCE_PRESENT) )
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

		if ( (vbatAdcVal > m_vbatPowerOffThreshold) || (true == POWER_SOURCE_PRESENT) )
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

	m_forcedVSysOutputOff = false;
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetVSysSwitchState returns the setpoint of the VSys current limit
 * if the output is actually on. If the output is off then 0 will be returned.
 *
 * @param	none
 * @retval	uint8_t		VSsys current limit set point in 0.1A resolution,
 * 						0 = VSys output is off.
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * POWERSOURCE_SetRegulatorConfigData updates the configuration for the power regulator,
 * this can be set to power detection, boost converter only or boost converter
 * + LDO. Power detection appears to do that same thing as boost converter + LDO
 * as when the regulation is on boost converter only there is no way of knowing
 * if the RPi is being powered by itself.
 *
 * @param	p_data		data buffer holding the configuration data
 * @param	len			length of the data within the buffer
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_SetRegulatorConfigData(const uint8_t * const p_data, const uint8_t len)
{
	uint8_t tempU8;

	if ( (p_data[0u] >= (uint8_t)POW_REGULATOR_MODE_COUNT) || (len < 1u) )
	{
		return;
	}

	NV_WriteVariable_U8(POWER_REGULATOR_CONFIG_NV_ADDR, p_data[0u]);

	if (NV_ReadVariable_U8(POWER_REGULATOR_CONFIG_NV_ADDR, &tempU8))
	{
		if (tempU8 < POW_REGULATOR_MODE_COUNT)
		{
			m_powerRegulatorConfig = (PowerRegulatorConfig_T)tempU8;
		}
	}
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetRegulatorConfigData populates a uint8 data buffer with the
 * regulater configuration data and sets the data length. Caller is responsible
 * for ensuring buffer can hold amount of data.
 *
 * @param	p_data		destination buffer to put the data
 * @param	p_len		popultaed data length
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_GetRegulatorConfigData(uint8_t * const p_data, uint16_t * const p_len)
{
	p_data[0u] = m_powerRegulatorConfig;
	*p_len = 1u;
}


// ****************************************************************************
/*!
 * POWERSOURCE_Set5vBoostEnable attempts to change the state of the boost converter,
 * if no state change is determined then the routine will not perform any action.
 *
 * If enabling the boost converter, the battery voltage is checked to ensure above
 * the minimum threshold or a charge source is available then LDO is disabled.
 * On enabling the converter when no external source is available the battery is
 * monitored to ensure there is enough power to support the load, if not then the
 * converter will then be disabled. On a successful enable the time is noted.
 *
 * On disabling the converter the LDO is switched off and the RPi detection
 * timer is reset so to synchronise the RPi power detection.
 *
 * @param	enabled		false = disable the boost converter
 * 						true = attempt to turn on the boost converter
 * @retval	none
 */
// ****************************************************************************
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

			// TODO - Original code pulsed power off-5us-on
			m_boostConverterEnabled = true;

			// Stamp time boost converter is enabled
			MS_TIME_COUNTER_INIT(m_boostOnTimeMs);

			return;
		}
		else
		{
			return;
		}
	}
	else
	{
		// Turn off the boost converter
		IODRV_SetPin(IODRV_PIN_POW_EN, false);

		MS_TIME_COUNTER_INIT(m_lastRPiPowerDetectTimeMs);

		m_boostConverterEnabled = false;

		POWERSOURCE_SetLDOEnable(false);

		return;
	}
}


// ****************************************************************************
/*!
 * POWERSOURCE_SetLDOEnable enables or disables the LDO regulator, the actual
 * operation is overridden by the configuration that has been set and if the
 * boost converter is enabled. If the configuration is set to LDO or POWER_DET
 * and the boost converter is enabled then the enabled parameter is respected.
 *
 * @param	enabled		true attempts to turn on the LDO
 * 						false attempts to turn off the LDO
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_SetLDOEnable(const bool enabled)
{
	if (m_powerRegulatorConfig == POW_REGULATOR_MODE_DCDC)
	{
		m_ldoEnabled = false;
	}
	else
	{
		// Power det and LDO enabled are the same thing
		// LDO should not be on if the boost isn't on.
		m_ldoEnabled = m_boostConverterEnabled;
	}

	IODRV_SetPin(IODRV_PIN_POWDET_EN, m_ldoEnabled);
}


// ****************************************************************************
/*!
 * POWERSOURCE_IsVsysEnabled gets the current state of the VSys output.
 *
 * @param	none
 * @retval	bool		false = VSys output is off
 * 						true = VSys output is on
 */
// ****************************************************************************
bool POWERSOURCE_IsVsysEnabled(void)
{
	return m_vsysEnabled;
}


// ****************************************************************************
/*!
 * POWERSOURCE_IsBoostConverterEnabled gets the current enable state of the boost
 * converter
 *
 * @param	none
 * @retval	bool		false = boost converter is disabled
 * 						true = boost converter is enabled
 */
// ****************************************************************************
bool POWERSOURCE_IsBoostConverterEnabled(void)
{
	return m_boostConverterEnabled;
}


// ****************************************************************************
/*!
 * POWERSOURCE_IsLDOEnabled gets the current enable state of the LDO converter.
 *
 * @param	none
 * @retval	bool		false = LDO disabled
 * 						true = LDO enabled
 */
// ****************************************************************************
bool POWERSOURCE_IsLDOEnabled(void)
{
	return m_ldoEnabled;
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetVInStatus	gets the current state of the VIn power input to the
 * charger IC. It is derived from the charger module.
 *
 * @param	none
 * @retval	PowerSourceStatus_T		current state of the VIn power input
 */
// ****************************************************************************
PowerSourceStatus_T POWERSOURCE_GetVInStatus(void)
{
	return m_powerInStatus;
}


// ****************************************************************************
/*!
 * POWERSOURCE_Get5VRailStatus gets the current state of the 5V rail connected
 * to the RPi. This will have been determined by attempting to work out if the
 * RPi is self powered, something that is not possible to do if the LDO converter
 * is not enabled by the regulator configuration set to boost converter only.
 * In this case the return will be not present status even though it may well be.
 *
 * Note: This does not detect the status of the output to the RPi.
 *
 * @param	none
 * @retval	PowerSourceStatus_T		current state of the 5V RPi power input
 */
// ****************************************************************************
PowerSourceStatus_T POWERSOURCE_Get5VRailStatus(void)
{
	return m_power5vIoStatus;
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetRPi5VPowerStatus returns the detection status of the RPi 5V
 * power input. This has a subtle difference to Get5VRail status in that it
 * indicates the internal state of the detection routine and gives further insight
 * into the 5V rail status.
 *
 * @param	none
 * @retval	POWERSOURCE_RPi5VStatus_t
 */
// ****************************************************************************
POWERSOURCE_RPi5VStatus_t POWERSOURCE_GetRPi5VPowerStatus(void)
{
	return m_rpi5VInDetStatus;
}


// ****************************************************************************
/*!
 * POWERSOURCE_NeedPoll tells the task manager that something needs an update
 * within this module.
 *
 * @param	none
 * @retval	bool		false = module does not require update
 * 						true -= module requires update
 */
// ****************************************************************************
bool POWERSOURCE_NeedPoll(void)
{
	return ( (m_rpi5VInDetStatus == RPI5V_DETECTION_STATUS_POWERED) ||
			(MS_TIME_COUNT(m_boostOnTimeMs) < POWERSOURCE_STABLISE_TIME_MS)
			);
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetForcedPowerOffStatus gets the state of the forced power off
 * flag. This will be set in the event of a power cycle reset and the boost
 * converter detected as previously enabled or if the battery voltage has
 * drooped below its minimum threshold.
 *
 * @param	none
 * @retval	bool		false = boost converter not enabled on power reset
 * 						true = boost converter detected enabled on power reset
 */
// ****************************************************************************
bool POWERSOURCE_GetForcedPowerOffStatus(void)
{
	return m_forcedPowerOff;
}


// ****************************************************************************
/*!
 * POWERSOURCE_GetForcedVSysOutputOffStatus gets the current state of the forced
 * VSys off flag. This would be set if running on battery power and the battery
 * voltage has drooped below the minimum threshold.
 *
 * @param	none
 * @retval	bool		false = VSys output not forced off
 * 						true = VSys output forced off
 */
// ****************************************************************************
bool POWERSOURCE_GetForcedVSysOutputOffStatus(void)
{
	return m_forcedVSysOutputOff;
}


// ****************************************************************************
/*!
 * POWERSOURCE_ClearForcedPowerOff clears the forced power off flag
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_ClearForcedPowerOff(void)
{
	m_forcedPowerOff = false;
}


// ****************************************************************************
/*!
 * POWERSOURCE_ClearForcedVSysOutputOff clears the forced VSys output off flag
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void POWERSOURCE_ClearForcedVSysOutputOff(void)
{
	m_forcedVSysOutputOff = false;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * POWERSOURCE_CheckPowerValid performs check on the power input to ensure system
 * is operating in the correct mode.
 *
 * If the power is enabled to the RPi and either of the 3v3 rail or the 5v rail
 * droops then the boost converter is disabled to prevent damage to the system
 * from a large load or fault within the boost converter.
 *
 * If the RPi is powered and the battery voltage droops below the minimum threshold
 * the VSys output is first turned off followed by the boost converter. The forced
 * power off flag is set and the forced VSys output off flags are set.
 *
 * If the RPi is not powered but the VSys output is on and the battery voltage
 * droops below the minimum threshold then the VSys output is turned off and the
 * forced VSsys output flag is set.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void POWERSOURCE_CheckPowerValid(void)
{
	const uint16_t v5RailMv = ANALOG_Get5VRailMv();
	const uint16_t aVddMv = ANALOG_GetAVDDMv();
	const uint16_t vBattMv = ANALOG_GetBatteryMv();
	const uint16_t batteryADCValue = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);
	const bool chargerHasPowerIn = CHARGER_IsChargeSourceAvailable();
	const uint32_t sysTime = HAL_GetTick();

	if ( true == m_boostConverterEnabled )
	{
		if (false == MS_TIMEREF_TIMEOUT(m_boostOnTimeMs, sysTime, POWERSOURCE_STABLISE_TIME_MS))
		{
			return;
		}

		if ( (v5RailMv < 2000u) && (aVddMv > 2500u) )
		{
			//5V DCDC is in fault overcurrent state, turn it off to prevent draining battery
			POWERSOURCE_Set5vBoostEnable(false);
			m_forcedPowerOff = true;

			return;
		}

		// if no sources connected, turn off 5V regulator and system switch when battery voltage drops below minimum
		if ( (batteryADCValue < m_vbatPowerOffThreshold)
				&& (m_rpi5VInDetStatus != RPI5V_DETECTION_STATUS_POWERED)
				&& (false == chargerHasPowerIn)
				)
		{
			if (true == m_vsysEnabled)
			{
				// Turn off Vsys and set forced off flag
				IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
				m_vsysEnabled = false;
				m_forcedVSysOutputOff = true;

				// Leave 2 ms for switch to react
				MS_TIME_COUNTER_INIT(forcedPowerOffCounter);

				return;
			}
			else if ( MS_TIME_COUNT(forcedPowerOffCounter) >= 2u)
			{

				POWERSOURCE_SetLDOEnable(false);
				POWERSOURCE_Set5vBoostEnable(false);

				m_forcedPowerOff = true;
				POWERMAN_SetWakeupOnChargePcntPt1(5u); // schedule wake up when power is applied

				return;
			}
		}
	}
	else
	{
		// If running on battery with VSys on (RPi is off!) turn off Vsys when it gets too low
		if ( (false == POWER_SOURCE_PRESENT)
				&& (vBattMv < m_vbatPowerOffThreshold)
				&& (true == m_vsysEnabled)
				)
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
			m_forcedVSysOutputOff = true;

			return;
		}
	}
}


// ****************************************************************************
/*!
 * POWERSOURCE_Process5VRailPower controls the behaviour of the 5V rail. If the
 * battery voltage is low and no charging source available then the 5v RPi voltage
 * output will be turned off. If the boost converter has just been enabled then
 * the LDO will be attempted to be turned on (dependent on configuration).
 * If the RPi is charging the battery and the voltage droops then the charge
 * rate will be backed off.
 *
 * Note: There is similar operation here with the check power valid routine
 * and probably should be sorted out.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
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
			m_forcedPowerOff = true;
		}
	}


	if (MS_TIMEREF_TIMEOUT(m_boostOnTimeMs, sysTime, 95u))
	{
		// Once the dcdc has settled enable the LDO.
		// Note: Will only enable if it has been configured so.
		POWERSOURCE_SetLDOEnable(true);
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


// ****************************************************************************
/*!
 * POWERSOURCE_RPi5vDetect attempts to determine if the 5v rail comes from the
 * RPi. This basically just checks the voltage level is above the LDO voltage if
 * it is enabled or if the voltage detected on the 5V rail is above a minimum
 * threshold. If the boost converter is enabled and the LDO is not then it is
 * not possible to determine the power status of the RPi.
 *
 * Note: The routine seems over complicated and should be refactored in context
 * of the rest of the module.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void POWERSOURCE_RPi5vDetect(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const uint16_t v5RailMv = ANALOG_Get5VRailMv();
	const POWERSOURCE_RPi5VStatus_t lastStatus = m_rpi5VInDetStatus;

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

	MS_TIMEREF_INIT(m_lastRPiPowerDetectTimeMs, sysTime);

	// If the boost converter is enabled
	if ( (true == m_boostConverterEnabled) )
	{
		m_rpiVLowCount = 0u;

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
	}
	else if (v5RailMv > POWERSOURCE_RPI_UNDER_MV)
	{
		if ( (RPI5V_DETECTION_STATUS_POWERED != m_rpi5VInDetStatus)
				&& (true == MS_TIMEREF_TIMEOUT(m_rpiDetTimeMs, sysTime, POWERSOURCE_RPI5V_POWERED_MS)) )
		{
			// If the voltage on the rail is more than the LDO can supply then the RPi is probably powered
			m_rpi5VInDetStatus = RPI5V_DETECTION_STATUS_POWERED;

			// Enable the boost converter
			POWERSOURCE_Set5vBoostEnable(true);
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


	// Make the charger do some work too.
	if (lastStatus != m_rpi5VInDetStatus)
	{
		CHARGER_RefreshSettings();
	}
}


// ****************************************************************************
/*!
 * POWERSOURCE_ProcessVINStatus determines the state of the VIn power source to
 * the charger IC and updates the powerInStatus. If the Vin status is weak or
 * over voltage the status is set to BAD, if the DPM is active within the charger
 * then the VIn is detected as week. If undervoltage then the status is detected
 * as not present.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void POWERSOURCE_ProcessVINStatus(void)
{
	const CHARGER_InputStatus_t vinStatus = CHARGER_GetInputStatus(CHARGER_INPUT_VIN);
	const uint8_t chargerDPMActive = CHARGER_IsDPMActive();

	if (CHARGER_INPUT_UVP == vinStatus)
	{
		m_powerInStatus = POW_SOURCE_NOT_PRESENT;
	}
	else if ( (CHARGER_INPUT_OVP == vinStatus) || (CHARGER_INPUT_WEAK == vinStatus) )
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


// ****************************************************************************
/*!
 * POWERSOURCE_Process5VRailStatus determines the state of the RPi 5V input based
 * on the detection state and the charger input. The detection state of anything
 * but powered will set the source state to not present, it could still be powered
 * but because the LDO is not enabled it is not possible to detect the voltage
 * level. If powered and charger detects UVP then the source is set to not powered,
 * this could be due to the charger not being allowed to use the 5v (USB) input
 * and the detection state will indicate the true state of the input. An over
 * voltage or input weak detection will set status as BAD, if the charge rate has
 * been reduced because of a power droop the state will be set to weak.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void POWERSOURCE_Process5VRailStatus(void)
{
	const bool rpi5vChargeEnable = CHARGER_GetRPi5vInputEnable();
	const CHARGER_InputStatus_t rpi5vChargeStatus = CHARGER_GetInputStatus(CHARGER_INPUT_RPI);


	if (RPI5V_DETECTION_STATUS_POWERED != m_rpi5VInDetStatus)
	{
		m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
	}
	else
	{
		m_power5vIoStatus = POW_SOURCE_NORMAL;

		if (true == rpi5vChargeEnable)
		{
			if (CHARGER_INPUT_UVP == rpi5vChargeStatus)
			{
				m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
			}
			else if ( (CHARGER_INPUT_OVP == rpi5vChargeStatus) ||
						(CHARGER_INPUT_WEAK == rpi5vChargeStatus)
					)
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
			if (0u != m_rpiVLowCount)
			{
				m_power5vIoStatus = POW_SOURCE_WEAK;
			}
			else
			{
				m_power5vIoStatus = POW_SOURCE_NORMAL;
			}
		}
	}
}
