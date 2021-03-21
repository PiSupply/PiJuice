/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "eeprom.h"

#include "iodrv.h"

#include "charger_bq2416x.h"
#include "fuel_gauge_lc709203f.h"
#include "power_source.h"
#include "command_server.h"
#include "led.h"
#include "button.h"
#include "analog.h"
#include "time_count.h"
#include "load_current_sense.h"
#include "rtc_ds1339_emu.h"
#include "power_management.h"
#include "io_control.h"
#include "execution.h"

#include "osloop.h"
#include "adc.h"


#define OWN1_I2C_ADDRESS		0x14
#define OWN2_I2C_ADDRESS		0x68
#define SMBUS_TIMEOUT_DEFAULT                 ((uint32_t)0x80618061)
#define I2C_MAX_RECEIVE_SIZE	((int16_t)255)

#define NEED_EVENT_POLL()		((chargerNeedPoll \
								|| extiFlag \
								|| rtcWakeupEventFlag \
								|| commandReceivedFlag \
								|| POW_SOURCE_NEED_POLL() \
								|| alarmEventFlag ))

/* Private variables ---------------------------------------------------------*/
#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"
#include "task.h"
#include "portmacro.h"
osThreadId_t defaultTaskHandle;
#endif
ADC_HandleTypeDef hadc;
DMA_HandleTypeDef hdma_adc;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;
//SMBUS_HandleTypeDef hsmbus;

RTC_HandleTypeDef hrtc;

IWDG_HandleTypeDef hiwdg;

TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim6;
TIM_HandleTypeDef htim15;
TIM_HandleTypeDef htim17;

uint8_t resetStatus = 0;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* Buffer used for I2C transfer */
  //uint8_t i2cTrfBuffer[256];

static uint8_t commandReceivedFlag = 0;

PowerState_T state = STATE_INIT;

uint32_t executionState __attribute__((section("no_init"))); // used to indicate if there was unpredictable reset like watchdog expired

uint32_t lastHostCommandTimer __attribute__((section("no_init")));

uint8_t i2cErrorCounter = 0;

extern uint32_t lastWakeupTimer;
extern uint8_t alarmEventFlag;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM6_Init(void);
static void MX_TIM15_Init(void);
static void MX_TIM17_Init(void);
//static void MX_SMBUS_Init(void);
static void MX_IWDG_Init(void);
//static void MX_WWDG_Init(void);

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

void MemInit(uint8_t *buffer, uint8_t val, int32_t size) {
	while((--size) > 0) buffer[size] = val;
}

typedef  void (*pFunction)(void);
pFunction Jump_To_Start;
void ButtonDualLongPressEventCb(void) {
	// Reset to default
	NvEreaseAllVariables();

	executionState = EXECUTION_STATE_CONFIG_RESET;
	while(1) {
	  LedSetRGB(LED1, 150, 0, 0);
	  LedSetRGB(LED2, 150, 0, 0);
	  HAL_Delay(500);
	  LedSetRGB(LED1, 0, 0, 150);
	  LedSetRGB(LED2, 0, 0, 150);
	  HAL_Delay(500);
	}
}

uint8_t extiFlag = 0;
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if (GPIO_Pin == GPIO_PIN_0)
  {
	  // CH_INT
	  chargerInterruptFlag = 1;
	  extiFlag = 1;
  } else if (GPIO_Pin == GPIO_PIN_7)
  {
	  // I2C SDA
	  extiFlag = 2;
  } else if (GPIO_Pin == GPIO_PIN_8) {
	  extiFlag = 4;
	  ioWakeupEvent = 1;
  } else {
	  // SW1, SW2, SW3
	  extiFlag = 3;
  }
}

