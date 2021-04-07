/*
 * hostcomms.h
 *
 *  Created on: 31.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

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

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define OWN1_I2C_ADDRESS				0x14
#define OWN2_I2C_ADDRESS				0x68

// 255 is max transaction length, byte 0 will have address, byte 1 will have command code
#define HOSTCOMMS_I2C_BUFFER_LEN		256u


typedef enum
{
	HOSTCOMMS_MODE_WAIT = 0u,
	HOSTCOMMS_MODE_RX = 1u,
	HOSTCOMMS_MODE_RXC = 2u,
	HOSTCOMMS_MODE_TX = 3u,
	HOSTCOMMS_MODE_FAULT = 4u
} HOSTCOMMS_Mode_t;


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint8_t m_hostcommsBuffer[HOSTCOMMS_I2C_BUFFER_LEN];
static volatile uint16_t m_rxLen;
static uint32_t m_txCount;
static uint32_t m_rxCount;
static uint32_t m_addrCount;

static HOSTCOMMS_Mode_t m_hostcommsMode;

static uint32_t m_lastHostCommandTimeMs __attribute__((section("no_init")));

static bool m_listening;


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern I2C_HandleTypeDef hi2c1;

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

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
	hi2c1.hdmatx->Instance->CMAR = (uint32_t)&m_hostcommsBuffer[2u];

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


// High priority tasks, deal with slave to host commands
void HOSTCOMMS_Service(uint32_t sysTime)
{
	uint8_t readCmdCode;
	uint16_t dataLen = 1u;

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
		readCmdCode = m_hostcommsBuffer[1u];

		// Something is requested by the host
		if (m_hostcommsBuffer[0u] == (I2C1->OAR1 & 0xFE))
		{
			// Is a pijuice request
			if ( (readCmdCode >= 0x80u) && (readCmdCode <= 0x8Fu) )
			{
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode - 0x80u, &m_hostcommsBuffer[2u], &dataLen);
				RtcSetPointer(readCmdCode - 0x80u + dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, &m_hostcommsBuffer[2u], &dataLen);
			}
		}
		else
		{
			// Is an RTC request
			if (readCmdCode <= 0x0Fu)
			{
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode, &m_hostcommsBuffer[2u], &dataLen);
				RtcSetPointer(readCmdCode + dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, &m_hostcommsBuffer[2u], &dataLen);
			}
		}

		hi2c1.hdmatx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmatx->ChannelIndex);
		hi2c1.hdmatx->Instance->CNDTR = HOSTCOMMS_I2C_BUFFER_LEN;
		hi2c1.hdmatx->Instance->CCR |= DMA_CCR_EN;
	}
}


// Low priority tasks, deal with host to slave commands
void HOSTCOMMS_Task(void)
{
	uint16_t dataLen;
	uint8_t readCmdCode;

	if (false == m_listening)
	{
		I2C1->CR1 |= (I2C_CR1_ADDRIE | I2C_CR1_STOPIE);
		m_listening = true;
	}

	if (HOSTCOMMS_MODE_RXC == m_hostcommsMode)
	{
		dataLen = m_rxLen;
		readCmdCode = m_hostcommsBuffer[1u];

		// Something sent from the host
		if (m_hostcommsBuffer[0u] == (I2C1->OAR1 & 0xFEu))
		{
			// Is a pijuice command
			if ( (readCmdCode >= 0x80u) && (readCmdCode <= 0x8Fu) )
			{
				dataLen -= 1u; // first is command
				RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode - 0x80u, &m_hostcommsBuffer[3u], &dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, &m_hostcommsBuffer[2u], &dataLen);
			}
		}
		else
		{
			// Is an RTC command
			if (readCmdCode <= 0x0Fu)
			{
				// rtc emulation range
				dataLen -= 1; // first is command
				RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode, &m_hostcommsBuffer[3u], &dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, &m_hostcommsBuffer[2u], &dataLen);
			}
		}

		// Enable the peripheral
		I2C1->CR1 |= I2C_CR1_PE;

		m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
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


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// INTERRUPT HANDLERS
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------

void I2C1_IRQHandler(void)
{
	const uint8_t addrMatch = (uint8_t)((I2C1->ISR >> 16u) & 0xFEu);

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

				m_hostcommsBuffer[0u] = addrMatch;

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

