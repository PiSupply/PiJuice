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


#define POW_5V_IO_DET_ADC_THRESHOLD		2950
#define VBAT_TURNOFF_ADC_THRESHOLD		0 // mV unit
#define POW_5V_DET_LDO_EN_STATUS()		(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11) == GPIO_PIN_SET)
#define POW_SOURCE_PRESENT()			(powerInStatus==POW_SOURCE_NORMAL || powerInStatus==POW_SOURCE_WEAK || power5vIoStatus==POW_SOURCE_NORMAL || power5vIoStatus==POW_SOURCE_WEAK)

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

PowerSourceStatus_T powerInStatus = POW_SOURCE_NOT_PRESENT;
PowerSourceStatus_T power5vIoStatus = POW_SOURCE_NOT_PRESENT;

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


// power ldo enable
// Doesn't make a lot of sense yet, is this enabling the LDO or not?
void POW_5V_DET_LDO_ENABLE(uint8_t enabled)
{
	const bool boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);

	if  (powerRegulatorConfig == POW_REGULATOR_MODE_DCDC)
	{
		// Turn off the linear regulator
		//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
		IODRV_SetPin(IODRV_PIN_POWDET_EN, false);
	}
	else if (powerRegulatorConfig == POW_REGULATOR_MODE_LDO)
	{
		if (true == boostConverterEnabled)
		{
			// Turn on the boost converter
			//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
			IODRV_SetPin(IODRV_PIN_POW_EN, true);
		}
		else
		{
			// Turn off the boost converter
			//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
			IODRV_SetPin(IODRV_PIN_POW_EN, false);
		}
	}
	else
	{
		// Turn on the boost converter based on enable
		IODRV_SetPin(IODRV_PIN_POW_EN, enabled);
		//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, enabled?GPIO_PIN_SET:GPIO_PIN_RESET);
	}
}

void Power5VSetModeLDO(void) {
	POW_5V_DET_LDO_ENABLE(1);
}

uint8_t Retry5VTurnOn(void)
{
	uint16_t batVolt;
	uint8_t n = 5;

	while(n-- > 0u)
	{
		DelayUs(200);
		// Check battery voltage
		batVolt = ANALOG_GetBatteryMv();

		if ( (false == POW_SOURCE_PRESENT() ) && (batVolt < m_vbatPowerOffThreshold) )
		{
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
			forcedPowerOffFlag = 1;
			return REGULATOR_5V_SWITCHING_STATUS_NO_ENERGY;
		}
	}

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	DelayUs(5u);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

	return REGULATOR_5V_SWITCHING_STATUS_SUCCESS;
}


// Return value never used!
bool POWERSOURCE_Set5vBoostEnable(bool enabled)
{
	uint8_t status;
	const bool boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);

	if (true == enabled)
	{
		if (true == boostConverterEnabled)
		{
			return true;
		}

		if ( (batteryVoltage > m_vbatPowerOffThreshold) || (false != POW_SOURCE_PRESENT()) )
		{

			POW_5V_DET_LDO_ENABLE(0);
			AnalogAdcWDGEnable(DISABLE);

			DelayUs(5);
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

			// Retry turn on in case of large capacitive load
			status = Retry5VTurnOn();

			if (status != REGULATOR_5V_SWITCHING_STATUS_SUCCESS)
			{
				return false;
			}

			status = Retry5VTurnOn();

			if (status != REGULATOR_5V_SWITCHING_STATUS_SUCCESS)
			{
				return false;
			}

			MS_TIME_COUNTER_INIT(pow5vOnTimeout);

			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		POW_5V_DET_LDO_ENABLE(0);
		AnalogAdcWDGEnable(DISABLE);

		// Turn off the boost converter
		//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
		IODRV_SetPin(IODRV_PIN_POW_EN, false);

		MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

		return true;
	}
}

__STATIC_INLINE void TurnVSysOutput(uint8_t onOff){
	if (onOff){
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	}
}