static uint16_t i2cAddrMatchCode = 0;
volatile static uint8_t i2cTransferDirection = 0;
static int16_t readCmdCode = 0;
uint32_t mainPollMsCounter;
static uint8_t       aSlaveReceiveBuffer[256]  = {0};
uint8_t      slaveTransmitBuffer[256]      = {0};
__IO static uint8_t  ubSlaveReceiveIndex       = 0;
uint32_t      uwTransferDirection       = 0;
//__IO uint32_t uwTransferInitiated       = 0;
//__IO uint32_t uwTransferEnded           = 0;
volatile uint8_t tstFlagi2c=0;
uint16_t dataLen;

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	tstFlagi2c=9;
	dataLen = 1;
	if (i2cAddrMatchCode == hi2c->Init.OwnAddress2) {
		uint8_t cmd = RtcGetPointer();
		RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, cmd, slaveTransmitBuffer, &dataLen);
		RtcSetPointer(cmd + 1);
		HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, 1, I2C_NEXT_FRAME);
	} else {
		slaveTransmitBuffer[0] = 0;
		HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, 1, I2C_NEXT_FRAME);
	}
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c1)
{
    ubSlaveReceiveIndex++;
	tstFlagi2c=1;
    if(HAL_I2C_Slave_Seq_Receive_IT(hi2c1, (uint8_t *)&aSlaveReceiveBuffer[ubSlaveReceiveIndex], 1, I2C_NEXT_FRAME) != HAL_OK) {
      Error_Handler();
    }
    tstFlagi2c=2;
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode)
{
	i2cAddrMatchCode = AddrMatchCode;
    //uwTransferInitiated = 1;
    uwTransferDirection = TransferDirection;

    // First of all, check the transfer direction to call the correct Slave Interface
    if(uwTransferDirection == I2C_DIRECTION_TRANSMIT) {
    	tstFlagi2c=3;
      if(HAL_I2C_Slave_Seq_Receive_IT(hi2c, (uint8_t *)&aSlaveReceiveBuffer[ubSlaveReceiveIndex], 1, I2C_FIRST_FRAME) != HAL_OK) {
        Error_Handler();
      }
      tstFlagi2c=4;
    }
    else {
		dataLen = 1;
		readCmdCode=aSlaveReceiveBuffer[0];
		slaveTransmitBuffer[0]=readCmdCode;

		if (AddrMatchCode == hi2c->Init.OwnAddress1 ) {
			if (readCmdCode >= 0x80 && readCmdCode <= 0x8F) {
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode - 0x80, slaveTransmitBuffer, &dataLen);
				RtcSetPointer(readCmdCode - 0x80 + dataLen);
			} else {
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, slaveTransmitBuffer, &dataLen);
			}
			tstFlagi2c=11;
		} else {
			if ( readCmdCode <= 0x0F ) {
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode, slaveTransmitBuffer, &dataLen);
				RtcSetPointer(readCmdCode + dataLen);
			} else {
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, slaveTransmitBuffer, &dataLen);
			}
			tstFlagi2c=12;
		}
		if(HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, dataLen, I2C_FIRST_AND_NEXT_FRAME) != HAL_OK) {
			Error_Handler();
		}
    }

	PowerMngmtHostPollEvent();
	MS_TIME_COUNTER_INIT(lastHostCommandTimer);
}

void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
	tstFlagi2c=7;
	//uwTransferEnded = 1;
	//uwTransferDirection = I2C_GET_DIR(hi2c);
	if (uwTransferDirection == I2C_DIRECTION_TRANSMIT) {
		dataLen = ubSlaveReceiveIndex;
		readCmdCode = aSlaveReceiveBuffer[0];
		if ( dataLen > 1) {
			if (i2cAddrMatchCode == (hi2c->Init.OwnAddress1 >>1)) {
				if (readCmdCode >= 0x80 && readCmdCode <= 0x8F) {
					dataLen -= 1; // first is command
					RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode - 0x80, aSlaveReceiveBuffer + 1, &dataLen);
				} else {
					CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, aSlaveReceiveBuffer, &dataLen);
					commandReceivedFlag = 1;
				}
			} else {
				if ( readCmdCode <= 0x0F ) {
					// rtc emulation range
					dataLen -= 1; // first is command
					RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode, aSlaveReceiveBuffer + 1, &dataLen);
				} else {
					CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, aSlaveReceiveBuffer, &dataLen);
					commandReceivedFlag = 1;
				}
			}
		}
	}

	ubSlaveReceiveIndex=0;
	HAL_I2C_EnableListen_IT(hi2c);
	tstFlagi2c=8;
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
	// Error_Handler() function is called when error occurs.
	// 1- When Slave don't acknowledge it's address, Master restarts communication.
	// 2- When Master don't acknowledge the last data transferred, Slave don't care in this example.
	/*if (HAL_I2C_GetError(hi2c) != HAL_I2C_ERROR_AF)
	{
		Error_Handler();
	}*/
	// Clear OVR flag
	__HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);
	/*ubSlaveReceiveIndex=0;
	HAL_I2C_EnableListen_IT(hi2c1);*/
	//hi2c1->Instance->ICR=0xFFFF;
	/*uint32_t cr=hi2c1->Instance->CR1;
	cr &= 0xFFFFFFFE;
	hi2c1->Instance->CR1=cr;
	DelayUs(1);
	cr = 0xFFFFFFFF;
	hi2c1->Instance->CR1=cr;*/
}

