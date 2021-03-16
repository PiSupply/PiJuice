/*
 * rtc_ds1339_emu.c
 *
 *  Created on: 19.01.2017.
 *      Author: milan
 */
// linux driver for rtc and alarm config.txt: dtoverlay=i2c-rtc,ds1339,wakeup-source
#include "rtc_ds1339_emu.h"
#include "stm32f0xx_hal.h"
#include "power_management.h"

#define RTC_REGISTERS_NUM	(0x3F+1) // free RAM reserved for compatibility with ds1307

extern RTC_HandleTypeDef hrtc;
RTC_AlarmTypeDef sAlarm;

static uint8_t rtc_buffer[RTC_REGISTERS_NUM] __attribute__((section("no_init")));//= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x07, 0, 0}; // rtc_bufferisters used for i2c master access
static uint8_t rtc_buffer_ptr __attribute__((section("no_init")));
//volatile uint8_t testRegAdr[50] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//volatile uint8_t testRegAdrW = 0xFF;
//volatile uint8_t testRegAdrI = 0;

static const uint8_t binHour24ToBcd[24] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23};
static const uint8_t binHour24ToBcdAmPm[24] = {0x12, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x10, 0x11, 0x32, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31};

static uint8_t weekDaysSelection __attribute__((section("no_init")));
static uint32_t hoursSelection __attribute__((section("no_init")));
static uint8_t minutesStep __attribute__((section("no_init")));
uint8_t alarmEventFlag __attribute__((section("no_init")));

extern uint8_t resetStatus;

RtcCommand_T rtcCommands[] =
{
		//RtcReadWriteTime
};

void RtcInit(void) {
	//static  uint8_t rtcBufferInit[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	if (!resetStatus) {
		weekDaysSelection = 0xFF;
		hoursSelection = 0xFFFFFFFF;
		minutesStep = 0;
		alarmEventFlag = 0;
		int i;
		for (i = 0; i < 17; i++) rtc_buffer[i] = 0;//rtcBufferInit[i];
	}
}

