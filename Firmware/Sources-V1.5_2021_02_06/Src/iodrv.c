/*
 * iodrv.c
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

// ----------------------------------------------------------------------------
// Include section - add all #includes here:

#include <main.h>

#include "system_conf.h"
#include "ave_filter.h"
#include "time_count.h"
#include "adc.h"
#include "iodrv.h"

// ----------------------------------------------------------------------------
// Defines section - add all #defines here:

#define PINMODE_INPUT				0u
#define PINMODE_OUTPUT				1u
#define PINMODE_ALTERNATE			2u
#define PINMODE_ANALOG				3u


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void IODRV_UpdatePins(const uint32_t sysTime);


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

IODRV_Pin_t m_pins[IODRV_MAX_IO_PINS] =
{
		{
				.adcChannel = IODRV_PIN_IO1_ADC_CHANNEL,
				.gpioPin_bm = (1u << IODRV_PIN_IO1_GPIO_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_IO1_GPIO_PIN_Pos,
				.gpioPort = IODRV_PIN_IO1_GPIO,
				.canConfigure = true
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_IO2_GPIO_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_IO2_GPIO_PIN_Pos,
				.gpioPort = IODRV_PIN_IO2_GPIO,
				.canConfigure = true
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_SW1_GPIO_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_SW1_GPIO_PIN_Pos,
				.gpioPort = IODRV_PIN_SW1_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_SW2_GPIO_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_SW2_GPIO_PIN_Pos,
				.gpioPort = IODRV_PIN_SW2_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_SW3_GPIO_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_SW3_GPIO_PIN_Pos,
				.gpioPort = IODRV_PIN_SW3_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_POWDET_EN_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_POWDET_EN_PIN_Pos,
				.gpioPort = IODRV_PIN_POWDET_EN_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_POW_EN_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_POW_EN_PIN_Pos,
				.gpioPort = IODRV_PIN_POW_EN_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_EXTVS_EN_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_EXTVS_EN_PIN_Pos,
				.gpioPort = IODRV_PIN_EXTVS_EN_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_TS_CTR1_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_TS_CTR1_PIN_Pos,
				.gpioPort = IODRV_PIN_TS_CTR1_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_TS_CTR2_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_TS_CTR2_PIN_Pos,
				.gpioPort = IODRV_PIN_TS_CTR2_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_CH_INT_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_CH_INT_PIN_Pos,
				.gpioPort = IODRV_PIN_CH_INT_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_ESYSLIM_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_ESYSLIM_PIN_Pos,
				.gpioPort = IODRV_PIN_ESYSLIM_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_BGINT_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_BGINT_PIN_Pos,
				.gpioPort = IODRV_PIN_BGINT_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_EE_A_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_EE_A_PIN_Pos,
				.gpioPort = IODRV_PIN_EE_A_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_EE_WP_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_EE_WP_PIN_Pos,
				.gpioPort = IODRV_PIN_EE_WP_GPIO,
				.canConfigure = false
		},
		{
				.adcChannel = MAX_ANALOG_CHANNELS,
				.gpioPin_bm = (1u << IODRV_PIN_RUN_PIN_Pos),
				.gpioPin_pos = IODRV_PIN_RUN_PIN_Pos,
				.gpioPort = IODRV_PIN_RUN_GPIO,
				.canConfigure = false
		}
};


static uint32_t m_lastPinUpdateTime;


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH GLOBAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * IODRV_Init configures the module to a known initial state
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void IODRV_Init(uint32_t sysTime)
{
	uint8_t i;

	for (i = 0u; i < IODRV_MAX_IO_PINS; i++)
	{
		m_pins[i].value = 0u;
	}

	MS_TIMEREF_INIT(m_lastPinUpdateTime, sysTime);
}


// ****************************************************************************
/*!
 * IODRV_Service performs periodic updates for this module
 *
 * @param	sysTime		current value of the ms tick timer
 * @retval	none
 */
// ****************************************************************************
void IODRV_Service(uint32_t sysTime)
{
	if (MS_TIMEREF_TIMEOUT(m_lastPinUpdateTime, sysTime, IODRV_PIN_UPDATE_PERIOD_MS))
	{
		IODRV_UpdatePins(sysTime);
	}
}