static uint32_t lowPowerDealyTimer;
static GPIO_InitTypeDef i2c_GPIO_InitStruct;

#if defined(RTOS_FREERTOS)
/* First define the portSUPPRESS_TICKS_AND_SLEEP() macro.  The parameter is the
time, in ticks, until the kernel next needs to execute. */
//#define vPortSuppressTicksAndSleep( xIdleTime ) vApplicationSleep( xIdleTime )
volatile int secPass,subPass;
/* Define the function that is called by portSUPPRESS_TICKS_AND_SLEEP(). */
void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{
unsigned long ulLowPowerTimeBeforeSleep, ulLowPowerTimeAfterSleep;
eSleepModeStatus eSleepStatus;

	commandReceivedFlag = 0;
    /* Read the current time from a time source that will remain operational
    while the microcontroller is in a low power state. */
    //ulLowPowerTimeBeforeSleep = ulGetExternalTime();

    /* Stop the timer that is generating the tick interrupt. */
	//HAL_SuspendTick(); //prvStopTickInterruptTimer();
	//LedSetRGB(1, 0, 0, 0);
    /* Enter a critical section that will not effect interrupts bringing the MCU
    out of sleep mode. */
	//portDISABLE_INTERRUPTS(); //disable_interrupts();

    /* Ensure it is still ok to enter the sleep mode. */
    eSleepStatus = eTaskConfirmSleepModeStatus();

    if( eSleepStatus == eAbortSleep )
    {
        /* A task has been moved out of the Blocked state since this macro was
        executed, or a context siwth is being held pending.  Do not enter a
        sleep state.  Restart the tick and exit the critical section. */
    	HAL_ResumeTick(); //prvStartTickInterruptTimer();
        //portENABLE_INTERRUPTS(); //enable_interrupts();
    }
    else
    {

        if (0) //( eSleepStatus == eNoTasksWaitingTimeout )
        {
            /* It is not necessary to configure an interrupt to bring the
            microcontroller out of its low power state at a fixed time in the
            future. */
            //prvSleep();
        }
        else
        {
            /* Configure an interrupt to bring the microcontroller out of its low
            power state at the time the kernel next needs to execute.  The
            interrupt must be generated from a source that remains operational
            when the microcontroller is in a low power state. */
            //vSetWakeTimeInterrupt( xExpectedIdleTime );
#if 1
            /* Enter the low power state. */
            //prvSleep();
    		AnalogStop();

    		static RTC_TimeTypeDef start;
    		HAL_RTC_GetTime(&hrtc, &start, RTC_FORMAT_BIN);

    		LedStop();
    		if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 8000, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
    		{
    			Error_Handler();
    		}
    		LedSetRGB(LED2, 0, 0, 0);
    		i2c_GPIO_InitStruct.Pin = GPIO_PIN_7;
    		i2c_GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    		i2c_GPIO_InitStruct.Pull = GPIO_NOPULL;
    	    HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);
    	    __disable_irq(); //portENTER_CRITICAL(); // __disable_irq()
    	    HAL_SuspendTick();
    	    __HAL_RCC_GPIOA_CLK_DISABLE(); __HAL_RCC_GPIOB_CLK_DISABLE(); __HAL_RCC_GPIOF_CLK_DISABLE();
    	    portDISABLE_INTERRUPTS();
    		//HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
    		HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

    		__HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOF_CLK_ENABLE();
    		i2c_GPIO_InitStruct.Pin       = GPIO_PIN_7;
    		i2c_GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
    		i2c_GPIO_InitStruct.Pull      = GPIO_NOPULL;//GPIO_PULLUP;
    		i2c_GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    		i2c_GPIO_InitStruct.Alternate = GPIO_AF1_I2C1;
    		HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);
    		//DelayUs(1000);
    		HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

    		static RTC_TimeTypeDef end;
    		HAL_RTC_GetTime(&hrtc, &end, RTC_FORMAT_BIN);
    		secPass = end.Seconds-start.Seconds;
    		subPass = end.SubSeconds-start.SubSeconds;

    		//PowerSourceExitLowPower();
    		AnalogStart();
    		DelayUs(150);
    		TimeTickCb(4000);
    		LedStart();
#else
    		vApplicationIdleHook();
#endif
            /* Determine how long the microcontroller was actually in a low power
            state for, which will be less than xExpectedIdleTime if the
            microcontroller was brought out of low power mode by an interrupt
            other than that configured by the vSetWakeTimeInterrupt() call.
            Note that the scheduler is suspended before
            portSUPPRESS_TICKS_AND_SLEEP() is called, and resumed when
            portSUPPRESS_TICKS_AND_SLEEP() returns.  Therefore no other tasks will
            execute until this function completes. */
             //ulLowPowerTimeAfterSleep = ulGetExternalTime();

            /* Correct the kernels tick count to account for the time the
            microcontroller spent in its low power state. */
            vTaskStepTick( 4000 ); // vTaskStepTick( ulLowPowerTimeAfterSleep - ulLowPowerTimeBeforeSleep );
            LedSetRGB(LED2, 0, 0, 50);
        }

        /* Exit the critical section - it might be possible to do this immediately
        after the prvSleep() calls. */
        __enable_irq();//portEXIT_CRITICAL();//portENABLE_INTERRUPTS(); //enable_interrupts();
        //LedSetRGB(1, 0, 0, 255);
        /* Restart the timer that is generating the tick interrupt. */
        HAL_ResumeTick(); //prvStartTickInterruptTimer();
    }
}
#endif
void WaitInterrupt() {

	commandReceivedFlag = 0;

	if (state == STATE_LOWPOWER) {
		HAL_SuspendTick();
		AnalogStop();

		LedStop();
		if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 8000, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
		{
			Error_Handler();
		}

		i2c_GPIO_InitStruct.Pin = GPIO_PIN_7;
		i2c_GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
		i2c_GPIO_InitStruct.Pull = GPIO_NOPULL;
	    HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);

		HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);
		//HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);

		i2c_GPIO_InitStruct.Pin       = GPIO_PIN_7;
		i2c_GPIO_InitStruct.Mode      = GPIO_MODE_AF_OD;
		i2c_GPIO_InitStruct.Pull      = GPIO_NOPULL;//GPIO_PULLUP;
		i2c_GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
		i2c_GPIO_InitStruct.Alternate = GPIO_AF1_I2C1;
		HAL_GPIO_Init(GPIOB, &i2c_GPIO_InitStruct);
		//DelayUs(1000);
		HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

		//PowerSourceExitLowPower();
		AnalogStart();
		DelayUs(150);
		TimeTickCb(4000);
		LedStart();
		HAL_ResumeTick();

		MS_TIME_COUNTER_INIT(lowPowerDealyTimer);
		//LedSetRGB(LED2, 100, 100, 100);
	} else if (state == STATE_NORMAL) {
		//state = STATE_NORMAL;
		//HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
		HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
	}
}

