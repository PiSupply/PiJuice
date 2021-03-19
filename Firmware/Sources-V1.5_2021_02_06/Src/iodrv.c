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

typedef struct
{
	uint16_t 			value;
	AVE_FILTER_U16_t	aveFilter;
	IODRV_PinType_t		pinType;
	uint16_t			gpioPin;
	GPIO_TypeDef*		gpioPort;
	uint8_t				gpioModer_pos;
	uint32_t			gpioOTyper_bm;
} IODRV_Pin_t;


// ----------------------------------------------------------------------------
// Function prototypes for functions that only have scope in this module:

void IODRV_UpdatePins(void);


// ----------------------------------------------------------------------------
// Variables that have scope from outside this module:


// ----------------------------------------------------------------------------
// Variables that only have scope in this module:

IODRV_Pin_t m_pins[] =
{
		{
				.gpioPin = IOPIN_1_GPIO_PIN,
				.gpioPort = IOPIN_1_GPIO,
				.gpioModer_pos = IOPIN_1_MODER_POS,
				.gpioOTyper_bm = GPIO_OTYPER_OT_7
		},
		{
				.gpioPin = IOPIN_2_GPIO_PIN,
				.gpioPort = IOPIN_2_GPIO,
				.gpioModer_pos = IOPIN_2_MODER_POS,
				.gpioOTyper_bm = GPIO_OTYPER_OT_8
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
	AVE_FILTER_U16_Reset(&m_pins[IOPIN_1].aveFilter);
	AVE_FILTER_U16_Reset(&m_pins[IOPIN_2].aveFilter);

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
		IODRV_UpdatePins();
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
uint16_t IODRV_ReadPinValue(uint32_t pin)
{
	if ( pin >= MAX_IO_PINS )
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
uint8_t IODRV_ReadPinOutputState(uint32_t pin)
{
	if ( pin >= MAX_IO_PINS )
	{
		return 0u;
	}

	return (m_pins[pin].gpioPort->ODR & m_pins[pin].gpioPin) != 0u;
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
bool IODRV_WritePin(uint32_t pin, uint8_t newValue)
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
bool IODRV_SetPin(uint32_t pin, uint8_t newValue)
{
	if ( (pin >= MAX_IO_PINS) || ((newValue != GPIO_PIN_RESET) || newValue != GPIO_PIN_SET) )
	{
		return false;
	}

	HAL_GPIO_WritePin(m_pins[pin].gpioPort, m_pins[pin].gpioPin, newValue);

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
bool IODRV_SetPinType(uint32_t pin, IODRV_PinType_t newType)
{
	bool result = false;

	if ( pin >= MAX_IO_PINS )
	{
		return false;
	}

	m_pins[pin].gpioPort->MODER &= ~(3u << m_pins[pin].gpioModer_pos);
	m_pins[pin].gpioPort->OTYPER &= ~m_pins[pin].gpioOTyper_bm;

	switch (newType)
	{
	case IOTYPE_DIGOUT_OPENDRAIN:
		m_pins[pin].gpioPort->OTYPER |= m_pins[pin].gpioOTyper_bm;
	case IOTYPE_DIGOUT_PUSHPULL:
		m_pins[pin].gpioPort->MODER |= (PINMODE_OUTPUT << m_pins[pin].gpioModer_pos);
		break;

	case IOTYPE_PWM_OPENDRAIN:
		m_pins[pin].gpioPort->OTYPER |= m_pins[pin].gpioOTyper_bm;
	case IOTYPE_PWM_PUSHPULL:
		m_pins[pin].gpioPort->MODER |= (PINMODE_ALTERNATE << m_pins[pin].gpioModer_pos);
		break;

	case IOTYPE_ANALOG:
		m_pins[pin].gpioPort->MODER |= (PINMODE_ANALOG << m_pins[pin].gpioModer_pos);
		break;

	default:
		break;
	}

	return result;

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
void IODRV_UpdatePins(void)
{
	uint8_t pin;
	uint16_t value;

	for (pin = 0u; pin < MAX_IO_PINS; pin++)
	{
		if (m_pins[pin].pinType == IOTYPE_ANALOG)
		{
			// Read analog pin value
			value = ADC_GetAverageValue(IOPIN_1_ADC_CHANNEL);
		}
		else if ((m_pins[pin].pinType == IOTYPE_PWM_PUSHPULL) || (m_pins[pin].pinType == IOTYPE_PWM_OPENDRAIN))
		{
			value = 0u;
		}
		else
		{
			AVE_FILTER_U16_Update(&m_pins[pin].aveFilter, HAL_GPIO_ReadPin(m_pins[pin].gpioPort, m_pins[pin].gpioPin));
			value = m_pins[pin].aveFilter.average;
		}

		m_pins[pin].value = value;
	}
}

