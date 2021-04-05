#include "main.h"
#include "system_conf.h"
#include "eeprom.h"
#include "nv.h"
#include "iodrv.h"


void E2_Init(void)
{
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET); // ee write protect

	uint16_t var = 0;

	EE_ReadVariable(ID_EEPROM_ADR_NV_ADDR, &var);

	if ( (((~var)&0xFF) == (var>>8)) )
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, (var&0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
	else
	{
		HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_RESET); // default ee Adr
	}
}


void E2_Task(void)
{

}