#if defined(RTOS_FREERTOS)
int ledflag = 2;
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {


	  // Do not disturb i2c transfer if this is i2c interrupt wakeup
	  if ( MS_TIME_COUNT(mainPollMsCounter) >= TICK_PERIOD_MS || NEED_EVENT_POLL() ) {

		//PowerSource5vIoDetectionTask();
		//AnalogTask();
		//ChargerTask();
		//FuelGaugeTask();
		//BatteryTask();
		//PowerSourceTask();
		if (alarmEventFlag) {
			EvaluateAlarm();
			alarmEventFlag = 0;
		}
		//LedTask();
		//if (MS_TIME_COUNT(mainPollMsCounter) > 98) {
			//ButtonTask();
			//LoadCurrentSenseTask();
			//PowerManagementTask();

		//}
		if ( (hi2c2.ErrorCode&(HAL_I2C_ERROR_TIMEOUT | HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO)) || hi2c2.State != HAL_I2C_STATE_READY || hi2c2.XferCount) {
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
			chargerI2cErrorCounter = 1;
		}
		if (chargerI2cErrorCounter > 10) {
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
			chargerI2cErrorCounter = 1;
		}

		if ( NEED_EVENT_POLL() ) {
			state = STATE_RUN;
		} else if ( ((GetLoadCurrent() <= 50 ) || (Get5vIoVoltage() < 4600 && !POW_VSYS_OUTPUT_EN_STATUS()) )
				&& MS_TIME_COUNT(lastHostCommandTimer) > 5000
				&& MS_TIME_COUNT(lowPowerDealyTimer) >= 22
				&& MS_TIME_COUNT(lastWakeupTimer) > 20000
				&& chargerStatus == CHG_NO_VALID_SOURCE
				&& !IsButtonActive()
				) {
			state = STATE_LOWPOWER;
		} else {
			state = STATE_NORMAL;
		}

		if ( extiFlag == 2 ) {
			MS_TIME_COUNTER_INIT(lastHostCommandTimer);
		}
		extiFlag = 0;

	    // Refresh IWDG: reload counter
	   /* if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
	    {
	      Error_Handler(); // Refresh Error
	    }*/
	    //__HAL_IWDG_RELOAD_COUNTER(&hiwdg); // use for testing

		MS_TIME_COUNTER_INIT(mainPollMsCounter);
		osDelay(20);
	  } else if (state == STATE_LOWPOWER) {
		  osDelay(4100);//vApplicationIdleHook(); //WaitInterrupt();
		  osDelay(10);
	  }

	  /*if (ledflag) {
		  osDelay(500);
		uint8_t ledBlink[] = {5, 100, 0, 80, 5, 0, 80, 0, 3};
		//LedCmdSetBlink(LED1, ledBlink, 9);
		LedCmdSetBlink(LED2, ledBlink, 9);
		//ledflag = 0;
	  }*/
  }
  /* USER CODE END 5 */
}
#endif

