/*
 * i2cdrv.c
 *
 *  Created on: 23 Mar 2021
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <string.h>

#include "main.h"
#include "system_conf.h"

#include "time_count.h"
#include "i2cdrv.h"

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void I2CDRV_ProcessDevice(I2CDRV_Device_t * p_device, uint32_t sysTime);


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:

extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static I2CDRV_Device_t m_devices[I2CDRV_MAX_DEVICES] =
{
		{
				.p_i2cInstance = I2C1,
				.status = I2CDRV_STATUS_BLOCKED,
				.index = 0u
		},
		{
				.p_i2cInstance = I2C2,
				.status = I2CDRV_STATUS_BLOCKED,
				.index = 1u
		}
};

static I2CDRV_EventCb_t m_deviceCallbacks[I2CDRV_MAX_DEVICES] = {NULL, NULL};


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// INTERRUPT HANDLERS
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
void I2C2_IRQHandler(void)
{
	I2CDRV_Device_t * p_device = &m_devices[1u];

	if (0u != (I2C2->ISR & I2C_ISR_TXIS))
	{
		// Transmit complete of address

		I2C2->CR1 &= ~I2C_CR1_TXIE;

		// Start transmitting data via dma
		if (p_device->status == I2CDRV_STATUS_BUSY_TX)
		{
			p_device->p_dmaTXChannelInstance->CCR |= DMA_CCR_EN;
		}
		else if(p_device->status == I2CDRV_STATUS_BUSY_RX)
		{
			I2C2->CR1 |= I2C_CR1_TCIE;

			I2C2->TXDR = p_device->data[0u];
		}
	}
	else if (0u != (I2C2->ISR & I2C_ISR_TC))
	{
		// Transmit complete indicates the address has been sent

		// Clear the interrupt enable
		I2C2->CR1 &= ~I2C_CR1_TCIE;

		// Enable the dma channel for receiving the data from the device
		p_device->p_dmaRXChannelInstance->CCR |= DMA_CCR_EN;

		// Clear previous datalen (may not be needed)
		I2C2->CR2 &= ~I2C_CR2_NBYTES;

		// Start the transfer!!
		// Start read is an extra bit from start write RD_WRN
		I2C2->CR2 |=  (p_device->datalen << I2C_CR2_NBYTES_Pos) | I2C_AUTOEND_MODE | (I2C_GENERATE_START_READ);
	}
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * I2CDRV_Init configures the module to a known initial state
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void I2CDRV_Init(const uint32_t sysTime)
{
	// Tell the dma controllers about the data addresses
	// They'll have already been configured for operation by CubeMX in the msp module.
	// Steal all the useful values out of the info structs

	m_devices[1u].p_dmaTXInstance = hi2c2.hdmatx->DmaBaseAddress;
	m_devices[1u].p_dmaTXChannelInstance = hi2c2.hdmatx->Instance;

	// Sneaky stm actually makes the channel index the isr and icr pos
	m_devices[1u].txDmaChannelIndex = hi2c2.hdmatx->ChannelIndex;

	m_devices[1u].p_dmaRXInstance = hi2c2.hdmarx->DmaBaseAddress;
	m_devices[1u].p_dmaRXChannelInstance = hi2c2.hdmarx->Instance;

	// Sneaky stm actually makes the channel index the isr and icr pos
	m_devices[1u].rxDmaChannelIndex = hi2c2.hdmarx->ChannelIndex;

	m_devices[1u].p_dmaTXChannelInstance->CPAR = (uint32_t)&I2C2->TXDR;
	m_devices[1u].p_dmaTXChannelInstance->CMAR = (uint32_t)m_devices[1u].data;

	m_devices[1u].p_dmaRXChannelInstance->CPAR = (uint32_t)&I2C2->RXDR;
	// Point to the 3rd byte as the first will contain the address to read
	// The second will contain the device address with the read flag set (bit 0)
	m_devices[1u].p_dmaRXChannelInstance->CMAR = (uint32_t)&m_devices[1u].data[2u];

	// Enable the DMA transfer mode
	I2C2->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN;

	// Turn off device until we're ready to use it.
	I2C2->CR1 &= ~I2C_CR1_PE;

	// Disable own address
	I2C2->OAR1 = 0u;


	m_devices[1u].status = I2CDRV_STATUS_READY;
}


void I2CDRV_Shutdown(void)
{
	I2C2->CR1 &= ~I2C_CR1_PE;
}


// ****************************************************************************
/*!
 * I2CDRV_Service performs periodic updates for this module
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void I2CDRV_Service(const uint32_t sysTime)
{
	I2CDRV_ProcessDevice(&m_devices[1u], sysTime);
}


// ****************************************************************************
/*!
 * I2CDRV_IsReady returns the ready status of a module device.
 *
 * @param	devIdx		index of the device that is required
 * @retval	bool		true = device is ready
 * 						false = device is busy or just not available
 */
