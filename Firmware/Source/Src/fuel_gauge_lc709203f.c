/*
 * fuel_gauge_lc709203f.c
 *
 *  Created on: 06.12.2016.
 *      Author: milan
 */

#include "fuel_gauge_lc709203f.h"
#include "stm32f0xx_hal.h"
#include "crc8_atm.h"
#include "time_count.h"
#include "analog.h"
#include "charger_bq2416x.h"

extern I2C_HandleTypeDef hi2c2;

uint16_t batteryVoltage = 0xFFFF;
uint16_t batteryRsoc = 0;
//volatile uint16_t emptyIndicator = 0;
uint16_t fuelGaugeTemp = 0;
int8_t batteryTemp = 25;
int16_t batteryCurrent = 0;

int8_t fuelGaugeStatus = 0;

static uint32_t fuelGaugeTaskTimer;

volatile int32_t dischargeRate = 0;
uint32_t dischargeCount;
static uint32_t dischargeCountTemp;

static uint16_t prevRsoc;

uint8_t fuelGaugeI2cErrorCounter = 0;

static uint16_t fgIcId = 0xFFFF;
volatile uint8_t fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

int8_t FuelGaugeReadWord(uint8_t cmd, uint16_t *word) {
	uint8_t readData[10] = {0x16, cmd, 0x17, 0, 0, 0};

	HAL_I2C_Mem_Read(&hi2c2, 0x16, cmd, 1, readData+3, 3, 1);
	/*HAL_Delay(2);
	HAL_I2C_Mem_Read_IT(&hi2c2, 0x16, cmd, 1, readData+3, 3);
	uint32_t timeout = HAL_GetTick() + 2;
	while (HAL_I2C_GetState(&hi2c2) != HAL_I2C_STATE_READY && timeout > HAL_GetTick());
	HAL_Delay(2);*/
	if (Crc8Block(0, readData, 5) == readData[5]) {
		*word = (((uint16_t)readData[4])<<8) | readData[3];
		fuelGaugeI2cErrorCounter = 0;
		return 0;
	} else {
		fuelGaugeI2cErrorCounter ++;
		return -1;
	}
}

void FuelGaugeWriteWord(uint8_t cmd, uint16_t word) {
	uint8_t writeData[5] = {0x16, cmd, word, word>>8, 0};
	writeData[4] = Crc8Block(0, writeData, 4);
	//HAL_Delay(2);
	HAL_I2C_Mem_Write(&hi2c2, 0x16, cmd, 1, (uint8_t*)&writeData[2], 3, 1);
	//uint32_t timeout = HAL_GetTick() + 2;
	//while (HAL_I2C_GetState(&hi2c2) != HAL_I2C_STATE_READY && timeout > HAL_GetTick());
	//HAL_Delay(2);
}

void FuelGaugeInit(void) {
	volatile int8_t succ;
	MS_TIME_COUNTER_INIT(fuelGaugeTaskTimer);

	//HAL_Delay(5);
	/*HAL_I2C_Mem_Read(&hi2c2, 0x16, 0x11, 1, (uint8_t*)&fgIcId, 2, 10);
	//int8_t succ = FuelGaugeReadWord(0x11, &fgIcId);
	volatile HAL_I2C_StateTypeDef i2c2Status = HAL_I2C_GetState(&hi2c2);
	if ( i2c2Status == HAL_I2C_STATE_TIMEOUT || fgIcId == 0xFFFF) {
		fuelGaugeStatus = 1;
		return;
	}*/

	if (FuelGaugeReadWord(0x11, &fgIcId) != 0) {
		fuelGaugeStatus = 1;
		return;
	}

	// Set operational mode
	FuelGaugeWriteWord(0x15, 0x0001); //3.7V

	// set APA
	FuelGaugeWriteWord(0x0b, 0x36); //FuelGaugeWriteWord(0x0b, 0x2d);//

	// set change of the parameter
	FuelGaugeWriteWord(0x12, 0x0001);

	// set APT
	FuelGaugeWriteWord(0x0C, 0x3000);//FuelGaugeWriteWord(0x0C, 0x001E);//

	if ( currentBatProfile != NULL && currentBatProfile->ntcB != 0xFFFF && currentBatProfile->ntcResistance == 1000 ) {
		// Set NTC B constant
		FuelGaugeWriteWord(0x06, currentBatProfile->ntcB);
	}

	// Set NTC mode
	FuelGaugeWriteWord(0x16, 0x0001);
	FuelGaugeWriteWord(0x16, 0x0001);
	fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

	FuelGaugeReadWord(0x09, &batteryVoltage);
	FuelGaugeReadWord(0x0F, &batteryRsoc);
	prevRsoc = batteryRsoc;
	dischargeCount = HAL_GetTick();
	dischargeCountTemp = dischargeCount;

	if (tempSensorConfig == BAT_TEMP_SENSE_CONFIG_AUTO_DETECT || tempSensorConfig == BAT_TEMP_SENSE_CONFIG_NTC)
	{
		succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
		if (succ!=0) succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);

		// check if NTC connected
		if (fuelGaugeTemp <= 0x09E4 || succ!=0 || currentBatProfile==NULL || currentBatProfile->ntcB == 0xFFFF || currentBatProfile->ntcResistance != 1000) {
			// not NTC connected should give reading below -20C
			fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
			// Set I2C mode
			FuelGaugeWriteWord(0x16, 0x0000);
			batteryTemp = mcuTemperature;
		} else {
			fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
			// Set NTC mode
			FuelGaugeWriteWord(0x16, 0x0001);
			batteryTemp = ((int16_t)fuelGaugeTemp - 2732) / 10;
		}
	} else {
		fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
		// Set I2C mode
		FuelGaugeWriteWord(0x16, 0x0000);
		batteryTemp = mcuTemperature;
	}
	//volatile uint16_t NTCset = 0xFFFF; // read NTC sensor config
	//succ = FuelGaugeReadWord(0x16, &NTCset);
}

