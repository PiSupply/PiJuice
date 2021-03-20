/*
 * osloop.h
 *
 *  Created on: 19.03.21
 *      Author: jsteggall
 */

#ifndef OSLOOP_H_
#define OSLOOP_H_

void OSLOOP_Init(void);
void OSLOOP_Service(void);
void OSLOOP_AtomicAccess(bool access);


#endif	/* OSLOOP_H_ */
