/*
 * power_source.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "power_source.h"
#include "charger_bq2416x.h"
#include "stm32f0xx_hal.h"
#include "nv.h"
#include "analog.h"
#include "fuel_gauge_lc709203f.h"
#include "time_count.h"
#include "load_current_sense.h"
#include "execution.h"

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
static uint16_t vbatPowOffTresh;

uint8_t pow5VChgLoadMaximumReached = 0;
uint32_t pow5vPresentCounter;

//static uint32_t delayedInitTimeCount;

static uint32_t pow5vDetTimeCount;

uint32_t pow5vOnTimeout __attribute__((section("no_init")));

static uint32_t forcedPowerOffCounter __attribute__((section("no_init")));

volatile uint8_t vsysSwitchLimit __attribute__((section("no_init")));

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
__STATIC_INLINE void POW_5V_DET_LDO_ENABLE(uint8_t enabled) {
	if  (powerRegulatorConfig == POW_REGULATOR_MODE_DCDC) {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	} else if (powerRegulatorConfig == POW_REGULATOR_MODE_LDO) {
		if (POW_5V_BOOST_EN_STATUS())
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);
		else
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, enabled?GPIO_PIN_SET:GPIO_PIN_RESET);
	}
}

void Power5VSetModeLDO(void) {
	POW_5V_DET_LDO_ENABLE(1);
}

__STATIC_INLINE int Retry5VTurnOn() {
	int n = 5;
	while(n--) {
		DelayUs(200);
		// Check battery voltage
		volatile int16_t batVolt = (GetSample(ADC_VBAT_SENS_CHN) * aVdd * 11) >> 15; // 4096 * 1374 / 1000
		if ( (!POW_SOURCE_PRESENT()) && batVolt < vbatPowOffTresh) {
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
			forcedPowerOffFlag = 1;
			return REGULATOR_5V_SWITCHING_STATUS_NO_ENERGY;
		}
	}

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
	DelayUs(5);
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

	return REGULATOR_5V_SWITCHING_STATUS_SUCCESS;
}

int8_t Turn5vBoost(uint8_t onOff) {
	if (onOff) {

		if (POW_5V_BOOST_EN_STATUS()) return 0;

		if ( batteryVoltage > vbatPowOffTresh || POW_SOURCE_PRESENT()/*chargerStatus != CHG_NO_VALID_SOURCE*/) {

			POW_5V_DET_LDO_ENABLE(0);
			AnalogAdcWDGEnable(DISABLE);

			DelayUs(5);
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);

			// Retry turn on in case of large capacitive load
			int status = Retry5VTurnOn();
			if (status != REGULATOR_5V_SWITCHING_STATUS_SUCCESS) return status;

			status = Retry5VTurnOn();
			if (status != REGULATOR_5V_SWITCHING_STATUS_SUCCESS) return status;

			MS_TIME_COUNTER_INIT(pow5vOnTimeout);

			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();

			return REGULATOR_5V_SWITCHING_STATUS_SUCCESS;
		} else {
			return REGULATOR_5V_SWITCHING_STATUS_NO_ENERGY;
		}
	} else {
		POW_5V_DET_LDO_ENABLE(0);
		AnalogAdcWDGEnable(DISABLE);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
		MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

		return REGULATOR_5V_SWITCHING_STATUS_SUCCESS;
	}
}

__STATIC_INLINE void TurnVSysOutput(uint8_t onOff){
	if (onOff){
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);
	}
}

