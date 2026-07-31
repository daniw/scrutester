/*
 * timer.c
 *
 *  Created on: 09.08.2025
 *      Author: ahorat
 */

#include "timer.h"
#include "error.h"
#include "main.h"
#include "list.h"
#include "dac.h"

/**
 * Struct that represents one timer event.
 */
typedef struct
{
	TIMER_TYPE type;
	uint8_t load;
	uint8_t count;
	EVENTS event;
	void* ret;
} TIMER_SUB;

/**
 * Struct the represent a timer instance and the event list.
 */
typedef struct
{
	TIM_HandleTypeDef* instance;
	uint32_t frq;
	TIMER_SUB* list[TIMER_SUB_COUNT];
	TIMER_SUB subs[TIMER_SUB_COUNT];
	LIST_STR liststr;
} timer_handle_t;

timer_handle_t timer_handle;

extern TIM_HandleTypeDef htim2;

/**
 * Callback from the timer interrupt. Rises the timer events.
 * @param timer pointer to the timer instance.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	// TIM6 is a separate, simpler always-running hardware timer that drives
	// the milliohmmeter mode's DAC square wave - it's not part of the
	// timer_handle/event-queue system below, so it's dispatched independently.
	if (htim->Instance == TIM6)
	{
		dac_sqwave_tick();
		return;
	}
	if(htim != timer_handle.instance)
		return;
	uint8_t i;
	TIMER_SUB* sub;
	//event_Take(EVENT_TTC);
	for (i = 0; i < timer_handle.liststr.count; i++)
	{
		sub = list_Get(&(timer_handle.liststr), i);
		if (!list_CheckRemove(sub))
		{
			sub->count--;
			if (sub->count == 0)
			{
				event_Add(sub->event, 0,  sub->ret);
				switch (sub->type)
				{
					case TIMER_TYPE_TICK:
						sub->count = sub->load;
						break;
					case TIMER_TYPE_TIMEOUT:
						list_Remove(&timer_handle.liststr, i);
						break;
				}
			}
		}
	}
	//event_Give();
}



/**
 * Initializes all three timers.
 * @return the initialization error code and 0 when there was no error
 */
void timer_init(void)
{
	list_Init(&(timer_handle.liststr), (void**) timer_handle.list, timer_handle.subs, sizeof(TIMER_SUB), TIMER_SUB_COUNT);
	timer_handle.frq = TIMER_FREQ;
	timer_handle.instance = &htim2;
}

/**
 * Starts all three timers.
 */
void timer_start(void)
{
	HAL_TIM_Base_Start_IT(timer_handle.instance);
}

/**
 * Adds a timer event to a timer instance.
 * @param timer pointer to the timer instance.
 * @param load value for the divider.
 * @param type of the timer.
 * @param event the event that is risen when the timer elapsed.
 * @param ret return value that is added to the event.
 */
void timer_addTimer(timer_handle_t* timer, uint8_t load, TIMER_TYPE type, EVENTS event, void* ret)
{
	TIMER_SUB* new;
	new = list_GetFree(&(timer->liststr));
	if (!list_CheckRemove(new))
	{
		new->type = type;
		new->load = load;
		new->count = load;
		new->event = event;
		new->ret = ret;
		list_Add(&(timer->liststr), new);
	}
	else
	{
		error_Add(ERROR_TTC_OVF, event);
	}
}

/**
 * Finds the timer instance that fist the best to the timer event time and adds the event.
 * @param time of the timer event in ms.
 * @param type of the timer.
 * @param event the event that is risen when the timer elapsed.
 * @param ret return value that is added to the event.
 */
void timer_add(uint32_t time, TIMER_TYPE type, EVENTS event, void* ret)
{
	uint32_t load;
	load = time * timer_handle.frq / 1000;
	if (load > 0)
	{
		timer_addTimer(&(timer_handle), (uint8_t) load, type, event, ret);
		return;
	}
}

/**
 * Removes a timer event from the timer instance.
 * @param event timer event that is removed.
 */
void timer_remove(EVENTS event)
{
	uint8_t index;
	TIMER_SUB* sub;
		for (index = 0; index < timer_handle.liststr.count; index++)
		{
			sub = list_Get(&(timer_handle.liststr), index);
			if (!list_CheckRemove(sub))
			{
				if (sub->event == event)
				{
					list_Remove(&(timer_handle.liststr), index);
					return;
				}
			}
		}
}

/**
 * Removes a timer event from the timer instance.
 * @param event timer event that is removed.
 * @param ret return data that has to match too
 */
/*void timer_removeSelective(EVENTS event, void* ret)
{
	uint8_t i, index;
	TIMER_SUB* sub;
		for (index = 0; index < timer_handle.liststr.count; index++)
		{
			sub = list_Get(&(timer_handle.liststr), index);
			if (!list_CheckRemove(sub))
			{
				if (sub->event == event && sub->ret == ret)
				{
					list_Remove(&(timer_handle.liststr), index);
					return;
				}
			}
		}

}*/
