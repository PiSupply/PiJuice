// ----------------------------------------------------------------------------
/*!
 * @file		hostcomms.h
 * @author    	John Steggall
 * @date       	31 March 2021
 * @brief       Header file for hostcomms.c
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef HOSTCOMMS_H_
#define HOSTCOMMS_H_


void HOSTCOMMS_Init(uint32_t sysTime);
void HOSTCOMMS_Service(uint32_t sysTime);
void HOSTCOMMS_Task(void);

bool HOSTCOMMS_IsCommandActive(void);
uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime);
void HOSTCOMMS_SetInterrupt(void);
void HOSTCOMMS_PiJuiceAddressSetEnable(const bool enabled);


#endif /* HOSTCOMMS_H_ */
