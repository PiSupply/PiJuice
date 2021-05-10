/*
 * config_switch_resistor.c
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */
#include "main.h"
#include "config_switch_resistor.h"

#define CONFIG_SW_01_ADC	2048 // 1.65
#define CONFIG_SW_10_ADC	2981 // 2.402
#define CONFIG_SW_11_ADC	3220 // 2.594

#define CONFIG_SW_ADC_TOL	5 // 2.594

#define CONFIG_SW_11_ADC_HIGH	(CONFIG_SW_11_ADC - CONFIG_SW_ADC_TOL)
#define CONFIG_SW_11_ADC_LOW	2986 // 2.403 - 2982 ~ 270K
#define CONFIG_SW_10_ADC_HIGH	(CONFIG_SW_10_ADC - CONFIG_SW_ADC_TOL)
#define CONFIG_SW_10_ADC_LOW	2712 // 2.182 - 2708 ~ 270K
#define CONFIG_SW_01_ADC_HIGH	(CONFIG_SW_01_ADC - CONFIG_SW_ADC_TOL)
#define CONFIG_SW_01_ADC_LOW	1732 // 1.392 - 1728 ~ 270K

#define CONFIG_SWR2_11_ADC_HIGH	1702 // 1.371
#define CONFIG_SWR2_11_ADC_LOW	(1398+8) // 1.126 - 1398 ~ 16.4K
#define CONFIG_SWR2_10_ADC_HIGH	1396 // 1.125
#define CONFIG_SWR2_10_ADC_LOW	(1121+8) // 0.903 - 1121 ~ 16.4K
#define CONFIG_SWR2_01_ADC_HIGH	665  // 0.536
#define CONFIG_SWR2_01_ADC_LOW	(506+8) // 0.408 - 506 ~ 16.4K

// v = R / (R + 50K) * 1.65
// adc = v / 3.3 * 4096 = R / (R + 50K) / 2 * 4096
// code = (CONFIG_SW_11_ADC_HIGH - R / (R + 50K) * 2048) * 128 / (CONFIG_SW_11_ADC_HIGH - CONFIG_SW_11_ADC_LOW)
// code * (CONFIG_SW_11_ADC_HIGH - CONFIG_SW_11_ADC_LOW) / 128 = CONFIG_SW_11_ADC_HIGH - R / (R + 50K) * 2048
// R / (R + 50K) = CONFIG_SW_11_ADC_HIGH / 2048 - code * (CONFIG_SW_11_ADC_HIGH - CONFIG_SW_11_ADC_LOW) / 128 / 2048 = k
// R=k*50000/(1-k)
// R1 * R2 / (R1 + R2) = R, R*R1+R*R2 = R1*R2, R*R1=R2*(R1-R), R2 = R*R1/(R1-R)

//volatile uint16_t resistorConfigVol;
int8_t switchConfigCode = -1;
int16_t resistorConfig1Code7 = -1;
int8_t resistorConfig2Code4 = -1;

void SwitchResConfigInit(uint32_t resistorConfigAdc)
{
	if ( resistorConfigAdc > CONFIG_SW_11_ADC_HIGH + 2)
	{
		switchConfigCode = 0x3;
	}
	else if ( resistorConfigAdc > CONFIG_SW_11_ADC_LOW )
	{
		switchConfigCode = 0x3;
		resistorConfig1Code7 = ((((CONFIG_SW_11_ADC_HIGH - resistorConfigAdc) * 128 / (CONFIG_SW_11_ADC_HIGH - CONFIG_SW_11_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if ( resistorConfigAdc > CONFIG_SW_10_ADC_HIGH + 2)
	{
		switchConfigCode = 0x2;
	}
	else if ( resistorConfigAdc > CONFIG_SW_10_ADC_LOW )
	{
		switchConfigCode = 0x2;
		resistorConfig1Code7 = ((((CONFIG_SW_10_ADC_HIGH - resistorConfigAdc) * 128 / (CONFIG_SW_10_ADC_HIGH - CONFIG_SW_10_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if ( resistorConfigAdc > CONFIG_SW_01_ADC_HIGH + 2)
	{
		switchConfigCode = 0x1;
	}
	else if ( resistorConfigAdc > CONFIG_SW_01_ADC_LOW )
	{
		switchConfigCode = 0x1;
		resistorConfig1Code7 = ((((CONFIG_SW_01_ADC_HIGH - resistorConfigAdc) * 128 / (CONFIG_SW_01_ADC_HIGH - CONFIG_SW_01_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_11_ADC_HIGH + 3)
	{
		// TODO - Figure out what goes here
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_11_ADC_LOW )
	{
		switchConfigCode = 0x3;
		resistorConfig2Code4 = ((((CONFIG_SWR2_11_ADC_HIGH - resistorConfigAdc) * 16 / (CONFIG_SWR2_11_ADC_HIGH - CONFIG_SWR2_11_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_10_ADC_HIGH + 3)
	{
		// TODO - Figure out what goes here
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_10_ADC_LOW )
	{
		switchConfigCode = 0x2;
		resistorConfig2Code4 = ((((CONFIG_SWR2_10_ADC_HIGH - resistorConfigAdc) * 16 / (CONFIG_SWR2_10_ADC_HIGH - CONFIG_SWR2_10_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_01_ADC_HIGH + 3)
	{
		// TODO - Figure out what goes here
	}
	else if ( resistorConfigAdc > CONFIG_SWR2_01_ADC_LOW )
	{
		switchConfigCode = 0x1;
		resistorConfig2Code4 = ((((CONFIG_SWR2_01_ADC_HIGH - resistorConfigAdc) * 16 / (CONFIG_SWR2_01_ADC_HIGH - CONFIG_SWR2_01_ADC_LOW)) << 1) + 1) >> 1;
	}
	else if (resistorConfigAdc < 8)
	{
		switchConfigCode = 0x0;
	}
}
