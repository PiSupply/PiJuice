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

// Address types to pass to change address
#define HOSTCOMMS_PRIMARY_ADDR		0u
#define HOSTCOMMS_SECONDARY_ADDR	1u
#define HOSTCOMMS_ADDR_TYPES		2u

void HOSTCOMMS_Init(const uint32_t sysTime);
void HOSTCOMMS_Service(const uint32_t sysTime);
void HOSTCOMMS_Task(void);

bool HOSTCOMMS_IsCommandActive(void);
uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime);
void HOSTCOMMS_SetInterrupt(void);
void HOSTCOMMS_PiJuiceAddressSetEnable(const bool enabled);
void HOSTCOMMS_ChangeAddress(const uint8_t addrType, const uint8_t addr);


#endif /* HOSTCOMMS_H_ */
