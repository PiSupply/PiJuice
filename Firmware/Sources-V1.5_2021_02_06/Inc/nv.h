/*
 * nv.h
 *
 *  Created on: 12.12.2016.
 *      Author: milan
 */

#ifndef NV_H_
#define NV_H_

#include "stdint.h"
#include "eeprom.h"

#define NV_INVALID_VARIABLE        ((uint16_t)0x000f)
#define NV_READ_VARIABLE_SUCCESS   ((uint16_t)0)
#define NV_VARIABLE_NON_STORED       ((uint16_t)0xffffffff)

#define NV_IS_DATA_INITIALIZED	(nvInitFlag != 0Xffff)
#define NV_IS_VARIABLE_VALID(var)	(((~var)&0xFF) == (var>>8))

void NV_Init(void);
void NV_SetDataInitialised(void);
void NV_Task(void);
void NV_SaveParameterReq(const NvVarId_T id, const uint16_t value);

void NV_FactoryReset(void);

bool NV_WriteVariable_U8(const uint16_t address, const uint8_t var);
bool NV_ReadVariable_U8(const uint16_t address, uint8_t * const p_var);

bool NV_WriteVariable_S8(const uint16_t address, const int8_t var);
bool NV_ReadVariable_S8(const uint16_t address, int8_t * const p_var);
void NV_WipeVariable(const uint16_t address);

uint16_t NvReadVariableU8(uint16_t VirtAddress, uint8_t *pVar);


#endif /* NV_H_ */
