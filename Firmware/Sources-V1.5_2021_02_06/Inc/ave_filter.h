// ----------------------------------------------------------------------------
/*!
 * @file		ave_filter.h
 * @author    	John Steggall
 * @date       	19 March 2021
 * @brief       Header file for ave_filter.h
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef AVE_FILTER_H_
#define AVE_FILTER_H_

#include "system_conf.h"

#if AVE_FILTER_ELEMENT_COUNT > 255u
#error average filter elements exceed indexer capacity
#endif

struct AVE_FILTER_U16_s
{
	uint16_t lastVal;
	uint16_t average;
	uint16_t elements[AVE_FILTER_ELEMENT_COUNT];
	uint32_t total;
	uint16_t filterPeriodMs;
	uint32_t lastFilterUpdateTime;
	uint8_t nextValueIdx;
};

typedef struct AVE_FILTER_U16_s AVE_FILTER_U16_t;

struct AVE_FILTER_S32_s
{
	int32_t lastVal;
	int32_t average;
	int32_t elements[AVE_FILTER_ELEMENT_COUNT];
	int64_t total;
	uint16_t filterPeriodMs;
	uint32_t lastFilterUpdateTime;
	uint8_t nextValueIdx;
};


typedef struct AVE_FILTER_S32_s AVE_FILTER_S32_t;


void AVE_FILTER_U16_Update(AVE_FILTER_U16_t * const p_filter, const uint16_t newValue);
void AVE_FILTER_U16_UpdatePeriodic(AVE_FILTER_U16_t * const p_filter, const uint16_t newValue, const uint32_t sysTime);
void AVE_FILTER_U16_Reset(AVE_FILTER_U16_t * const aveFilter);
void AVE_FILTER_U16_InitPeriodic(AVE_FILTER_U16_t * const aveFilter, const uint32_t sysTime, const uint16_t filterPeriodMs);

void AVE_FILTER_S32_Update(AVE_FILTER_S32_t * const p_filter, const int32_t newValue);
void AVE_FILTER_S32_UpdatePeriodic(AVE_FILTER_S32_t * const p_filter, const int32_t newValue, const uint32_t sysTime);
void AVE_FILTER_S32_Reset(AVE_FILTER_S32_t * const aveFilter);
void AVE_FILTER_S32_InitPeriodic(AVE_FILTER_S32_t * const aveFilter, const uint32_t sysTime, const uint16_t filterPeriodMs);

#endif /* AVE_FILTER_H_ */
