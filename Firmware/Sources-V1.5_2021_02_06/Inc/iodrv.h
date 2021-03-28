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


typedef struct
{
	uint16_t 			value;
	uint8_t				debounceCounter;
	uint32_t			lastDigitalChangeTime;
	uint32_t			lastPosPulseWidthTimeMs;
	uint32_t			lastNegPulseWidthTimeMs;
	uint8_t				adcChannel;
	uint32_t			analogConversionFactor;
	IODRV_PinType_t		pinType;
	uint16_t			gpioPin_bm;
	uint8_t				gpioPin_pos;
	GPIO_TypeDef*		gpioPort;
	uint16_t			invert_bm;
	bool				canConfigure;
} IODRV_Pin_t;


void IODRV_Init(uint32_t sysTime);
void IODRV_Service(uint32_t sysTime);
void IODRV_Shutdown(void);

uint16_t IODRV_ReadPinValue(uint8_t pin);
bool IODRV_ReadPinOutputState(uint8_t pin);
const IODRV_Pin_t * IODRV_GetPinInfo(uint8_t pin);
bool IODRV_SetPinType(uint8_t pin, IODRV_PinType_t newType);
bool IODRV_WritePin(uint8_t pin, bool newValue);
bool IODRV_SetPin(uint8_t pin, bool newValue);
bool IODRV_SetPinPullDir(uint8_t pin, uint32_t pullDirection);

#endif /* IODRV_H_ */