void FuelGaugeTask(void) {
	volatile int8_t succ;
	static uint8_t ntcDetectCnt;
	static uint8_t updateCnt;
	/*if (fuelGaugeStatus) {
		batteryTemp = mcuTemperature;
		return;// if not present
	}*/

	if ( MS_TIME_COUNT(fuelGaugeTaskTimer) > 125 ) {

		// set APA
		//FuelGaugeWriteWord(0x0b, 0x2d);

		volatile uint16_t batVol = GetSampleVoltage(POW_VBAT_SENS_CHN);
		if (CHARGER_IS_BATTERY_PRESENT() && batVol > (2550 * 1000 / 1374)) {

			MS_TIME_COUNTER_INIT(fuelGaugeTaskTimer);
			// read battery voltage
			succ = FuelGaugeReadWord(0x09, &batteryVoltage);

			/*volatile uint16_t changeOfTheParameter = 0xFFFF;
			succ = FuelGaugeReadWord(0x12, &changeOfTheParameter);

			volatile uint16_t apa = 0xFFFF;
			succ = FuelGaugeReadWord(0x0b, &apa);*/

			// read state of charge
			succ = FuelGaugeReadWord(0x0F, &batteryRsoc);

			if (batteryRsoc != prevRsoc && (HAL_GetTick() - dischargeCount) > 500) {
				dischargeRate = ((int32_t)prevRsoc - batteryRsoc) * (int32_t)1843200 / (int32_t)(HAL_GetTick() - dischargeCount);
				batteryCurrent = (dischargeRate * (currentBatProfile!=NULL ? currentBatProfile->capacity : 10000)) >> 10;
				dischargeCount = HAL_GetTick();
				dischargeCountTemp = HAL_GetTick();
				prevRsoc = batteryRsoc;
			} else if ((HAL_GetTick() - dischargeCount) > 1843200) {
				dischargeRate = 0;
				dischargeCount = HAL_GetTick();
				dischargeCountTemp = HAL_GetTick();
			} else if ( (HAL_GetTick() - dischargeCountTemp) > 50000) {
				//batteryCurrent = (int32_t)(dischargeRate>0?1800:-1800) * (currentBatProfile!=NULL ? currentBatProfile->capacity : 10000) / (int32_t)(HAL_GetTick() - dischargeCount);
				//dischargeRate = ((int32_t)prevRsoc - batteryRsoc) * (int32_t)1843200 / (int32_t)(HAL_GetTick() - dischargeCount);
				int16_t newCurr;
				if (dischargeRate!=0) newCurr = (((int32_t)(dischargeRate>0?1:-1)) * (int32_t)1843200 / (int32_t)(HAL_GetTick() - dischargeCount) * (currentBatProfile!=NULL ? currentBatProfile->capacity : 10000)) >> 10;
				else newCurr = 0;
				//if ( (newCurr>0?newCurr:-newCurr) < (batteryCurrent>0?batteryCurrent:-batteryCurrent) ) batteryCurrent = newCurr;
				/*if (batteryCurrent > 120) {
					batteryCurrent -= 120;
				} else if (batteryCurrent < -120) {
					batteryCurrent += 120;
				}*/
				//batteryCurrent -= (batteryCurrent >> 2);
				dischargeCountTemp = HAL_GetTick();
			}

			if (fuelGaugeTempMode == FUEL_GAUGE_TEMP_MODE_THERMISTOR) {
				// read battery temperature
				succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
				if (succ!=0) succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp); // try once more
				//HAL_I2C_Mem_Read(&hi2c2, 0x16, 0x08, 1, (uint8_t*)&fuelGaugeTemp, 2, 10);
				if (succ==0) {
					// check if NTC connected
					if (fuelGaugeTemp <= 0x09E4 || currentBatProfile==NULL || currentBatProfile->ntcB == 0xFFFF || currentBatProfile->ntcResistance != 1000) {
						if (tempSensorConfig == BAT_TEMP_SENSE_CONFIG_AUTO_DETECT || tempSensorConfig == BAT_TEMP_SENSE_CONFIG_ON_BOARD)
						{
							// not NTC connected should give reading below -20C
							fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
							// Set I2C mode
							FuelGaugeWriteWord(0x16, 0x0000);
							batteryTemp = mcuTemperature;
						} else {
							batteryTemp = -127; // this will disable charging with NTC Config
						}
					} else {
						batteryTemp = ((int16_t)fuelGaugeTemp - 2732) / 10;
						if (ntcDetectCnt++ > 30) {
							// Set NTC mode
							FuelGaugeWriteWord(0x16, 0x0001);
							ntcDetectCnt = 0;
						}
					}
				} else {
					batteryTemp = mcuTemperature; // use board temperature as backup
				}
			} else {
				// try to detect sensor connection in
				if ( ntcDetectCnt++ > 25 && (tempSensorConfig == BAT_TEMP_SENSE_CONFIG_AUTO_DETECT || tempSensorConfig == BAT_TEMP_SENSE_CONFIG_NTC)) {
					FuelGaugeWriteWord(0x16, 0x0001);
					succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
					if (fuelGaugeTemp <= 0x09E4 || currentBatProfile==NULL || currentBatProfile->ntcB == 0xFFFF || currentBatProfile->ntcResistance != 1000) {
						// not NTC connected should give reading below -20C
						// Set I2C mode
						FuelGaugeWriteWord(0x16, 0x0000);
						batteryTemp = mcuTemperature;
					} else {
						fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
					}
					ntcDetectCnt = 0;
				} else {
					fuelGaugeTemp = mcuTemperature * 10 + 2732;
					fuelGaugeTemp = fuelGaugeTemp > 0x0D04 ? 0x0D04 : fuelGaugeTemp;
					fuelGaugeTemp = fuelGaugeTemp < 0x09E4 ? 0x09E4 : fuelGaugeTemp;
					FuelGaugeWriteWord(0x08, fuelGaugeTemp);
					batteryTemp = mcuTemperature;
					//succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
				}
			}

			updateCnt ++;
			if (updateCnt == 5) FuelGaugeWriteWord(0x12, 0x0001); // set change of the parameter
			if (updateCnt == 80) FuelGaugeWriteWord(0x0b, 0x36); // set APA
			if (updateCnt == 190) FuelGaugeWriteWord(0x0C, 0x3000); // set APT
		} else {
			batteryRsoc = 0;
			batteryVoltage = batVol * 1374 / 1000;
			batteryTemp = mcuTemperature;
		}

	}

}

