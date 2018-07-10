/*
 * nv.c
 *
 *  Created on: 13.12.2016.
 *      Author: milan
 */
#include "nv.h"

static int16_t nvSaveParmeterReq = -1;
static uint16_t nvSaveParmeterValue = 0xFFFF;
uint16_t nvInitFlag = 0xFFFF;

uint16_t VirtAddVarTab[NV_VAR_NUM] = {
	NV_VAR_LIST
};

void NvInit(void){
	/* Unlock the Flash Program Erase controller */
	FLASH_Unlock();

	/* EEPROM Init */
	EE_Init();

	EE_ReadVariable(NV_START_ID, &nvInitFlag);
}

void NvEreaseAllVariables(void) {
	int32_t i;

	for (i=NV_START_ID;i<NV_VAR_NUM;i++) {
		EE_WriteVariable(i, 0xFFFF);
	}

	//FLASH_ErasePage(PAGE0_BASE_ADDRESS);
	//FLASH_ErasePage(PAGE1_BASE_ADDRESS);
	//EE_Init();
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
