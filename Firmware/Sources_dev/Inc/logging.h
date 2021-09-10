/*
 * logging.h
 *
 *  Created on: 06/24/2021
 *      Author: milan
 */

#ifndef LOGGING_H_
#define LOGGING_H_

#include "stdint.h"
#include "stm32f0xx_hal.h"

#define LOG_BUF_SIZE	1024*1 // must be 2^n
#define LOG_BUF_MASK	0x3FF//((uint16_t)LOG_BUF_SIZE-1) //0x1FFF
#define LOG_BUF_FRAME_SIZE	32
#define LOG_MAX_MESSAGES	(LOG_BUF_SIZE/LOG_BUF_FRAME_SIZE)
#define LOG_MSG_LEN	31

typedef enum {
	NO_LOG = 0,
	LOG_MESSAGE,
	LOG_VALUE,
	LOG_RESERVED1,
	LOG_5VREG_ON,
	LOG_5VREG_OFF,
	WAKEUP_EVT,
	ALARM_EVT,
	MCU_RESET,
	LOG_RESERVED2,
	ALARM_WRITE
} LogMsgId_T;

typedef struct __attribute__((packed))
{
	uint32_t capacity; // mAh
} WakeupLog_T;

void LoggingInit(void);
void LogPut(LogMsgId_T id);
void LoggingReadMessageCmd(uint8_t data[], uint16_t *len);
int8_t LoggingWriteConfigCmd(uint8_t data[], uint16_t len);
uint8_t *LoggingInitMessage(LogMsgId_T id);

#endif /* LOGGING_H_ */