int main(void)
{
	//HAL_Init();
	if (executionState != EXECUTION_STATE_NORMAL && executionState != EXECUTION_STATE_UPDATE && executionState != EXECUTION_STATE_CONFIG_RESET) {
		if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
			executionState = EXECUTION_STATE_POWER_RESET;
		} else {//if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)){
			// updating from old firmware without executionState defined
			executionState = EXECUTION_STATE_UPDATE;
		} // else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) {
	}
	__HAL_RCC_CLEAR_RESET_FLAGS();

	if ( executionState == EXECUTION_STATE_NORMAL ) {
		resetStatus = 1;
	} else {
		// initialize globals
		resetStatus = 0;
	}

	__HAL_FLASH_PREFETCH_BUFFER_ENABLE();

	HAL_MspInit();

	NvInit();

	// Configure the system clock
	SystemClock_Config();

	// Initialize all configured peripherals
	MX_GPIO_Init();
	if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10) == GPIO_PIN_RESET && executionState == EXECUTION_STATE_POWER_RESET) {
		executionState = EXECUTION_STATE_POWER_ON;
	}

	MX_DMA_Init();
	MX_ADC_Init();
	//MX_WWDG_Init();
	MX_I2C1_Init();//MX_SMBUS_Init();//  // NOTE: need 48KHz clock to work on 400KHz
	MX_I2C2_Init();
	MX_RTC_Init();
	MX_TIM3_Init();
	MX_TIM15_Init();
	MX_TIM17_Init();
	MX_TIM1_Init();
	MX_TIM14_Init();
	MX_TIM6_Init();

	HAL_InitTick(TICK_INT_PRIORITY);

	OSLOOP_Init();

	// Wait for the adc to become ready
	while (false == ADC_GetFilterReady())
	{
		asm volatile ("nop");
	}

	if (!resetStatus) MS_TIME_COUNTER_INIT(lastHostCommandTimer);

	MS_TIME_COUNTER_INIT(mainPollMsCounter);
	MS_TIME_COUNTER_INIT(lowPowerDealyTimer);

	MX_IWDG_Init();

#if defined(RTOS_FREERTOS)
	osKernelInitialize();
	/* Create the thread(s) */
	/* definition and creation of defaultTask */
	const osThreadAttr_t defaultTask_attributes = {
	.name = "defaultTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 128
	};
	/* add threads, ... */
	defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
