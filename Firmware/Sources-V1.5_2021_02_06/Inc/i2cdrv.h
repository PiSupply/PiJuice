// ----------------------------------------------------------------------------
/*!
 * @file		i2cdrv.h
 * @author    	John Steggall
 * @date       	31 March 2021
 * @brief       Header file for i2cdrv.c
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef I2CDRV_H_
#define I2CDRV_H_

typedef enum
{
	I2CDRV_EVENT_NONE = 0u,
	I2CDRV_EVENT_TX_COMPLETE = 1u,
	I2CDRV_EVENT_RX_COMPLETE = 2u,
	I2CDRV_EVENT_TX_FAILED = 3u,
	I2CDRV_EVENT_RX_FAILED = 4u
} I2CDRV_Device_Event_t;

typedef enum
{
	I2CDRV_TRANSACTION_TX = 0u,
	I2CDRV_TRANSACTION_RX = 1u
} I2CDRV_TransactionType_t;

typedef enum
{
	I2CDRV_STATUS_READY = 0u,
	I2CDRV_STATUS_BUSY_TX = 1u,
	I2CDRV_STATUS_BUSY_RX = 2u,
	I2CDRV_STATUS_BLOCKED = 255u
} I2CDRV_Device_Status;

typedef struct
{
	I2C_TypeDef * p_i2cInstance;
	DMA_TypeDef * p_dmaTXInstance;
	DMA_TypeDef * p_dmaRXInstance;
	DMA_Channel_TypeDef * p_dmaTXChannelInstance;
	DMA_Channel_TypeDef * p_dmaRXChannelInstance;
	uint8_t rxDmaChannelIndex;
	uint8_t txDmaChannelIndex;
	I2CDRV_Device_Event_t event;
	uint8_t data[I2CDRV_MAX_BUFFER_SIZE];
	uint8_t datalen;
	I2CDRV_Device_Status status;
	uint32_t transactTime;
	uint16_t timeout;
	uint8_t index;
} I2CDRV_Device_t;

typedef void (*I2CDRV_EventCb_t)(const I2CDRV_Device_t * const p_i2cdevice);


void I2CDRV_Init(const uint32_t sysTime);
void I2CDRV_Service(const uint32_t sysTime);
void I2CDRV_Shutdown(void);

bool I2CDRV_IsReady(uint8_t devIdx);
bool I2CDRV_Transact(const uint8_t deviceIdx, const uint8_t addr,
					const uint8_t * const data,	const uint8_t len,
					I2CDRV_TransactionType_t transactType, const I2CDRV_EventCb_t callback,
					const uint16_t timeout, const uint32_t sysTime);

#endif
