/*
 * ave_filter.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef AVE_FILTER_H_
#define AVE_FILTER_H_

/* Must not be more than 255, 16 is a good number */
#define AVE_FILTER_ELEMENT_COUNT	16

#if AVE_FILTER_ELEMENT_COUNT > 255u
#error average filter elements exceed indexer capacity
#endif

struct AVE_FILTER_U16_s
{
	uint16_t lastVal;
	uint16_t average;
	uint16_t elements[AVE_FILTER_ELEMENT_COUNT];
	uint32_t total;
	uint8_t nextValueIdx;
};

typedef struct AVE_FILTER_U16_s AVE_FILTER_U16_t;

void AVE_FILTER_U16_Update(AVE_FILTER_U16_t * const p_filter, const uint16_t newValue);
void AVE_FILTER_U16_Reset(AVE_FILTER_U16_t * const aveFilter);


#endif /* AVE_FILTER_H_ */
