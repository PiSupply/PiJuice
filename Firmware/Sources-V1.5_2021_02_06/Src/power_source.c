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
//#define POW_5V_DET_LDO_EN_STATUS()		(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_SET)
#define POW_SOURCE_PRESENT()			(m_powerInStatus==POW_SOURCE_NORMAL || m_powerInStatus==POW_SOURCE_WEAK || m_power5vIoStatus==POW_SOURCE_NORMAL || m_power5vIoStatus==POW_SOURCE_WEAK)

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void PowerSourceTask(void *argument);
static void PowerSource5vIoDetectionTask(void *argument);

static osThreadId_t powSourceTaskHandle, detectGpioPowTaskHandle;

static const osThreadAttr_t powSourceTask_attributes = {
	.name = "powSourceTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 128
};
static const osThreadAttr_t detectGpioPowTask_attributes = {
	.name = "detectGpioPowTask",
	.priority = (osPriority_t) osPriorityHigh,
	.stack_size = 256
};
#endif

uint8_t forcedPowerOffFlag __attribute__((section("no_init")));
uint8_t forcedVSysOutputOffFlag __attribute__((section("no_init")));

extern uint8_t resetStatus;
extern uint16_t wakeupOnCharge;

volatile int32_t adcDmaPos = -1;

uint8_t pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_UNKNOWN;
static uint16_t m_vbatPowerOffThreshold;

uint8_t pow5VChgLoadMaximumReached = 0;
uint32_t pow5vPresentCounter;

//static uint32_t delayedInitTimeCount;

static uint32_t pow5vDetTimeCount;

uint32_t pow5vOnTimeout __attribute__((section("no_init")));

static uint32_t forcedPowerOffCounter __attribute__((section("no_init")));

static uint8_t m_vsysSwitchLimit __attribute__((section("no_init")));
static bool m_boostConverterEnabled;
static bool m_vsysEnabled;
static bool m_ldoEnabled;

static PowerSourceStatus_T m_powerInStatus = POW_SOURCE_NOT_PRESENT;
static PowerSourceStatus_T m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;

static void POWERSOURCE_ProcessVIN(const uint8_t chargerInStatus);
static void POWERSOURCE_Process5VRail(const uint8_t chargerUSBStatus);

PowerRegulatorConfig_T powerRegulatorConfig = POW_REGULATOR_MODE_POW_DET;

//volatile uint32_t adcWdTicks;
void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef* hadc){
	adcDmaPos = __HAL_DMA_GET_COUNTER(hadc->DMA_Handle); //hadc->DMA_Handle->Instance->CNDTR;
	//adcWdTicks = HAL_GetTick();
}

#define REGULATOR_5V_TURN_ON() \
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); \
	AnalogAdcWDGEnable(ENABLE); \
	AnalogPowerIsGood();



