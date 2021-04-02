/*
 * load_current_sense.h
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#ifndef LOAD_CURRENT_SENSE_H_
#define LOAD_CURRENT_SENSE_H_

void LoadCurrentSenseInit(void);
void MeasurePMOSLoadCurrent(void);
int16_t ISENSE_GetLoadCurrentMa(void);
int8_t CalibrateLoadCurrent(void);
void CalibrateZeroCurrent(void);
void Calibrate50mACurrent(void);
void Calibrate500mACurrent(void);

#endif /* LOAD_CURRENT_SENSE_H_ */
