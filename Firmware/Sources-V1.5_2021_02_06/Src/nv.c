/*
 * nv.c
 *
 *  Created on: 13.12.2016.
 *      Author: milan
 */
#include "nv.h"

#include "led.h"
#include "execution.h"

static int16_t nvSaveParmeterReq = -1;
static uint16_t nvSaveParmeterValue = 0xFFFF;
uint16_t nvInitFlag = 0xFFFF;

void NV_EraseAllVariables(void);


uint16_t VirtAddVarTab[NV_VAR_NUM] =
{
	NV_VAR_LIST
};


void NV_Init(void)
{
	/* Unlock the Flash Program Erase controller */
	FLASH_Unlock();

	/* EEPROM Init */
	EE_Init();

	EE_ReadVariable(NV_START_ID, &nvInitFlag);
}


void NV_FactoryReset(void)
{
	// Reset to default
	NV_EraseAllVariables();

	executionState = EXECUTION_STATE_CONFIG_RESET;

	while(1)
	{
	  LedSetRGB(LED1, 150, 0, 0);
	  LedSetRGB(LED2, 150, 0, 0);
	  HAL_Delay(500);
	  LedSetRGB(LED1, 0, 0, 150);
	  LedSetRGB(LED2, 0, 0, 150);
	  HAL_Delay(500);
	}
}


void NV_EraseAllVariables(void)
{
	int32_t i;

	for (i = NV_START_ID; i < NV_VAR_NUM; i++)
	{
		EE_WriteVariable(i, 0xFFFFu);
	}
}


void NvSetDataInitialized(void) {
	if (!NV_IS_DATA_INITIALIZED) {
		EE_WriteVariable(NV_START_ID, 0);
	}
}


void NvTask(void) {

	if (nvSaveParmeterReq >= 0) {
		EE_WriteVariable(BAT_PROFILE_NV_ADDR, nvSaveParmeterValue);
		nvSaveParmeterReq = -1;
	}
}


void NvSaveParameterReq(NvVarId_T id, uint16_t value) {
	nvSaveParmeterReq = id;
	nvSaveParmeterValue = value;
}


uint16_t NvReadVariableU8(uint16_t VirtAddress, uint8_t *pVar)
{
	uint16_t var = 0;
	uint16_t succ = EE_ReadVariable(VirtAddress, &var);

	if (succ==0)
	{
		if (NV_IS_VARIABLE_VALID(var))
		{
			*pVar = var&0xFF;
			return NV_READ_VARIABLE_SUCCESS;
		}
		else if ((var&0xFF) == (var>>8))
		{
			return NV_VARIABLE_NON_STORED;
		}
		else
			return NV_INVALID_VARIABLE;
	}
	else
	{
		return succ;
	}
}