void POWERSOURCE_Init(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint32_t sysTime = HAL_GetTick();

	m_boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);
	m_vsysEnabled = IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN);

	uint16_t batVolt;
	uint16_t var = 0u;

	// initialize global variables after power-up
	if (!resetStatus)
	{
		forcedPowerOffFlag = 0u;
		forcedVSysOutputOffFlag = 0u;
		forcedPowerOffCounter = 0u;
		pow5vOnTimeout = UINT32_MAX;
	}

	MS_TIMEREF_INIT(pow5vPresentCounter, sysTime);
	MS_TIMEREF_INIT(pow5vDetTimeCount, sysTime);

	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);

	if ( UTIL_NV_ParamInitCheck_U16(var) )
	{
		uint8_t temp = (uint8_t)(var & 0xFFu);

		if (temp < POW_REGULATOR_MODE_COUNT)
		{
			powerRegulatorConfig = temp;
		}
	}

	// Set battery cut off threshold
	m_vbatPowerOffThreshold = (NULL != currentBatProfile) ?
				currentBatProfile->cutoffVoltage * (20u + VBAT_TURNOFF_ADC_THRESHOLD) :
				3000u + VBAT_TURNOFF_ADC_THRESHOLD;

	AnalogAdcWDGConfig(ANALOG_CHANNEL_VBAT,  m_vbatPowerOffThreshold);

	DelayUs(100u);

	batVolt = ANALOG_GetBatteryMv();

	// powerEnableState
	// maintain regulator state before reset
	if ( true == m_boostConverterEnabled )
	{
		// if there is mcu power-on, but reg was on, it can be power lost fault condition, check sources
		if (executionState == EXECUTION_STATE_POWER_RESET && batVolt < m_vbatPowerOffThreshold && CHARGER_INSTAT())
		{
			POWERSOURCE_Set5vBoostEnable(false);
			forcedPowerOffFlag = 1u;
			wakeupOnCharge = 5u; // schedule wake up when there is enough energy
		}
		else
		{
			IODRV_SetPin(IODRV_PIN_POW_EN, true);

			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();

			MS_TIMEREF_INIT(pow5vOnTimeout, sysTime);
		}
	}
	else
	{
		POWERSOURCE_SetLDOEnable(false);
		POWERSOURCE_Set5vBoostEnable(false);
	}

	if ( executionState == EXECUTION_STATE_POWER_ON || executionState == EXECUTION_STATE_POWER_RESET )
	{
		// after power-up state is 500mA limit
		//HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
		IODRV_SetPin(IODRV_PIN_ESYSLIM, true);
		m_vsysSwitchLimit = 5;
	}
	else
	{
		if (m_vsysSwitchLimit == 21 )
		{
			//HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1,  GPIO_PIN_RESET); // 2.1A limit value, is valid only after reset
			IODRV_SetPin(IODRV_PIN_ESYSLIM, false);
		}
		else
		{
			//HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
			IODRV_SetPin(IODRV_PIN_ESYSLIM, true);
		}
	}

	// vsysEnableState
	//if ( (GPIO_PIN_RESET == vsysEnableState) && (executionState != EXECUTION_STATE_POWER_ON) )
	if ( (true == m_vsysEnabled) && (EXECUTION_STATE_POWER_ON != executionState) )
	{
		if ( (EXECUTION_STATE_POWER_RESET == executionState)  && (batVolt < m_vbatPowerOffThreshold) && (CHARGER_INSTAT()) )
		{
			// Disable vsys
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);

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


// Does it need to have this value passed through??
void CheckMinimumPower(const uint16_t v5RailMv)
{
	const uint16_t aVddMv = ANALOG_GetAVDDMv();
	const uint16_t vBattMv = ANALOG_GetBatteryMv();
	const uint16_t batteryADCValue = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);

	if ( true == m_boostConverterEnabled )
	{
		if ( (v5RailMv < 2000u) && (aVddMv > 2500u) )
		{
			//5V DCDC is in fault overcurrent state, turn it off to prevent draining battery
			POWERSOURCE_Set5vBoostEnable(false);
			forcedPowerOffFlag = 1u;
			return;
		}

		// if no sources connected, turn off 5V regulator and system switch when battery voltage drops below minimum
		if ( (batteryADCValue < GetAdcWDGThreshold())
				&& (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT)
				&& (false != CHARGER_INSTAT())
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
			}
			else if ( MS_TIME_COUNT(forcedPowerOffCounter) >= 2u)
			{

				POWERSOURCE_SetLDOEnable(false);
				POWERSOURCE_Set5vBoostEnable(false);

				forcedPowerOffFlag = 1u;
				wakeupOnCharge = 5u; // schedule wake up when power is applied
			}
		}
	}
	else
	{
		// If running on battery turn off Vsys when it gets too low
		if ( (false == POW_SOURCE_PRESENT())
				&& (vBattMv < m_vbatPowerOffThreshold)
				&& (true == m_vsysEnabled)
				)
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
			forcedVSysOutputOffFlag = 1u;
		}
	}
}


