/*
 * config_switch_resistor.h
 *
 *  Created on: 10.03.2017.
 *      Author: milan
 */

#ifndef CONFIG_SWITCH_RESISTOR_H_
#define CONFIG_SWITCH_RESISTOR_H_

extern int8_t switchConfigCode;
extern int16_t resistorConfig1Code7;
extern int8_t resistorConfig2Code4;

void SwitchResConfigInit(uint32_t resistorConfigAdc);


#endif /* CONFIG_SWITCH_RESISTOR_H_ */