// ****************************************************************************
bool I2CDRV_IsReady(uint8_t devIdx)
{
	if (devIdx < I2CDRV_MAX_DEVICES)
	{
		return m_devices[devIdx].status == I2CDRV_STATUS_READY;
	}

	return false;
}


// ****************************************************************************
/*!
 * I2CDRV_Transact starts a transaction with an i2c device hooked on the bus.
 * The transaction can be transmit or receive but for a receive transaction the
 * module is limited to only one memory address byte. In both cases the device
 * address is limited to the standard 7bit also. The caller is expected to either
 * provide a call back or poll the is isready to see if the transaction is complete,
 * the latter being slightly dangerous if not careful as the device could be used
 * and the data becoming inconsistent.
 *
 * @param	devIdx			index of the device that is required
 * @param	addr			address of the i2c device, pre bit shifted..
 * 							Note: this is important!!
 *
 * @param   data			data for the transaction, if a memory read/write then
 * 							call with the 8 bit memory address in byte 0.
 * 							On completion of a memory read, the 1st two bytes contain
 * 							the memory address in byte 0 and the device address or'd
 * 							with 0x01 as presented to the device (read/write bit).
 *
 * @param	len				length of data to send or length of data to receive, when
 * 							receiving, this does not include the 1st two bytes, just
 * 							the data that is expected back (add one on for checksum
 * 							if the device sends one and it is required.)
 *
 * @param	transactType	I2CDRV_TRANSACTION_TX = transit transaction
 * 							I2CDRV_TRANSACTION_RX = receive transaction
 *
 * @param	callback		pointer to a callback function, if NULL is passed then
 * 							the callback is ignored.
 *
 * @param	sysTime			current value of the ms tick timer
 *
 * @retval	bool			true if the transaction can be loaded
 * 							false if the transaction can't be performed
 *
 */
