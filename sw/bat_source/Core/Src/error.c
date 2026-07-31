/*
 * error.c
 *
 *  Created on: 10.09.2019
 *      Author: Andreas Horat
 */

#include "error.h"

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
 * Saves the error into the error file.
 * @param code error code.
 * @param time time of when the error occurred.
 * @param date date of when the error occurred.
 * @param data a number that indicates the location in the code where the error occurred.
 */
void error_Write(ERROR_CODE code, uint32_t time, uint32_t date, uint32_t data)
{
	/*int state = FR_OK;
	FIL file;
	char datas[9];
	UINT len, lenw;
	state = f_open(&file, ERROR_FILE_PATH, FA_WRITE | FA_OPEN_ALWAYS);
	if (state == FR_OK)
	{
		state = f_lseek(&file, file.obj.objsize);
	}
	if (state == FR_OK)
	{
		util_Int2Date(datas, &len, date);
		datas[len] = ' ';
		state = f_write(&file, datas, len + 1, &lenw);
		if (state == FR_OK)
		{
			util_Int2Time(datas, &len, time);
			datas[len] = '>';
			state = f_write(&file, datas, len + 1, &lenw);
		}
		if (state == FR_OK)
		{
			util_Int2Str(datas, &len, code, 16, 8);
			datas[len] = ',';
			state = f_write(&file, datas, len + 1, &lenw);
			if (state == FR_OK)
			{
				util_Int2Str(datas, &len, data, 16, 8);
				datas[len] = '\n';
				state = f_write(&file, datas, len + 1, &lenw);
			}
		}
		f_close(&file);
	}*/
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
	ERROR_CODE e;
	uint64_t error;
	//if (sd_Mounted())
		if (queue_NotEmpty(error_queue))
		{
			error_Write(queue_Get(error_queue).code,
			queue_Get(error_queue).time,
			queue_Get(error_queue).date,
			queue_Get(error_queue).data);
			queue_Pop(error_queue, ERROR_QUEUE_SIZE);
			return 1;
		}

	return 0;
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
		error_Work();
	}
	queue_Set(error_queue).code=code;
	queue_Set(error_queue).data = data;
	queue_Set(error_queue).date = 0;// parameter_Get(PARAMETER_RTC_DATE);
	queue_Set(error_queue).time = 0;//parameter_Get(PARAMETER_RTC_TIME);
	queue_Push(error_queue, ERROR_QUEUE_SIZE);

	__set_PRIMASK(primask);
}