void PowerSourceInit(void) {

	// initialize global variables after power-up
	if (!resetStatus) {
		forcedPowerOffFlag = 0;
		forcedVSysOutputOffFlag = 0;
		forcedPowerOffCounter = 0;
		pow5vOnTimeout = (uint32_t)0-1;
	}

	MS_TIME_COUNTER_INIT(pow5vPresentCounter);
	MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

	uint16_t var = 0;
	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);
	if (((~var)&0xFF) == (var>>8)) {
		uint8_t temp = var&0xFF;
		if (temp < POW_REGULATOR_MODE_END) {
			powerRegulatorConfig = temp;
		}
	}

	vbatPowOffTresh = currentBatProfile!=NULL ? (uint16_t)(currentBatProfile->cutoffVoltage)*20+VBAT_TURNOFF_ADC_THRESHOLD : (uint16_t)3000+VBAT_TURNOFF_ADC_THRESHOLD;
	AnalogAdcWDGConfig(ADC_VBAT_SENS_CHN,  vbatPowOffTresh);

	DelayUs(100);
	volatile uint16_t batVolt = GetSampleVoltage(ADC_VBAT_SENS_CHN)*(int32_t)1374/1000;

	// maintain regulator state before reset
	if ( HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_SET ) {
		// if there is mcu power-on, but reg was on, it can be power lost fault condition, check sources
		if (executionState == EXECUTION_STATE_POWER_RESET && batVolt < vbatPowOffTresh && CHARGER_INSTAT()) {
			Turn5vBoost(0);
			forcedPowerOffFlag = 1;
			wakeupOnCharge = 5; // schedule wake up when there is enough energy
		} else {
			HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
			AnalogAdcWDGEnable(ENABLE);
			AnalogPowerIsGood();
			MS_TIME_COUNTER_INIT(pow5vOnTimeout);
		}
	} else {
		POW_5V_DET_LDO_ENABLE(0);
		Turn5vBoost(0);
	}

	if ( executionState == EXECUTION_STATE_POWER_ON || executionState == EXECUTION_STATE_POWER_RESET ) {
		// after power-up state is 500mA limit
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
		vsysSwitchLimit = 5;
	} else {
		if (vsysSwitchLimit == 21 )
			HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1,  GPIO_PIN_RESET); // 2.1A limit value, is valid only after reset
		else
			HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
	}

	if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12) == GPIO_PIN_RESET && executionState != EXECUTION_STATE_POWER_ON ) { // after power-up vsys switch should be disabled
		if ( executionState == EXECUTION_STATE_POWER_RESET && batVolt < vbatPowOffTresh && CHARGER_INSTAT() ) {
			TurnVSysOutput(0);
			forcedVSysOutputOffFlag = 1;
		} else {
			TurnVSysOutput(1);
		}
	} else {
		TurnVSysOutput(0);
	}

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

#if defined(RTOS_FREERTOS)
	powSourceTaskHandle = osThreadNew(PowerSourceTask, (void*)NULL, &powSourceTask_attributes);
	detectGpioPowTaskHandle = osThreadNew(PowerSource5vIoDetectionTask, (void*)NULL, &detectGpioPowTask_attributes);
#endif
}

/*__STATIC_INLINE*/ void CheckMinimumPower(int16_t volt5) {
	if ( POW_5V_BOOST_EN_STATUS() ) {
		volatile int16_t tstv = volt5;
		if (volt5 < 2000 && aVdd > 2500) {
			//5V DCDC is in fault overcurrent state, turn it off to prevent draining battery
			Turn5vBoost(0);
			forcedPowerOffFlag = 1;
			return;
		}

		volatile uint16_t samt = GetSample(ADC_VBAT_SENS_CHN);
		// if no sources connected, turn off 5V regulator and system switch when battery voltage drops below minimum
		if ( samt < GetAdcWDGThreshold() && pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && CHARGER_INSTAT()) {
			if ( POW_VSYS_OUTPUT_EN_STATUS() ) {
				TurnVSysOutput(0);
				forcedVSysOutputOffFlag = 1;
				MS_TIME_COUNTER_INIT(forcedPowerOffCounter); // leave 2 ms for switch to react
			} else if ( MS_TIME_COUNT(forcedPowerOffCounter) >= 2) {
				POW_5V_DET_LDO_ENABLE(0);
				Turn5vBoost(0);
				forcedPowerOffFlag = 1;
				wakeupOnCharge = 5; // schedule wake up when power is applied
			}
		}
	} else {
		int16_t batVolt = (GetSample(ADC_VBAT_SENS_CHN) * aVdd * 11) >> 15; // 4096 * 1374 / 1000
		if ( (!POW_SOURCE_PRESENT() /*chargerStatus == CHG_NO_VALID_SOURCE*/) && batVolt < vbatPowOffTresh && POW_VSYS_OUTPUT_EN_STATUS()) {
			TurnVSysOutput(0);
			forcedVSysOutputOffFlag = 1;
		}
	}
}

