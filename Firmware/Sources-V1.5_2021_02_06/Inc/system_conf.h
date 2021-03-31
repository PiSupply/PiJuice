/*
 * system_conf.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef SYSTEM_CONF_H_
#define SYSTEM_CONF_H_

#define TIMER_OSLOOP						TIM6
#define OSLOOP_TIMER_IRQHandler				TIM6_IRQHandler

#define IODRV_PIN_IO1						0u
#define IODRV_PIN_IO1_GPIO					GPIOA
#define IODRV_PIN_IO1_GPIO_PIN_Pos			7u
#define IODRV_PIN_IO1_ADC_CHANNEL			ANALOG_CHANNEL_IO1
#define IODRV_PIN_IO1_ADC_CONV_K			ADC_TO_MV_K
#define IODRV_PIN_IO1_INVERT_bm				0u

#define IODRV_PIN_IO2						1u
#define IODRV_PIN_IO2_GPIO					GPIOA
#define IODRV_PIN_IO2_GPIO_PIN_Pos			8u
#define IODRV_PIN_IO2_INVERT_bm				0u

/* SW1 - Note: this is SW1 as on the PCB not schematic!! */
/* EXTI12 Pull down rising edge trigger */
#define IODRV_PIN_SW2						2u
#define IODRV_PIN_SW2_GPIO					GPIOB
#define IODRV_PIN_SW2_GPIO_PIN_Pos			12u
#define IODRV_PIN_SW2_INVERT_bm				0u

/* SW2 - Note: this is SW2 as on the PCB not schematic!! */
/* EXTI3 Pull down rising edge trigger */
#define IODRV_PIN_SW1						3u
#define IODRV_PIN_SW1_GPIO					GPIOC
#define IODRV_PIN_SW1_GPIO_PIN_Pos			13u
#define IODRV_PIN_SW1_INVERT_bm				0u

/* SW3 */
/* EXTI2 Pull down rising edge trigger */
#define IODRV_PIN_SW3						4u
#define IODRV_PIN_SW3_GPIO					GPIOB
#define IODRV_PIN_SW3_GPIO_PIN_Pos			2u
#define IODRV_PIN_SW3_INVERT_bm				0u

/* LDO regulator enable */
/* Output pushpull */
#define IODRV_PIN_POWDET_EN					5u
#define IODRV_PIN_POWDET_EN_GPIO			GPIOA
#define IODRV_PIN_POWDET_EN_PIN_Pos			11u
#define IODRV_PIN_POWDET_EN_INVERT_bm		0u

/* Boost converter enable */
/* Output tbd but initially set to input */
#define IODRV_PIN_POW_EN					6u
#define IODRV_PIN_POW_EN_GPIO				GPIOA
#define IODRV_PIN_POW_EN_PIN_Pos			10u
#define IODRV_PIN_POW_EN_INVERT_bm			0u

/* VSys enable */
/* Output tbd but initially set to input */
#define IODRV_PIN_EXTVS_EN					7u
#define IODRV_PIN_EXTVS_EN_GPIO				GPIOA
#define IODRV_PIN_EXTVS_EN_PIN_Pos			12u
#define IODRV_PIN_EXTVS_INVERT_bm			0x01u

/* Charger TS */
/* Ouput pushpull */
#define IODRV_PIN_TS_CTR1					8u
#define IODRV_PIN_TS_CTR1_GPIO				GPIOA
#define IODRV_PIN_TS_CTR1_PIN_Pos			6u
#define IODRV_PIN_TS_CTR1_INVERT_bm			0u

/* Output pushpull */
#define IODRV_PIN_TS_CTR2					9u
#define IODRV_PIN_TS_CTR2_GPIO				GPIOA
#define IODRV_PIN_TS_CTR2_PIN_Pos			15u
#define IODRV_PIN_TS_CTR2_INVERT_bm			0u

/* Charger interrupt */
/* EXTI0 falling edge trigger */
#define IODRV_PIN_CH_INT					10u
#define IODRV_PIN_CH_INT_GPIO				GPIOF
#define IODRV_PIN_CH_INT_PIN_Pos			0u
#define IODRV_PIN_CH_INT_INVERT_bm			0u

/* VSys current limit control - Sets 500mA drive limit */
/* Output open drain */
#define IODRV_PIN_ESYSLIM					11u
#define IODRV_PIN_ESYSLIM_GPIO				GPIOF
#define IODRV_PIN_ESYSLIM_PIN_Pos			1u
#define IODRV_PIN_ESYSLIM_INVERT_bm			0u

/* Fuel guage alarm output */
/* Maybe not initialised? Input no pull */
#define IODRV_PIN_BGINT						12u
#define IODRV_PIN_BGINT_GPIO				GPIOB
#define IODRV_PIN_BGINT_PIN_Pos				1u
#define IODRV_PIN_BGINT_INVERT_bm			0u

