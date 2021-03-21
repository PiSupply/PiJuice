/*
 * io_control.h
 *
 *  Created on: 23.10.2017.
 *      Author: milan
 */

#ifndef IO_CONTROL_H_
#define IO_CONTROL_H_

void MX_TIM1_Init(void);
void MX_TIM14_Init(void);

void IoControlInit();

void IoSetConfiguarion(uint8_t pin, uint8_t data[], uint8_t len);
void IoGetConfiguarion(uint8_t pin, uint8_t data[], uint16_t *len);

void IoWrite(const uint8_t extIOPinIdx, const uint8_t * const data, const uint16_t len);
void IoRead(const uint8_t extIOPinIdx, uint8_t * const data, uint16_t * const len);


#endif /* IO_CONTROL_H_ */
