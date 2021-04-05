/*
 * load_current_sense.c
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#include <stdlib.h>
#include "main.h"

#include "adc.h"
#include "analog.h"
#include "nv.h"
#include "time_count.h"
#include "power_source.h"

#include "system_conf.h"
#include "util.h"
#include "iodrv.h"

#include "load_current_sense.h"
#include "current_sense_tables.h"

#define ISENSE_CAL_POINTS			3u

static uint8_t m_kta;
static uint8_t m_ktb;
static int16_t m_resLoadCurrCalibOffset = 0;
static uint32_t m_resLoadCurrCalibScale_K;

static int16_t m_calibrationPointsR[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsF[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsB[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsI[ISENSE_CAL_POINTS];


static bool ISENSE_ReadNVCalibration(void);
static uint16_t ISENSE_GetPMOSLoadCurrentMa(void);
static float ISENSE_GetRefLoadCurrentMa(void);
static int16_t ISENSE_GetResSenseCurrentMa(void);
static void ISENSE_CalibrateCurrent(const uint8_t pointIdx, const uint16_t currentMa);


void ISENSE_Init(void)
{
	uint8_t i = 0;

	while(i < ISENSE_CAL_POINTS)
	{
		m_calibrationPointsI[i] = 0u;
		i++;
	}

	// Load calibration values from NV
	ISENSE_ReadNVCalibration();
}


void ISENSE_Task(void)
{
	// do something?
}


int16_t ISENSE_GetLoadCurrentMa(void)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const bool ldoEnabled = POWERSOURCE_IsLDOEnabled();
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();
	const uint16_t fetDetectCurrentMa = ISENSE_GetPMOSLoadCurrentMa();

	int16_t result = ISENSE_GetResSenseCurrentMa();
/*
	if (pow5vInDetStatus == POW_SOURCE_NOT_PRESENT)
	{
		if ( (true == boostConverterEnabled) && (true == ldoEnabled) )
		{
			if ( (result > -300) && (result < 800) )
			{
				result = fetDetectCurrentMa;
			}
		}
	}*/

	return result;
}


// Return value not used.
bool ISENSE_CalibrateLoadCurrent(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const int16_t loadSenseResMa = ISENSE_GetResSenseCurrentMa();
	float curr;
	float k12;
	float ktNorm;

	m_resLoadCurrCalibOffset = (loadSenseResMa - 51);  // 5.1v/100ohm

	POWERSOURCE_Set5vBoostEnable(true);

	if (false == boostConverterEnabled)
	{
		return false;
	}

	POWERSOURCE_SetLDOEnable(true);

	DelayUs(10000u);

	ktNorm = 0.0052f * mcuTemperature + 0.9376f;

	curr = ISENSE_GetRefLoadCurrentMa();

	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();
	DelayUs(10000u);
	curr += ISENSE_GetRefLoadCurrentMa();

	k12 = 52.0f * 8 / (curr * ktNorm);

	m_kta = 0.0052f * k12 * 1024u * 8u;
	m_ktb = 0.9376f * k12 * 32u;

	EE_WriteVariable(VDG_ILOAD_CALIB_KTA_NV_ADDR, m_kta | ((uint16_t)~m_kta<<8));

	EE_WriteVariable(VDG_ILOAD_CALIB_KTB_NV_ADDR, m_ktb | ((uint16_t)~m_ktb<<8));

	uint8_t c = (m_resLoadCurrCalibOffset / 10u);

	EE_WriteVariable(RES_ILOAD_CALIB_ZERO_NV_ADDR, c | ((uint16_t)~c<<8));

	if (m_resLoadCurrCalibOffset > 1250 || m_resLoadCurrCalibOffset < -1250)
	{
		return false;
	}

	return ISENSE_ReadNVCalibration();
}


// No resistance on RPi 5V
void ISENSE_CalibrateZeroCurrent(void)
{
	ISENSE_CalibrateCurrent(0u, 0u);
}


// Fit 94R to RPi 5V (ca. 51mA @ 4.8v)
void ISENSE_Calibrate51mACurrent(void)
{
	ISENSE_CalibrateCurrent(1u, 50u);
}


// Fit 9.4R to RPi 5V (ca. 510mA @ 4.8v)
void ISENSE_Calibrate510mACurrent(void)
{
	ISENSE_CalibrateCurrent(2u, 510u);
}


bool ISENSE_WriteNVCalibration(void)
{
	uint8_t i = ISENSE_CAL_POINTS;
	uint32_t spanK = 0u;
	int16_t spanIAct = m_calibrationPointsI[ISENSE_CAL_POINTS - 1u] - m_calibrationPointsI[0u];
	int16_t spanIMeas = m_calibrationPointsR[ISENSE_CAL_POINTS - 1u] - m_calibrationPointsR[0u];

	// Spans must be positive and not 0.
	if ( (spanIAct <= 0) || (spanIMeas <= 0) )
	{
		return false;
	}

	// Find the span correction value
	if (false == UTIL_FixMulInverse_U16_U16(spanIAct, spanIMeas, &spanK))
	{
		return false;
	}

	// Span can only be 0 to 1.99999999999
	if (spanK >= 0x20000u)
	{
		return false;
	}

	// Lose a bit of resolution to fit in a uint16 slot in NV and store.
	spanK >>= 1u;

	NV_WriteVariable_U8(ISENSE_RES_SPAN_L, (uint8_t)(spanK & 0xFFu));

	spanK >>= 8u;

	NV_WriteVariable_U8(ISENSE_RES_SPAN_H, (uint8_t)(spanK & 0xFFu));

	NV_WriteVariable_S8(RES_ILOAD_CALIB_ZERO_NV_ADDR, m_calibrationPointsR[0u] / 10);

	// Reset cal points
	while(i < ISENSE_CAL_POINTS)
	{
		m_calibrationPointsI[i] = 0u;
	}

	// TODO - test mid point
	// TODO - calibrate power detect fet drive

	return ISENSE_ReadNVCalibration();
}


