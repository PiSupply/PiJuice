/*
 * system_conf.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef SYSTEM_CONF_H_
#define SYSTEM_CONF_H_

#define IOPIN_1						0u
#define IOPIN_1_GPIO				GPIOA
#define IOPIN_1_GPIO_PIN			GPIO_PIN_7
#define IOPIN_1_MODER_POS			GPIO_MODER_MODER7_Pos
#define IOPIN_1_ADC_CHANNEL			ANALOG_CHANNEL_IO1

#define IOPIN_2						1u
#define IOPIN_2_GPIO				GPIOA
#define IOPIN_2_GPIO_PIN			GPIO_PIN_8
#define IOPIN_2_MODER_POS			GPIO_MODER_MODER8_Pos

#define MAX_IO_PINS					2u

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

#define IODRV_PIN_UPDATE_PERIOD_MS	10u
#define ADC_SAMPLE_PERIOD_MS		50u


#endif /* SYSTEM_CONF_H_ */
