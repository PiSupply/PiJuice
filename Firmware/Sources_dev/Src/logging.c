/*
 * logging.c
 *
 *  Created on: 06/24/2021
 *      Author: milan
 */

#include "logging.h"
#include "rtc_ds1339_emu.h"
#include "execution.h"
#include "nv.h"

#if defined LOGGING
uint8_t log_buf[LOG_BUF_SIZE] __attribute__((section("no_init")));
uint16_t log_last __attribute__((section("no_init"))); // index of the beginning of last message in buffer
uint8_t log_last_seq_num __attribute__((section("no_init"))); // sequence number of last message in buffer
uint16_t log_read_num __attribute__((section("no_init"))); // message to be read by host
uint8_t log_config __attribute__((section("no_init"))); // enable/disable configuration
uint8_t log_read_flag = 0;

#define LOG_INIT_FRAME(id) \
	log_last -= LOG_BUF_FRAME_SIZE; \
	log_last &= LOG_BUF_MASK; \
	log_buf[log_last] = LOG_MSG_LEN; /* first byte in frame is message length */  \
	log_buf[log_last+1] = ++log_last_seq_num; /* second byte in frame is sequence number */ \
	log_buf[log_last+2] = id;   /* third byte in frame is log message id */ \

#define IS_LOG_ENABLED(id) ((id>3 && id<10) ? log_config&(0x01<<(id-3)) : log_config&0x01)

uint8_t *LoggingInitMessage(LogMsgId_T id) {
	if (!IS_LOG_ENABLED(id)) return NULL;
	// reserve message frame for later log
	// some log messages needs process to be completed in order to collect report data, like 5V regulator turn on
	LOG_INIT_FRAME(id);

	// fill message data starting from fourth byte
	uint8_t *pBuf = log_buf+log_last+3;
	RtcReadTime(pBuf, 1);
	return log_buf+log_last+11;
}

void LogPut(LogMsgId_T id) {
	switch (id) {
	case LOG_5VREG_ON:
		LOG_INIT_FRAME(LOG_5VREG_ON);

		// fill message data starting from fourth byte
		uint8_t *pBuf = log_buf+log_last+3;
		RtcReadTime(pBuf, 1);

		break;
	default:
		break;
	}
}

void LoggingInit(void) {
	//if ( executionState == EXECUTION_STATE_POWER_ON || executionState == EXECUTION_STATE_POWER_RESET) {
	//volatile uint16_t mask = LOG_BUF_MASK;
		int i = 0;
		while(i < LOG_BUF_SIZE) log_buf[i++] = 0;
		log_last = LOG_BUF_FRAME_SIZE;
		log_last_seq_num = 0xFF;
		log_read_num = 0;

		uint8_t cfg = 0xFF;
		if (NvReadVariableU8(LOG_CONFIG_NV_ADDR, (uint8_t*)&cfg) != NV_READ_VARIABLE_SUCCESS ) {
			log_config = 0x00;
		} 	else {
			log_config = ~cfg;
		}
	//}

	log_last &= LOG_BUF_MASK;
}

void LoggingReadMessageCmd(uint8_t data[], uint16_t *len) {
	*len = LOG_MSG_LEN;

	if (log_config == 0 || log_read_flag == 1) {
		int i = LOG_MSG_LEN;
		while(i--) data[i] = 0;
		data[2] = 0x01;
		data[3] = log_config;
		return;
	}
	if (log_read_num >= LOG_MAX_MESSAGES || log_buf[0] == 0) {
		// NO MORE messages to read, send empty buffer so host is notified
		int i = LOG_MSG_LEN;
		while(i--) data[i] = 0;

		*len = LOG_MSG_LEN;
		log_read_num = 0;
		return;
	}
	//data[0] = log_last_seq_num - log_read_num; // sequence number of currently read message
	uint8_t *pBuf = &log_buf[(log_last+(uint16_t)log_read_num*LOG_BUF_FRAME_SIZE)&LOG_BUF_MASK]; // Get pointer to frame

	pBuf ++; // skip first byte
	int i = 0;
	while(i < LOG_MSG_LEN) {
		data[i] = pBuf[i]; // copy frame to message buffer
		i++;
	}

	 log_read_num++;
}

int8_t LoggingWriteConfigCmd(uint8_t data[], uint16_t len) {
	if (data[0] == 0) {
		log_read_num = 0; // reset read sequence number
		log_read_flag = 0;
		return 0;
	}
	if (data[0] == 0x01) {
		if (data[1]!=log_config) {
			NvWriteVariableU8(LOG_CONFIG_NV_ADDR, ~data[1]);
			uint8_t cfg = 0xFF;
			if (NvReadVariableU8(LOG_CONFIG_NV_ADDR, (uint8_t*)&cfg) != NV_READ_VARIABLE_SUCCESS ) {
				log_config = 0x00;
			} 	else {
				log_config = ~cfg;
			}
		}
		log_read_flag = 1;
		return 0;
	}
	if (data[0] == 0x02) {
		log_read_flag = 1;
		return 0;
	}
	log_read_flag = 0;
	return 0;
}
#endif //LOGGING
