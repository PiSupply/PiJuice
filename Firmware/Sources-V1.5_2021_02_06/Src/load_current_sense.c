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

typedef struct
{
	uint16_t iActual;
	uint16_t iFet;
	int16_t iRes;
	uint16_t vBoost;
	uint8_t temperature;
} ISENSE_CalPoint_t;


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

static bool ISENSE_ReadNVCalibration(void);
static uint16_t ISENSE_CalculatePMOSLoadCurrentMa(void);
static int16_t ISENSE_CalculateResSenseCurrentMa(void);
static void ISENSE_CalibrateCurrent(const uint8_t pointIdx, const uint16_t currentMa);
static float ISENSE_ConvertFETDrvToX(const uint16_t fetDrvAdc, const int8_t temperature);
static void ISENSE_CalculateLoadCurrentMa(void);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint8_t m_kta;
static uint8_t m_ktb;
static int16_t m_resLoadCurrCalibOffset = 0;
static uint32_t m_resLoadCurrCalibScale_K;
static ISENSE_CalPoint_t m_calPoints[ISENSE_CAL_POINTS];
static uint32_t m_lastCurrentUpdateTimeMs;
static int16_t m_loadCurrentMa;
static int16_t m_loadResMa;
static int16_t m_loadFetMa;


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * ISENSE_Init configures the module to a known initial state, calibration values
 * are initialised.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Init(void)
{
	uint8_t i = 0;

	while(i < ISENSE_CAL_POINTS)
	{
		m_calPoints[i].iActual = 0u;
		i++;
	}

	m_loadCurrentMa = 0;

	// Load calibration values from NV
	ISENSE_ReadNVCalibration();

	MS_TIME_COUNTER_INIT(m_lastCurrentUpdateTimeMs);
}


// ****************************************************************************
/*!
 * ISENSE_Task performs periodic updates for this module, calculates the load current.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	// do something?
	if (MS_TIMEREF_TIMEOUT(m_lastCurrentUpdateTimeMs, sysTime, ISENSE_UPDATE_PERIOD))
	{
		MS_TIMEREF_INIT(m_lastCurrentUpdateTimeMs, sysTime);

		ISENSE_CalculateLoadCurrentMa();
	}
}


// ****************************************************************************
/*!
 * ISENSE_GetLoadCurrentMa returns the current in milliamps being sourced, can
 * be from the fet drive if the RPi does not have it's own power source or from
 * the load sense resistor depending on whether the battery is charging and the
 * VSys output is on. The measurement from the load sense resistor is quite noisy
 * but does give a stab in the dark figure! A positive current means the current
 * direction is flowing out of the pijuice (charge source/battery).
 *
 * @param	none
 * @retval	int16_t
 */
// ****************************************************************************
int16_t ISENSE_GetLoadCurrentMa(void)
{
	return m_loadCurrentMa;
}


// ****************************************************************************
/*!
 * ISENSE_GetSenseResistorMa returns the current in milliamps being sourced as
 * detected by the load sense resistor. The los SNR means this is quite noisy
 * but does give a stab in the dark figure! A positive current means the current
 * direction is flowing out of the pijuice (charge source/battery).
 *
 * @param	none
 * @retval	int16_t		current in mA detected by the sense resistor
 */
// ****************************************************************************
int16_t ISENSE_GetSenseResistorMa(void)
{
	return m_loadResMa;
}


// ****************************************************************************
/*!
 * ISENSE_GetFetMa returns the current in milliamps being sourced as
 * detected by the LDO fet drive. The signal is quite stable and relatively accurate
 * but can only be used when the LDO is enabled and the current is being sourced.
 *
 * Note: if the LDO is disabled then this value is invalid.
 *
 * @param	none
 * @retval	uint16_t	current in mA detected by the LDO fet drive
 */
// ****************************************************************************
uint16_t ISENSE_GetFetMa(void)
{
	return m_loadFetMa;
}