/**
  * @brief  Alarm callback
  * @param  hrtc : RTC handle
  * @retval None
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *phrtc)
{
	// if alarm 1 interrupt enabled activate int signal
	//if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) )
		//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
	alarmEventFlag = 1;
}

void EvaluateAlarm(void)
{
	static volatile RTC_TimeTypeDef sTime;
	static RTC_DateTypeDef dateConf;
	uint32_t tempReg;
	//static volatile uint8_t min;
	// if alarm 1 interrupt enabled activate int signal
	//if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) )
		//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);

		uint8_t weekDayMatch = 0;
		if (weekDaysSelection != 0xFF || !(sAlarm.AlarmMask&0x80000000)) {
			//HAL_RTC_GetDate(&hrtc, &dateConf, RTC_FORMAT_BIN);
			tempReg = hrtc.Instance->DR;
			tempReg = 0;
			tempReg = hrtc.Instance->DR;
			tempReg = 0;
			tempReg = hrtc.Instance->DR;
			dateConf.WeekDay = (tempReg >> 13)&0x07;
			if ( (0x01 << (dateConf.WeekDay)) & weekDaysSelection ) {
				weekDayMatch = 1;
			}
		} else {
			weekDayMatch = 1;
		}

		if (weekDayMatch) {
			if (hoursSelection != 0xFFFFFFFF || minutesStep > 1) {
				tempReg = hrtc.Instance->TR;
				tempReg = 0;
				tempReg = hrtc.Instance->TR;
				tempReg = 0;
				tempReg = hrtc.Instance->TR;
				//HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
				sTime.Minutes = ((tempReg >> 8)&0x0F) + ((tempReg >> 12)&0x07) * 10;
				if ( minutesStep <= 1 || (sTime.Minutes % minutesStep) == 0 ) {
					sTime.Hours = ((tempReg >> 16)&0x0F) + ((tempReg >> 20)&0x07) * 10;
					sTime.TimeFormat = (uint8_t)((tempReg & (RTC_TR_PM)) >> 16);
					if (hrtc.Init.HourFormat == RTC_HOURFORMAT_24) {
						if ( (0x00000001 << sTime.Hours) & hoursSelection ) {
							if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
							rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
						}
					} else if ( sTime.TimeFormat == RTC_HOURFORMAT12_AM ) {
						if (sTime.Hours < 12) {
							if ( (0x00000001 << sTime.Hours) & hoursSelection ) {
								if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
								rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
							}
						} else if ( 0x00000001 & hoursSelection ) {
							if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
							rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
						}
					} else {
						if (sTime.Hours < 12) {
							if ( (0x00010000 << sTime.Hours) & hoursSelection ) {
								if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
								rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
							}
						} else if ( 0x00010000 & hoursSelection ) {
							if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
							rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
						}
					}
				}
			} else {
				if ( (rtc_buffer[0x0E]&0x04) && (rtc_buffer[0x0E]&0x01) ) rtcWakeupEventFlag = 1;
				rtc_buffer[0x0F] |= 0x01; // set alarm 1 flag
			}
		}
	//}
}

void RtcDs1339ProcessRequest(uint8_t dir, uint8_t command, uint8_t *pData, uint16_t *dataLen) {
	uint8_t i;
	if (command == 0) {
		if ( dir == I2C_DIRECTION_RECEIVE ) {
			//testRegAdr[0] = pData[0]; // debug only
			RtcReadTime(rtc_buffer, 0);
			i = 7;
			while (i--) pData[i] = rtc_buffer[i];
			*dataLen = 7;
		} else {
			//testRegAdrW = pData[0]; // debug only
			i = *dataLen;
			while (i--) rtc_buffer[i] = pData[i];
			RtcWriteTime(rtc_buffer, 0);
		}
	} else if (command == 7) {
		if ( dir == I2C_DIRECTION_RECEIVE ) {
			RtcReadAlarm1(&rtc_buffer[7], 0);
			i = 9;
			while (i--) pData[i] = rtc_buffer[i + 7];
			*dataLen = 9;
		} else {
				i = *dataLen;
				while (i--)
					if (i<9) rtc_buffer[i + 7] = pData[i];
					/*else if (i==8) {
						rtc_buffer[i + 7] &= pData[i] | 0xFC;
					}*/
				RtcWriteAlarm1(&rtc_buffer[7], 0);
			}
	} else if (command == 0x0E) { // control register
			if ( dir == I2C_DIRECTION_RECEIVE ) {
				RtcReadControlStatus(pData, dataLen);
//				pData[0] = rtc_buffer[command];
//				pData[1] = rtc_buffer[command+1];
//				*dataLen = 2;
			} else {
				RtcWriteControlStatus(pData, *dataLen);
//				rtc_buffer[command] = pData[0];
//				if (*dataLen > 1) {
//					//if ((pData[1]&0x01) == 0x00) // deactivate alarm interrupt signal
//						//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
//					rtc_buffer[command+1] = rtc_buffer[command+1]&(pData[1] | 0xFC); // clear A1F, A2F
//				}
			}
	} else if (command == 0x0F) {
		if ( dir == I2C_DIRECTION_RECEIVE ) {
			pData[0] = rtc_buffer[command];
			*dataLen = 1;
		} else {
			//if ((pData[0]&0x01) == 0x00) // deactivate alarm interrupt signal
				//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
			rtc_buffer[command] = rtc_buffer[command] & (pData[0] | 0xFC); // clear A1F, A2F
		}
	} else 	if (command >=0 && command < RTC_REGISTERS_NUM ){
		if (dir == I2C_DIRECTION_TRANSMIT) {
			rtc_buffer[command] = pData[0];
		} else {
			pData[0] = rtc_buffer[command];
			*dataLen = 1;
		}
	} else {
		return;
	}
	rtc_buffer_ptr = command;
}

uint8_t RtcGetPointer() {
	return rtc_buffer_ptr;
}

uint8_t RtcSetPointer(uint8_t val) {
	if (val <= 0x0F)
		rtc_buffer_ptr = val;
}