#endif

	LoadCurrentSenseInit();
	BatteryInit();
	BUTTON_Init();

	while(1)
	{
		BUTTON_Task();
		HAL_Delay(100u);

	    // Refresh IWDG: reload counter
	    if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
	    {
	      Error_Handler(); // Refresh Error
	    }
	}

	if (executionState == EXECUTION_STATE_POWER_ON) {
		HAL_Delay(100);  // after power-on, charger and fuel gauge requires initialization time
		FuelGaugeIcPreInit();
	}
	ChargerInit();
	PowerSourceInit();
	FuelGaugeInit();
	PowerManagementInit();
	LedInit();
	BUTTON_Init();
	RtcInit();
	IoControlInit();

	NvSetDataInitialized();

	/*if ( executionState == EXECUTION_STATE_CONFIG_RESET ) {
		LedSetRGB(1, 0, 255, 0);
	} else if (executionState == EXECUTION_STATE_POWER_RESET) {
		LedSetRGB(1, 255, 0, 0);
	} else if (executionState == EXECUTION_STATE_UPDATE) {
		LedSetRGB(1, 0, 0, 255);
	} else {
		LedSetRGB(1, 0, 0, 0);
	}*/

	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET); // ee write protect
	uint16_t var = 0;
	EE_ReadVariable(ID_EEPROM_ADR_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) ) {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, (var&0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	} else {
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // default ee Adr
	}

	state = STATE_NORMAL;

	HAL_I2C_EnableListen_IT(&hi2c1);

	executionState = EXECUTION_STATE_NORMAL; // after initialization indicate it for future wd resets

#if defined(RTOS_FREERTOS)
	/* Start scheduler */
	osKernelStart();

	while (1)
	{
	}
#else
	/* Infinite loop */
	while (1)
	{
	  // Do not disturb i2c transfer if this is i2c interrupt wakeup
	  if ( MS_TIME_COUNT(mainPollMsCounter) >= TICK_PERIOD_MS || NEED_EVENT_POLL() ) {

		PowerSource5vIoDetectionTask();
		AnalogTask();
		ChargerTask();
		FuelGaugeTask();
		BatteryTask();
		PowerSourceTask();
		if (alarmEventFlag || __HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) != RESET) {
			EvaluateAlarm();
			alarmEventFlag = 0;
			__HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
		}
		LedTask();

		//if (MS_TIME_COUNT(mainPollMsCounter) > 98) {
			BUTTON_Task();
			LoadCurrentSenseTask();
			PowerManagementTask();

		//}

		if ( (hi2c2.ErrorCode&(HAL_I2C_ERROR_TIMEOUT | HAL_I2C_ERROR_BERR | HAL_I2C_ERROR_ARLO)) || hi2c2.State != HAL_I2C_STATE_READY || hi2c2.XferCount)
		{
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
			chargerI2cErrorCounter = 1;
		}

		if (chargerI2cErrorCounter > 10)
		{
			HAL_I2C_DeInit(&hi2c2);
			MX_I2C2_Init();
			chargerI2cErrorCounter = 1;
		}

		if ( NEED_EVENT_POLL() )
		{
			state = STATE_RUN;
		}
		else if ( ((ANALOG_Get5VRailMa() <= 50 ) || ( (ANALOG_Get5VRailMv() < 4600u) && (false == POW_VSYS_OUTPUT_EN_STATUS()) ) )
				&& MS_TIME_COUNT(lastHostCommandTimer) > 5000u
				&& MS_TIME_COUNT(lowPowerDealyTimer) >= 22u
				&& MS_TIME_COUNT(lastWakeupTimer) > 20000u
				&& chargerStatus == CHG_NO_VALID_SOURCE
				&& !BUTTON_IsButtonActive()
				)
		{
			state = STATE_LOWPOWER;
		}
		else
		{
			state = STATE_NORMAL;
		}

		if ( extiFlag == 2 )
		{
			MS_TIME_COUNTER_INIT(lastHostCommandTimer);
		}

		extiFlag = 0;

	    // Refresh IWDG: reload counter
	    if (HAL_IWDG_Refresh(&hiwdg) != HAL_OK)
	    {
	      Error_Handler(); // Refresh Error
	    }
	    //__HAL_IWDG_RELOAD_COUNTER(&hiwdg); // use for testing

		MS_TIME_COUNTER_INIT(mainPollMsCounter);
	  }
	  WaitInterrupt();
	}
#endif
}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSI14
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSI14State = RCC_HSI14_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.HSI14CalibrationValue = 16;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_OFF;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;//RCC_PLL_ON;//
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

    /**Initializes the CPU, AHB and APB busses clocks
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;//RCC_SYSCLKSOURCE_PLLCLK;//
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;//RCC_I2C1CLKSOURCE_SYSCLK;//
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
#if !defined(RTOS_FREERTOS)
    /**Configure the Systick interrupt time
    */
  //HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/10);

    /**Configure the Systick
    */
  //HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  //HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
#endif
}

static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.DMAContinuousRequests = ENABLE;
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_1;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_3;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_4;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_5;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_7;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}


/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

}


