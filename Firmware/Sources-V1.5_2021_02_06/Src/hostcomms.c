#include "main.h"
#include "eeprom.h"
#include "nv.h"

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


#define OWN1_I2C_ADDRESS				0x14
#define OWN2_I2C_ADDRESS				0x68
#define SMBUS_TIMEOUT_DEFAULT			((uint32_t)0x80618061)
#define I2C_MAX_RECEIVE_SIZE			((int16_t)255)		/* int? */


// 255 is max transaction length, byte 0 will have command code
#define HOSTCOMMS_I2C_BUFFER_LEN		256u


typedef enum
{
	HOSTCOMMS_MODE_WAIT = 0u,
	HOSTCOMMS_MODE_RX = 1u,
	HOSTCOMMS_MODE_RXC = 2u,
	HOSTCOMMS_MODE_TX = 3u,
	HOSTCOMMS_MODE_FAULT = 4u
} HOSTCOMMS_Mode_t;


static uint8_t m_hostcommsBuffer[HOSTCOMMS_I2C_BUFFER_LEN];
static volatile uint8_t m_rxLen;
static uint32_t m_txCount;
static uint32_t m_rxCount;
static uint32_t m_addrCount;

static HOSTCOMMS_Mode_t m_hostcommsMode;

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
static bool m_i2cError;

extern I2C_HandleTypeDef hi2c1;


void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
	tstFlagi2c=9;
	dataLen = 1;

	if (i2cAddrMatchCode == hi2c->Init.OwnAddress2)
	{
		uint8_t cmd = RtcGetPointer();
		RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, cmd, slaveTransmitBuffer, &dataLen);
		RtcSetPointer(cmd + 1);
		HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, 1, I2C_NEXT_FRAME);
	}
	else
	{
		slaveTransmitBuffer[0] = 0;
		HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, 1, I2C_NEXT_FRAME);
	}
}


void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c1)
{
    ubSlaveReceiveIndex++;
	tstFlagi2c=1;

	if(HAL_I2C_Slave_Seq_Receive_IT(hi2c1, (uint8_t *)&aSlaveReceiveBuffer[ubSlaveReceiveIndex], 1, I2C_NEXT_FRAME) != HAL_OK)
    {
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
    if(uwTransferDirection == I2C_DIRECTION_TRANSMIT)
    {
    	tstFlagi2c=3;
		if(HAL_I2C_Slave_Seq_Receive_IT(hi2c, (uint8_t *)&aSlaveReceiveBuffer[ubSlaveReceiveIndex], 1, I2C_FIRST_FRAME) != HAL_OK)
		{
		Error_Handler();
		}

		tstFlagi2c=4;
    }
    else
    {
		dataLen = 1;
		readCmdCode=aSlaveReceiveBuffer[0];
		slaveTransmitBuffer[0]=readCmdCode;

		if (AddrMatchCode == hi2c->Init.OwnAddress1)
		{
			if (readCmdCode >= 0x80 && readCmdCode <= 0x8F)
			{
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode - 0x80, slaveTransmitBuffer, &dataLen);
				RtcSetPointer(readCmdCode - 0x80 + dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, slaveTransmitBuffer, &dataLen);
			}

			tstFlagi2c=11;
		}
		else
		{
			if ( readCmdCode <= 0x0F )
			{
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode, slaveTransmitBuffer, &dataLen);
				RtcSetPointer(readCmdCode + dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, slaveTransmitBuffer, &dataLen);
			}

			tstFlagi2c=12;
		}

		if(HAL_I2C_Slave_Seq_Transmit_IT(hi2c, (uint8_t *)slaveTransmitBuffer, dataLen, I2C_FIRST_AND_NEXT_FRAME) != HAL_OK)
		{
			Error_Handler();
		}
    }

	PowerMngmtHostPollEvent();

	MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
}


