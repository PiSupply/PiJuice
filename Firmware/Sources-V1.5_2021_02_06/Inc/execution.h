/*
 * execution.h
 *
 *  Created on: 26.03.2018.
 *      Author: milan
 */

#ifndef EXECUTION_H_
#define EXECUTION_H_

typedef enum
{
	EXECUTION_STATE_POWER_ON = 0,
	EXECUTION_STATE_POWER_RESET = 0x11111111ul,
	EXECUTION_STATE_NORMAL = 0xAAAAAAAAul,
	EXECUTION_STATE_UPDATE = 0xA1A15151,
	EXECUTION_STATE_CONFIG_RESET = 0xA5A5A5A5
} EXECUTION_State_t;

extern EXECUTION_State_t executionState;

#endif /* EXECUTION_H_ */