/* I2C1 init function not used, MX_SMBUS_Init used instead*/
static void MX_I2C1_Init(void)
{

  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00FF0000;//0x00C4092A;//0x00300000;//0x00900000 for 48000 i2c clock
	uint16_t var = 0;
	EE_ReadVariable(OWN_ADDRESS1_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) ) {
		// Use NV address
		hi2c1.Init.OwnAddress1 = var&0xFF;
	} else {
		// Use default address
		hi2c1.Init.OwnAddress1 = OWN1_I2C_ADDRESS << 1;
	}
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_ENABLE;//I2C_DUALADDRESS_DISABLE;
	EE_ReadVariable(OWN_ADDRESS2_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) ) {
		// Use NV address
		hi2c1.Init.OwnAddress2 = var&0xFF;
	} else {
		// Use default address
		hi2c1.Init.OwnAddress2 = OWN2_I2C_ADDRESS << 1;
	}
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  //I2C_Disable_IRQ(&hi2c1, (0x00000011U)/*I2C_XFER_ERROR_IT*/);
  //__HAL_I2C_DISABLE_IT(&hi2c1, I2C_IT_ERRI | I2C_IT_NACKI);

    /**Configure Analogue filter
    */
  /*if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }*/

}
#if 0
static void MX_SMBUS_Init(void) {
	hsmbus.Instance = I2C1;
	hsmbus.Init.Timing = 0x00300000;//0x00900000 for 48000 i2c clock
	hsmbus.Init.AnalogFilter = SMBUS_ANALOGFILTER_DISABLED;//SMBUS_ANALOGFILTER_ENABLED;//
	uint16_t var = 0;
	EE_ReadVariable(OWN_ADDRESS1_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) ) {
		// Use NV address
		hsmbus.Init.OwnAddress1 = var&0xFF;
	} else {
		// Use default address
		hsmbus.Init.OwnAddress1 = OWN1_I2C_ADDRESS << 1;
	}
	hsmbus.Init.AddressingMode = SMBUS_ADDRESSINGMODE_7BIT;
	hsmbus.Init.DualAddressMode = SMBUS_DUALADDRESS_ENABLED;//I2C_DUALADDRESS_DISABLE;
	EE_ReadVariable(OWN_ADDRESS2_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) ) {
		// Use NV address
		hsmbus.Init.OwnAddress2 = var&0xFF;
	} else {
		// Use default address
		hsmbus.Init.OwnAddress2 = OWN2_I2C_ADDRESS << 1;
	}
	hsmbus.Init.OwnAddress2Masks = SMBUS_OA2_NOMASK;
	hsmbus.Init.GeneralCallMode = SMBUS_GENERALCALL_DISABLED;
	hsmbus.Init.NoStretchMode = SMBUS_NOSTRETCH_DISABLED;
	hsmbus.Init.PacketErrorCheckMode = SMBUS_PEC_DISABLED;
	hsmbus.Init.PeripheralMode = SMBUS_PERIPHERAL_MODE_SMBUS_SLAVE;//SMBUS_PERIPHERAL_MODE_SMBUS_SLAVE_ARP ;
	hsmbus.Init.SMBusTimeout = SMBUS_TIMEOUT_DEFAULT;
	if (HAL_SMBUS_Init(&hsmbus) != HAL_OK)
	{
		Error_Handler();
	}

	// Configure Analogue filter
	/*if (HAL_I2CEx_ConfigAnalogFilter(&hsmbus, SMBUS_ANALOGFILTER_ENABLE) != HAL_OK)
	{
	  Error_Handler();
	}*/
}
#endif
/* I2C2 init function */
static void MX_I2C2_Init(void)
{

  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x20000A0D;//0x0010020B;//0x2000090E;//0x0010020A;//0x00900000;//0x2000090E;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Analogue filter
    */
  /*if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }*/

}

/* RTC init function */
static void MX_RTC_Init(void)
{

  //RTC_TimeTypeDef sTime;
  //RTC_DateTypeDef sDate;
  //RTC_AlarmTypeDef sAlarm;

    /**Initialize RTC Only
    */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

// for testing/debug only
#if 0
  /* (1) Write access for RTC registers */
  /* (2) Disable wake up timerto modify it */
  /* (3) Wait until it is allow to modify wake up reload value */
  /* (4) Modify wake up value reload counter to have a wake up each 1Hz */
  /* (5) Enable wake up counter and wake up interrupt */
  /* (6) Disable write access */
  RTC->WPR = 0xCA; /* (1) */
  RTC->WPR = 0x53; /* (1) */
  RTC->CR &= ~RTC_CR_WUTE; /* (2) */
  while ((RTC->ISR & RTC_ISR_WUTWF) != RTC_ISR_WUTWF) /* (3) */
  {
   /* add time out here for a robust application */
  }
  RTC->WUTR = 0x9C0; /* (4) */
  RTC->CR = RTC_CR_WUTE | RTC_CR_WUTIE | 0x00000000; /* (5) */
  RTC->WPR = 0xFE; /* (6) */
  volatile uint32_rtccr = RTC->CR;
  RTC->WPR = 0x64; /* (6) */
#endif
  /*if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 2000, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
    Error_Handler();
  }*/

    /**Enable the Alarm A
    */
  /*sAlarm.Alarm = RTC_ALARM_A;
  sAlarm.AlarmDateWeekDay = RTC_WEEKDAY_MONDAY;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_NONE;
  sAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;
  sAlarm.AlarmTime.Hours = 0x02;
  sAlarm.AlarmTime.Minutes = 0x20;
  sAlarm.AlarmTime.Seconds = 0x04;
  sAlarm.AlarmTime.SubSeconds = 0x56;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }*/

}