void RtcWriteTime(uint8_t *buffer, uint8_t extended) {
	RTC_TimeTypeDef sTime;

	sTime.SecondFraction = 127; // 1s / 256 resolution
	sTime.Seconds = buffer[0]&0x7F;
	sTime.Minutes = buffer[1]&0x7F;
	if (buffer[2] & 0x40) {
		if (hrtc.Init.HourFormat != RTC_HOURFORMAT_12) {
			hrtc.Init.HourFormat = RTC_HOURFORMAT_12;
			if (HAL_RTC_Init(&hrtc) != HAL_OK)
			{
				Error_Handler();
			}
		}
		sTime.TimeFormat = buffer[2] & 0x20 ? RTC_HOURFORMAT12_PM : RTC_HOURFORMAT12_AM;
		sTime.Hours = buffer[2] & 0x1F;
	} else {
		if (hrtc.Init.HourFormat != RTC_HOURFORMAT_24) {
			hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
			if (HAL_RTC_Init(&hrtc) != HAL_OK)
			{
				Error_Handler();
			}
		}
		sTime.Hours = buffer[2]&0x3F;
	}

	RTC_DateTypeDef dateConf;
	dateConf.WeekDay = buffer[3];
	dateConf.Date = buffer[4];
	dateConf.Month = buffer[5];
	dateConf.Year = buffer[6];

	if (extended) {
		sTime.SubSeconds = buffer[7];
		if ((buffer[8]&0x03) == 2) {
			sTime.DayLightSaving = RTC_DAYLIGHTSAVING_SUB1H;
		} else if ((buffer[8]&0x03) == 1) {
			sTime.DayLightSaving = RTC_DAYLIGHTSAVING_ADD1H;
		} else {
			sTime.DayLightSaving =  RTC_DAYLIGHTSAVING_NONE;
		}
		sTime.StoreOperation = (buffer[8]&0x04) ? RTC_STOREOPERATION_SET : RTC_STOREOPERATION_RESET;
	} else {
		sTime.SubSeconds = 0;
		sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	}

	HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD);
	HAL_RTC_SetDate(&hrtc, &dateConf, RTC_FORMAT_BCD);
}

void RtcReadTime(uint8_t *buffer, uint8_t extended) {
	RTC_TimeTypeDef sTime;
	HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BCD);
	buffer[0] = sTime.Seconds & 0x7F;
	buffer[1] = sTime.Minutes;
	if (hrtc.Init.HourFormat == RTC_HOURFORMAT_12) {
		buffer[2] = sTime.Hours;
		buffer[2] |= sTime.TimeFormat == RTC_HOURFORMAT12_PM ? 0x20 : 0;
		buffer[2] |= 0x40;
	} else {
		buffer[2] = sTime.Hours;
	}

	RTC_DateTypeDef dateConf;
	HAL_RTC_GetDate(&hrtc, &dateConf, RTC_FORMAT_BCD);
	buffer[3] = dateConf.WeekDay;
	buffer[4] = dateConf.Date;
	buffer[5] = dateConf.Month;
	buffer[6] = dateConf.Year;

	if (extended) {
		buffer[7] = sTime.SubSeconds;
		buffer[8] = 0;
		if (sTime.DayLightSaving == RTC_DAYLIGHTSAVING_SUB1H) {
			buffer[8] |= 2&0x03;
		} else if (sTime.DayLightSaving == RTC_DAYLIGHTSAVING_ADD1H) {
			buffer[8] |= 1&0x03;
		}
		buffer[8] |= (sTime.StoreOperation == RTC_STOREOPERATION_SET) ? 0x04 : 0;
	}
}

void RtcReadAlarm1(uint8_t *buffer, uint8_t extended) {
	HAL_RTC_GetAlarm(&hrtc, &sAlarm, RTC_ALARM_A, RTC_FORMAT_BCD);
	buffer[0] = sAlarm.AlarmTime.Seconds | (sAlarm.AlarmMask&0x80);
	sAlarm.AlarmMask >>= 8;
	buffer[1] = sAlarm.AlarmTime.Minutes | (sAlarm.AlarmMask&0x80);
	sAlarm.AlarmMask >>= 8;
	if (hrtc.Init.HourFormat == RTC_HOURFORMAT_12) {
		buffer[2] = sAlarm.AlarmTime.Hours;
		buffer[2] |= sAlarm.AlarmTime.TimeFormat == RTC_HOURFORMAT12_PM ? 0x20 : 0;
		buffer[2] |= 0x40 | (sAlarm.AlarmMask&0x80);
	} else {
		buffer[2] = sAlarm.AlarmTime.Hours | (sAlarm.AlarmMask&0x80);
	}
	sAlarm.AlarmMask >>= 8;
	buffer[3] = sAlarm.AlarmDateWeekDay;
	buffer[3] |= (sAlarm.AlarmDateWeekDaySel == RTC_ALARMDATEWEEKDAYSEL_WEEKDAY) ? 0x40 : 0;
	buffer[3] |= (sAlarm.AlarmMask&0x80);

	if (extended) {
		buffer[4] = hoursSelection;
		buffer[5] = hoursSelection >> 8;
		buffer[6] = hoursSelection >> 16;

		buffer[7] = minutesStep;
		buffer[8] = weekDaysSelection;
	}
}

