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

#define EVENT_TIMER_LOAD ((1 << 29) - 1)

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

//XScuTimer event_timer;
uint32_t event_old, event_new, event_per[EVENT_EVENT + 1];
EVENTS event_last, event_prelast;
uint8_t event_overflow;

/**
 * Callback of the event timer interrupt.
 * @param timer
 */
void event_CallBack(void)
{
	//event_Add(EVENT_EVENT, (void*) 0);
}

/**
 * Initializes the event queue and the event timer.
 * @return the initialization error code and 0 when there was no error
 */
int event_Init(void)
{
	uint8_t i;
	event_last = EVENT_INIT;
	event_old = EVENT_TIMER_LOAD;
	event_new = 0;
	for (i = 0; i < EVENT_EVENT + 1; i++)
	{
		event_per[i] = 0;
	}
	queue_Init(event_queue);

	//XScuTimer_EnableAutoReload(&event_timer);
	//XScuTimer_LoadTimer(&event_timer, EVENT_TIMER_LOAD);


	event_overflow = 0;
	return 0;
}

/**
 * Starts the event timer.
 */
void event_TimerStart(void)
{
	//XScuTimer_Start(&event_timer);
}

/**
 * Stops the event timer.
 */
void event_TimerStop(void)
{
	//XScuTimer_Stop(&event_timer);
}

/**
 * Calculates the CPU load for the different event tasks.
 * @param e event task which load is returned.
 * @return load of a specific event.
 */
uint32_t event_Timer(EVENTS e)
{
	uint8_t i;
	uint32_t ret = 0;
	for (i = 0; i < EVENT_EVENT + 1; i++)
	{
		event_per[i] >>= 1;
		if (i == e)
		{
			ret = event_per[i];
		}
	}
	return ret;
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
	 */
	if (overflowed)
	{
		error_Add(ERROR_EVENT_OVF, (event_old << 16) | (e));
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
	event_Take(e.event);
	return e;
}

/**
 * Takes the CPU time in a Interrupt.
 * @param e event task.
 */
void event_Take(EVENTS e)
{
	if (e != event_last)
	{
		//event_new = XScuTimer_GetCounterValue(&event_timer);
		event_per[event_last] += (event_old - event_new) & EVENT_TIMER_LOAD;
		event_prelast = event_last;
		event_last = e;
		event_old = event_new;
	}
}

/**
 * Gives back the CPU from an interrupt.
 */
void event_Give(void)
{
	event_Take(event_prelast);
}
