/*
 * fuel_gauge_lc709203f.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "main.h"

#include "osloop.h"
#include "system_conf.h"


#include "time_count.h"

#include "adc.h"
#include "analog.h"
#include "charger_bq2416x.h"
#include "power_source.h"
#include "execution.h"
#include "nv.h"

#include "i2cdrv.h"
#include "crc.h"

#include "fuel_gauge.h"
#include "fuelguage_conf.h"

uint16_t m_fuelGaugeTemp = 0;
int8_t m_batteryTemp = 25;

volatile int16_t batteryCurrent;

static uint32_t fuelGaugeTaskTimer;

volatile int32_t dischargeRate = 0;
uint32_t dischargeCount;
static uint32_t dischargeCountTemp;


static uint8_t prevBatPresent = 0;

int8_t fuelGaugeI2cErrorCounter = 0;

BatteryTempSenseConfig_T tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
RsocMeasurementConfig_T rsocMeasurementConfig = RSOC_MEASUREMENT_AUTO_DETECT;

int8_t ntcFaultFlag __attribute__((section("no_init")));
uint16_t fgIcId __attribute__((section("no_init")));

const int16_t logTbl[256]={-24562, -21803, -19743, -18097, -16728, -15554, -14528, -13616, -12795, -12050, -11366, -10735, -10149, -9602, -9090, -8607, -8151, -7720, -7310, -6919, -6547, -6190, -5848, -5520, -5205, -4901, -4608, -4326, -4052, -3788, -3532, -3283, -3042, -2808, -2580, -2358, -2142, -1932, -1727, -1527, -1332, -1141, -955, -773, -595, -420, -249, -82, 81, 242, 400, 554, 706, 855, 1002, 1145, 1287, 1426, 1562, 1697, 1829, 1959, 2087, 2213, 2338, 2460, 2581, 2700, 2817, 2932, 3046, 3158, 3269, 3378, 3486, 3593, 3698, 3802, 3904, 4005, 4105, 4204, 4302, 4398, 4494, 4588, 4681, 4773, 4864, 4954, 5043, 5132, 5219, 5305, 5391, 5475, 5559, 5642, 5724, 5805, 5885, 5965, 6044, 6122, 6199, 6276, 6352, 6427, 6501, 6575, 6648, 6721, 6793, 6864, 6935, 7005, 7074, 7143, 7212, 7279, 7347, 7413, 7479, 7545, 7610, 7675, 7739, 7802, 6487, 7188, 7834, 8432, 8990, 9513, 10004, 10467, 10905, 11322, 11718, 12096, 12457, 12803, 13135, 13454, 13761, 14057, 14343, 14619, 14886, 15144, 15395, 15638, 15875, 16104, 16328, 16545, 16757, 16964, 17165, 17362, 17554, 17741, 17925, 18104, 18280, 18451, 18620, 18785, 18947, 19105, 19261, 19413, 19563, 19710, 19855, 19997, 20137, 20274, 20409, 20542, 20673, 20802, 20928, 21053, 21176, 21297, 21416, 21534, 21650, 21764, 21877, 21988, 22098, 22207, 22313, 22419, 22523, 22626, 22728, 22828, 22927, 23025, 23122, 23217, 23312, 23406, 23498, 23589, 23680, 23769, 23858, 23945, 24032, 24117, 24202, 24286, 24369, 24451, 24533, 24613, 24693, 24772, 24851, 24928, 25005, 25081, 25157, 25231, 25305, 25379, 25452, 25524, 25595, 25666, 25736, 25806, 25875, 25944, 26011, 26079, 26146, 26212, 26278, 26343, 26408, 26472, 26536, 26599, 26661, 26724, 26785, 26847, 26908, 26968, 27028, 27088};
const int16_t ocvSocTableNormLipo[256]={4269,3804,3447,3141,2877,2641,2435,2247,2075,1923,1788,1669,1573,1497,1441,1395,1359,1331,1306,1285,1265,1248,1233,1219,1205,1193,1181,1169,1155,1138,1121,1101,1078,1056,1033,1012,993,973,954,937,919,901,884,867,851,833,818,802,788,773,758,747,734,721,711,699,687,676,665,654,642,632,622,609,600,589,578,568,557,547,536,526,517,507,499,489,481,472,462,454,446,438,430,420,412,404,396,388,378,370,362,354,345,338,328,320,310,302,293,284,277,268,260,250,241,232,223,213,204,195,185,175,163,155,144,135,123,113,103,92,82,69,59,47,36,23,12,0,-11,-25,-37,-49,-63,-76,-89,-103,-118,-132,-147,-161,-177,-192,-207,-224,-239,-256,-273,-291,-308,-327,-344,-363,-382,-401,-420,-441,-460,-481,-501,-523,-543,-565,-589,-610,-632,-656,-678,-702,-724,-748,-771,-796,-819,-842,-865,-889,-912,-936,-960,-984,-1008,-1031,-1056,-1079,-1102,-1127,-1150,-1175,-1199,-1224,-1247,-1273,-1297,-1323,-1347,-1371,-1396,-1421,-1446,-1471,-1499,-1525,-1551,-1577,-1604,-1632,-1658,-1685,-1712,-1740,-1768,-1796,-1824,-1851,-1880,-1909,-1938,-1967,-1995,-2024,-2053,-2082,-2112,-2142,-2173,-2203,-2234,-2264,-2296,-2327,-2358,-2388,-2420,-2452,-2484,-2515,-2548,-2580,-2614,-2646,-2679,-2712,-2747,-2780,-2814,-2850,-2884,-2920,-2956,-2993,-3031,-3069,-3110,-3154,-3200,-3256};
const int16_t ocvSocTableNormLifepo4[256]={28097,26548,25030,23690,22426,21239,20117,19105,18094,17174,16307,15467,14648,13910,13247,12597,11996,11441,10945,10475,10040,9663,9293,8975,8686,8399,8155,7934,7715,7522,7342,7178,7007,6856,6700,6554,6403,6253,6109,5967,5814,5677,5539,5413,5285,5172,5054,4949,4832,4730,4629,4528,4428,4330,4227,4137,4045,3947,3859,3774,3695,3618,3532,3448,3371,3297,3221,3152,3076,3007,2946,2880,2803,2741,2676,2608,2535,2475,2401,2326,2259,2192,2124,2045,1977,1910,1829,1763,1690,1624,1554,1485,1417,1358,1293,1228,1171,1114,1058,992,941,881,829,774,728,684,634,585,544,502,461,418,389,353,322,286,255,229,197,172,145,122,101,76,51,33,12,0,-19,-39,-64,-78,-97,-107,-127,-145,-158,-171,-191,-209,-224,-233,-248,-259,-272,-290,-306,-323,-334,-352,-353,-373,-384,-401,-410,-426,-448,-461,-477,-494,-503,-521,-541,-552,-573,-586,-609,-628,-643,-668,-688,-710,-731,-753,-775,-798,-827,-850,-882,-913,-942,-979,-1006,-1042,-1080,-1113,-1149,-1185,-1225,-1264,-1303,-1339,-1379,-1424,-1471,-1517,-1546,-1593,-1628,-1665,-1703,-1745,-1788,-1823,-1861,-1887,-1926,-1958,-1989,-2023,-2060,-2089,-2115,-2140,-2169,-2192,-2220,-2237,-2261,-2284,-2311,-2334,-2343,-2368,-2396,-2410,-2428,-2447,-2464,-2481,-2497,-2519,-2536,-2556,-2578,-2595,-2616,-2635,-2662,-2684,-2705,-2753,-2765,-2795,-2836,-2870,-2908,-2961,-3014,-3082,-3165,-3277,-3432,-3694,-4197,-5466};
uint16_t ocvSocTbl[256] __attribute__((section("no_init")));
uint16_t rSocTbl[256] __attribute__((section("no_init")));
uint8_t rSocTempCompesateTbl[256] __attribute__((section("no_init")));
uint16_t c0 __attribute__((section("no_init")));
int32_t soc __attribute__((section("no_init")));


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void FUELGUAGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice);
bool FUELGUAGE_WriteWord(const uint8_t cmd, const uint16_t word);
bool FUELGUAGE_ReadWord(const uint8_t cmd, uint16_t *const word);


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

static uint16_t m_i2cReadResult;
static bool m_i2cSuccess;
static FUELGUAGE_Status_t m_deviceStatus;
static uint16_t m_batteryMv;
static uint16_t m_socPt1;
static uint8_t m_icInitState;
static uint16_t m_lastSoc;
static uint32_t m_lastSocTimeMs;
static uint8_t m_batteryTemperature;
static RsocMeasurementConfig_T m_rsocMeasurementConfig = RSOC_MEASUREMENT_AUTO_DETECT;
static BatteryTempSenseConfig_T m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
static uint8_t m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;



void FUELGUAGE_I2C_Callback(const I2CDRV_Device_t * const p_i2cdrvDevice)
{
	if (p_i2cdrvDevice->event == I2CDRV_EVENT_RX_COMPLETE)
	{
		crc_t crc = crc_8_init(FUELGUAGE_I2C_ADDR);
		// crc includes address, mem address, address | 0x01, data
		crc = crc_8_update(crc, p_i2cdrvDevice->data, 4u);

		if (crc == p_i2cdrvDevice->data[4u])
		{
			m_i2cReadResult = (uint16_t)p_i2cdrvDevice->data[2u] | (p_i2cdrvDevice->data[3u] << 8u);

			m_i2cSuccess = true;
		}
		else
		{
			m_i2cSuccess = false;
		}
	}
	else if (p_i2cdrvDevice->event == I2CDRV_EVENT_TX_COMPLETE)
	{
		m_i2cSuccess = true;
	}
	else
	{
		m_i2cSuccess = false;
	}
}


bool FUELGUAGE_ReadWord(const uint8_t cmd, uint16_t *const word)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;

	m_i2cSuccess = false;

	transactGood = I2CDRV_Transact(FUELGUAGE_I2C_PORTNO, FUELGUAGE_I2C_ADDR, &cmd, 3u,
						I2CDRV_TRANSACTION_RX, FUELGUAGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	*word = m_i2cReadResult;

	return m_i2cSuccess;
}


bool FUELGUAGE_WriteWord(const uint8_t cmd, const uint16_t word)
{
	const uint32_t sysTime = HAL_GetTick();
	bool transactGood;
	uint8_t writeData[5u] = { cmd, word, (word >> 8u), 0u, 0u};

	crc_t crc = crc_8_init(FUELGUAGE_I2C_ADDR);
	crc = crc_8_update(crc, writeData, 3u);

	writeData[3u] = (uint8_t)crc;

	m_i2cSuccess = false;

	transactGood = I2CDRV_Transact(FUELGUAGE_I2C_PORTNO, FUELGUAGE_I2C_ADDR, &cmd, 4u,
						I2CDRV_TRANSACTION_TX, FUELGUAGE_I2C_Callback,
						1000u, sysTime
						);

	if (false == transactGood)
	{
		return false;
	}

	while(false == I2CDRV_IsReady(FUELGUAGE_I2C_PORTNO))
	{
		// Wait for transfer
	}

	return m_i2cSuccess;
}


bool FUELGUAGE_IcInit(void)
{
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();

	m_icInitState = 0u;

	// Check to see if the device is online
	if (FUELGUAGE_ReadWord(0x11, &fgIcId))
	{
		m_icInitState++;

		// Set operational mode
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_POWER_MODE, POWER_MODE_OPERATIONAL))
		{
			return false;
		}

		m_icInitState++;

		// set APA
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_APA, 0x36u))
		{
			return false;
		}

		m_icInitState++;

		// set change of the parameter
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_PARAM_NO_SET, BATT_PROFILE_1))
		{
			return false;
		}

		m_icInitState++;

		// set APT
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_APT, 0x3000u))
		{
			return false;
		}

		m_icInitState++;

		if ( (NULL != currentBatProfile) && (0xFFFFu != currentBatProfile->ntcB) )
		{
			// Set NTC B constant
			if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_THERMB, currentBatProfile->ntcB))
			{
				return false;
			}
		}

		m_icInitState++;

		// Set NTC mode
		if (false == FUELGUAGE_WriteWord(FG_MEM_ADDR_THERM_TYPE, THERM_TYPE_NTC))
		{
			return false;
		}

		m_temperatureMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

		m_icInitState++;

		m_batteryTemperature = mcuTemperature;

		return true;
	}

	return false;

}

void FUELGUAGE_Init(void)
{
	uint8_t config;

	if (NV_READ_VARIABLE_SUCCESS == NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config))
	{
		tempSensorConfig = (uint8_t)(config & 0x07u);
		rsocMeasurementConfig = (uint8_t)(config >> 4u) & 0x03u;
	}

	if (EXECUTION_STATE_NORMAL != executionState)
	{
		// FuelGaugeDvInit();
	}


	MS_TIME_COUNTER_INIT(fuelGaugeTaskTimer);

#if defined(RTOS_FREERTOS)
	fgTaskHandle = osThreadNew(FuelGaugeTask, (void*)NULL, &fgTask_attributes);
#endif
}

void SocEvaluateDirectDynVoltage(uint16_t batVolt, int32_t dt) {
	// Dynamic voltage state of charge evaluation

}


void SocEvaluateFuelGaugeIc(void)
{
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();

	if (batteryRsoc != prevRsoc && (HAL_GetTick() - dischargeCount) > 500) {
		dischargeRate = ((int32_t)prevRsoc - batteryRsoc) * (int32_t)1843200 / (int32_t)(HAL_GetTick() - dischargeCount);
		//__disable_irq();
		batteryCurrent = (dischargeRate * (currentBatProfile!=NULL ? currentBatProfile->capacity : 10000)) >> 10;
		//__enable_irq();
		dischargeCount = HAL_GetTick();
		dischargeCountTemp = HAL_GetTick();
		prevRsoc = batteryRsoc;
	} else if ((HAL_GetTick() - dischargeCount) > 1843200) {
		dischargeRate = 0;
		dischargeCount = HAL_GetTick();
		dischargeCountTemp = HAL_GetTick();
	} else if ( (HAL_GetTick() - dischargeCountTemp) > 50000) {
		/* Not sure what newCurr is yet
		int16_t newCurr;
		if (dischargeRate!=0) newCurr = (((int32_t)(dischargeRate>0?1:-1)) * (int32_t)1843200 / (int32_t)(HAL_GetTick() - dischargeCount) * (currentBatProfile!=NULL ? currentBatProfile->capacity : 10000)) >> 10;
		else newCurr = 0; */

		dischargeCountTemp = HAL_GetTick();
	}
}


