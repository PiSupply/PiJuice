// ----------------------------------------------------------------------------
/*!
 * @file		hostcomms.c
 * @author    	John Steggall
 * @date       	31 March 2021
 * @brief       hostcomms module deals with the i2c trasnactions between the
 * 				host and this device. Two distinct addresses are used to facilitate
 * 				commands from the pijuice interface and the emulated RTC.
 * 				The RTC has a dedicated buffer here to give a fast response to
 * 				the kernel driver which seems to be a bit sensitive to clock
 * 				stretching.
 *
 * @note		time references are linked to the last time the service routine
 * 				was run due to the concurrency of the interrupt routine, the
 * 				timer could be compared out of sync and think it's 49.7(ish) days
 * 				behind!
 *
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
#define I2C_CR1_WUPEN					(1u << 18u)

#define RTC_REG_TIME_Msk				(0x7Fu << 0u)
#define RTC_REG_ALARM1_Msk				(0xFu << 7u)
#define RTC_REG_ALARM2_Msk				(0x7u << 11u)
#define RTC_REG_CTRL_Msk				(0x1u << 14u)
#define RTC_REG_STATUS_Msk				(0x1u << 15u)

// 255 is max transaction length, byte 0 will have address, byte 1 will have command code
#define HOSTCOMMS_I2C_BUFFER_LEN		256u


typedef enum
{
	HOSTCOMMS_MODE_WAIT = 0u,
	HOSTCOMMS_MODE_RX = 1u,
	HOSTCOMMS_MODE_RXC = 2u,
	HOSTCOMMS_MODE_TX = 3u,
	HOSTCOMMS_MODE_TX_CLOCK = 4u,
	HOSTCOMMS_MODE_FAULT = 5u
} HOSTCOMMS_Mode_t;


typedef struct
{
	uint8_t addr;
	uint8_t cmd;
	uint32_t time;
} HOSTCOMMS_Msg_t;


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint8_t m_hostcommsBuffer[HOSTCOMMS_I2C_BUFFER_LEN];
static uint16_t m_rxLen;
static uint32_t m_txCount;
static uint32_t m_rxCount;
static uint32_t m_addrCount;
static uint32_t m_i2cBusyTime;
static uint32_t m_i2cResetCount;

static HOSTCOMMS_Mode_t m_hostcommsMode;
static uint32_t m_lastServiceTime;

static uint32_t m_lastHostCommandTimeMs __attribute__((section("no_init")));

static uint8_t m_rtcBuffer[17u];
static uint32_t m_rtcRegUpdate_bm;

// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern I2C_HandleTypeDef hi2c1;

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * HOSTCOMMS_Init configures the module to a known initial state, initialised
 * the peripheral and DMA channels for the data transfer.
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void HOSTCOMMS_Init(const uint32_t sysTime)
{
	uint8_t tempU8;

	HAL_I2C_DisableListen_IT(&hi2c1);

	I2C1->CR1 &= ~(I2C_CR1_PE);

	I2C1->OAR1 = 0u;
	I2C1->OAR2 = 0u;

	m_txCount = 0u;
	m_rxCount = 0u;
	m_addrCount = 0u;

	m_i2cResetCount = 0u;


	if (executionState != EXECUTION_STATE_NORMAL)
	{
		MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
	}


	if (NV_ReadVariable_U8(OWN_ADDRESS1_NV_ADDR, &tempU8))
	{
		I2C1->OAR1 = tempU8;
	}
	else
	{
		// Use default address
		I2C1->OAR1 = (OWN1_I2C_ADDRESS << 1u);
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

	// Clear any digital filter settings
	I2C1->CR1 &= ~(I2C_CR1_DNF_Msk);

	// Enable the DMA transfer mode
	I2C1->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN | I2C_CR1_WUPEN;

	// Enable the interrupts
	I2C1->CR1 |= (I2C_CR1_ADDRIE | I2C_CR1_STOPIE);

	// Enable the i2c device
	I2C1->CR1 |= I2C_CR1_PE;
}


void HOSTCOMMS_PiJuiceAddressSetEnable(const bool enabled)
{
	if (enabled)
	{
		I2C1->OAR1 |= I2C_OAR1_OA1EN;
	}
	else
	{
		I2C1->OAR1 &= I2C_OAR1_OA1EN;
	}
}


// ****************************************************************************
/*!
 * HOSTCOMMS_Service performs the command server read tasks with a higher priority
 * than the write tasks which may contain delays that would cause issue to the
 * osloop system. The peripheral is also checked and cleared of errors here.
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void HOSTCOMMS_Service(const uint32_t sysTime)
{
	uint8_t readCmdCode;
	uint16_t dataLen = 1u;

	if (I2C_ISR_BUSY != (I2C1->ISR & I2C_ISR_BUSY))
	{
		MS_TIMEREF_INIT(m_i2cBusyTime, m_lastServiceTime);
	}

	// Timeout after a second, will catch fault mode
	if ( ( MS_TIMEREF_TIMEOUT(m_lastHostCommandTimeMs, m_lastServiceTime, 100u) &&
			(HOSTCOMMS_MODE_WAIT != m_hostcommsMode) &&
			(HOSTCOMMS_MODE_RXC != m_hostcommsMode) ) ||
			MS_TIMEREF_TIMEOUT(m_i2cBusyTime, m_lastServiceTime, 500u)
			)
	{
		// Something bad must have happened, reset the peripheral
		I2C1->CR1 &= ~(I2C_CR1_PE);

		// Disable the dma controllers
		hi2c1.hdmatx->Instance->CCR &= ~(DMA_CCR_EN);
		hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

		// Re-enable the peripheral and addr interrupt
		I2C1->CR1 |= I2C_CR1_PE;
		I2C1->CR1 |= I2C_CR1_ADDRIE;

		m_i2cResetCount++;
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
				RtcDs1339ProcessRequest(I2C_DIRECTION_RECEIVE, readCmdCode - 0x80u, &m_hostcommsBuffer[1u], &dataLen);
				RtcSetPointer(readCmdCode - 0x80u + dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_READ, &m_hostcommsBuffer[1u], &dataLen);
			}

			hi2c1.hdmatx->Instance->CMAR = (uint32_t)&m_hostcommsBuffer[1u];
			hi2c1.hdmatx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmatx->ChannelIndex);
			hi2c1.hdmatx->Instance->CNDTR = (sizeof(m_hostcommsBuffer) - 1u);
			hi2c1.hdmatx->Instance->CCR |= DMA_CCR_EN;
		}
	}

	m_lastServiceTime = sysTime;
}


// ****************************************************************************
/*!
 * HOSTCOMMS_Task performs the command server write tasks with a lowish priority
 * to make sure any write commands that contain delays that would cause issue to
 * the osloop system. The RTC buffer is updated here too, calling the rtc module
 * where needed after a host write and reading the rtc value ready for the host
 * to fetch.
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void HOSTCOMMS_Task(void)
{
	uint16_t dataLen;
	uint8_t readCmdCode;

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
				RtcDs1339ProcessRequest(I2C_DIRECTION_TRANSMIT, readCmdCode - 0x80u, &m_hostcommsBuffer[2u], &dataLen);
			}
			else
			{
				CmdServerProcessRequest(MASTER_CMD_DIR_WRITE, &m_hostcommsBuffer[1u], &dataLen);
			}
		}

		m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
	}

	// Stop the interrupt so the clock will stretch briefly if an attempt to access device
	I2C1->CR1 &= ~(I2C_CR1_ADDRIE);

	// If the host isn't transferring RTC data, update the buffer here
	// Burns clock cycles and briefly blocks i2c port, maybe some optimisation could be had.
	if (HOSTCOMMS_MODE_TX_CLOCK != m_hostcommsMode)
	{


		if (0u != (m_rtcRegUpdate_bm & RTC_REG_TIME_Msk))
		{
			m_rtcRegUpdate_bm &= ~(RTC_REG_TIME_Msk);

			RtcWriteTime(&m_rtcBuffer[0u], false);
		}

		if (0u != (m_rtcRegUpdate_bm & RTC_REG_ALARM1_Msk))
		{
			m_rtcRegUpdate_bm &= ~(RTC_REG_ALARM1_Msk);

			RtcWriteAlarm1(&m_rtcBuffer[7u], false);
		}

		if (0u != (m_rtcRegUpdate_bm & (RTC_REG_CTRL_Msk | RTC_REG_STATUS_Msk)))
		{
			dataLen = (0u == (m_rtcRegUpdate_bm & RTC_REG_STATUS_Msk)) ? 1u : 2u;
			m_rtcRegUpdate_bm &= ~(RTC_REG_CTRL_Msk | RTC_REG_STATUS_Msk);

			RtcWriteControlStatus(&m_rtcBuffer[0xEu], dataLen);
		}

		m_rtcRegUpdate_bm &= ~(RTC_REG_ALARM2_Msk);

		RtcReadTime(m_rtcBuffer, false);
		RtcReadAlarm1(&m_rtcBuffer[7u], false);
		RtcReadControlStatus(&m_rtcBuffer[0xEu], &dataLen);
	}

	// Restore the ADDRIE state depending on the mode, addrie will get restored on the next spin
	// if rxc has just occurred.
	I2C1->CR1 |= (HOSTCOMMS_MODE_RXC == m_hostcommsMode) ? 0u : I2C_CR1_ADDRIE;
}



// ****************************************************************************
/*!
 * HOSTCOMMS_GetLastCommandAgeMs returns the time elapsed in milliseconds since the last
 * host access.
 *
 * @param	sysTime		current value of the system tick timer
 * @retval	uint32_t	time elapsed in mS since the last host access
 */
