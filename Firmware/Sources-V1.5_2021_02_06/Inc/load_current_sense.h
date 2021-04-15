/*
 * load_current_sense.h
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#ifndef LOAD_CURRENT_SENSE_H_
#define LOAD_CURRENT_SENSE_H_

void ISENSE_Init(void);
void ISENSE_Task(void);

int16_t ISENSE_GetLoadCurrentMa(void);
void ISENSE_CalibrateLoadCurrent(void);
void ISENSE_CalibrateZeroCurrent(void);
void ISENSE_Calibrate51mACurrent(void);
void ISENSE_Calibrate510mACurrent(void);
bool ISENSE_WriteNVCalibration(void);

#endif /* LOAD_CURRENT_SENSE_H_ */
