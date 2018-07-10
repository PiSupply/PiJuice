/*
 * crc8_atm.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "crc8_atm.h"

#define POLYNOMIAL    (0x1070U << 3)

unsigned char Crc8( unsigned char inCrc, unsigned char inData )
{
    int i;
    unsigned short  data;

    data = inCrc ^ inData;
    data <<= 8;

    for ( i = 0; i < 8; i++ )
    {
                if (( data & 0x8000 ) != 0 )
        {
            data = data ^ POLYNOMIAL;
        }
                data = data << 1;
    }
    return (unsigned char)( data >> 8 );

} // Crc8
/****************************************************************************/
/**
*   Calculates the CRC-8 used as part of SMBus over a block of memory.
*/

uint8_t Crc8Block( uint8_t crc, uint8_t *data, uint8_t len )
{
    while ( len > 0 )
    {
        crc = Crc8( crc, *data++ );
        len--;
    }

    return crc;

} // Crc8Block
