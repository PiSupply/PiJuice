// ----------------------------------------------------------------------------
/*!
 * @file		osloop.h
 * @author    	John Steggall
 * @date       	19 March 2021
 * @brief       Header file for osloop.c
 * @note        Please refer to the .c file for a detailed description.
 *
 */
// ----------------------------------------------------------------------------

#ifndef OSLOOP_H_
#define OSLOOP_H_

void OSLOOP_Init(void);
void OSLOOP_Service(void);
void OSLOOP_Shutdown(void);
void OSLOOP_Restart(void);

void OSLOOP_AtomicAccess(const bool access);


#endif	/* OSLOOP_H_ */
