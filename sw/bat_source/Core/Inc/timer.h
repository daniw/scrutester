/*
 * timer.h
 *
 *  Created on: 09.08.2025
 *      Author: ahorat
 */

#ifndef SRC_TIMER_H_
#define SRC_TIMER_H_

#include "event.h"

/**
 * Enum that defines whether the timer event is rise repetitive or only once.
 */
typedef enum
{
	TIMER_TYPE_TICK,  //!< TIMER_TYPE_TICK
	TIMER_TYPE_TIMEOUT//!< TIMER_TYPE_TIMEOUT
} TIMER_TYPE;

#define TIMER_SUB_COUNT 2
#define TIMER_FREQ 50 // in Hz

void timer_init(void);
void timer_start(void);
void timer_add(uint32_t time, TIMER_TYPE type, EVENTS event, void* ret);
void timer_remove(EVENTS event);
//void timer_removeSelective(EVENTS event, void* ret);


#endif /* SRC_TIMER_H_ */
