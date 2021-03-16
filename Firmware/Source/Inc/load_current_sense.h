/*
 * load_current_sense.h
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#ifndef LOAD_CURRENT_SENSE_H_
#define LOAD_CURRENT_SENSE_H_

#include "stdint.h"

//extern int16_t pow5vIoLoadCurrent;

void LoadCurrentSenseInit(void);
#if !defined(RTOS_FREERTOS)
void LoadCurrentSenseTask(void);
#endif
void MeasurePMOSLoadCurrent(void);
int32_t GetLoadCurrent(void);
int8_t CalibrateLoadCurrent(void);

#endif /* LOAD_CURRENT_SENSE_H_ */