// ****************************************************************************
/*!
 * ISENSE_CalibrateLoadCurrent performs a single point calibration at 50mA, being
 * a single point the resistor will not be as accurate but the fet drive is unaffected.
 * Will take a while as the current sense filter is slowed down to try and get
 * a fairly stable figure.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void ISENSE_CalibrateLoadCurrent(void)
{
	ISENSE_CalibrateCurrent(ISENSE_CAL_POINT_MID, 50u);

	float k12;
	float ktNorm;

	m_resLoadCurrCalibOffset = m_calPoints[ISENSE_CAL_POINT_MID].iRes - 51u;
	m_resLoadCurrCalibScale_K = 0x10000u;

	NV_WriteVariable_S8(RES_ILOAD_CALIB_ZERO_NV_ADDR, m_resLoadCurrCalibOffset / 10u);

	NV_WipeVariable(ISENSE_RES_SPAN_L);
	NV_WipeVariable(ISENSE_RES_SPAN_H);

	ktNorm = 0.0052f * m_calPoints[ISENSE_CAL_POINT_MID].temperature + 0.9376f;

	k12 = 52.0f / (ISENSE_ConvertFETDrvToX(m_calPoints[ISENSE_CAL_POINT_MID].iFet, m_calPoints[ISENSE_CAL_POINT_MID].temperature) * ktNorm);

	m_kta = 0.0052f * k12 * 1024u * 8u;
	m_ktb = 0.9376f * k12 * 32u;

	NV_WriteVariable_U8(VDG_ILOAD_CALIB_KTA_NV_ADDR, m_kta);
	NV_WriteVariable_U8(VDG_ILOAD_CALIB_KTB_NV_ADDR, m_ktb);
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
 * ISENSE_WriteNVCalibration takes the mid point and the high points of calibration
 * to dial out the errors in the load resistor. The resistor appears to be non linear
 * around the 0 point so the 0 offset is interpolated from the mid point. The fet
 * drive calibration is performed from the mid point only. Both the mid and the
 * low points muist be different and correct in polarity or the calibration will
 * fail not be performed. The values are written to NV memory.
 *
 * Note: zero point is not required.
 *
 * @param	none
 * @retval	bool		false = calibration not performed
 * 						true = calibration performed and values updated
 */
// ****************************************************************************
bool ISENSE_WriteNVCalibration(void)
{
	uint8_t i = 0u;
	uint32_t spanK = 0u;
	int16_t spanIAct = m_calPoints[ISENSE_CAL_POINT_HIGH].iActual - m_calPoints[ISENSE_CAL_POINT_MID].iActual;
	int16_t spanIMeas = m_calPoints[ISENSE_CAL_POINT_HIGH].iRes - m_calPoints[ISENSE_CAL_POINT_MID].iRes;
	float k12;
	float ktNorm;

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

	NV_WriteVariable_S8(RES_ILOAD_CALIB_ZERO_NV_ADDR, (m_calPoints[ISENSE_CAL_POINT_MID].iRes - m_calPoints[ISENSE_CAL_POINT_MID].iActual) / 10u);

	if (0u != m_calPoints[ISENSE_CAL_POINT_MID].iActual)
	{
		ktNorm = 0.0052f * m_calPoints[ISENSE_CAL_POINT_MID].temperature + 0.9376f;

		k12 = (float)m_calPoints[ISENSE_CAL_POINT_MID].iActual / (ISENSE_ConvertFETDrvToX(m_calPoints[ISENSE_CAL_POINT_MID].iFet, m_calPoints[ISENSE_CAL_POINT_MID].temperature) * ktNorm);

		m_kta = 0.0052f * k12 * 1024u * 8u;
		m_ktb = 0.9376f * k12 * 32u;

		NV_WriteVariable_U8(VDG_ILOAD_CALIB_KTA_NV_ADDR, m_kta);
		NV_WriteVariable_U8(VDG_ILOAD_CALIB_KTB_NV_ADDR, m_ktb);
	}

	// Reset cal points
	while(i < ISENSE_CAL_POINTS)
	{
		m_calPoints[i].iActual = 0u;
		i++;
	}

	return ISENSE_ReadNVCalibration();
}


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * ISENSE_CalculateLoadCurrentMa calculates and populates the module values for
 * both methods of current sense (fet and res). Depending on the mode of operation
 * it will choose which method is the most appropriate and populate the main value
 * to be retrieved using ISENSE_GetLoadCurrentMa.
 *
 * Note: if the LDO is off then the value calculated for the fet will be invalid.
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
static void ISENSE_CalculateLoadCurrentMa(void)
{
	const bool boostConverterEnabled = POWERSOURCE_IsBoostConverterEnabled();
	const bool ldoEnabled = POWERSOURCE_IsLDOEnabled();
	const PowerSourceStatus_T pow5vInDetStatus = POWERSOURCE_Get5VRailStatus();
	const BatteryStatus_T batteryStatus = BATTERY_GetStatus();
	const bool vSysEnabled = POWERSOURCE_IsVsysEnabled();

	m_loadFetMa = ISENSE_CalculatePMOSLoadCurrentMa();
	m_loadResMa = ISENSE_CalculateResSenseCurrentMa();

	if (pow5vInDetStatus == POW_SOURCE_NOT_PRESENT)
	{
		if (true == boostConverterEnabled)
		{
			// TODO - Work out why these values
			if ( (m_loadResMa > -300) && (m_loadResMa < 800) && (true == ldoEnabled) )
			{
				m_loadCurrentMa = m_loadFetMa;
			}
			else
			{
				m_loadCurrentMa = m_loadResMa;
			}
		}
		else
		{
			// No power in or out
			m_loadCurrentMa = 0;
		}
	}
	else
	{
		// Check to see if there should be any current being exchanged
		if ( (BAT_STATUS_CHARGING_FROM_5V_IO == batteryStatus) || (true == vSysEnabled) || true == boostConverterEnabled )
		{
			// Have to us the sense resistor unfortunately
			m_loadCurrentMa = m_loadResMa;
		}
		else
		{
			m_loadCurrentMa = 0;
		}
	}
}


