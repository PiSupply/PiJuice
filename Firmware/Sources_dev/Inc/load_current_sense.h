/*
 * load_current_sense.h
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#ifndef LOAD_CURRENT_SENSE_H_
#define LOAD_CURRENT_SENSE_H_

#include "stdint.h"

#define HARD_REV_BELOW_2_3	0
#define HARD_REV_2_3_AND_ABOVE	1 // introduced current sensor amp NCS213
#define HARD_REV_UNKNOWN	0xFF

extern uint8_t hardwareRev;
//extern int16_t pow5vIoLoadCurrent;

void LoadCurrentSenseInit(void);
#if !defined(RTOS_FREERTOS)
void LoadCurrentSenseTask(void);
#endif
void MeasurePMOSLoadCurrent(void);
int32_t GetLoadCurrent(void);
int8_t CalibrateLoadCurrent(void);

#endif /* LOAD_CURRENT_SENSE_H_ */