#if defined(RTOS_FREERTOS)
static void PowerSource5vIoDetectionTask(void *argument) {
  for(;;)
  {
	CheckMinimumPower();

	if ( POW_5V_BOOST_EN_STATUS() ) {

		if (POW_5V_DET_LDO_EN_STATUS()) {
			volatile uint16_t samp = GetSample(POW_DET_SENS_CHN);
			if ( samp < POW_5V_IO_DET_ADC_THRESHOLD) {
				if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
					MS_TIME_COUNTER_INIT(pow5vPresentCounter);
					ChargerUsbInCurrentLimitStepDown();
				}
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
				ChargerSetUSBLockout(CHG_USB_IN_LOCK);
				POW_5V_DET_LDO_ENABLE(0);
			}
		} else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_UNKNOWN;
			if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
			ChargerSetUSBLockout(CHG_USB_IN_LOCK);
			ChargerUsbInCurrentLimitStepDown();
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
		}

		if ( pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && MS_TIME_COUNT(pow5vDetTimeCount) > 95 ) {
			MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

			if (!POW_5V_DET_LDO_EN_STATUS()) {
				POW_5V_DET_LDO_ENABLE(1);
				//Delay(100);
			}

			// find out if PMOS goes to cutoff or active state
			volatile uint8_t i = 200, fetCutoffCount = 0, fetActiveCount = 0;
			//volatile uint16_t activeSamples[6];
			volatile uint32_t sam = 0;
			while ( i-- && fetActiveCount < 3) {
				DelayUs(120);//HAL_Delay(2);//
			    sam = GetSample(POW_DET_SENS_CHN);//analogIn[ind];
			    if (sam >= POW_5V_IO_DET_ADC_THRESHOLD) fetCutoffCount ++;
			    else fetCutoffCount = 0;
			    if (sam < POW_5V_IO_DET_ADC_THRESHOLD)	{
			    	fetActiveCount ++;
			    }
			    else fetActiveCount = 0;
			    CheckMinimumPower();
			    //activeSamples[i] = sam;
			}
			if ( fetCutoffCount >= 200 ) {//if ( sam >= POW_5V_IO_DET_ADC_THRESHOLD ) {
				// turn on usb in if pmos is cutoff
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
				ChargerSetUSBLockout(CHG_USB_IN_UNLOCK);
				//pow5vIoLoadCurrent = 0;
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			} else {
				if (fetActiveCount >= 3) {
					MeasurePMOSLoadCurrent();//pow5vIoLoadCurrent = GetLoadCurrent();
					pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				}
				POW_5V_DET_LDO_ENABLE(0);
			}
		} /*else {
			pow5vIoLoadCurrent = 0;
		}*/
	} else {
		volatile int16_t volt5 = Get5vIoVoltage();
		if (volt5 < 4800) {
			if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
				ChargerUsbInCurrentLimitStepDown();
			}
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_LOCK);
			if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
		} else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && MS_TIME_COUNT(pow5vDetTimeCount) > 500) {
			Turn5vBoost(1);
			POW_5V_DET_LDO_ENABLE(1);
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_UNLOCK); // turn on charger in
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			wakeupOnCharge = 0xFFFF;
		}
	}

	if ( MS_TIME_COUNT(pow5vPresentCounter) > 800 ) {
		MS_TIME_COUNTER_INIT(pow5vPresentCounter);
		if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT ) {
			if (pow5VChgLoadMaximumReached > 1) {
				ChargerUsbInCurrentLimitStepUp();
				pow5VChgLoadMaximumReached = 2;
			} else if (pow5VChgLoadMaximumReached == 0) {
				pow5VChgLoadMaximumReached = 3;
			}
		} else if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
			// this means input is disconnected, and flag can be cleared
			pow5VChgLoadMaximumReached = 0;
			ChargerUsbInCurrentLimitSetMin();
		}
	}
	if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT) { // if !POW_SOURCE_NEED_POLL()
		osDelay(1);
	} else {
		osDelay(20);
	}
  }
}
#else
void PowerSource5vIoDetectionTask(void) {

	volatile uint32_t timePassed =  MS_TIME_COUNT(pow5vOnTimeout);
	if ( MS_TIME_COUNT(pow5vOnTimeout) < POW_5V_TURN_ON_TIMEOUT ) {
		volatile int16_t batVolt;
		if ( (!POW_SOURCE_PRESENT()) && MS_TIME_COUNT(pow5vOnTimeout) > 0) {
			batVolt = (GetSample(ADC_VBAT_SENS_CHN) * aVdd * 11) >> 15; // 4096 * 1374 / 1000
			if (batVolt < vbatPowOffTresh) {
				Turn5vBoost(0);
				forcedPowerOffFlag = 1;
			}
		}
		// Wait for 5V to become stable after turn on timeout
		return;
	}

	volatile int16_t volt5 = Get5vIoVoltage();

	CheckMinimumPower(volt5);

	if ( POW_5V_BOOST_EN_STATUS() ) {

		if (POW_5V_DET_LDO_EN_STATUS()) {
			volatile uint16_t samp = GetSample(POW_DET_SENS_CHN);
			if ( samp < POW_5V_IO_DET_ADC_THRESHOLD) {
				if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
					MS_TIME_COUNTER_INIT(pow5vPresentCounter);
					ChargerUsbInCurrentLimitStepDown();
				}
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
				ChargerSetUSBLockout(CHG_USB_IN_LOCK);
				POW_5V_DET_LDO_ENABLE(0);
			}
		} else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_UNKNOWN;
			if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
			ChargerSetUSBLockout(CHG_USB_IN_LOCK);
			ChargerUsbInCurrentLimitStepDown();
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
		}

		if ( pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && MS_TIME_COUNT(pow5vDetTimeCount) > 95 ) {
			MS_TIME_COUNTER_INIT(pow5vDetTimeCount);

			if (!POW_5V_DET_LDO_EN_STATUS()) {
				POW_5V_DET_LDO_ENABLE(1);
				//Delay(100);
			}

			// find out if PMOS goes to cutoff or active state
			volatile uint8_t i = 200, fetCutoffCount = 0, fetActiveCount = 0;
			//volatile uint16_t activeSamples[6];
			volatile uint32_t sam = 0;
			while ( i-- && fetActiveCount < 3) {
				DelayUs(120);//HAL_Delay(2);//
			    sam = GetSample(POW_DET_SENS_CHN);//analogIn[ind];
			    if (sam >= POW_5V_IO_DET_ADC_THRESHOLD) fetCutoffCount ++;
			    else fetCutoffCount = 0;
			    if (sam < POW_5V_IO_DET_ADC_THRESHOLD)	{
			    	fetActiveCount ++;
			    }
			    else fetActiveCount = 0;
			    CheckMinimumPower(volt5);
			    //activeSamples[i] = sam;
			}
			if ( fetCutoffCount >= 200 ) {//if ( sam >= POW_5V_IO_DET_ADC_THRESHOLD ) {
				// turn on usb in if pmos is cutoff
				pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
				ChargerSetUSBLockout(CHG_USB_IN_UNLOCK);
				//pow5vIoLoadCurrent = 0;
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			} else {
				if (fetActiveCount >= 3) {
					MeasurePMOSLoadCurrent();//pow5vIoLoadCurrent = GetLoadCurrent();
					pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
				}
				POW_5V_DET_LDO_ENABLE(0);
			}
		} /*else {
			pow5vIoLoadCurrent = 0;
		}*/
	} else {
		if (volt5 < 4800) {
			if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
				MS_TIME_COUNTER_INIT(pow5vPresentCounter);
				ChargerUsbInCurrentLimitStepDown();
			}
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_NOT_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_LOCK);
			if (pow5VChgLoadMaximumReached > 1) pow5VChgLoadMaximumReached --;
		} else if (pow5vInDetStatus != POW_5V_IN_DETECTION_STATUS_PRESENT && MS_TIME_COUNT(pow5vDetTimeCount) > 500) {
			REGULATOR_5V_TURN_ON(); //Turn5vBoost(1);
			POW_5V_DET_LDO_ENABLE(1);
			pow5vInDetStatus = POW_5V_IN_DETECTION_STATUS_PRESENT;
			ChargerSetUSBLockout(CHG_USB_IN_UNLOCK); // turn on charger in
			MS_TIME_COUNTER_INIT(pow5vPresentCounter);
			//wakeupOnCharge = 0xFFFF;
		}
	}

	if ( MS_TIME_COUNT(pow5vPresentCounter) > 800 ) {
		MS_TIME_COUNTER_INIT(pow5vPresentCounter);
		if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_PRESENT ) {
			if (pow5VChgLoadMaximumReached > 1) {
				ChargerUsbInCurrentLimitStepUp();
				pow5VChgLoadMaximumReached = 2;
			} else if (pow5VChgLoadMaximumReached == 0) {
				pow5VChgLoadMaximumReached = 3;
			}
		} else if (pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_NOT_PRESENT) {
			// this means input is disconnected, and flag can be cleared
			pow5VChgLoadMaximumReached = 0;
			ChargerUsbInCurrentLimitSetMin();
		}
	}
}
#endif