void POWERSOURCE_5VIoDetection_Task(void)
{

	const uint16_t v5RailMv = ANALOG_Get5VRailMv();
	const uint16_t vBattMv = ANALOG_GetBatteryMv();

	uint16_t powdetADCValue;

	uint8_t i;

	uint8_t fetCutoffCount = 0;
	uint8_t fetActiveCount = 0;

	// Check to see if the boost converter has just switched on.
	if ( MS_TIME_COUNT(pow5vOnTimeout) < POW_5V_TURN_ON_TIMEOUT )
	{
		// Not sure about this, is it looking to see if the power source has been removed?
		if ( (false == POW_SOURCE_PRESENT()) && MS_TIME_COUNT(pow5vOnTimeout) > 0u)
		{
			if (vBattMv < m_vbatPowerOffThreshold)
			{
				POWERSOURCE_Set5vBoostEnable(false);
				forcedPowerOffFlag = 1;
			}
		}

		// Wait for 5V to become stable after turn on timeout
		return;
	}

	CheckMinimumPower(v5RailMv);

	// If the boost converter is enabled
	if (true == m_boostConverterEnabled)
	{
		if (true == m_ldoEnabled)
		{
			powdetADCValue = ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET);

			if ( powdetADCValue < POW_5V_IO_DET_ADC_THRESHOLD)
			{
				if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT)
				{
					MS_TIME_COUNTER_INIT(pow5vPresentCounter);
					ChargerUsbInCurrentLimitStepDown();
				}

				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;

				if (pow5VChgLoadMaximumReached > 1)
				{
					pow5VChgLoadMaximumReached --;
				}

				ChargerSetUSBLockout(CHG_USB_IN_LOCK);
				POWERSOURCE_SetLDOEnable(false);
			}
		}
		else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT)
		{
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_UNKNOWN;

			if (pow5VChgLoadMaximumReached > 1u)
			{
				pow5VChgLoadMaximumReached --;
			}

			ChargerSetUSBLockout(CHG_USB_IN_LOCK);
			ChargerUsbInCurrentLimitStepDown();
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
		}

		if ( pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && MS_TIME_COUNT(pow5vDetTimeCount) > 95u )
		{
			MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

			if (false == m_ldoEnabled)
			{
				POWERSOURCE_SetLDOEnable(true);
			}

			// find out if PMOS goes to cutoff or active state
			i = 200u;

			while ( (i-- > 0u) && (fetActiveCount < 3u) )
			{
				// Was 120us but the ADC doesn't read that fast now!
				DelayUs(1000u);

				powdetADCValue = ADC_GetInstantValue(ANALOG_CHANNEL_POW_DET);

			    if (powdetADCValue >= POW_5V_IO_DET_ADC_THRESHOLD)
			    {
			    	fetCutoffCount++;
			    	fetActiveCount = 0;
			    }
			    else
			    {
			    	fetCutoffCount = 0;
			    	fetActiveCount++;
			    }

			    CheckMinimumPower(v5RailMv);
			}

			if (fetCutoffCount >= 200u)
			{
				// turn on usb in if pmos is cutoff
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
				ChargerSetUSBLockout(CHG_USB_IN_UNLOCK);
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			}
			else
			{
				if (fetActiveCount >= 3)
				{
					MeasurePMOSLoadCurrent();
					pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				}

				POWERSOURCE_SetLDOEnable(false);
			}
		}
	}
	else
	{
		// LDO regulator is set to 4.79V
		if (v5RailMv < 4800u)
		{
			if (POW_5V_IN_DETECTION_STATUS_NOT_PRESENT != pow5vInDetStatus)
			{
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
				ChargerUsbInCurrentLimitStepDown();
			}

			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_LOCK);

			if (pow5VChgLoadMaximumReached > 1)
			{
				pow5VChgLoadMaximumReached --;
			}

		}
		else if ( (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT) && (MS_TIME_COUNT(pow5vDetTimeCount) > 500u) )
		{
			REGULATOR_5V_TURN_ON(); //Turn5vBoost(1);
			POWERSOURCE_SetLDOEnable(true);
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_UNLOCK); // turn on charger in
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			//wakeupOnCharge = 0xFFFF;
		}
	}

	if (MS_TIME_COUNT(pow5vPresentCounter) > 800u)
	{
		MS_TIME_COUNTER_INIT(pow5vPresentCounter);

		if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT )
		{
			if (pow5VChgLoadMaximumReached > 1u)
			{
				ChargerUsbInCurrentLimitStepUp();
				pow5VChgLoadMaximumReached = 2u;
			}
			else if (pow5VChgLoadMaximumReached == 0u)
			{
				pow5VChgLoadMaximumReached = 3u;
			}
		}
		else if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_NOT_PRESENT)
		{
			// this means input is disconnected, and flag can be cleared
			pow5VChgLoadMaximumReached = 0u;
			ChargerUsbInCurrentLimitSetMin();
		}
	}
}





void POWERSOURCE_Task(void)
{
	POWERSOURCE_ProcessVIN(CHARGER_INSTAT());
	POWERSOURCE_Process5VRail(CHARGER_USBSTAT());
}


void PowerSourceSetBatProfile(const BatteryProfile_T * const batProfile)
{

	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();

	if (NULL == currentBatProfile)
	{
		m_vbatPowerOffThreshold = 3000u + VBAT_TURNOFF_ADC_THRESHOLD;
	}
	else
	{
		m_vbatPowerOffThreshold = (currentBatProfile->cutoffVoltage * 20u) + VBAT_TURNOFF_ADC_THRESHOLD;
	}

	//vbatPowOffTresh = (currentBatProfile != NULL) ? (uint16_t)(currentBatProfile->cutoffVoltage) * 20u + VBAT_TURNOFF_ADC_THRESHOLD : 3000u + VBAT_TURNOFF_ADC_THRESHOLD;
	AnalogAdcWDGConfig(ANALOG_CHANNEL_VBAT,  m_vbatPowerOffThreshold);
}


