/*
 * load_current_sense.c
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */
// ----------------------------------------------------------------------------
// Include section - add all #includes here:

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


// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define ISENSE_CAL_POINTS			3u
#define ISENSE_CAL_POINT_LOW		0u
#define ISENSE_CAL_POINT_MID		1u
#define ISENSE_CAL_POINT_HIGH		2u


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static bool ISENSE_ReadNVCalibration(void);
static uint16_t ISENSE_GetPMOSLoadCurrentMa(void);
static uint16_t ISENSE_GetRefLoadCurrentMa(void);
static int16_t ISENSE_GetResSenseCurrentMa(void);
static void ISENSE_CalibrateCurrent(const uint8_t pointIdx, const uint16_t currentMa);
static uint16_t ISENSE_ConvertFETDrvToMa(const uint16_t fetDrvAdc, const int8_t temperature);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint8_t m_kta;
static uint8_t m_ktb;
static int16_t m_resLoadCurrCalibOffset = 0;
static uint32_t m_resLoadCurrCalibScale_K;

static int16_t m_calibrationPointsIRes[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsFET[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsBoost[ISENSE_CAL_POINTS];
static uint16_t m_calibrationPointsIRef[ISENSE_CAL_POINTS];
static int8_t m_calibrationPointsTemp[ISENSE_CAL_POINTS];


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * ISENSE_Init configures the module to a known initial state
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Init(void)
{
	uint8_t i = 0;

	while(i < ISENSE_CAL_POINTS)
	{
		m_calibrationPointsIRef[i] = 0u;
		i++;
	}

	// Load calibration values from NV
	ISENSE_ReadNVCalibration();
}


// ****************************************************************************
/*!
 * ISENSE_Task performs periodic updates for this module
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Task(void)
{
	// do something?
}


// ****************************************************************************
/*!
 * ISENSE_GetLoadCurrentMa
 *
 * @param	none
 * @retval	int16_t
 */
// ****************************************************************************
int16_t ISENSE_GetLoadCurrentMa(void)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const bool ldoEnabled = POWERSOURCE_IsLDOEnabled();
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();
	const uint16_t fetDetectCurrentMa = ISENSE_GetPMOSLoadCurrentMa();

	int16_t result = ISENSE_GetResSenseCurrentMa();

	if (pow5vInDetStatus == POW_SOURCE_NOT_PRESENT)
	{
		if ( (true == boostConverterEnabled) && (true == ldoEnabled) )
		{
			if ( (result > -300) && (result < 800) )
			{
				result = fetDetectCurrentMa;
			}
		}
	}

	return result;
}


// ****************************************************************************
/*!
 * ISENSE_CalibrateLoadCurrent
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
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


// ****************************************************************************
/*!
 * ISENSE_CalibrateZeroCurrent records the adc values for a load current of 0mA.
 *
 * Fit 1M to RPi 5V (ca. 0mA @ 4.8v) before calling this routine.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_CalibrateZeroCurrent(void)
{
	ISENSE_CalibrateCurrent(ISENSE_CAL_POINT_LOW, 0u);
}


// ****************************************************************************
/*!
 * ISENSE_Calibrate51mACurrent records the adc values for a load current of 51mA.
 *
 * Fit 94R to RPi 5V (ca. 51mA @ 4.8v) before calling this routine.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Calibrate51mACurrent(void)
{
	ISENSE_CalibrateCurrent(ISENSE_CAL_POINT_MID, 51u);
}



// ****************************************************************************
/*!
 * ISENSE_Calibrate510mACurrent records the adc values for a load current of 510mA.
 *
 * Fit 9.4R to RPi 5V (ca. 510mA @ 4.8v) before calling this routine.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Calibrate510mACurrent(void)
{
	ISENSE_CalibrateCurrent(ISENSE_CAL_POINT_HIGH, 510u);
}


// ****************************************************************************
/*!
 * ISENSE_WriteNVCalibration
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
bool ISENSE_WriteNVCalibration(void)
{
	uint8_t i = ISENSE_CAL_POINTS;
	uint32_t spanK = 0u;
	int16_t spanIAct = m_calibrationPointsIRef[ISENSE_CAL_POINTS - 1u] - m_calibrationPointsIRef[0u];
	int16_t spanIMeas = m_calibrationPointsIRes[ISENSE_CAL_POINTS - 1u] - m_calibrationPointsIRes[0u];

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

	NV_WriteVariable_S8(RES_ILOAD_CALIB_ZERO_NV_ADDR, m_calibrationPointsIRes[0u] / 10u);

	// Reset cal points
	while(i < ISENSE_CAL_POINTS)
	{
		m_calibrationPointsIRef[i] = 0u;
	}

	// TODO - test mid point
	// TODO - calibrate power detect fet drive

	return ISENSE_ReadNVCalibration();
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * ISENSE_ReadNVCalibration
 *
 * @param	none
 * @retval	bool
 */
// ****************************************************************************
static bool ISENSE_ReadNVCalibration(void)
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


// ****************************************************************************
/*!
 * ISENSE_ReadNVCalibration
 *
 * @param	none
 * @retval	uint16_t
 */
// ****************************************************************************
static uint16_t ISENSE_GetPMOSLoadCurrentMa(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const uint16_t powDet = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET));
	const uint16_t iNorm = ISENSE_ConvertFETDrvToMa(powDet, mcuTemperature);

	int32_t result = (((int32_t)m_kta * mcuTemperature + (((uint16_t)m_ktb) << 8u) ) * ((int32_t)(iNorm + 0.5f))) >> 13u;

	// Current can't go backwards here!
	return (result > 0) ? result : 0u;
}


