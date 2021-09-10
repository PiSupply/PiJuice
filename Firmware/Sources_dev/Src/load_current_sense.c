/*
 * load_current_sense.c
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */


#include "load_current_sense.h"
#include "analog.h"
#include "nv.h"
#include "time_count.h"
#include "power_source.h"
#include "execution.h"

#if defined(RTOS_FREERTOS)
#include "cmsis_os.h"

static void LoadCurrentSenseTask(void *argument);

static osThreadId_t currSenseTaskHandle;

static const osThreadAttr_t currSenseTask_attributes = {
	.name = "currSenseTask",
	.priority = (osPriority_t) osPriorityNormal,
	.stack_size = 128
};
#endif

#define ID_T_POLY_COEFF_VDG_START 	240
#define ID_T_POLY_COEFF_VDG_END 	800
#define ID_T_POLY_COEFF_VDG_INC 	10
#define ID_T_POLY_COEFF_LEN 		(((int16_t)ID_T_POLY_COEFF_VDG_END - ID_T_POLY_COEFF_VDG_START) / ID_T_POLY_COEFF_VDG_INC + 1)

// Table of poly coefficients of approximated PMOS drain current dependence on temperature and drain to gate voltage
						 	 	 	 	  //{-0.0188,-0.0191,-0.0185,-0.0175,-0.0161,-0.0144,-0.0125,-0.0103,-0.008,-0.0055,-0.0029,0.0001,0.003,0.0058,0.0086,0.011,0.0135,0.0158,0.0173,0.0189,0.0198,0.0203,0.0199,0.019,0.0173,0.015,0.0113,0.0068,0.0011,-0.0056,-0.0137,-0.0231,-0.0339,-0.0463,-0.0603,-0.0763,-0.0938,-0.1126,-0.1336,-0.1558,-0.1802,-0.1558,-0.1558};