// ****************************************************************************
/*!
 * ISENSE_ReadNVCalibration read the NV memory and populates the calibration figures.
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
	NV_ReadVariable_U8(VDG_ILOAD_CALIB_KTB_NV_ADDR, &m_ktb);
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

	return readResult;
}


// ****************************************************************************
/*!
 * ISENSE_CalculatePMOSLoadCurrentMa derives the current being drawn across the
 * LDO fet by fitting a preset curve and modifying using the CPU temperature.
 *
 * @param	none
 * @retval	uint16_t		current being drawn from the LDO in mA
 */
// ****************************************************************************
static uint16_t ISENSE_CalculatePMOSLoadCurrentMa(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const uint16_t powDet = ADC_CalibrateValue(ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET));
	const float iNorm = ISENSE_ConvertFETDrvToX(powDet, mcuTemperature);

	int32_t result = ((m_kta * mcuTemperature + (((uint16_t)m_ktb) << 8) ) * ((int32_t)(iNorm + 0.5))) >> 13;

	// Current can't go backwards here!
	return (result > 0) ? result : 0u;
}


// ****************************************************************************
/*!
 * ISENSE_ConvertFETDrvToX returns a value that has a relationship to the amount
 * of current being drawn across the FET. This can be used as a factor for converting
 * the current using the reference tables.
 *
 * @param	fetDrv				fet drive value from ADC
 * @param	temperature			temperature in degrees
 * @retval	float				converted value
 */
// ****************************************************************************
static float ISENSE_ConvertFETDrvToX(const uint16_t fetDrvAdc, const int8_t temperature)
{
	const uint16_t fetDrvConv = UTIL_FixMul_U32_U16(ISENSE_POWDET_K, fetDrvAdc);
	const uint16_t vdg = (fetDrvConv > 4790u) ? 0u : 4790u - fetDrvConv;

	uint16_t coeffIdx;
	float result;

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
	result = a[coeffIdx] * temperature * temperature + b[coeffIdx] * temperature + c[coeffIdx];

	// Make sure its positive
	return (result > 0.0f) ? result : 0.0f;
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
static int16_t ISENSE_CalculateResSenseCurrentMa(void)
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
 * should be 0mA, 51mA and 510mA. The mid point 51mA is required to calibrate the
 * FET drive. The boost converter and LDO enables are overridden and returned back
 * to their original state. The routine will block for the amount of time it takes
 * to fill the current sense filter. Could be a while.
 *
 * @param	pointIdx		calibration point index
 * @param	currentMa		actual current applied to the 5v rail
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

	m_calPoints[pointIdx].iActual = currentMa;

	// Override any config settings
	IODRV_SetPin(IODRV_PIN_POWDET_EN, false);
	IODRV_SetPin(IODRV_PIN_POW_EN, true);

	HAL_Delay(FILTER_PERIOD_MS_CS1 * AVE_FILTER_ELEMENT_COUNT);

	// Get boost converter voltage
	m_calPoints[pointIdx].vBoost = ANALOG_Get5VRailMv();

	// Enable LDO
	IODRV_SetPin(IODRV_PIN_POWDET_EN, true);

	// Wait for the filter to work.
	HAL_Delay(1000u * AVE_FILTER_ELEMENT_COUNT);

	// Log current sense value
	m_calPoints[pointIdx].iRes = ADC_GetCurrentSenseAverage();

	// Log LDO drive value
	m_calPoints[pointIdx].iFet = ADC_CalibrateValue(
									ADC_GetAverageValue(ANALOG_CHANNEL_POW_DET)
									);

	// Log temperature
	m_calPoints[pointIdx].temperature = ANALOG_GetMCUTemp();

	// Restore normal I sense filter period
	ADC_SetIFilterPeriod(FILTER_PERIOD_MS_ISENSE);

	// Restore power regulation
	POWERSOURCE_SetLDOEnable(ldoEnabled);
	POWERSOURCE_Set5vBoostEnable(boostEnabled);
}
