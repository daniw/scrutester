/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.h
  * @brief   This file contains all the function prototypes for
  *          the i2c.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_H__
#define __I2C_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

#include "queue.h"
#include "error.h"
#include "event.h"

/* USER CODE END Includes */

extern I2C_HandleTypeDef hi2c4;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_I2C4_Init(void);

/* USER CODE BEGIN Prototypes */

void i2c_Start(void);
void i2c_Read(uint8_t address, uint8_t* buf, uint32_t numbytes,  void* callback, void* argument, void* error_callback);
void i2c_Write(uint8_t address, uint8_t* buf, uint32_t numbytes, void* callback, void* argument, void* error_callback);
uint8_t i2c_WriteBlocking(uint8_t address, uint8_t* buf, uint32_t numbytes);
uint8_t i2c_ReadBlocking(uint8_t address, uint8_t* buf, uint32_t numbytes);
uint8_t i2c_IsDeviceReadyBlocking(uint8_t address);
void i2c_check_timeout(void);

/** @brief Bus errors and timeouts since boot. */
uint32_t i2c_error_count(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __I2C_H__ */

