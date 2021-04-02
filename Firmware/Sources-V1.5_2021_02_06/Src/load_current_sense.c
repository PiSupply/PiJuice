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


static uint8_t m_kta;
static uint8_t m_ktb;
static int16_t m_resLoadCurrCalibOffset = 0;
static uint32_t m_loadCurrCalibScale_K;

static bool ISENSE_ReadNVCalibration(void);
static uint16_t ISENSE_GetPMOSLoadCurrentMa(void);
static float ISENSE_GetRefLoadCurrentMa(void);
int16_t ISENSE_GetResSenseCurrentMa(void);


void ISENSE_Init(void)
{
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
	const int16_t loadSenseResMa = ISENSE_GetResSenseCurrentMa();
	const uint16_t fetDetectCurrentMa = ISENSE_GetPMOSLoadCurrentMa();

	int16_t result = 0;

	if (pow5vInDetStatus == POW_SOURCE_NOT_PRESENT)
	{
		if ( (true == boostConverterEnabled) && (true == ldoEnabled) )
		{
			if ( (loadSenseResMa > -300) && (loadSenseResMa < 800) )
			{
				result = fetDetectCurrentMa;
			}
		}
	}

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


void ISENSE_CalibrateZeroCurrent(void)
{

}


void ISENSE_Calibrate50mACurrent(void)
{

}


void ISENSE_Calibrate500mACurrent(void)
{

}


bool ISENSE_WriteNVCalibration(void)
{
	return ISENSE_ReadNVCalibration();
}


bool ISENSE_ReadNVCalibration(void)
{
	m_kta = 0u; // invalid value
	m_ktb = 0u; // invalid value
	m_resLoadCurrCalibOffset = 0;

	uint16_t var;
	EE_ReadVariable(VDG_ILOAD_CALIB_KTA_NV_ADDR, &var);

	if (false == UTIL_NV_ParamInitCheck_U16(var))
	{
		return false;
	}

	m_kta = (uint8_t)(var&0xFFu);

	EE_ReadVariable(VDG_ILOAD_CALIB_KTB_NV_ADDR, &var);

	if (false == UTIL_NV_ParamInitCheck_U16(var))
	{
		return false;
	}

	m_ktb = (uint8_t)(var&0xFFu);

	EE_ReadVariable(RES_ILOAD_CALIB_ZERO_NV_ADDR, &var);

	if (false == UTIL_NV_ParamInitCheck_U16(var))
	{
		return false;
	}

	m_resLoadCurrCalibOffset = ((int8_t)(var&0xFF)) * 10u;

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

	bool negCurrent = (result < 0);

	result = UTIL_FixMul_U32_U16(m_loadCurrCalibScale_K, abs(result));

	if (true == negCurrent)
	{
		result = -result;
	}

	return result - m_resLoadCurrCalibOffset;
}