/* I2C EEprom address control */
/* Output pushpull */
#define IODRV_PIN_EE_A						13u
#define IODRV_PIN_EE_A_GPIO					GPIOB
#define IODRV_PIN_EE_A_PIN_Pos				3u
#define IODRV_PIN_EE_A_INVERT_bm			0u

/* I2C EEprom write control */
/* Output pushpull */
#define IODRV_PIN_EE_WP						14u
#define IODRV_PIN_EE_WP_GPIO				GPIOB
#define IODRV_PIN_EE_WP_PIN_Pos				8u
#define IODRV_PIN_EE_WP_INVERT_bm			0u

/* RPi RUN pin */
/* Output open drain, no pull */
#define IODRV_PIN_RUN						15u
#define IODRV_PIN_RUN_GPIO					GPIOB
#define IODRV_PIN_RUN_PIN_Pos				13u
#define IODRV_PIN_RUN_INVERT_bm				0u

#define IODRV_MAX_IO_PINS					16u

#define ANALOG_CHANNEL_CS1					0u
#define ANALOG_CHANNEL_CS2					1u
#define ANALOG_CHANNEL_VBAT					2u
#define ANALOG_CHANNEL_NTC					3u
#define ANALOG_CHANNEL_POW_DET				4u
#define ANALOG_CHANNEL_BATTYPE				5u
#define ANALOG_CHANNEL_IO1					6u
#define ANALOG_CHANNEL_MPUTEMP				7u
#define ANALOG_CHANNEL_INTREF				8u

#define FILTER_PERIOD_MS_CS1				10u
#define FILTER_PERIOD_MS_CS2				10u
#define FILTER_PERIOD_MS_VBAT				10u
#define FILTER_PERIOD_MS_NTC				10u
#define FILTER_PERIOD_MS_POW_DET			10u
#define FILTER_PERIOD_MS_BATTYPE			10u
#define FILTER_PERIOD_MS_IO1				10u
#define FILTER_PERIOD_MS_MPUTEMP			10u
#define FILTER_PERIOD_MS_INTREF				10u

#define MAX_ANALOG_CHANNELS					9u

#define TEMP30_CAL_ADDR 					((uint16_t*)((uint32_t)0x1FFFF7B8u))
#define VREFINT_CAL_ADDR 					((uint16_t*)((uint32_t)0x1FFFF7BAu))
#define ADC_TO_MV_K							52800u
#define ADC_TO_BATTMV_K						72547ul
#define ADC_TO_5VRAIL_MV_K					105601ul
#define ADC_TO_5VRAIL_ISEN_K				21120000ul

#define IODRV_PIN_DEBOUNCE_COUNT			5u

#define BUTTON_EVENT_EXPIRE_PERIOD_MS		30000u
#define BUTTON_MAX_BUTTONS					3u

#define BUTTON_1_IODRV_PIN					IODRV_PIN_SW1
#define BUTTON_2_IODRV_PIN					IODRV_PIN_SW2
#define BUTTON_3_IODRV_PIN					IODRV_PIN_SW3

#define BUTTON_STATIC_LONG_PRESS_TIME		19600u
#define BUTTON_EVENT_FUNC_USER_EVENT		0x20u
#define BUTTON_EVENT_FUNC_SYS_EVENT			0x10u

#define IODRV_PIN_UPDATE_PERIOD_MS			10u
#define ADC_SAMPLE_PERIOD_MS				0u

#define I2CDRV_MAX_DEVICES					2u
#define I2CDRV_MAX_BUFFER_SIZE				12u		/* Big enough to read all registers from charger */

#define FUELGUAGE_I2C_ADDR					0x16u
#define FUELGUAGE_I2C_PORTNO				1u

#define FUELGUAGE_MIN_BATT_MV				2500u
#define FUELGUAGE_SOC_TO_I_K				4748u	/* Divide by 13.888888 */

#define FUELGUAGE_TASK_PERIOD_MS			125u
#define POWERMANAGE_TASK_PERIOD_MS			500u


#define CHARGER_I2C_ADDR					0xD6u
#define CHARGER_I2C_PORTNO					1u

#define CHARGER_INPUT_VIN					0u
#define CHARGER_INPUT_RPI					1u
#define CHARGER_INPUT_CHANNELS				2u

#define BATTERY_UPDATE_STATUS_PERIOD_MS		10u
#define BATTERY_CHARGE_LED_UPDATE_PERIOD_MS	900u

#define POWERSOURCE_5VRAIL_PROCESS_PERIOD_MS	100u
#define POWERSOURCE_STABLISE_TIME_MS		40u
#define POWERSROUCE_RPI5V_DETECT_PERIOD_MS	10u
#define POWERSOURCE_RPI5V_POWERED_MS		50u
#define POWERSOURCE_RPI_VLOW_MAX_COUNT		10u
#define POWERSOURCE_LDO_MV					4800u
#define POWERSOURCE_RPI_LOW_MV				4800u
#define POWERSOURCE_RPI_UNDER_MV			4500u

#endif /* SYSTEM_CONF_H_ */
