/*
 * crc8_atm.h
 *
 *  Created on: 06.12.2016.
 *      Author: mil
 */

#ifndef CRC8_ATM_H_
#define CRC8_ATM_H_

#include "stdint.h"

uint8_t Crc8Block( uint8_t crc, uint8_t *data, uint8_t len );

#endif /* CRC8_ATM_H_ */