void RtcWriteAlarm1(uint8_t *buffer, uint8_t extended) {
	sAlarm.AlarmTime.Seconds = buffer[0] & 0x7F;
	sAlarm.AlarmTime.Minutes = buffer[1] & 0x7F;
	if (buffer[2] & 0x40) {
		if (hrtc.Init.HourFormat != RTC_HOURFORMAT_12) {
			// convert hour format to 24 if different
			uint8_t binHours = (buffer[2]&0x0F) + ((buffer[2]&0x10)?10:0);
			if (buffer[2]&0x20) {
				if (binHours < 12) binHours += 12;
			} else {
				if (binHours > 11) binHours = 0;
			}
			sAlarm.AlarmTime.Hours = binHour24ToBcd[binHours];
		} else {
			sAlarm.AlarmTime.Hours = buffer[2] & 0x1F;
			sAlarm.AlarmTime.TimeFormat = buffer[2] & 0x20 ? RTC_HOURFORMAT12_PM : RTC_HOURFORMAT12_AM;
		}
	} else {
		if (hrtc.Init.HourFormat != RTC_HOURFORMAT_24) {
			// convert hour format to 12 if different
			uint8_t binHours = (buffer[2]&0x0F) + ((buffer[2]&0x10)?10:0) + ((buffer[2]&0x20)?10:0);
			uint8_t h = binHour24ToBcdAmPm[binHours];
			sAlarm.AlarmTime.Hours = h & 0x1F;
			sAlarm.AlarmTime.TimeFormat = h & 0x20 ? RTC_HOURFORMAT12_PM : RTC_HOURFORMAT12_AM;
		} else {
			sAlarm.AlarmTime.Hours = buffer[2]&0x3F;
		}
	}
	sAlarm.AlarmDateWeekDaySel = (buffer[3] & 0x40) ? RTC_ALARMDATEWEEKDAYSEL_WEEKDAY : RTC_ALARMDATEWEEKDAYSEL_DATE;
	sAlarm.AlarmDateWeekDay = buffer[3] & 0x3F;

	sAlarm.AlarmMask = buffer[3]&0x80;
	sAlarm.AlarmMask <<= 8;
	sAlarm.AlarmMask |= buffer[2]&0x80;
	sAlarm.AlarmMask <<= 8;
	sAlarm.AlarmMask |= buffer[1]&0x80;
	sAlarm.AlarmMask <<= 8;
	sAlarm.AlarmMask |= buffer[0]&0x80;

	if (extended) {
		hoursSelection = buffer[6];
		hoursSelection <<= 8;
		hoursSelection |= buffer[5];
		hoursSelection <<= 8;
		hoursSelection |= buffer[4];
		minutesStep = buffer[7];
		weekDaysSelection = buffer[8];

		if (!(buffer[0] || buffer[1] || buffer[2] || buffer[3])) {
			HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
		}

	} else {
		hoursSelection = 0xFFFFFFFF;
		weekDaysSelection = 0xFF;
		minutesStep = 0;
	}

	HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD);
}

void RtcWriteControlStatus(uint8_t *buffer, uint16_t dataLen) {
	rtc_buffer[0x0E] = buffer[0];
	if (dataLen > 1) {
		//if ((pData[1]&0x01) == 0x00) // deactivate alarm interrupt signal
			//HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
		rtc_buffer[0x0E + 1] = rtc_buffer[0x0E + 1]&(buffer[1] | 0xFC); // clear A1F, A2F
	}
}

void RtcReadControlStatus(uint8_t *buffer, uint16_t *dataLen)
{
	buffer[0] = rtc_buffer[0x0E];
	buffer[1] = rtc_buffer[0x0E + 1];
	*dataLen = 2;
}
