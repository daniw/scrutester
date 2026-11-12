/*
 * error.h
 *
 *  Created on: 10.09.2019
 *      Author: Andreas
 */

#ifndef HYBRID_CONV_LIB_ERROR_H_
#define HYBRID_CONV_LIB_ERROR_H_

#include "stdint.h"

typedef enum
{
	ERROR_NONE,
	ERROR_IIC_READCONF,
	ERROR_IIC_INIT,
	ERROR_IIC_SELFTEST,
	ERROR_IIC_OVF,
	ERROR_TTC_OVF,
	ERROR_EVENT_OVF,
	ERROR_EEPROM_TIMEOUT

} ERROR_CODE;

int error_Init(void);
char error_Work(void);
void error_Add(ERROR_CODE error, uint32_t data);

#endif /* HYBRID_CONV_LIB_ERROR_H_ */