bool ISENSE_ReadNVCalibration(void)
{
	m_kta = 0u; // invalid value
	m_ktb = 0u; // invalid value
	m_resLoadCurrCalibOffset = 0;

	uint8_t tempU8;
	bool readResult = true;

	NV_ReadVariable_U8(VDG_ILOAD_CALIB_KTA_NV_ADDR, &m_kta);
	NV_ReadVariable_U8(VDG_ILOAD_CALIB_KTA_NV_ADDR, &m_ktb);
	NV_ReadVariable_U8(RES_ILOAD_CALIB_ZERO_NV_ADDR, &tempU8);
	m_resLoadCurrCalibOffset = ((int8_t)tempU8 * 10u);


	if (false == NV_ReadVariable_U8(ISENSE_RES_SPAN_L, &tempU8))
	{
		readResult = false;
	}

	m_resLoadCurrCalibScale_K = (tempU8 << 1u);

	if (false == NV_ReadVariable_U8(ISENSE_RES_SPAN_H, &tempU8))
	{
		readResult = false;
	}

	// Check for valid value
	if (true == readResult)
	{
		m_resLoadCurrCalibScale_K |= (tempU8 << 9u);
	}
	else
	{
		// Use default adjustment value if bad
		m_resLoadCurrCalibScale_K = 0x10000u;
	}


	return true;
}


uint16_t ISENSE_GetPMOSLoadCurrentMa(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();

	int16_t result = ((m_kta * mcuTemperature + (((uint16_t)m_ktb) << 8) ) * ((int32_t)(ISENSE_GetRefLoadCurrentMa() + 0.5f))) >> 13;

	// Current can't go backwards here!
	return (result < 0) ? 0 : result;
}


float ISENSE_GetRefLoadCurrentMa(void)
{
	const uint16_t aVdd = ANALOG_GetAVDDMv();
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	float result;
	uint16_t coeffIdx;

	int32_t vdg = 4790u - ( (ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET) * aVdd) >> 11u );

	// Check i isn't minimum
	coeffIdx = (vdg >= ID_T_POLY_COEFF_VDG_START) ?
					(vdg - ID_T_POLY_COEFF_VDG_START + ID_T_POLY_COEFF_VDG_INC / 2u) / ID_T_POLY_COEFF_VDG_INC :
					0u;

	// Check i hasn't maxed out
	if (coeffIdx > ID_T_POLY_LAST_COEFF_IDX)
	{
		coeffIdx = ID_T_POLY_LAST_COEFF_IDX;
	}

	// Should be some brackets here somewhere!
	result = a[coeffIdx] * mcuTemperature * mcuTemperature + b[coeffIdx] * mcuTemperature + c[coeffIdx];

	// Make sure its positive
	return (result > 0) ? result : 0;
}


int16_t ISENSE_GetResSenseCurrentMa(void)
{
	int16_t result = ADC_GetCurrentSenseAverage();

	result = UTIL_FixMul_U32_S16(m_resLoadCurrCalibScale_K, result);

	return result - m_resLoadCurrCalibOffset;
}


void ISENSE_CalibrateCurrent(const uint8_t pointIdx, const uint16_t currentMa)
{
	const bool boostEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const bool ldoEnabled = POWERSOURCE_IsLDOEnabled();

	if (pointIdx >= 3u)
	{
		return;
	}

	// Adjust filter period to try and stabilise the sense resistor.
	ADC_SetIFilterPeriod(1000u);

	m_calibrationPointsI[pointIdx] = currentMa;

	// Override any config settings
	IODRV_SetPin(IODRV_PIN_POWDET_EN, false);
	IODRV_SetPin(IODRV_PIN_POW_EN, true);

	HAL_Delay(FILTER_PERIOD_MS_CS1 * AVE_FILTER_ELEMENT_COUNT);

	// Get boost converter voltage
	m_calibrationPointsB[pointIdx] = ADC_CalibrateValue(
									ADC_GetAverageValue(ANALOG_CHANNEL_CS1)
									);

	// Enable LDO
	IODRV_SetPin(IODRV_PIN_POWDET_EN, true);

	// Wait for the filter to work.
	HAL_Delay(1000u * AVE_FILTER_ELEMENT_COUNT);

	// Get current sense value
	m_calibrationPointsR[pointIdx] = ADC_GetCurrentSenseAverage();

	// Get LDO drive value
	m_calibrationPointsF[pointIdx] = ADC_CalibrateValue(
									ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET)
									);

	// Restore normal I sense filter period
	ADC_SetIFilterPeriod(FILTER_PERIOD_MS_ISENSE);

	// Restore power regulation
	POWERSOURCE_SetLDOEnable(ldoEnabled);
	POWERSOURCE_Set5vBoostEnable(boostEnabled);
}