// 21 = on & 2.1A limit, 5 - on and 0.5A limit, anything else is off.
void POWERSOURCE_SetVSysSwitchState(uint8_t state)
{
	const uint16_t vbatAdcVal = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);
	const uint16_t wdgThreshold = GetAdcWDGThreshold();

	if (5u == state)
	{
		// Set VSys I limit pin
		IODRV_SetPin(IODRV_PIN_ESYSLIM, true);

		if ( (vbatAdcVal > wdgThreshold) || POW_SOURCE_PRESENT() )
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, true);
			m_vsysEnabled = true;
		}

		m_vsysSwitchLimit = state;
	}
	else if (21u == state)
	{
		// Reset VSys I limit pin
		IODRV_SetPin(IODRV_PIN_ESYSLIM, false);

		if ( (vbatAdcVal > wdgThreshold) || POW_SOURCE_PRESENT() )
		{
			IODRV_SetPin(IODRV_PIN_EXTVS_EN, true);
			m_vsysEnabled = true;
		}

		m_vsysSwitchLimit = state;
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

void SetPowerRegulatorConfigCmd(uint8_t data[], uint8_t len)
{
	uint8_t temp;
	uint16_t var = 0u;

	if ( data[0] >= (uint8_t)POW_REGULATOR_MODE_COUNT )
	{
		return;
	}

	EE_WriteVariable(POWER_REGULATOR_CONFIG_NV_ADDR, data[0] | ((uint16_t)(~data[0])<<8));

	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);
	if (UTIL_NV_ParamInitCheck_U16(var))
	{
		temp = (uint8_t)(var & 0xFFu);

		if (temp < POW_REGULATOR_MODE_COUNT)
		{
			powerRegulatorConfig = temp;
		}
	}
}

void GetPowerRegulatorConfigCmd(uint8_t data[], uint16_t *len) {
	data[0] = powerRegulatorConfig;
	*len = 1;
}


// Return value never used!
void POWERSOURCE_Set5vBoostEnable(const bool enabled)
{

	uint16_t vBattMv = ANALOG_GetBatteryMv();
	uint8_t retrys = 2u;

	if (true == enabled)
	{
		if (true == m_boostConverterEnabled)
		{
			// Already there.
			return;
		}

		if ( (vBattMv > m_vbatPowerOffThreshold) || (false != POW_SOURCE_PRESENT()) )
		{
			// Turn off LDO
			POWERSOURCE_SetLDOEnable(false);

			AnalogAdcWDGEnable(DISABLE);

			// Wait for it to happen
			DelayUs(5);

			// Turn on boost converter
			IODRV_SetPin(IODRV_PIN_POW_EN, true);

			if (false == POW_SOURCE_PRESENT())
			{
				while (retrys > 0u)
				{
					DelayUs(200u);

					vBattMv = ANALOG_GetBatteryMv();

					if (vBattMv > m_vbatPowerOffThreshold)
					{
						break;
					}

					retrys--;
				}

				if (retrys == 0u)
				{
					IODRV_SetPin(IODRV_PIN_POW_EN, false);

					return;
				}
			}

			// Original code pulsed power off-5us-on

			m_boostConverterEnabled = true;

			// Stamp time boost converter is enabled
			MS_TIME_COUNTER_INIT(pow5vOnTimeout);

			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();

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
		MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

		m_boostConverterEnabled = false;

		return;
	}
}


void POWERSOURCE_SetLDOEnable(const bool enabled)
{
	if  (powerRegulatorConfig == POW_REGULATOR_MODE_DCDC)
	{
		m_ldoEnabled = false;
	}
	else if (powerRegulatorConfig == POW_REGULATOR_MODE_LDO)
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


void POWERSOURCE_ProcessVIN(const uint8_t chargerInStatus)
{
	if (chargerInStatus == 0x03u)
	{
		m_powerInStatus = POW_SOURCE_NOT_PRESENT;
	}
	else if ( (chargerInStatus == 0x01u) || (chargerInStatus == 0x02u) )
	{
		m_powerInStatus = POW_SOURCE_BAD;
	}
	else if (CHARGER_IS_DPM_MODE_ACTIVE())
	{
		m_powerInStatus = POW_SOURCE_WEAK;
	}
	else
	{
		m_powerInStatus = POW_SOURCE_NORMAL;
	}
}


void POWERSOURCE_Process5VRail(const uint8_t chargerUSBStatus)
{
	if (usbInLockoutStatus == CHG_USB_IN_UNLOCK)
	{
		if (chargerUSBStatus == 0x03u)
		{
			m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
		}
		else if ( chargerUSBStatus == 0x01u || chargerUSBStatus == 0x02u)
		{
			m_power5vIoStatus = POW_SOURCE_BAD;
		}
		else if (1u == pow5VChgLoadMaximumReached)
		{
			m_power5vIoStatus = POW_SOURCE_WEAK;
		}
		else
		{
			m_power5vIoStatus = POW_SOURCE_NORMAL;
		}
	}
	else if (POW_5V_IN_DETECTION_STATUS_PRESENT != pow5vInDetStatus)
	{
		m_power5vIoStatus = POW_SOURCE_NOT_PRESENT;
	}
	else
	{
		m_power5vIoStatus = POW_SOURCE_NORMAL;
	}
}