// ****************************************************************************
uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime)
{
	return MS_TIMEREF_DIFF(m_lastHostCommandTimeMs, sysTime);
}


// ****************************************************************************
/*!
 * HOSTCOMMS_SetInterrupt just updates the time the last i2c message came in, should
 * already be done in the interrupt routine but just in case.!
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void HOSTCOMMS_SetInterrupt(void)
{

	MS_TIME_COUNTER_INIT(m_lastHostCommandTimeMs);
}


// ****************************************************************************
/*!
 * HOSTCOMMS_ChangeAddress handles the immediate update of an address change for
 * either of the slave addresses. It will be called from the command server so the
 * assumption is that there will not be an ongoing transaction as the hardware will
 * be tied with the RXC event. The address is 7 bits for i2c and it should be
 * pre-shifted before calling. Sett addr type to select the primary or secondary
 * slave address
 *
 * @param	addrType		HOSTCOMMS_PRIMARY_ADDR or HOSTCOMMS_SECONDARY_ADDR
 * @param	addr			7bit pre-shifted address to set
 * @retval	none
 */
// ****************************************************************************
void HOSTCOMMS_ChangeAddress(const uint8_t addrType, const uint8_t addr)
{
	if (addrType < HOSTCOMMS_ADDR_TYPES)
	{
		HAL_I2C_DisableListen_IT(&hi2c1);

		I2C1->CR1 &= ~(I2C_CR1_PE);

		if (HOSTCOMMS_PRIMARY_ADDR == addrType)
		{
			I2C1->OAR1 &= ~(I2C_OAR1_OA1EN);
			I2C1->OAR1 = addr | I2C_OAR1_OA1EN;
		}
		else
		{
			I2C1->OAR2 &= ~(I2C_OAR2_OA2EN);
			I2C1->OAR2 = addr | I2C_OAR2_OA2EN;
		}

		// Enable the interrupts
		I2C1->CR1 |= (I2C_CR1_ADDRIE | I2C_CR1_STOPIE);

		// Enable the i2c device
		I2C1->CR1 |= I2C_CR1_PE;
	}
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
	uint32_t addrClear = I2C_ICR_ADDRCF;
	uint32_t rtcReg_bm;

	MS_TIMEREF_INIT(m_i2cBusyTime, m_lastServiceTime);

	if (I2C_ISR_ADDR == (I2C1->ISR & I2C_ISR_ADDR))
	{
		// Check data direction
		if (I2C_ISR_DIR == (I2C1->ISR & I2C_ISR_DIR))
		{
			// Host is attempting to read

			// Check to make sure the host has given a command
			if ( (HOSTCOMMS_MODE_RX == m_hostcommsMode) &&
					(1u == (m_rxLen = (HOSTCOMMS_I2C_BUFFER_LEN - hi2c1.hdmarx->Instance->CNDTR)))
					)
			{
				// Disable rx dma
				hi2c1.hdmarx->Instance->CCR &= ~(DMA_CCR_EN);

				if (addrMatch == (I2C1->OAR1 & 0xFEu))
				{
					// Is from pijuice, let the service routine handle it
					m_hostcommsMode = HOSTCOMMS_MODE_TX;
					m_hostcommsBuffer[0u] = addrMatch;
				}
				else if (m_hostcommsBuffer[1u] < sizeof(m_rtcBuffer))
				{
					// Is the RTC, can deal with this right now
					hi2c1.hdmatx->Instance->CMAR = (uint32_t)&m_rtcBuffer[m_hostcommsBuffer[1u]];
					hi2c1.hdmatx->DmaBaseAddress->IFCR |= (DMA_FLAG_GL1 << hi2c1.hdmatx->ChannelIndex);
					hi2c1.hdmatx->Instance->CNDTR = sizeof(m_rtcBuffer) - m_hostcommsBuffer[1u];
					hi2c1.hdmatx->Instance->CCR |= DMA_CCR_EN;

					m_hostcommsMode = HOSTCOMMS_MODE_TX_CLOCK;
				}
				else
				{
					m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
				}
			}
			else
			{
				m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
			}
		}
		else
		{
			// Host is sending something
			MS_TIMEREF_INIT(m_lastHostCommandTimeMs, m_lastServiceTime);

			if (HOSTCOMMS_MODE_WAIT == m_hostcommsMode)
			{
				m_rxLen = 0u;

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
			else
			{
				// Out of sync somehow
				m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
			}
		}

		m_addrCount++;

		// Clear the interrupt
		I2C1->ICR |= addrClear;
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
				if (addrMatch == (I2C1->OAR1 & 0xFEu))
				{
					m_hostcommsMode = HOSTCOMMS_MODE_RXC;

					// Let the service routine know the message is to be dealt with, probably not needed
					// as the RTC address is dealt with here.
					m_hostcommsBuffer[0u] = addrMatch;

					// Stretch the next address clock until the command has been dealt with so the
					// host knows the device is busy.
					I2C1->CR1 &= ~(I2C_CR1_ADDRIE);
				}
				else if ( (m_hostcommsBuffer[1u] + m_rxLen) <= sizeof(m_rtcBuffer) )
				{
					// Is for RTC, deal with this now.
					m_rxLen--;

					rtcReg_bm = (1u << (m_hostcommsBuffer[1u] + m_rxLen));

					while (m_rxLen > 0u)
					{
						m_rxLen--;
						rtcReg_bm >>= 1u;

						m_rtcBuffer[m_hostcommsBuffer[1u] + m_rxLen] = m_hostcommsBuffer[2u + m_rxLen];
						m_rtcRegUpdate_bm |= rtcReg_bm;
					}

					m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
				}
				else
				{
					// Out of sync somehow
					m_hostcommsMode = HOSTCOMMS_MODE_FAULT;
				}

				m_rxCount++;
			}
			else
			{
				// This happens when i2cdetect sends an addr followed by a stop condition
				m_hostcommsMode = HOSTCOMMS_MODE_WAIT;
			}
		}
		else if ( (HOSTCOMMS_MODE_TX == m_hostcommsMode) || (HOSTCOMMS_MODE_TX_CLOCK == m_hostcommsMode) )
		{
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