/* TIM3 init function */
static void MX_TIM3_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_OC_InitTypeDef sConfigOC;

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /* Common configuration for all channels */
  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;
  sConfigOC.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCIdleState  = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  sConfigOC.Pulse = 0;//(uint32_t)(((uint32_t)(666 - 1))/2);
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_MspPostInit(&htim3);

}


/**
  * @brief TIM6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM6_Init(void)
{

  /* USER CODE BEGIN TIM6_Init 0 */

  /* USER CODE END TIM6_Init 0 */

  /* USER CODE BEGIN TIM6_Init 1 */

  /* USER CODE END TIM6_Init 1 */
  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 8 - 1;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = 10000;
  htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM6_Init 2 */

  /* USER CODE END TIM6_Init 2 */

}


/* TIM15 init function */
static void MX_TIM15_Init(void)
{

  TIM_MasterConfigTypeDef sMasterConfig;
  TIM_OC_InitTypeDef sConfigOC;
  //TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

  htim15.Instance = TIM15;
  htim15.Init.Prescaler = 2;
  htim15.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim15.Init.Period = 65535;
  htim15.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim15.Init.RepetitionCounter = 0;
  if (HAL_TIM_PWM_Init(&htim15) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim15, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_ConfigChannel(&htim15, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }

  /*sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim15, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }*/

  HAL_TIM_MspPostInit(&htim15);

}

/* TIM17 init function */
static void MX_TIM17_Init(void)
{

  TIM_OC_InitTypeDef sConfigOC;
  //TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;

  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 0;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 65535;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim17, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /*sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim17, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }*/

  HAL_TIM_MspPostInit(&htim17);

}

/* IWDG init function */
static void MX_IWDG_Init(void)
{

  // ##-3- Configure the IWDG peripheral ######################################*/
  // Set counter reload value to obtain 250ms IWDG TimeOut.
  //   IWDG counter clock Frequency = LsiFreq / 32
  //   Counter Reload Value = 250ms / IWDG counter clock period
  //                     = 0.25s / (32/LsiFreq)
  //                      = LsiFreq / (32 * 4)
  //                      = LsiFreq / 128
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Reload    = 1300;//LSI_VALUE / 4; // 8 seconds
  hiwdg.Init.Window    = IWDG_WINDOW_DISABLE;

  DelayUs(100);
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    // Initialization Error
    Error_Handler();
  }
/*
  IWDG->KR = 0x0000CCCC; // (1)
  DelayUs(100);
  IWDG->KR = 0x00005555; // (2)
  DelayUs(100);
  IWDG->PR = IWDG_PRESCALER_256; // (3)
  //DelayUs(100);
  IWDG->RLR = 1300; // (4)
  //DelayUs(100);
  while (IWDG->SR) // (5)
  {
   // add time out here for a robust application
  }
  IWDG->KR = 0x0000AAAA; // (6)
  //DelayUs(100);

  //##-4- Start the IWDG #####################################################
  if (__HAL_IWDG_START(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
*/
}

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : PF0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;//GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  /*Configure GPIO pin : PF1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

  // Boost and vsys switch enable configure as inputs to capture state before reset
  GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA10 PA11 PA12, PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_6|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  // Configure GPIO pins : PB2, PB12 as button inputs
  GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;//GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  // Configure GPIO pins : PC13 - SW2(power button)
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;//GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : BGINT_Pin no pull (external circuit) */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  //Configure GPIOB output pins
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB13 open drain */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6|GPIO_PIN_11|GPIO_PIN_15, GPIO_PIN_RESET);

  // deactivate RUN signal
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);

  /* Enable and set EXTI line 12,13 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* Enable and set EXTI line charger interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

  /* Enable and set EXTI line 2, SW2 Interrupt to the lowest priority */
  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 2, 0);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
  while(1)
  {
  }
  /* USER CODE END Error_Handler */
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */

/**
  * @}
*/

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
