/*
 * command_server.h
 *
 *  Created on: 07.12.2016.
 *      Author: milan
 */

#ifndef COMMAND_SERVER_H_
#define COMMAND_SERVER_H_

#include "stdint.h"

#define MASTER_CMD_DIR_READ		1
#define MASTER_CMD_DIR_WRITE	0

typedef void (*MasterCommand_T)(uint8_t dir, uint8_t *pData, uint16_t *dataLen);

void CommandServerInit(void);
int8_t CmdServerProcessRequest(uint8_t dir, uint8_t *pData, uint16_t *dataLen);

#endif /* COMMAND_SERVER_H_ */
