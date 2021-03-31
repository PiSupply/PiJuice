#ifndef HOSTCOMMS_H_
#define HOSTCOMMS_H_


void HOSTCOMMS_Init(void);
void HOSTCOMMS_Task(void);

bool HOSTCOMMS_IsCommandActive(void);
uint32_t HOSTCOMMS_GetLastCommandAgeMs(const uint32_t sysTime);
void HOSTCOMMS_SetInterrupt(void);


#endif /* HOSTCOMMS_H_ */
