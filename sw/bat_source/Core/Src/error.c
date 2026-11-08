/*
 * error.c
 *
 *  Created on: 10.09.2019
 *      Author: Andreas Horat
 */

#include "error.h"

#include <stdio.h>
#include "stdint.h"
#include "queue.h"
#include "main.h" // CMSIS __get_PRIMASK/__disable_irq/__set_PRIMASK

#define ERROR_QUEUE_SIZE 8

typedef struct
{
	ERROR_CODE code;
	uint32_t data;
	uint32_t date;
	uint32_t time;
} ERROR_ENTRY; // not "ERROR": CMSIS (via main.h) defines that as an ErrorStatus enumerator

/**
 * Struct for Error Queue
 */
struct error_queue
{
	ERROR_ENTRY queue[ERROR_QUEUE_SIZE];
	uint8_t head;
	uint8_t tail;
};

struct error_queue error_queue;

/**
 * Prints a queued error on the CLI.
 * @param code error code.
 * @param time unused.
 * @param date unused.
 * @param data a number that indicates the location in the code where the error occurred.
 */
void error_Write(ERROR_CODE code, uint32_t time, uint32_t date, uint32_t data)
{
	(void) time;
	(void) date;
	printf("ERROR %u data %lu\r\n", (unsigned) code, (unsigned long) data);
}

/**
 * Initializes the error system
 */
int error_Init(void)
{
	queue_Init(error_queue);
	return 0;
}

char error_Work(void)
{
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	if (!queue_NotEmpty(error_queue))
	{
		__set_PRIMASK(primask);
		return 0;
	}
	ERROR_CODE code = queue_Get(error_queue).code;
	uint32_t time = queue_Get(error_queue).time;
	uint32_t date = queue_Get(error_queue).date;
	uint32_t data = queue_Get(error_queue).data;
	(void) queue_Pop(error_queue, ERROR_QUEUE_SIZE);
	__set_PRIMASK(primask);

	error_Write(code, time, date, data); // main context only: prints
	return 1;
}

/**
 * Adds a error to the error queue
 *
 * @param code Error code from error.h
 * @param data additional data from the code to explain the error
 */
void error_Add(ERROR_CODE code, uint32_t data)
{
	/*
	 * Reachable from both main context (eeprom_waitReady(), timer_addTimer(),
	 * i2c_Add()) and ISR context (event_Add()'s overflow path, itself called
	 * from the I2C and TIM2 ISRs), so the queue has to be guarded the same
	 * way the event and I2C queues are. Without this, an error logged from an
	 * ISR that lands between a main-context queue_Set() and its queue_Push()
	 * writes the same slot, and one of the two entries is silently lost --
	 * which is a poor failure mode for the mechanism that exists to record
	 * failures.
	 *
	 * PRIMASK is saved and restored rather than unconditionally re-enabled,
	 * so this stays correct when called from inside another critical section.
	 */
	uint32_t primask = __get_PRIMASK();
	__disable_irq();

	if (queue_Full(error_queue, ERROR_QUEUE_SIZE))
	{
		// Full: drop the oldest; may run in an ISR, so no printing here.
		(void) queue_Pop(error_queue, ERROR_QUEUE_SIZE);
	}
	queue_Set(error_queue).code=code;
	queue_Set(error_queue).data = data;
	queue_Set(error_queue).date = 0;// parameter_Get(PARAMETER_RTC_DATE);
	queue_Set(error_queue).time = 0;//parameter_Get(PARAMETER_RTC_TIME);
	queue_Push(error_queue, ERROR_QUEUE_SIZE);

	__set_PRIMASK(primask);
}