// ****************************************************************************
/*!
 * IODRV_ReadPin returns the value of the pin after it has been processed by the
 * average filter, could be analog (0..4095) or digital (0..1). The caller is
 * expected to know!!
 *
 * @param	pin			index of the pin that is required
 * @retval	uint16_t	value of the pin that is required
 */
// ****************************************************************************
uint16_t IODRV_ReadPinValue(uint8_t pin)
{
	if ( pin >= IODRV_MAX_IO_PINS )
	{
		return 0u;
	}

	return m_pins[pin].value;
}


// ****************************************************************************
/*!
 * IODRV_ReadPinOutputState returns the value of the output data register, giving
 * the expected drive level (could still be pulled down)
 *
 * @param	pin			index of the pin that is required
 * @retval	uint8_t		value of the pin that is required
 */
// ****************************************************************************
uint8_t IODRV_ReadPinOutputState(uint8_t pin)
{
	if ( pin >= IODRV_MAX_IO_PINS )
	{
		return 0u;
	}

	return (m_pins[pin].gpioPort->ODR & m_pins[pin].gpioPin_bm) != 0u;
}


// ****************************************************************************
/*!
 * IODRV_WritePin sets the digital output value of an io pin, checks to make sure
 * the pin is in bounds and the value is in bounds. Will return false if the pin
 * is not a digital output.
 *
 * @param	pin			index of the pin that is required
 * @param	newValue	value to be set, maps to GPIO_PIN_SET and GPIO_RESET
 * @retval	bool		returns true is the pin is an output, false if an input
 */
// ****************************************************************************
bool IODRV_WritePin(uint8_t pin, uint8_t newValue)
{
	if ( (m_pins[pin].pinType == IOTYPE_DIGOUT_PUSHPULL) || (m_pins[pin].pinType <= IOTYPE_DIGOUT_OPENDRAIN) )
	{
		return IODRV_SetPin(pin, newValue);
	}

	return false;
}


// ****************************************************************************
/*!
 * IODRV_SetPin sets the digital output value of an io pin, checks to make sure
 * the pin is in bounds and the value is in bounds.
 *
 * @param	pin			index of the pin that is required
 * @param	newValue	value to be set, maps to GPIO_PIN_SET and GPIO_RESET
 * @retval	bool		returns true if the arguments are correct
 */
// ****************************************************************************
bool IODRV_SetPin(uint8_t pin, uint8_t newValue)
{
	if ( (pin >= IODRV_MAX_IO_PINS) || ((newValue != GPIO_PIN_RESET) || newValue != GPIO_PIN_SET) )
	{
		return false;
	}

	HAL_GPIO_WritePin(m_pins[pin].gpioPort, m_pins[pin].gpioPin_bm, newValue);

	return true;
}


// ****************************************************************************
/*!
 * IODRV_SetPinType sets the required operation for the io pin, this can be analog,
 * digital, or PWM. The output and PWM modes are push pull or open drain. The PWM
 * must be configured separately.
 *
 * @param	pin			index of the pin that is required to be set
 * @param	newType		type of io to be configured
 * @retval	bool		true if the pin has been set
 */
// ****************************************************************************
bool IODRV_SetPinType(uint8_t pin, IODRV_PinType_t newType)
{
	const uint32_t otyper_bm = (1u << m_pins[pin].gpioPin_pos);
	const uint32_t moder_pos = (m_pins[pin].gpioPin_pos * 2u);

	if ( (pin >= IODRV_MAX_IO_PINS) || (false == m_pins[pin].canConfigure) )
	{
		return false;
	}

	m_pins[pin].gpioPort->MODER &= ~(3u << moder_pos);
	m_pins[pin].gpioPort->OTYPER &= ~(otyper_bm);

	switch (newType)
	{
	case IOTYPE_DIGOUT_OPENDRAIN:
		m_pins[pin].gpioPort->OTYPER |= otyper_bm;
	case IOTYPE_DIGOUT_PUSHPULL:
		m_pins[pin].gpioPort->MODER |= (PINMODE_OUTPUT << moder_pos);
		break;

	case IOTYPE_PWM_OPENDRAIN:
		m_pins[pin].gpioPort->OTYPER |= otyper_bm;
	case IOTYPE_PWM_PUSHPULL:
		m_pins[pin].gpioPort->MODER |= (PINMODE_ALTERNATE << moder_pos);
		break;

	case IOTYPE_ANALOG:
		m_pins[pin].gpioPort->MODER |= (PINMODE_ANALOG << moder_pos);
		break;

	default:
		break;
	}

	return true;

}