#if defined(RTOS_FREERTOS)
static void PowerSourceTask(void *argument) {
	for(;;)
	{
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
		osDelay(20);
	}
}
#else
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
#endif

void PowerSourceSetBatProfile(const BatteryProfile_T* batProfile) {
	//vbatAdcTresh = currentBatProfile != NULL ? ((uint32_t)(currentBatProfile->cutoffVoltage*20+VBAT_TURNOFF_ADC_THRESHOLD) * 2981) /*20 * 4096 * 1000 / 1374*/ / 3300 : (((uint32_t)150*20+VBAT_TURNOFF_ADC_THRESHOLD) * 2981) / 3300;
	//analogWDGConfig.LowThreshold = POW_5V_IO_DET_ADC_THRESHOLD;
	//AnalogAdcWDGConfig(&analogWDGConfig);
	vbatPowOffTresh = currentBatProfile!=NULL ? (uint16_t)(currentBatProfile->cutoffVoltage)*20+VBAT_TURNOFF_ADC_THRESHOLD : (uint16_t)3000+VBAT_TURNOFF_ADC_THRESHOLD;
	AnalogAdcWDGConfig(ADC_VBAT_SENS_CHN,  vbatPowOffTresh);
}

void PowerSourceSetVSysSwitchState(uint8_t state) {
	if (state == 5) {
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_SET);
		if ( GetSample(ADC_VBAT_SENS_CHN) > GetAdcWDGThreshold() || POW_SOURCE_PRESENT()/*chargerStatus != CHG_NO_VALID_SOURCE*/ )
			TurnVSysOutput(1);
		vsysSwitchLimit = state;
	} else if (state == 21) {
		HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, GPIO_PIN_RESET);
		if ( GetSample(ADC_VBAT_SENS_CHN) > GetAdcWDGThreshold() || POW_SOURCE_PRESENT()/*chargerStatus != CHG_NO_VALID_SOURCE*/ )
			TurnVSysOutput(1);
		vsysSwitchLimit = state;
	} else {
		TurnVSysOutput(0);
	}
	forcedVSysOutputOffFlag = 0;
}

uint8_t PowerSourceGetVSysSwitchState() {
	if (POW_VSYS_OUTPUT_EN_STATUS()) {
		return vsysSwitchLimit;
		/*if (HAL_GPIO_ReadPin(GPIOF, GPIO_PIN_1) == GPIO_PIN_SET) {
			return 5;
		} else {
			return 21;
		}*/
	} else {
		return 0;
	}
}

void SetPowerRegulatorConfigCmd(uint8_t data[], uint8_t len) {
	if (data[0] >= POW_REGULATOR_MODE_END) return;
	uint16_t var = 0;
	EE_WriteVariable(POWER_REGULATOR_CONFIG_NV_ADDR, data[0] | ((uint16_t)(~data[0])<<8));

	EE_ReadVariable(POWER_REGULATOR_CONFIG_NV_ADDR, &var);
	if (((~var)&0xFF) == (var>>8)) {
		uint8_t temp = var&0xFF;
		if (temp < POW_REGULATOR_MODE_END) {
			powerRegulatorConfig = temp;
		}
	}
}

void GetPowerRegulatorConfigCmd(uint8_t data[], uint16_t *len) {
	data[0] = powerRegulatorConfig;
	*len = 1;
}
