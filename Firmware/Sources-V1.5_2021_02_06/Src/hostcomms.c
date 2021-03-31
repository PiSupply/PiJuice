#include "main.h"
#include "eeprom.h"

#include "system_conf.h"
#include "iodrv.h"
#include "crc.h"

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
#include "taskman.h"
#include "adc.h"
#include "i2cdrv.h"
#include "util.h"

#include "hostcomms.h"


#define OWN1_I2C_ADDRESS		0x14
#define OWN2_I2C_ADDRESS		0x68
#define SMBUS_TIMEOUT_DEFAULT	((uint32_t)0x80618061)
#define I2C_MAX_RECEIVE_SIZE	((int16_t)255)		/* int? */

static bool m_commandReceivedFlag = false;
static uint32_t m_lastHostCommandTimeMs __attribute__((section("no_init")));


static uint16_t i2cAddrMatchCode = 0;
volatile static uint8_t i2cTransferDirection = 0;
static int16_t readCmdCode = 0;

static uint8_t       	aSlaveReceiveBuffer[256]  = {0};
static uint8_t      	slaveTransmitBuffer[256]      = {0};
static __IO uint8_t  	ubSlaveReceiveIndex       = 0;
static uint32_t      	uwTransferDirection       = 0;
static volatile uint8_t tstFlagi2c=0;
static uint16_t 		dataLen;

static bool m_listening;

extern I2C_HandleTypeDef hi2c1;


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

	MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
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
					m_commandReceivedFlag = true;
				}
			} else {
				if ( readCmdCode <= 0x0F ) {
					// rtc emulation range
					dataLen -= 1; // first is command
					RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode, aSlaveReceiveBuffer + 1, &dataLen);
				} else {
					CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, aSlaveReceiveBuffer, &dataLen);
					m_commandReceivedFlag = true;
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
	// Clear OVR flag
	__HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF);
}


void HOSTCOMMS_Init(void)
{
	uint16_t var = 0;

	HAL_I2C_DisableListen_IT(&hi2c1);

	if (executionState != EXECUTION_STATE_NORMAL)
	{
		MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
	}


	EE_ReadVariable(OWN_ADDRESS1_NV_ADDR, &var);
	if ( (((~var)&0xFF) == (var>>8)) )
	{
		// Use NV address
		hi2c1.Init.OwnAddress1 = var&0xFF;
	}
	else
	{
		// Use default address
		hi2c1.Init.OwnAddress1 = OWN1_I2C_ADDRESS << 1;
	}


	EE_ReadVariable(OWN_ADDRESS2_NV_ADDR, &var);

	if ( (((~var)&0xFF) == (var>>8)) )
	{
		// Use NV address
		hi2c1.Init.OwnAddress2 = var&0xFF;
	}
	else
	{
		// Use default address
		hi2c1.Init.OwnAddress2 = OWN2_I2C_ADDRESS << 1;
	}

	m_listening = false;
}


void HOSTCOMMS_Task(void)
{
	if (false == m_listening)
	{
		m_listening = (HAL_OK == HAL_I2C_EnableListen_IT(&hi2c1));
	}
}


uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime)
{
	return MS_TIMEREF_DIFF(m_lastHostCommandTimeMs, sysTime);
}


bool HOSTCOMMS_IsCommandActive(void)
{
	return m_commandReceivedFlag;
}


void HOSTCOMMS_SetInterrupt(void)
{
	MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
}