void FuelGaugeSetBatProfile(const BatteryProfile_T *batProfile) {
	int8_t succ;
	if (fuelGaugeStatus) return;// if not present

	if ( batProfile != NULL && batProfile->ntcB != 0xFFFF && batProfile->ntcResistance == 1000 ) {
		// Set NTC B constant
		FuelGaugeWriteWord(0x06, batProfile->ntcB);
	}

	// Set NTC mode
	FuelGaugeWriteWord(0x16, 0x0001);
	FuelGaugeWriteWord(0x16, 0x0001);
	fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;

	/*succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);
	if (succ!=0) succ = FuelGaugeReadWord(0x08, &fuelGaugeTemp);

	// check if NTC connected
	if (fuelGaugeTemp <= 0x09E4 || succ!=0 || batProfile==NULL || batProfile->ntcB == 0xFFFF || batProfile->ntcResistance != 1000) {
		// not NTC connected should give reading below 20C
		fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_I2C;
		// Set I2C mode
		FuelGaugeWriteWord(0x16, 0x0000);
		batteryTemp = mcuTemperature;
	} else {
		fuelGaugeTempMode = FUEL_GAUGE_TEMP_MODE_THERMISTOR;
		// Set NTC mode
		FuelGaugeWriteWord(0x16, 0x0001);
	}*/
}