void FUELGUAGE_Task(void)
{
	const uint32_t sysTime = HAL_GetTick();
	const int16_t mcuTemperature = ANALOG_GetMCUTemp();
	const BatteryProfile_T * currentBatProfile = BATTERY_GetActiveProfile();
	const uint16_t battMv = ANALOG_GetBatteryMv();
	uint32_t socTimeDiff;

	uint16_t newBatterySoc;
	uint16_t newBatteryVoltage;
	bool icOnline = true;

	static uint8_t updateCnt;


	if (MS_TIMEREF_TIMEOUT(fuelGaugeTaskTimer, sysTime, FUELGUAGE_TASK_PERIOD_MS))
	{
		if (battMv < FUELGUAGE_MIN_BATT_MV)
		{
			// If the battery voltage is less than the device operating limit then don't even bother!
			m_deviceStatus = FUELGUAGE_STATUS_OFFLINE;
			m_socPt1 = 0u;
			m_battMv = 0u;

			return;
		}

		if ( (FUELGUAGE_STATUS_OFFLINE == m_deviceStatus) )
		{
			if (true == FUELGUAGE_IcInit())
			{
				m_deviceStatus = FUELGUAGE_STATUS_ONLINE;
			}
		}

		if ( (FUELGUAGE_STATUS_ONLINE == m_deviceStatus))
		{
			if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_CELL_MV, &newBatteryVoltage))
			{
				m_batteryMv = newBatteryVoltage;
			}

			if (true == FUELGUAGE_ReadWord(FG_MEM_ADDR_RSOC, &newBatterySoc))
			{

				if (m_lastSoc != newBatterySoc)
				{
					m_lastSoc = newBatterySoc;

					socTimeDiff = MS_TIMEREF_DIFF(m_lastSocTimeMs, HAL_GetTick());
					MS_TIMEREF_INIT(m_lastSocTimeMs, sysTick);
				}



			}
		}
	}

	int32_t dt = MS_TIME_COUNT(fuelGaugeTaskTimer);
	if ( dt > 125 ) {

		MS_TIME_COUNTER_INIT(fuelGaugeTaskTimer);
		uint16_t batVolt = ANALOG_GetBatteryMv();

		if (CHARGER_IS_BATTERY_PRESENT() && batVolt > 2550) {
			if (!prevBatPresent) {
				prevBatPresent = 1;
				if (rsocMeasurementConfig == RSOC_MEASUREMENT_DIRECT_DV) soc = GetSocFromOCV(batVolt);
				return;
			}
			updateCnt++;

			if (rsocMeasurementConfig == RSOC_MEASUREMENT_AUTO_DETECT && fuelGaugeI2cErrorCounter < 5 && fuelGaugeI2cErrorCounter > -5) {
				if ( fuelGaugeI2cErrorCounter >= 0 ) {
					// in case fuel gauge ic is present use it
					if (updateCnt&0x04) SocEvaluateFuelGaugeIc();
				} else {
					// try to reinitialize
					if (FuelGaugeIcInit() != 0) {
						fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter > -127 ? fuelGaugeI2cErrorCounter - 1 : -127;
					} else {
						fuelGaugeI2cErrorCounter = 0;
					}
				}
			} else {
				batteryVoltage = batVolt;
				int32_t ind = soc>>23;
				int32_t dif= (int32_t)ocvSocTbl[ind]-batteryVoltage;
				uint16_t rsoc = ((int32_t)rSocTbl[ind] * rSocTempCompesateTbl[(uint8_t)batteryTemp]) >> 8;
				batteryCurrent = (dif*(((uint32_t)c0*rsoc)>>6))>>10;
				int32_t dSoC = (((int32_t)dif*dt*(((int32_t)596*rsoc)>>8)))>>8;//dif*596*dt/res;
				soc -= dSoC;
				soc = soc<=2139095040?soc:2139095040;
				soc = (soc>=0)?soc:0;
				batteryRsoc = (soc>>7)*125>>21;
			}

			if (updateCnt&0x08)  {
				if ( tempSensorConfig == BAT_TEMP_SENSE_CONFIG_AUTO_DETECT || tempSensorConfig == BAT_TEMP_SENSE_CONFIG_NTC ) {
					if ( fuelGaugeI2cErrorCounter < 5 && fuelGaugeI2cErrorCounter >= 0 ) {
						// if left tries
						if (fuelGaugeTempMode == FUEL_GAUGE_TEMP_MODE_THERMISTOR) {
							// try to read battery temperature from fuel gauge ic
							succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
							if (succ == 0) {
								fuelGaugeI2cErrorCounter = 0;
								// check if NTC measurement is valid, compatible NTC sensor should give temp reading above -20C
								if (fuelGaugeTemp <= 0x09E4 || currentBatProfile==NULL || currentBatProfile->ntcB == 0xFFFF ) {
									// in case of invalid measurement, use on board measurement and update fuel gauge
									batteryTemp = mcuTemperature;
									ntcFaultFlag = 1;
									// Set I2C mode
									if (FuelGaugeWriteWord(0x16, 0x0000) == 0) fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
								} else {
									volatile int16_t ntcTemp;
									if (currentBatProfile->ntcResistance == 1000) {
										ntcTemp = ((int16_t)fuelGaugeTemp - 2732) / 10;
									} else {
										int32_t dr25 = currentBatProfile->ntcResistance / 10;
										int32_t it = dr25<261 ? (dr25-4)>>1 : ((dr25+2300)*13)>>8;
										it = it < 0 ? 0 : it;
										it = it > 255 ? 255 : it;
										volatile int16_t Tx10 = (currentBatProfile->ntcB * (int32_t)fuelGaugeTemp) / (currentBatProfile->ntcB*10 - (((int32_t)fuelGaugeTemp*logTbl[it])>>13)); // T = (B * T10k) / (B - T10k*log(k))
										ntcTemp = Tx10 - 273;
									}
									if ( ntcTemp>=23 && ntcTemp<=27 && (mcuTemperature<15 || mcuTemperature>45) ) {
										// there can be fixed resistor instead of NTC, use mcu measurement instead
										batteryTemp = mcuTemperature;
										if (FuelGaugeWriteWord(0x16, 0x0000) == 0) fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
									} else {
										batteryTemp = ntcTemp;
									}
									ntcFaultFlag = 0;
								}
							} else if (succ > 0) {
								// failed reading mean no fuel gauge ic on board
								fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter < 127 ? fuelGaugeI2cErrorCounter + 1 : 127;
							}
						} else {
							fuelGaugeTemp = mcuTemperature * 10 + 2732;
							fuelGaugeTemp = fuelGaugeTemp > 0x0D04 ? 0x0D04 : fuelGaugeTemp;
							fuelGaugeTemp = fuelGaugeTemp < 0x09E4 ? 0x09E4 : fuelGaugeTemp;
							succ = FuelGaugeWriteWord(0x08, fuelGaugeTemp);
							if (succ == 0) {
								fuelGaugeI2cErrorCounter = 0;
								// alternate to thermistor mode
								if (FuelGaugeWriteWord(0x16, 0x0001) == 0) fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
							} else if (succ > 0) {
								fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter < 127 ? fuelGaugeI2cErrorCounter + 1 : 127;
							}
							batteryTemp = mcuTemperature;
						}
					} else if ( fuelGaugeI2cErrorCounter > -5 && fuelGaugeI2cErrorCounter < 0 ) {
						// ic is not properly initialized, retry
						if (FuelGaugeIcInit() != 0) {
							fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter > -127 ? fuelGaugeI2cErrorCounter - 1 : -127;
						} else {
							fuelGaugeI2cErrorCounter = 0;
						}
					} else {
						// fuel gauge ic is not responsive or absent
						if ( currentBatProfile != NULL ) {
							// use direct NTC measurement
							volatile uint16_t ntcAdcSample = ADC_GetAverageValue(ANALOG_CHANNEL_NTC);

							if (ntcAdcSample<3000 && ntcAdcSample>5) { // sensor is connected
								//volatile int32_t r = ntcAdcSample * (int32_t)240000 / (4096 - ntcAdcSample);
								int32_t dr25 = ntcAdcSample * (int32_t)240000 / ((int16_t)4096 - ntcAdcSample)*10 / currentBatProfile->ntcResistance;
								uint16_t beta = currentBatProfile->ntcB;
								int32_t it = dr25<261 ? (dr25-4)>>1 : ((dr25+2300)*13)>>8;
								it = it < 0 ? 0 : it;
								it = it > 255 ? 255 : it;
								int16_t ntcTemp =  (int32_t)65593*beta / (logTbl[it]* (int32_t)8 + (int32_t)220*beta) - 273; //1.0 / (log((double)r/r25)/beta + (double)1.0/298.15) - 273.15;
								if ( (ntcTemp>=23 && ntcTemp<=27 && (mcuTemperature<15 || mcuTemperature>45)) || ntcTemp <-29 || ntcTemp > 90 ) {
									// there can be fixed resistor instead of NTC, use mcu measurement instead
									batteryTemp = mcuTemperature;
								} else {
									batteryTemp = ntcTemp;
								}
								ntcFaultFlag = 0;
							} else {
								batteryTemp = mcuTemperature;
								ntcFaultFlag = 1;
							}
						} else {
							batteryTemp = mcuTemperature;
						}
					}
				} else  {
					if ( tempSensorConfig == BAT_TEMP_SENSE_CONFIG_ON_BOARD ) {
						batteryTemp = mcuTemperature;
					} else {
						batteryTemp = 25;
					}
					if ( fuelGaugeI2cErrorCounter == 0 ) {
						fuelGaugeTemp = mcuTemperature * 10 + 2732;
						fuelGaugeTemp = fuelGaugeTemp > 0x0D04 ? 0x0D04 : fuelGaugeTemp;
						fuelGaugeTemp = fuelGaugeTemp < 0x09E4 ? 0x09E4 : fuelGaugeTemp;
						if (fuelGaugeTempMode == FUEL_GAUGE_TEMP_MODE_THERMISTOR) {
							// Set I2C mode
							if (FuelGaugeWriteWord(0x16, 0x0000) == 0) {
								fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
								fuelGaugeI2cErrorCounter = 0;
							} else if (succ > 0) {
								fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter < 127 ? fuelGaugeI2cErrorCounter + 1 : 127;
							}
						}
						if (fuelGaugeTempMode == FUEL_GAUGE_TEMP_MODE_I2C) {
							// write temperature data to fuel gauge
							if ( FuelGaugeWriteWord(0x08, fuelGaugeTemp) == 0) {
								fuelGaugeI2cErrorCounter = 0;
							} else if (succ > 0) {
								fuelGaugeI2cErrorCounter = fuelGaugeI2cErrorCounter < 127 ? fuelGaugeI2cErrorCounter + 1 : 127;
							}
						}
					}
				}
			}
		} else {
			prevBatPresent = 0;
			fuelGaugeI2cErrorCounter = -1; // indicate that fuel gauge needs initialization after battery insertion
			batteryRsoc = 0;
			batteryVoltage = batVolt;
			batteryTemp = mcuTemperature;
		}
	}
}


