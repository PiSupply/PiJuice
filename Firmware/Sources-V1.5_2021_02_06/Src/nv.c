/*
 * nv.c
 *
 *  Created on: 13.12.2016.
 *      Author: milan
 */

#include "main.h"
#include "system_conf.h"


#include "led.h"
#include "execution.h"

#include "util.h"

#include "nv.h"

static int16_t nvSaveParmeterReq = -1;
static uint16_t nvSaveParmeterValue = 0xFFFF;
static uint16_t nvInitFlag = 0xFFFF;

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

	while(true)
	{
		LED_SetRGB(LED_LED1_IDX, 150u, 0u, 0u);
		LED_SetRGB(LED_LED2_IDX, 150u, 0u, 0u);

		HAL_Delay(500u);

		LED_SetRGB(LED_LED1_IDX, 0u, 0u, 150u);
		LED_SetRGB(LED_LED2_IDX, 0u, 0u, 150u);

		HAL_Delay(500u);
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


bool NV_WriteVariable_S8(const uint16_t address, const int8_t var)
{
	return NV_WriteVariable_U8(address, (uint8_t)var);
}


bool NV_WriteVariable_U8(const uint16_t address, const uint8_t var)
{
	return (HAL_OK == EE_WriteVariable(address, (uint16_t)var | ((uint16_t)(~var) << 8u)));
}


bool NV_ReadVariable_S8(const uint16_t address, int8_t * const p_var)
{
	return NV_ReadVariable_U8(address, (uint8_t*)p_var);
}


bool NV_ReadVariable_U8(const uint16_t address, uint8_t * const p_var)
{
	uint16_t nVar;
	const bool readResult = (HAL_OK == EE_ReadVariable(address, &nVar));

	if (false == readResult)
	{
		return false;
	}

	if (false == UTIL_NV_ParamInitCheck_U16(nVar))
	{
		return false;
	}

	*p_var = nVar;

	return true;
}


void NV_WipeVariable(const uint16_t address)
{
	EE_WriteVariable(address, 0xFFFFu);
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