// ****************************************************************************
bool I2CDRV_Transact(const uint8_t deviceIdx, const uint8_t addr,
					const uint8_t * const data,	const uint8_t len,
					I2CDRV_TransactionType_t transactType, const I2CDRV_EventCb_t callback,
					const uint16_t timeout, const uint32_t sysTime)
{

	if (deviceIdx >= I2CDRV_MAX_DEVICES)
	{
		return false;
	}

	I2CDRV_Device_t * const p_device = &m_devices[deviceIdx];

	if (p_device->status != I2CDRV_STATUS_READY)
	{
		return false;
	}

	if ( (len > sizeof(p_device->data)) || (len == 0u) )
	{
		return false;
	}


	p_device->datalen = len;
	p_device->event = I2CDRV_EVENT_NONE;


	m_deviceCallbacks[deviceIdx] = callback;

	if (transactType == I2CDRV_TRANSACTION_TX)
	{
		// Copy data to transmit
		memcpy(p_device->data, data, len);

		// Transmit action is pretty straightforward.... Just spam out the data.
		p_device->p_i2cInstance->CR2 = I2C_AUTOEND_MODE | (len << I2C_CR2_NBYTES_Pos);

		// Disable DMA channel for transmitter
		p_device->p_dmaTXChannelInstance->CCR &= ~DMA_CCR_EN;

		// Clear all flags
		p_device->p_dmaTXInstance->IFCR = (DMA_FLAG_GL1 << p_device->txDmaChannelIndex);

		// Bung in the amount of data to send
		p_device->p_dmaTXChannelInstance->CNDTR = len;

		p_device->status = I2CDRV_STATUS_BUSY_TX;

	}
	else if (transactType == I2CDRV_TRANSACTION_RX)
	{
		// Receive is more complex, the address needs to be sent in the 1st and maybe 2nd byte
		// the hardware then needs to switch to receive mode and grab the incomming data.

		// This implementation luckily only requires one byte so it'll just send out the address
		// in the first iteration of the interrupt routine.

		// Only need the first byte to read
		p_device->data[0u] = data[0u];

		// Populate the address to send and the read flag
		p_device->data[1u] = (addr | 0x01u);

		// Clear rest of the buffer
		memset(&p_device->data[2u], 0u, len);

		// Make sure the stop bit doesn't get set, load in 1 byte for address
		p_device->p_i2cInstance->CR2 = (1u << I2C_CR2_NBYTES_Pos);

		// Disable DMA channel for transmitter
		p_device->p_dmaRXChannelInstance->CCR &= ~DMA_CCR_EN;

		// Clear all flags
		p_device->p_dmaRXInstance->IFCR = (DMA_FLAG_GL1 << p_device->rxDmaChannelIndex);

		// Bung in the amount of data to expect
		p_device->p_dmaRXChannelInstance->CNDTR = len;

		// Notify what the device is busy doing
		p_device->status = I2CDRV_STATUS_BUSY_RX;

	}
	else
	{
		return false;
	}

	// Note transaction start time
	MS_TIMEREF_INIT(p_device->transactTime, sysTime);

	// Load in the max time for the transaction
	p_device->timeout = timeout;

	// Enable the transmit interrupt and turn on the peripheral
	p_device->p_i2cInstance->CR1 |= I2C_CR1_PE | I2C_CR1_TXIE;

	// Initiate the transaction
	p_device->p_i2cInstance->CR2 |= (addr | I2C_GENERATE_START_WRITE);

	return true;
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * I2CDRV_ProcessDevice checks the i2c hardware and the dma hardware to see if
 * the transaction is complete or has any errors. Registered callbacks are
 * processed here, errors are not at all analysed but will be cleared either after
 * the callback has been performed (i2c device is disabled) or on next transaction
 * call (dma controller).
 *
 * @param	p_device	pointer to the i2cdrv struct
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void I2CDRV_ProcessDevice(I2CDRV_Device_t * p_device, uint32_t sysTime)
{
	DMA_Channel_TypeDef * p_dmaChannel;
	DMA_TypeDef * p_dma;
	uint8_t dmaChannelPos;


	// If blocked or ready then alles gut
	if ( (I2CDRV_STATUS_BUSY_TX == p_device->status) || (I2CDRV_STATUS_BUSY_RX == p_device->status) )
	{
		p_dmaChannel = (I2CDRV_STATUS_BUSY_TX == p_device->status) ?
												p_device->p_dmaTXChannelInstance :
												p_device->p_dmaRXChannelInstance;

		p_dma = (I2CDRV_STATUS_BUSY_TX == p_device->status) ?
												p_device->p_dmaTXInstance :
												p_device->p_dmaRXInstance;

		dmaChannelPos = (I2CDRV_STATUS_BUSY_TX == p_device->status) ?
												p_device->txDmaChannelIndex :
												p_device->rxDmaChannelIndex;

		// Check for complete flag
		if ( 0u != (p_dma->ISR & (DMA_FLAG_TC1 << dmaChannelPos)) )
		{
			p_device->event = (I2CDRV_STATUS_BUSY_TX == p_device->status) ?
												I2CDRV_EVENT_TX_COMPLETE :
												I2CDRV_EVENT_RX_COMPLETE;
		}
		else
		{
			// Check for errors in data transfer or just a timeout because something else went wrong?
			if (MS_TIMEREF_TIMEOUT(p_device->transactTime, sysTime, p_device->timeout)
					|| (0u != (I2C2->ISR & (I2C_ISR_NACKF | I2C_ISR_BERR | I2C_ISR_ARLO)))
					/*|| (0u != (p_dma->ISR & (DMA_FLAG_TE1 << dmaChannelPos)))*/
			)
			{
				// Disable DMA channel for transmitter
				p_dmaChannel->CCR &= ~DMA_CCR_EN;

				// Notify event
				p_device->event = (I2CDRV_STATUS_BUSY_TX == p_device->status) ?
												I2CDRV_EVENT_TX_FAILED :
												I2CDRV_EVENT_RX_FAILED;
			}
		}
	}

	// Raise on event
	if (p_device->event != I2CDRV_EVENT_NONE)
	{
		if (m_deviceCallbacks[p_device->index] != NULL)
		{
			m_deviceCallbacks[p_device->index](p_device);
		}

		p_device->event = I2CDRV_EVENT_NONE;
		p_device->status = I2CDRV_STATUS_READY;

		// Turn off the device, clears all isr flags but keeps the existing configuration
		p_device->p_i2cInstance->CR1 &= ~I2C_CR1_PE;
	}

	return;
}