void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c)
{
	tstFlagi2c=7;

	if (uwTransferDirection == I2C_DIRECTION_TRANSMIT)
	{
		dataLen = ubSlaveReceiveIndex;

		readCmdCode = aSlaveReceiveBuffer[0];

		if (dataLen > 1)
		{
			if (i2cAddrMatchCode == (hi2c->Init.OwnAddress1 >>1))
			{
				if (readCmdCode >= 0x80 && readCmdCode <= 0x8F)
				{
					dataLen -= 1; // first is command
					RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode - 0x80, aSlaveReceiveBuffer + 1, &dataLen);
				}
				else
				{
					CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, aSlaveReceiveBuffer, &dataLen);
				}
			}
			else
			{
				if (readCmdCode <= 0x0F)
				{
					// rtc emulation range
					dataLen -= 1; // first is command
					RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode, aSlaveReceiveBuffer + 1, &dataLen);
				}
				else
				{
					CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, aSlaveReceiveBuffer, &dataLen);
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

	m_i2cError = true;
}


void I2C1_IRQHandler(void)
{
	const uint8_t addrMatch = I2C1->ISR >> 16u;

	if (I2C_ISR_ADDR == (I2C1->ISR & I2C_ISR_ADDR))
	{
		// Check data direction
		if (I2C_ISR_DIR == (I2C1->ISR & I2C_ISR_DIR))
		{
			m_rxLen = (HOSTCOMMS_I2C_BUFFER_LEN - hi2c1.hdmarx->Instance->CNDTR);

			// Disable rx dma
			hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

			// Check for repeated start
			if ( (HOSTCOMMS_MODE_RX == m_hostcommsMode) && (1u == m_rxLen) )
			{
				// Data is for slave to master
				m_hostcommsMode = HOSTCOMMS_MODE_TX;
			}
			else
			{
				m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
			}
		}
		else
		{
			if (HOSTCOMMS_MODE_WAIT == m_hostcommsMode)
			{
				MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);

				m_hostcommsBuffer[0u] = addrMatch & 0xFEu;

				// Data is for master to slave
				m_hostcommsMode = HOSTCOMMS_MODE_RX;

				// Disable rx dma
				hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

				// Clear dma flags
				hi2c1.hdmarx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmarx->ChannelIndex);

				// Load in max data size
				hi2c1.hdmarx->Instance->CNDTR = HOSTCOMMS_I2C_BUFFER_LEN;

				// Enable dma device
				hi2c1.hdmarx->Instance->CCR |= DMA_CCR_EN;
			}
		}

		m_addrCount++;

		// Clear the interrupt
		I2C1->ICR |= I2C_ICR_ADDRCF;
	}
	else
	{
		if (HOSTCOMMS_MODE_RX == m_hostcommsMode)
		{
			m_rxLen = (HOSTCOMMS_I2C_BUFFER_LEN - hi2c1.hdmarx->Instance->CNDTR);
			// Check amount of data received
			// 1 byte == command code, wait for repeated start
			// More == rx complete, do something with it

			// Disable rx dma
			hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

			if (m_rxLen > 1u)
			{
				m_hostcommsMode = HOSTCOMMS_MODE_RXC;

				// Turn off the perpheral until the command has been dealt with
				I2C1->CR1 &= ~(I2C_CR1_PE);

				m_rxCount++;
			}
			else
			{
				m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
			}
		}
		else if (HOSTCOMMS_MODE_TX == m_hostcommsMode)
		{
			// TODO - Take it as transmit?

			// Disable the DMA controller
			hi2c1.hdmatx->Instance->CCR &= ~(DMA_CCR_EN);

			// Flush TX register as will be loaded by the DMA controller
			I2C1->ISR |= I2C_ISR_TXE;

			// Done here, next.
			m_hostcommsMode = HOSTCOMMS_MODE_WAIT;

			m_txCount++;
		}
		else
		{
			// Out of sync somehow
			m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
		}

		// Clear the interrupt
		I2C1->ICR |= I2C_ICR_STOPCF;
	}
}


