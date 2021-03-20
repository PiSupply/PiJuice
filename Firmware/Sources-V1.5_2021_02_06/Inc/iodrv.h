/*
 * iodrv.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef IODRV_H_
#define IODRV_H_

#include <stdbool.h>

typedef enum
{
	IOTYPE_NONE = 0u,
	IOTYPE_ANALOG = 1u,
	IOTYPE_DIGIN = 2u,
	IOTYPE_DIGOUT_PUSHPULL = 3u,
	IOTYPE_DIGOUT_OPENDRAIN = 4u,
	IOTYPE_PWM_PUSHPULL = 5u,
	IOTYPE_PWM_OPENDRAIN = 6u,
	IOTYPES_COUNT = 7u	/* Strictly 6 but 6 is the last one and 0 is ignored! */
} IODRV_PinType_t;

void IODRV_Init(uint32_t sysTime);
void IODRV_Service(uint32_t sysTime);
uint16_t IODRV_ReadPin(uint32_t pin);
bool IODRV_SetPinType(uint32_t pin, IODRV_PinType_t newType);
bool IODRV_WritePin(uint32_t pin, uint8_t newValue);
bool IODRV_SetPin(uint32_t pin, uint8_t newValue);
bool IODRV_SetPinPullDir(uint32_t pin, uint32_t pullDirection);

#endif /* IODRV_H_ */