// ****************************************************************************
/*!
 * ISENSE_ConvertFETDrvToMa
 *
 * @param	fetDrv				fet drive value from ADC
 * @param	temperature			temperature in degrees
 * @retval	uint16_t			converted current in mA
 */
// ****************************************************************************
static uint16_t ISENSE_ConvertFETDrvToMa(const uint16_t fetDrvAdc, const int8_t temperature)
{
	const uint16_t vdg = 4790u - (fetDrvAdc > 4790u) ? fetDrvAdc : 4790u;

	uint16_t coeffIdx;
	int16_t result;

	// Check index isn't minimum
	coeffIdx = (vdg >= ID_T_POLY_COEFF_VDG_START) ?
					( (vdg - ID_T_POLY_COEFF_VDG_START) + (ID_T_POLY_COEFF_VDG_INC / 2u) ) / ID_T_POLY_COEFF_VDG_INC :
					0u;

	// Check index hasn't maxed out
	if (coeffIdx > ID_T_POLY_LAST_COEFF_IDX)
	{
		coeffIdx = ID_T_POLY_LAST_COEFF_IDX;
	}

	// Should be some brackets here somewhere!
	result = (a[coeffIdx] + b[coeffIdx] + c[coeffIdx]) * temperature;

	// Make sure its positive
	return (result > 0) ? (uint16_t)result : 0u;
}


// ****************************************************************************
/*!
 * ISENSE_GetRefLoadCurrentMa
 *
 * @param	none
 * @retval	int16_t
 */
// ****************************************************************************
static uint16_t ISENSE_GetRefLoadCurrentMa(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const uint16_t powDetValue = UTIL_FixMul_U32_U16(ISENSE_POWDET_K, ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET)));
	const uint16_t vdg = 4790u - (powDetValue > 4790u) ? powDetValue : 4790u;

	uint16_t coeffIdx;
	int16_t result;

	// Check index isn't minimum
	coeffIdx = (vdg >= ID_T_POLY_COEFF_VDG_START) ?
					(vdg - ID_T_POLY_COEFF_VDG_START + ID_T_POLY_COEFF_VDG_INC / 2u) / ID_T_POLY_COEFF_VDG_INC :
					0u;

	// Check i hasn't maxed out
	if (coeffIdx > ID_T_POLY_LAST_COEFF_IDX)
	{
		coeffIdx = ID_T_POLY_LAST_COEFF_IDX;
	}

	// Should be some brackets here somewhere!
	result = (a[coeffIdx] + b[coeffIdx] + c[coeffIdx]) * mcuTemperature;

	// Make sure its positive
	return (result > 0) ? (uint16_t)result : 0u;
}


// ****************************************************************************
/*!
 * ISENSE_GetResSenseCurrentMa returns a calibrated value taken from the volt drop
 * across FB2. The measurement is quite noisy due to the low SNR.
 *
 * @param	none
 * @retval	int16_t
 */
// ****************************************************************************
static int16_t ISENSE_GetResSenseCurrentMa(void)
{
	int16_t result = ADC_GetCurrentSenseAverage();

	result = UTIL_FixMul_U32_S16(m_resLoadCurrCalibScale_K, result);

	return result - m_resLoadCurrCalibOffset;
}


// ****************************************************************************
/*!
 * ISENSE_CalibrateCurrent records the values for the current sense resistor, power
 * detect fet drive and the actual current and bundles them into an array so that
 * calibration can be performed on all values. There are 3 points possible to store,
 * should be 0mA, 51mA and 510mA.
 *
 * @param	pointIdx
 * @param	currentMa
 * @retval	none
 */
// ****************************************************************************
static void ISENSE_CalibrateCurrent(const uint8_t pointIdx, const uint16_t currentMa)
{
	const bool boostEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const bool ldoEnabled = POWERSOURCE_IsLDOEnabled();

	if (pointIdx >= 3u)
	{
		return;
	}

	// Adjust filter period to try and stabilise the sense resistor.
	ADC_SetIFilterPeriod(1000u);

	m_calibrationPointsIRef[pointIdx] = currentMa;

	// Override any config settings
	IODRV_SetPin(IODRV_PIN_POWDET_EN, false);
	IODRV_SetPin(IODRV_PIN_POW_EN, true);

	HAL_Delay(FILTER_PERIOD_MS_CS1 * AVE_FILTER_ELEMENT_COUNT);

	// Get boost converter voltage
	m_calibrationPointsBoost[pointIdx] = ADC_CalibrateValue(
									ADC_GetAverageValue(ANALOG_CHANNEL_CS1)
									);

	// Enable LDO
	IODRV_SetPin(IODRV_PIN_POWDET_EN, true);

	// Wait for the filter to work.
	HAL_Delay(1000u * AVE_FILTER_ELEMENT_COUNT);

	// Log current sense value
	m_calibrationPointsIRes[pointIdx] = ADC_GetCurrentSenseAverage();

	// Log LDO drive value
	m_calibrationPointsFET[pointIdx] = ADC_CalibrateValue(
									ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET)
									);

	// Log temperature
	m_calibrationPointsTemp[pointIdx] = ANALOG_GetMCUTemp();

	// Restore normal I sense filter period
	ADC_SetIFilterPeriod(FILTER_PERIOD_MS_ISENSE);

	// Restore power regulation
	POWERSOURCE_SetLDOEnable(ldoEnabled);
	POWERSOURCE_Set5vBoostEnable(boostEnabled);
}
