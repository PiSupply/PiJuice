/*
 * i2cdrv.c
 *
 *  Created on: 23 Mar 2021
 *      Author: jsteggall
 */

#include <string.h>

#include "main.h"
#include "system_conf.h"

#include "time_count.h"
#include "i2cdrv.h"


extern I2C_HandleTypeDef hi2c1;
extern I2C_HandleTypeDef hi2c2;


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
			I2C2->TXDR = p_device->data[0u];

			I2C2->CR1 |= I2C_CR1_TCIE;
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
		I2C2->CR2 |= ~I2C_CR2_NBYTES;

		// Start the transfer!!
		// Start read is an extra bit from start write RD_WRN
		I2C2->CR2 |=  (p_device->datalen << I2C_CR2_NBYTES_Pos) | (I2C_GENERATE_START_READ);
	}
}


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
	m_devices[1u].p_dmaRXChannelInstance->CMAR = (uint32_t)m_devices[1u].data;

	// Enable the DMA transfer mode
	I2C2->CR1 |= I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN;

	// Turn off device until we're ready to use it.
	I2C2->CR1 &= ~I2C_CR1_PE;

	m_devices[1u].status = I2CDRV_STATUS_READY;
}


void I2CDRV_ProcessDevice(I2CDRV_Device_t * p_device, uint32_t sysTime)
{
	uint8_t isrPos = p_device->txDmaChannelIndex;

	// If blocked or ready then alles gut
	if (I2CDRV_STATUS_BUSY_TX == p_device->status)
	{
		// Check for tx complete
		if ( 0u != (p_device->p_dmaTXInstance->ISR & ((0x1UL << 1u) << isrPos)) )
		{
			p_device->event = I2CDRV_EVENT_TX_COMPLETE;
		}

		// Time out after 100ms, what can be taking that long???
		if (MS_TIMEREF_TIMEOUT(p_device->transactTime, sysTime, 100u))
		{
			// Disable DMA channel for transmitter
			p_device->p_dmaTXChannelInstance->CCR &= ~DMA_CCR_EN;

			p_device->event = I2CDRV_EVENT_TX_FAILED;
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
		p_device->p_i2cInstance->CR1 &= ~I2C_CR1_PE;
	}

	return;
}


void I2CDRV_Service(const uint32_t sysTime)
{

	I2CDRV_ProcessDevice(&m_devices[1u], sysTime);

	// Check for transaction errors
	// Check for receive completion
	// Notify low priority task of events
}


bool I2CDRV_IsReady(uint8_t devIdx)
{
	if (devIdx < I2CDRV_MAX_DEVICES)
	{
		return m_devices[devIdx].status == I2CDRV_STATUS_READY;
	}

	return false;
}


bool I2CDRV_Transact(const uint8_t deviceIdx, const uint8_t addr,
					const uint8_t * const data,	const uint8_t len,
					I2CDRV_TransactionType_t transactType, const I2CDRV_EventCb_t callback,
					const uint32_t sysTime)
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

	if (len > sizeof(p_device->data))
	{
		return false;
	}

	memcpy(p_device->data, data, len);

	p_device->datalen = len;
	p_device->checksumPassed = false;
	p_device->event = I2CDRV_EVENT_NONE;


	m_deviceCallbacks[deviceIdx] = callback;

	if (transactType == I2CDRV_TRANSACTION_TX)
	{
		// Transmit action is pretty straightforward.... Just spam out the data.
		p_device->p_i2cInstance->CR2 = I2C_AUTOEND_MODE;

		// Disable DMA channel for transmitter
		p_device->p_dmaTXChannelInstance->CCR &= ~DMA_CCR_EN;

		// Clear all flags
		p_device->p_dmaTXInstance->IFCR = (DMA_FLAG_GL1 << p_device->txDmaChannelIndex);

		// Bung in the amount of data to send
		p_device->p_dmaTXChannelInstance->CNDTR = len;

		// Not sure if this requires setting every time
		p_device->p_dmaTXChannelInstance->CMAR = (uint32_t)p_device->data;

		p_device->status = I2CDRV_STATUS_BUSY_TX;


	}
	else if (transactType == I2CDRV_TRANSACTION_RX)
	{
		// Receive is more complex, the address needs to be sent in the 1st and maybe 2nd byte
		// the hardware then needs to switch to receive mode and grab the incomming data.

		// This implementation luckily only requires one byte so it'll just send out the address
		// in the first iteration of the interrupt routine.

		// Make sure the stop bit doesn't get set.
		p_device->p_i2cInstance->CR2 = 0u;

		// Disable DMA channel for transmitter
		p_device->p_dmaRXChannelInstance->CCR &= ~DMA_CCR_EN;

		// Clear all flags
		p_device->p_dmaRXInstance->IFCR = (DMA_FLAG_GL1 << p_device->txDmaChannelIndex);

		// Bung in the amount of data to expect
		p_device->p_dmaRXChannelInstance->CNDTR = 1u;

		// Not sure if this requires setting every time
		p_device->p_dmaTXChannelInstance->CPAR = (uint32_t)p_device->data;

		// Notify what the device is busy doing
		p_device->status = I2CDRV_STATUS_BUSY_TX;


	}
	else
	{
		return false;
	}

	// Note transaction start time
	MS_TIMEREF_INIT(p_device->transactTime, sysTime);

	// Enable the transmit interrupt and turn on the peripheral
	p_device->p_i2cInstance->CR1 |= I2C_CR1_PE | I2C_CR1_TXIE;

	// Initiate the transaction
	p_device->p_i2cInstance->CR2 |= (len << I2C_CR2_NBYTES_Pos) | addr | I2C_GENERATE_START_WRITE;

	return true;
}


