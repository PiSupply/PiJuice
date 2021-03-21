/*
 * system_conf.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef SYSTEM_CONF_H_
#define SYSTEM_CONF_H_

#define TIMER_OSLOOP				TIM6
#define OSLOOP_TIMER_IRQHandler		TIM6_IRQHandler

#define IODRV_PIN_IO1				0u
#define IODRV_PIN_IO1_GPIO			GPIOA
#define IODRV_PIN_IO1_GPIO_PIN_Pos	7u
#define IODRV_PIN_IO1_ADC_CHANNEL	ANALOG_CHANNEL_IO1
#define IODRV_PIN_IO1_ADC_CONV_K	ADC_TO_MV_K

#define IODRV_PIN_IO2				1u
#define IODRV_PIN_IO2_GPIO			GPIOA
#define IODRV_PIN_IO2_GPIO_PIN_Pos	8u

/* SW1 - Note: this is SW1 as on the PCB not schematic!! */
/* EXTI12 Pull down rising edge trigger */
#define IODRV_PIN_SW2				2u
#define IODRV_PIN_SW2_GPIO			GPIOB
#define IODRV_PIN_SW2_GPIO_PIN_Pos	12u

/* SW2 - Note: this is SW2 as on the PCB not schematic!! */
/* EXTI3 Pull down rising edge trigger */
#define IODRV_PIN_SW1				3u
#define IODRV_PIN_SW1_GPIO			GPIOC
#define IODRV_PIN_SW1_GPIO_PIN_Pos	13u

/* SW3 */
/* EXTI2 Pull down rising edge trigger */
#define IODRV_PIN_SW3				4u
#define IODRV_PIN_SW3_GPIO			GPIOB
#define IODRV_PIN_SW3_GPIO_PIN_Pos	2u

/* LDO regulator enable */
/* Output pushpull */
#define IODRV_PIN_POWDET_EN			5u
#define IODRV_PIN_POWDET_EN_GPIO	GPIOA
#define IODRV_PIN_POWDET_EN_PIN_Pos	11u

/* Boost converter enable */
/* Output tbd but initially set to input */
#define IODRV_PIN_POW_EN			6u
#define IODRV_PIN_POW_EN_GPIO		GPIOA
#define IODRV_PIN_POW_EN_PIN_Pos	10u

/* VSys enable */
/* Output tbd but initially set to input */
#define IODRV_PIN_EXTVS_EN			7u
#define IODRV_PIN_EXTVS_EN_GPIO		GPIOA
#define IODRV_PIN_EXTVS_EN_PIN_Pos	12u

/* Charger TS */
/* Ouput pushpull */
#define IODRV_PIN_TS_CTR1			8u
#define IODRV_PIN_TS_CTR1_GPIO		GPIOA
#define IODRV_PIN_TS_CTR1_PIN_Pos	6u

/* Output pushpull */
#define IODRV_PIN_TS_CTR2			9u
#define IODRV_PIN_TS_CTR2_GPIO		GPIOA
#define IODRV_PIN_TS_CTR2_PIN_Pos	15u

/* Charger interrupt */
/* EXTI0 falling edge trigger */
#define IODRV_PIN_CH_INT			10u
#define IODRV_PIN_CH_INT_GPIO		GPIOF
#define IODRV_PIN_CH_INT_PIN_Pos	0u

/* VSys current limit control */
/* Output open drain */
#define IODRV_PIN_ESYSLIM			11u
#define IODRV_PIN_ESYSLIM_GPIO		GPIOF
#define IODRV_PIN_ESYSLIM_PIN_Pos	1u

/* Fuel guage alarm output */
/* Maybe not initialised? Input no pull */
#define IODRV_PIN_BGINT				12u
#define IODRV_PIN_BGINT_GPIO		GPIOB
#define IODRV_PIN_BGINT_PIN_Pos		1u

/* I2C EEprom address control */
/* Output pushpull */
#define IODRV_PIN_EE_A				13u
#define IODRV_PIN_EE_A_GPIO			GPIOB
#define IODRV_PIN_EE_A_PIN_Pos		3u

/* I2C EEprom write control */
/* Output pushpull */
#define IODRV_PIN_EE_WP				14u
#define IODRV_PIN_EE_WP_GPIO		GPIOB
#define IODRV_PIN_EE_WP_PIN_Pos		8u

/* RPi RUN pin */
/* Output open drain, no pull */
#define IODRV_PIN_RUN				15u
#define IODRV_PIN_RUN_GPIO			GPIOB
#define IODRV_PIN_RUN_PIN_Pos		13u


#define IODRV_MAX_IO_PINS			16u

#define ANALOG_CHANNEL_CS1			0u
#define ANALOG_CHANNEL_CS2			1u
#define ANALOG_CHANNEL_VBAT			2u
#define ANALOG_CHANNEL_NTC			3u
#define ANALOG_CHANNEL_POW_DET		4u
#define ANALOG_CHANNEL_BATTYPE		5u
#define ANALOG_CHANNEL_IO1			6u
#define ANALOG_CHANNEL_MPUTEMP		7u
#define ANALOG_CHANNEL_INTREF		8u

#define MAX_ANALOG_CHANNELS			9u

#define TEMP30_CAL_ADDR 			((uint16_t*)((uint32_t)0x1FFFF7B8u))
#define VREFINT_CAL_ADDR 			((uint16_t*)((uint32_t)0x1FFFF7BAu))
#define ADC_TO_MV_K					52800u
#define ADC_TO_BATTMV_K				72547u
#define ADC_TO_5VRAIL_ISEN_K		0x10000

#define IODRV_PIN_DEBOUNCE_COUNT	5u

#define BUTTON_EVENT_EXPIRE_PERIOD_MS	30000u

#define IODRV_PIN_UPDATE_PERIOD_MS	10u
#define ADC_SAMPLE_PERIOD_MS		50u


#endif /* SYSTEM_CONF_H_ */
