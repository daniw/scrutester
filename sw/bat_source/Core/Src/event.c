/*
 * event.c
 *
 *  Created on: 24.11.2016
 *      Author: User
 */

#include "event.h"
#include "queue.h"
#include "error.h"
#include "main.h"

/*
 * head/tail are shared between the producers (I2C and timer ISRs, and
 * potentially the main loop) and the single consumer (event_Get(), called
 * from main()). They must be volatile: the consumer's queue_NotEmpty() check
 * in event_Get() and the producers' overflow check against tail both need to
 * observe writes made from the other execution context, and nothing else
 * here forces the compiler to re-read them on every access.
 */
struct event_queue{
	EVENT_STRUCT queue[EVENT_QUEUE_SIZE];
	volatile uint8_t head;
	volatile uint8_t tail;
};
struct event_queue event_queue;

uint8_t event_overflow;

/**
 * Initializes the event queue and the event timer.
 * @return the initialization error code and 0 when there was no error
 */
int event_Init(void)
{
	queue_Init(event_queue);

	event_overflow = 0;
	return 0;
}

/**
 * Adds an event to the event queue.
 * @param e event to be added.
 * @param data event data.
 */
void event_Add(EVENTS e, void* callback, void* argument)
{
	uint8_t oldhead, newhead;
	uint32_t primask;
	uint8_t overflowed = 0;

	/*
	 * Reserve, fill and publish the slot as one indivisible step.
	 *
	 * The obvious lock-free shape -- fill the slot, then CAS head forward --
	 * is NOT safe here, because reading head and filling the slot are two
	 * separate steps. A producer can read head = N, be preempted before it
	 * writes anything, and resume only after a second producer has filled
	 * slot N and published it. The first producer then overwrites a slot the
	 * consumer may already be about to read, and retries into the next one:
	 * the second producer's event is lost and the first producer's is
	 * delivered twice. Since main() dispatches EVENT_IIC_RX/TX by calling
	 * e.callback as a function pointer, that is not a benign corruption.
	 *
	 * Producers are the I2C and TIM2 ISRs and (potentially) the main loop.
	 * None of them is the ADC control-loop ISR, and this section is a handful
	 * of stores, so masking interrupts here costs the 25 kHz control loop
	 * tens of nanoseconds against its 40 us period. Correctness under
	 * preemption is worth far more than that, especially now that the
	 * interrupt priorities are no longer all equal and producers genuinely
	 * can nest.
	 *
	 * PRIMASK is saved and restored rather than unconditionally re-enabled,
	 * so this composes if a caller is already inside a critical section.
	 */
	primask = __get_PRIMASK();
	__disable_irq();

	oldhead = event_queue.head;
	newhead = (oldhead + 1) % EVENT_QUEUE_SIZE;
	if (newhead == event_queue.tail)
	{
		overflowed = (event_overflow == 0);
		event_overflow = 1;
	}
	else
	{
		event_overflow = 0;
		event_queue.queue[oldhead].event = e;
		event_queue.queue[oldhead].callback = callback;
		event_queue.queue[oldhead].argument = argument;
		event_queue.head = newhead;
	}

	__set_PRIMASK(primask);

	/*
	 * Logged outside the critical section: error_Add() can itself call
	 * error_Work(), so keeping it out of here bounds how long interrupts
	 * stay masked.
	 *
	 * Note: the old high half came from the removed CPU-load profiling counter
	 * and carried no meaning; passing just the event ID now.
	 */
	if (overflowed)
	{
		error_Add(ERROR_EVENT_OVF, (uint32_t) e);
	}
}

/**
 * Returns the next event in the event queue.
 * @return event and event data
 */
EVENT_STRUCT event_Get(void)
{
	EVENT_STRUCT e;
	e.event = EVENT_VOID;
	if (queue_NotEmpty(event_queue))
	{
		e.event = queue_Get(event_queue).event;
		e.callback = queue_Get(event_queue).callback;
		e.argument = queue_Get(event_queue).argument;
		queue_Pop(event_queue, EVENT_QUEUE_SIZE);
	}
	return e;
}