void PowerSourceInit(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint32_t sysTime = HAL_GetTick();
	const bool powerEnableState = IODRV_ReadPinValue(IODRV_PIN_POW_EN);
	const bool vsysEnableState = IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN);

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
	if ( true == powerEnableState )
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
			//HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

			IODRV_SetPin(IODRV_PIN_POW_EN, true);

			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();
			MS_TIMEREF_INIT(pow5vOnTimeout, sysTime);
		}
	}
	else
	{
		POW_5V_DET_LDO_ENABLE(0);
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
	if ( (true == vsysEnableState) && (EXECUTION_STATE_POWER_ON != executionState) )
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
	const bool boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);
	const bool vsysEnabled = IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN);

	if ( true == boostConverterEnabled )
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
			if (true == vsysEnabled)
			{
				IODRV_SetPin(IODRV_PIN_EXTVS_EN, false);
				forcedVSysOutputOffFlag = 1;
				MS_TIME_COUNTER_INIT(forcedPowerOffCounter); // leave 2 ms for switch to react
			}
			else if ( MS_TIME_COUNT(forcedPowerOffCounter) >= 2u)
			{
				POW_5V_DET_LDO_ENABLE(0);
				POWERSOURCE_Set5vBoostEnable(false);

				forcedPowerOffFlag = 1u;
				wakeupOnCharge = 5u; // schedule wake up when power is applied
			}
		}
	}
	else
	{
		if ( (false == POW_SOURCE_PRESENT())
				&& (vBattMv < m_vbatPowerOffThreshold)
				&& (true == vsysEnabled)
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
	const bool boostConverterEnabled = IODRV_ReadPinValue(IODRV_PIN_POW_EN);

	uint16_t powdetADCValue;


	if ( MS_TIME_COUNT(pow5vOnTimeout) < POW_5V_TURN_ON_TIMEOUT )
	{
		if ( (!POW_SOURCE_PRESENT()) && MS_TIME_COUNT(pow5vOnTimeout) > 0)
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

	if ( true == boostConverterEnabled )
	{
		if (POW_5V_DET_LDO_EN_STATUS())
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
				POW_5V_DET_LDO_ENABLE(0u);
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

			if (!POW_5V_DET_LDO_EN_STATUS())
			{
				POW_5V_DET_LDO_ENABLE(1);
			}

			// find out if PMOS goes to cutoff or active state
			volatile uint8_t i = 200, fetCutoffCount = 0, fetActiveCount = 0;

			volatile uint32_t sam = 0;

			while ( (i-- > 0u) && (fetActiveCount < 3u) )
			{
				// Was 120us but the ADC doesn't read that fast now!
				DelayUs(1000u);

				powdetADCValue = ADC_GetInstantValue(ANALOG_CHANNEL_POW_DET);

			    if (powdetADCValue >= POW_5V_IO_DET_ADC_THRESHOLD)
			    {
			    	fetCutoffCount++;
			    }
			    else
			    {
			    	fetCutoffCount = 0;
			    }

			    if (sam < POW_5V_IO_DET_ADC_THRESHOLD)
			    {
			    	fetActiveCount ++;
			    }
			    else
			    {
			    	fetActiveCount = 0;
			    }

			    CheckMinimumPower(v5RailMv);
			}

			if (fetCutoffCount >= 200u)
			{//if ( sam >= POW_5V_IO_DET_ADC_THRESHOLD ) {
				// turn on usb in if pmos is cutoff
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
				ChargerSetUSBLockout(CHG_USB_IN_UNLOCK);
				//pow5vIoLoadCurrent = 0;
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			}
			else
			{
				if (fetActiveCount >= 3)
				{
					MeasurePMOSLoadCurrent();//pow5vIoLoadCurrent = GetLoadCurrent();
					pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				}

				POW_5V_DET_LDO_ENABLE(0u);
			}
		}
	}
	else
	{
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
			POW_5V_DET_LDO_ENABLE(1);
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


void PowerSourceTask(void) {

	uint8_t powstat = CHARGER_INSTAT();
	if (powstat == 0x03) {
		powerInStatus = POW_SOURCE_NOT_PRESENT;
	} else if (powstat == 0x01 || powstat == 0x02) {
		powerInStatus = POW_SOURCE_BAD;
	} else if (CHARGER_IS_DPM_MODE_ACTIVE()) {
		powerInStatus = POW_SOURCE_WEAK;
	} else {
		powerInStatus = POW_SOURCE_NORMAL;
	}

	if (usbInLockoutStatus == CHG_USB_IN_UNLOCK) {
		powstat = CHARGER_USBSTAT();
		if (/*!CHARGER_IS_USBIN_LOCKED() ||*/ powstat == 0x03) {
			power5vIoStatus = POW_SOURCE_NOT_PRESENT;
		} else if ( powstat == 0x01 || powstat == 0x02) {
			power5vIoStatus = POW_SOURCE_BAD;
		} else if (/*CHARGER_IS_DPM_MODE_ACTIVE() &&*/ pow5VChgLoadMaximumReached == 1 ) {
			power5vIoStatus = POW_SOURCE_WEAK;
		} else {
			power5vIoStatus = POW_SOURCE_NORMAL;
		}
	} else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT) {
		power5vIoStatus = POW_SOURCE_NOT_PRESENT;
	} else {
		power5vIoStatus = POW_SOURCE_NORMAL;
	}
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


void PowerSourceSetVSysSwitchState(uint8_t state)
{
	const uint16_t vbatAdcVal = ADC_GetAverageValue(ANALOG_CHANNEL_VBAT);
	const uint16_t wdgThreshold = GetAdcWDGThreshold();

	if (5u == state)
	{
		// Set VSys I limit pin
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);

		if ( (vbatAdcVal > wdgThreshold) || POW_SOURCE_PRESENT() )
		{
			TurnVSysOutput(1u);
		}

		m_vsysSwitchLimit = state;
	}
	else if (21u == state)
	{
		// Reset VSys I limit pin
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);

		if ( (vbatAdcVal > wdgThreshold) || POW_SOURCE_PRESENT() )
		{
			TurnVSysOutput(1u);
		}

		m_vsysSwitchLimit = state;
	}
	else
	{
		TurnVSysOutput(0u);
	}

	forcedVSysOutputOffFlag = 0u;
}

uint8_t PowerSourceGetVSysSwitchState()
{
	if (IODRV_ReadPinValue(IODRV_PIN_EXTVS_EN))
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