// OSLoop initialised
void HOSTCOMMS_Init(uint32_t sysTime)
{
	uint8_t tempU8;

	HAL_I2C_DisableListen_IT(&hi2c1);

	I2C1->CR1 &= ~(I2C_CR1_PE);

	I2C1->OAR1 = 0u;
	I2C1->OAR2 = 0u;

	m_txCount = 0u;
	m_rxCount = 0u;
	m_addrCount = 0u;

	m_listening = false;

	if (executionState != EXECUTION_STATE_NORMAL)
	{
		MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
	}


	if (NV_ReadVariable_U8(OWN_ADDRESS1_NV_ADDR, &tempU8))
	{
		I2C1->OAR1 = (tempU8 << 1u) | I2C_OAR1_OA1EN;
	}
	else
	{
		// Use default address
		I2C1->OAR1 = (OWN1_I2C_ADDRESS << 1u) | I2C_OAR1_OA1EN;
	}


	if (NV_ReadVariable_U8(OWN_ADDRESS2_NV_ADDR, &tempU8))
	{
		// Note: this is not shifting address as original code
		I2C1->OAR2 = tempU8 | I2C_OAR2_OA2EN;
	}
	else
	{
		// Use default address
		I2C1->OAR2 = (OWN2_I2C_ADDRESS << 1u) | I2C_OAR2_OA2EN;
	}


	// Assign the peripheral address
	hi2c1.hdmarx->Instance->CPAR = (uint32_t)&I2C1->RXDR;
	// Point to the second byte, the first will contain the device address.
	hi2c1.hdmarx->Instance->CMAR = (uint32_t)&m_hostcommsBuffer[1u];

	// Disable the dma channel for now
	hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

	// Assign the peripheral address
	hi2c1.hdmatx->Instance->CPAR = (uint32_t)&I2C1->TXDR;

	// The tx buffer will be dynamic but point to the hostcomms buffer for now
	hi2c1.hdmatx->Instance->CMAR = (uint32_t)&m_hostcommsBuffer[1u];

	// Disable the dma channel for now
	hi2c1.hdmatx->Instance->CCR &= ~(DMA_CCR_EN);

	// Clear any flags
	hi2c1.hdmarx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmarx->ChannelIndex);
	hi2c1.hdmatx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmatx->ChannelIndex);

	// Enable the DMA transfer mode
	I2C1->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN;

	// Enable the i2c device
	I2C1->CR1 |= I2C_CR1_PE;
}


// Low priority tasks, deal with host to slave commands
void HOSTCOMMS_Task(void)
{
	if (false == m_listening)
	{
		I2C1->CR1 |= (I2C_CR1_ADDRIE | I2C_CR1_STOPIE);
		m_listening = true;
	}

	if (HOSTCOMMS_MODE_RXC == m_hostcommsMode)
	{
		// Something sent from the host
		if (m_hostcommsBuffer[0u] == (I2C1->OAR1 & 0xFE))
		{
			// Is a pijuice command
		}
		else
		{
			// Is an RTC command
		}

		// Enable the peripheral
		I2C1->CR1 |= I2C_CR1_PE;

		m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
	}
}


// High priority tasks, deal with slave to host commands
void HOSTCOMMS_Service(uint32_t sysTime)
{
	// Timeout after a second, will catch fault mode
	if ( MS_TIMEREF_TIMEOUT(m_lastHostCommandTimeMs, sysTime, 1000u) &&
			(HOSTCOMMS_MODE_WAIT != m_hostcommsMode) &&
			(HOSTCOMMS_MODE_RXC != m_hostcommsMode)
			)
	{
		// Something bad must have happened, reset the device
		I2C1->CR1 &= ~(I2C_CR1_PE);

		// Disable the dma controllers
		hi2c1.hdmatx->Instance->CCR &= ~(DMA_CCR_EN);
		hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

		// Re-enable the peripheral
		I2C1->CR1 |= I2C_CR1_PE;

		m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
	}

	if (HOSTCOMMS_MODE_TX == m_hostcommsMode)
	{
		// Something is requested by the host
		if (m_hostcommsBuffer[0u] == (I2C1->OAR1 & 0xFE))
		{
			// Is a pijuice request

			// Is a pijuice request for rtc
		}
		else
		{
			// Is an RTC request
		}

		hi2c1.hdmatx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmatx->ChannelIndex);
		hi2c1.hdmatx->Instance->CNDTR = HOSTCOMMS_I2C_BUFFER_LEN;
		hi2c1.hdmatx->Instance->CCR |= DMA_CCR_EN;
	}
}


uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime)
{
	return MS_TIMEREF_DIFF(m_lastHostCommandTimeMs, sysTime);
}


void HOSTCOMMS_SetInterrupt(void)
{
	MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
}