static const float a[ID_T_POLY_COEFF_LEN] = {0.00672, 0.0065, 0.00628, 0.00606, 0.00584, 0.00562, 0.0054, 0.00518, 0.00496, 0.00474, 0.00452, 0.0043, 0.00408, 0.00386, 0.00364, 0.00342, 0.0032, 0.00298, 0.00276, 0.00254, 0.00232, 0.0021, 0.00188, 0.00166, 0.00144, 0.00122, 0.001, 0.00065, 0.0003, 0,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
		//{ 0.0103, 0.0099, 0.0095, 0.0091, 0.0087, 0.0083, 0.0079, 0.0075, 0.0071, 0.0067, 0.0063, 0.0059, 0.0055, 0.0051, 0.0047, 0.0043, 0.0039, 0.0035, 0.0031, 0.0027, 0.0023, 0.0019, 0.0015, 0.0011, 0.0007, 0.0003, -0.0001, -0.0005, -0.0009, -0.0013,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
		//{ 0.018148, 0.01765, 0.017132, 0.016594, 0.016036, 0.015458, 0.01486, 0.014242, 0.013604, 0.012946, 0.012268, 0.01157, 0.010852, 0.010114, 0.009356, 0.008578, 0.00778, 0.006962, 0.006124, 0.005266, 0.004388, 0.00349, 0.002572, 0.001634, 0.000676, -0.000302, -0.0013, -0.002318, -0.003356, -0.004414,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
		//{0.02796, 0.028, 0.02796, 0.02784, 0.02764, 0.02736, 0.027, 0.02656, 0.02604, 0.02544, 0.02476, 0.024, 0.02316, 0.02224, 0.02124, 0.02016, 0.019, 0.01776, 0.01644, 0.01504, 0.01356, 0.012, 0.01036, 0.00864, 0.00684, 0.00496, 0.003, 0.00096, -0.00116, -0.00336,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
		//{0.03632, 0.03545, 0.03452, 0.03353, 0.03248, 0.03137, 0.0302, 0.02897, 0.02768, 0.02633, 0.02492, 0.02345, 0.02192, 0.02033, 0.01868, 0.01697, 0.0152, 0.01337, 0.01148, 0.00953, 0.00752, 0.00545, 0.00332, 0.00113, -0.00112, -0.00343, -0.0058, -0.00823, -0.01072, -0.01327,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
//{0,0,0,0,0,0,0,0,0,0,0,0,0.0028,0.0031,0.0028,0.0026,0.0024,0.0024,0.0024,0.0024,0.0025,0.0025,0.0026,0.0026,0.0026,0.0026,0.0025,0.0023,0.002,0.0016,-0.0051,-0.0092,-0.0139,-0.0193,-0.0254,-0.0323,-0.0399,-0.0483,-0.0576,-0.0677,-0.0788,-0.0909,-0.104,-0.1181,-0.1311,-0.1458,-0.1612,-0.1774,-0.1945,-0.2123,-0.231,-0.2506,-0.271,-0.2922,-0.3144,-0.3374,-0.3614};
						 	 	 	 	  //{2.1296,2.0782,1.9572,1.8184,1.6645,1.4981,1.3222,1.1398,0.9543,0.7691,0.588,0.3775,0.1915,0.0219,-0.1394,-0.2502,-0.3679,-0.4625,-0.479,-0.4989,-0.4544,-0.3769,-0.2106,0.0011,0.2771,0.6116,1.0613,1.5835,2.1978,2.8995,3.7209,4.6451,5.6802,6.8469,8.1411,9.5966,11.16,12.828,14.671,16.588,18.676,16.588,16.588};
static const float b[ID_T_POLY_COEFF_LEN] = {0.2169,0.1571,0.1201,0.1042,0.1078,0.1295,0.1676,0.2207,0.2872,0.3655,0.4541,0.5514,0.3989,0.4694,0.5615,0.6495,0.7352,0.8204,0.9068,0.9962,1.0904,1.1911,1.3001,1.4193,1.5503,1.695,1.8551,2.0323,2.2286,2.4456,3.0549,3.5263,4.0515,4.6335,5.2757,5.981,6.7528,7.5942,8.5084,9.4986,10.568,11.719,12.957,14.282,15.501,16.858,18.278,19.763,21.312,22.926,24.607,26.354,28.169,30.052,32.004,34.025,36.117};
									      //{-49.855,-47.355,-43.582,-39.622,-35.508,-31.274,-26.956,-22.594,-18.229,-13.905,-9.6695,-4.7774,-0.339,3.8561,8.0158,11.287,14.931,18.354,20.432,22.947,24.503,25.822,25.767,25.318,24.128,22.378,18.922,14.729,9.4428,3.2338,-4.5243,-13.402,-23.5,-35.189,-48.31,-63.502,-79.554,-96.315,-115.23,-134.03,-154.69,-134.05,-134.06};
static const float c[ID_T_POLY_COEFF_LEN] = {-8.2299,-4.4796,-1.6241,0.4295,1.774,2.5026,2.708,2.4833,1.9213,1.115,0.1574,-0.8587,3.632,3.669,4.126,5.003,6.3,8.017,10.154,12.711,15.688,19.085,22.902,27.139,31.796,36.873,42.37,48.287,54.624,61.381,68.558,76.155,84.172,92.609,101.47,110.74,120.44,130.56,141.09,152.05,163.43,175.23,187.44,200.08,217.37,234.16,252.12,271.29,291.73,313.5,336.64,361.2,387.24,414.82,443.99,474.79,507.28};
// ID = a * T^2 + b * T + c
// a = a[index], b = b[index], c = c[index]
// index = (VDG - ID_T_POLY_COEFF_VDG_START) / ID_T_POLY_COEFF_VDG_INC

uint8_t hardwareRev __attribute__((section("no_init")));
uint8_t currSensorTypeDetCnt __attribute__((section("no_init")));

static int32_t currBuffer[16] = {0};
static uint8_t currBufferInd = 0;

volatile static uint8_t kta;
volatile static uint8_t ktb;

volatile static int16_t resLoadCurrCalib = 0;

uint8_t pow5vIoResLoadCurrentStat[16];

int16_t pow5vIoPMOSLoadCurrent = 0;
static int32_t pow5vIoResLoadCurrent = 0;


static int32_t GetLoadCurrentNCS() {
	int32_t v5vSample = GetSampleAverage(0);
	if (v5vSample < 1500) return 0; // SENSOR IS OFF IF 5v IS OFF, v5V < 2.2V * 1.1
	/*float volatile v5v = (GetSampleAverage(0)*aVdd)>>11;//Get5vIoVoltage();//GetSampleVoltage(0) * 2;
	float volatile vrefin = aVdd*27/74; ~1204
	float volatile vref = vrefin + (v5v-vrefin)*17.148/1017.148; // 17.148K = 47K || 27K
	float volatile vout = ((GetSampleAverage(1)*aVdd)>>12); //GetSampleVoltage(1);//
	int32_t volatile vsens =  (( 1469 + ((GetSampleAverage(0)*138)>>12) - GetSampleAverage(1) )*aVdd)>>12; //( 4096*27/74*(1000/1017.148) + GetSampleAverage(0)*2*17.148/1017.148 - GetSampleAverage(1) )*aVdd/4096;//vref - vout;
*/
	volatile int32_t current =  (( 1469 + ((v5vSample*138)>>12) - GetSampleAverage(1) )*aVdd*10+1)>>14;//vsens*5/2;//vsens*1000/50/8;//(vref - vout)*1000/50/8;//(float)aVdd*(GetSampleAverage(1) - 4096*4)*5/4096/8;//
	return current;
}

static float GetRefLoadCurrent() {
	int32_t vdg = 4790 - ((GetSample(POW_DET_SENS_CHN)*aVdd)>>11);//ANALOG_GET_VDG_AVG();
	//vdg *= vdgCalibCoeff * mcuTemperature;
	//vdg >>= 10;
	int16_t i = vdg >= ID_T_POLY_COEFF_VDG_START ? (vdg - ID_T_POLY_COEFF_VDG_START + ID_T_POLY_COEFF_VDG_INC / 2) / ID_T_POLY_COEFF_VDG_INC : 0;
	i = i >= ID_T_POLY_COEFF_LEN ? ID_T_POLY_COEFF_LEN - 1 : i;

	volatile float current = a[i] * mcuTemperature * mcuTemperature + b[i] * mcuTemperature + c[i];
	return current > 0 ? current : 0;
}

static int32_t GetResSenseCurrent(void) {
	volatile int32_t samAvg = GetSampleAverageDiff(0, 1);
	return (samAvg * aVdd * 25) >> 8; // 4096 * 2 * aVdd * 100;
}

int32_t GetLoadCurrent(void) {
	if (hardwareRev == HARD_REV_UNKNOWN) {
		//volatile int32_t cr = GetSampleAverage(0);
		return -1;//pow5vIoResLoadCurrent>>4;
	} else if (hardwareRev == HARD_REV_2_3_AND_ABOVE) {
		//volatile int32_t rcr = GetLoadCurrentNCS();//GetResSenseCurrent();
		return pow5vIoResLoadCurrent >>4;
		//return ((currBuffer[0] + currBuffer[1] + currBuffer[2] + currBuffer[3] + currBuffer[4] + currBuffer[5] + currBuffer[6] + currBuffer[7]) >> 3);
	} else {
		if ( pow5vInDetStatus == POW_5V_IN_DETECTION_STATUS_NOT_PRESENT ) {
			if ( POW_5V_BOOST_EN_STATUS() )  {
				if (((pow5vIoResLoadCurrent>>4) - resLoadCurrCalib) < 800 && ((pow5vIoResLoadCurrent>>4) - resLoadCurrCalib) > -300 )
					return pow5vIoPMOSLoadCurrent;
				else
					return (pow5vIoResLoadCurrent>>4) - resLoadCurrCalib;
			} else {
				return 0;
			}
		} else {
			return (pow5vIoResLoadCurrent>>4) - resLoadCurrCalib;
		}
		//return ((currBuffer[0] + currBuffer[1] + currBuffer[2] + currBuffer[3] + currBuffer[4] + currBuffer[5] + currBuffer[6] + currBuffer[7]) >> 3) - resLoadCurrCalib;
		/*uint8_t i;
		int8_t cnt = 0;
		int32_t sum = 0;
		int16_t d = pow5vIoResLoadCurrent < 0 ? - (pow5vIoResLoadCurrent >> 2) : pow5vIoResLoadCurrent >> 2;
		int16_t h = pow5vIoResLoadCurrent + d;
		int16_t r = pow5vIoResLoadCurrent - d;
		for (i=0; i < 16; i++) {
			if (currBuffer[i] < h && currBuffer[i] > r) {
				sum += currBuffer[i];
				cnt ++;
			}
		}

		return cnt ? sum / cnt - resLoadCurrCalib : pow5vIoResLoadCurrent - resLoadCurrCalib;*/
		return (pow5vIoResLoadCurrent>>4) - resLoadCurrCalib;
	}
}

void MeasurePMOSLoadCurrent(void) {
	pow5vIoPMOSLoadCurrent = ((kta * mcuTemperature + (((uint16_t)ktb) << 8) ) * ((int32_t)(GetRefLoadCurrent()+0.5))) >> 13; //ktNorm * k12 * refCurr
}

void GetCurrStat(uint8_t stat[]) {
	uint8_t i;
	for (i = 0; i < 8; i++) {
		uint16_t ind = (ADC_BUFFER_LENGTH/8) * i;
		int16_t diff = ((ADC_GET_BUFFER_SAMPLE(ind) - ADC_GET_BUFFER_SAMPLE(ind+1)) >> 1) + 8;
		if (diff > 15) diff = 15;
		if (diff < 0) diff = 0;
		stat[diff] ++;
	}
}

#if defined(RTOS_FREERTOS)
void LoadCurrentSenseTask(void *argument) {
	for(;;)
	{
		osDelay(98);
		if (AnalogSamplesReady()) {
			volatile int32_t newCurr = GetResSenseCurrent();
			if (newCurr < 3000 && newCurr > -3000) {
				uint8_t i = (currBufferInd++)&0x0F;
				pow5vIoResLoadCurrent -= currBuffer[i] >> 4;
				currBuffer[i] = newCurr;
				pow5vIoResLoadCurrent += currBuffer[i] >> 4;
			}
		}
	}
}
#else
void LoadCurrentSenseTask(void) {
	if (AnalogSamplesReady()) {
		if (hardwareRev == HARD_REV_UNKNOWN) {
			if (currSensorTypeDetCnt < 16) {
				volatile int32_t v5v = Get5vIoVoltage();//GetSampleAverage(0);
				if (v5v > 4500) {
					uint8_t i = (currBufferInd++)&0x0F;
					pow5vIoResLoadCurrent -= currBuffer[i];// >> 4;
					currBuffer[i] =  GetResSenseCurrent();
					pow5vIoResLoadCurrent += currBuffer[i];// >> 4;
					currSensorTypeDetCnt ++;
				}
			} else {
				if ((pow5vIoResLoadCurrent>>4) < 3000) {
					hardwareRev = HARD_REV_BELOW_2_3;
				} else {
					hardwareRev = HARD_REV_2_3_AND_ABOVE;
				}
			}
		} else {
			volatile int32_t newCurr;
			if (hardwareRev == HARD_REV_2_3_AND_ABOVE) {
				newCurr = GetLoadCurrentNCS();
			} else {
				newCurr = GetResSenseCurrent();
			}
			if (newCurr > 4000 || newCurr < -3000) {
				newCurr = 0;
			}

			uint8_t i = (currBufferInd++)&0x0F;
			pow5vIoResLoadCurrent -= currBuffer[i];// >> 4;
			currBuffer[i] = newCurr;
			pow5vIoResLoadCurrent += currBuffer[i];// >> 4;
		}
	}

	/*int32_t sum = 0;
	for (i = 0; i < 16; i++) sum += currBuffer[i];
	pow5vIoResLoadCurrent = sum >> 4;*/

	/*GetCurrStat(pow5vIoResLoadCurrentStat);
	if ( !(i&0x0F) ) {
		uint8_t j;
		uint8_t maxStat = 0;
		volatile int8_t maxInd = 0;
		for (j = 0; j < 16; j++) {
			if (pow5vIoResLoadCurrentStat[j] > maxStat) {
				maxStat = pow5vIoResLoadCurrentStat[j];
				maxInd = j;
			}
			//pow5vIoResLoadCurrentStat[j] = 0;
		}

		int16_t s = 0, cnt = 0;

		for (j = maxInd - 2; j <= (maxInd+2); j++) {
			s += (int16_t)pow5vIoResLoadCurrentStat[j] * j;
			cnt += pow5vIoResLoadCurrentStat[j];
		}
		for (j = 0; j < 16; j++)  pow5vIoResLoadCurrentStat[j] = 0;
		pow5vIoResLoadCurrent = ( (int32_t)(((float)s/cnt)-8) * 2 * aVdd * 25) >> 8;
	}*/
}
#endif

static int8_t ReadILoadCalibCoeffs(void) {
	kta = 0; // invalid value
	ktb = 0; // invalid value
	resLoadCurrCalib = 0;

	uint16_t var;
	EE_ReadVariable(VDG_ILOAD_CALIB_KTA_NV_ADDR, &var);
	if ( (((~var)&0xFF) != (var>>8)) ) return 1;
	kta = var&0xFF;

	EE_ReadVariable(VDG_ILOAD_CALIB_KTB_NV_ADDR, &var);
	if ( (((~var)&0xFF) != (var>>8)) ) return 1;
	ktb = var&0xFF;

	EE_ReadVariable(RES_ILOAD_CALIB_ZERO_NV_ADDR, &var);
	if ( (((~var)&0xFF) != (var>>8)) ) return 1;
	uint8_t c = var&0xFF;

	if (c == 127) {
		hardwareRev = HARD_REV_2_3_AND_ABOVE;
		resLoadCurrCalib = 0;
	} else {
		hardwareRev = HARD_REV_BELOW_2_3;
		resLoadCurrCalib = (int16_t)10 * ((int8_t)(var&0xFF));
	}

	return 0;
}

void LoadCurrentSenseInit(void) {
	hardwareRev = HARD_REV_UNKNOWN;

	ReadILoadCalibCoeffs();

	currSensorTypeDetCnt = 0;

	if (executionState == EXECUTION_STATE_POWER_ON) {
		int i=0;
		while(i<16) {currBuffer[i] = 0; i++;}
	}

#if defined(RTOS_FREERTOS)
	currSenseTaskHandle = osThreadNew(LoadCurrentSenseTask, (void*)NULL, &currSenseTask_attributes);
#endif
}

int8_t CalibrateLoadCurrent(void) {

	resLoadCurrCalib = (pow5vIoResLoadCurrent>>4) - 51;  // 5.1v/100ohm

	Turn5vBoost(1);
	if (!POW_5V_BOOST_EN_STATUS()) return 1;
	Power5VSetModeLDO();
	DelayUs(10000);

	float ktNorm = 0.0052 * mcuTemperature + 0.9376;
	volatile float curr = GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	DelayUs(10000);
	curr += GetRefLoadCurrent();
	//if ( curr > (4*52) || curr < (52/4) ) return 2;
	float k12 = (float)52 * 8 / (curr * ktNorm); // = k / ktNorm;
	kta = 0.0052 * k12 * 1024 * 8;
	ktb = 0.9376 * k12 * 32;
	EE_WriteVariable(VDG_ILOAD_CALIB_KTA_NV_ADDR, kta | ((uint16_t)~kta<<8));
	EE_WriteVariable(VDG_ILOAD_CALIB_KTB_NV_ADDR, ktb | ((uint16_t)~ktb<<8));

	volatile int32_t ncsCurr = GetLoadCurrentNCS() - 50;
	if (ncsCurr < 40 && ncsCurr > -40) {
		// NCS current sensor is used
		uint8_t c = 127; // 127 - code that means NCS sensor is detected in calibration process
		EE_WriteVariable(RES_ILOAD_CALIB_ZERO_NV_ADDR, c | ((uint16_t)~c<<8));
	} else {
		uint8_t c = resLoadCurrCalib / 10;
		EE_WriteVariable(RES_ILOAD_CALIB_ZERO_NV_ADDR, c | ((uint16_t)~c<<8));
		if (resLoadCurrCalib > 1250 || resLoadCurrCalib < -1250) return 2;
	}

	if ( ReadILoadCalibCoeffs() != 0 ) return 3;

	return 0;
}