// ****************************************************************************
/*!
 * IODRV_SetPinPullDir sets the required pull direction for the io pin, this can be
 * up, down or none.
 *
 * @param	pin				index of the pin that is required to be set
 * @param	pullDirection	direction the pin is to be pulled:
 * 							GPIO_NOPULL, GPIO_PULLUP, GPIO_PULLDOWN
 * @retval	bool			true if the pin has been set
 */
// ****************************************************************************
bool IODRV_SetPinPullDir(uint8_t pin, uint32_t pullDirection)
{
	const uint32_t pupdr_pos = (m_pins[pin].gpioPin_pos * 2u);

	if ( (pin >= IODRV_MAX_IO_PINS) || (false == m_pins[pin].canConfigure) || (pullDirection > 2u))
	{
		return false;
	}

	m_pins[pin].gpioPort->PUPDR &= ~(3u << pupdr_pos);
	m_pins[pin].gpioPort->PUPDR |= (pullDirection << pupdr_pos);

	return true;
}


// ****************************************************************************
/*!
 * IODRV_GetPinInfo gets a const pointer to the pin informationd
 *
 * @param	pin				index of the pin that is required to be set
 * @retval	IODRV_Pin_t*	const pointer to the pin data, NULL if pin not valid
 */
// ****************************************************************************
const IODRV_Pin_t * IODRV_GetPinInfo(uint8_t pin)
{
	if ( (pin >= IODRV_MAX_IO_PINS) )
	{
		return NULL;
	}

	return &m_pins[pin];
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// FUNCTIONS WITH LOCAL SCOPE
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// ****************************************************************************
/*!
 * IODRV_UpdatePins updates the value for each pin
 *
 * @param	none
 * @retval	none
 */
// ****************************************************************************
void IODRV_UpdatePins(const uint32_t sysTime)
{
	uint8_t pin;
	uint16_t value = 0u;
	uint8_t pinType;
	uint32_t moder_pos;

	for (pin = 0u; pin < IODRV_MAX_IO_PINS; pin++)
	{
		moder_pos = (m_pins[pin].gpioPin_pos * 2u);
		pinType = (m_pins[pin].gpioPort->MODER >> moder_pos) & 0x03u;

		if (pinType == PINMODE_ANALOG)
		{
			// Read analog pin value, will be 0 if not connected to the ADC
			value = ADC_GetAverageValue(m_pins[pin].adcChannel);
		}
		else if (pinType == PINMODE_ALTERNATE)
		{
			value = 0u;
		}
		else // Must be digital then
		{
			if (GPIO_PIN_SET == HAL_GPIO_ReadPin(m_pins[pin].gpioPort, m_pins[pin].gpioPin_bm))
			{
				if (m_pins[pin].debounceCounter < IODRV_PIN_DEBOUNCE_COUNT)
				{
					m_pins[pin].debounceCounter++;
				}
				else if (m_pins[pin].value != GPIO_PIN_SET)
				{
					/* log positive pulse width */
					m_pins[pin].lastPosPulseWidthTimeMs = MS_TIMEREF_DIFF(m_pins[pin].lastDigitalChangeTime, sysTime);

					/* log the change time */
					MS_TIMEREF_INIT(m_pins[pin].lastDigitalChangeTime, sysTime);

					m_pins[pin].value = GPIO_PIN_SET;
				}
			}
			else
			{
				if (m_pins[pin].debounceCounter > 0u)
				{
					m_pins[pin].debounceCounter--;
				}
				else if (m_pins[pin].value != GPIO_PIN_RESET)
				{
					/* log negative pulse width */
					m_pins[pin].lastNegPulseWidthTimeMs = MS_TIMEREF_DIFF(m_pins[pin].lastDigitalChangeTime, sysTime);

					/* log the change time */
					m_pins[pin].lastDigitalChangeTime = sysTime;
					m_pins[pin].value = GPIO_PIN_RESET;
				}
			}
		}

		m_pins[pin].value = value;
	}
}