void FuelGaugeSetBatProfile(const BatteryProfile_T *batProfile) {

	if ( batProfile != NULL && batProfile->ntcB != 0xFFFF /*&& batProfile->ntcResistance == 1000*/ ) {
		if ( fuelGaugeI2cErrorCounter < 5 && fuelGaugeI2cErrorCounter >= 0 ) {
			// Set NTC B constant
			if (FuelGaugeWriteWord(0x06, batProfile->ntcB)!= 0) {
				// declare as non initialized
				fuelGaugeI2cErrorCounter = -1;
			}
		}
	}

	FuelGaugeDvInit();
}


void FUELGUAGE_SetConfig(const uint8_t * const data, const uint16_t len)
{
	const uint8_t newTempConfig = data[0u] & 0x07u;
	const uint8_t newRsocConfig = (uint8_t)((data[0u] & 0x30u) >> 4u);

	uint8_t config;

	if ( (newTempConfig >= BAT_TEMP_SENSE_CONFIG_TYPES)
			|| (newRsocConfig >= RSOC_MEASUREMENT_CONFIG_TYPES)
			)
	{
		return;
	}

	NvWriteVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, data[0u]);

	if (NvReadVariableU8(FUEL_GAUGE_CONFIG_NV_ADDR, &config) == NV_READ_VARIABLE_SUCCESS)
	{
		m_rsocMeasurementConfig = (RsocMeasurementConfig_T)newRsocConfig;
		m_tempSensorConfig = (BatteryTempSenseConfig_T)newTempConfig;
	}
	else
	{
		m_tempSensorConfig = BAT_TEMP_SENSE_CONFIG_AUTO_DETECT;
		m_rsocMeasurementConfig = RSOC_MEASUREMENT_AUTO_DETECT;
	}
}


void FUELGUAGE_GetConfig(uint8_t * const data, uint16_t * const len)
{
	data[0u] = (m_tempSensorConfig & 0x07u) | ( (m_rsocMeasurementConfig & 0x03u) << 4u);
	*len = 1u;
}
