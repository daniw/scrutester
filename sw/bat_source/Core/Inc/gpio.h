/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.h
  * @brief   This file contains all the function prototypes for
  *          the gpio.c file
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
#ifndef __GPIO_H__
#define __GPIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_GPIO_Init(void);

/* USER CODE BEGIN Prototypes */

void gpio_irq_priority_override(void);

#define GPIO_SET_DISPLAY_WR(value) HAL_GPIO_WritePin(DISPLAY_WR_GPIO_Port, DISPLAY_WR_Pin,value)
#define GPIO_SET_CONV_CTRL_EN(value) HAL_GPIO_WritePin(CONV_CTRL_EN_GPIO_Port, CONV_CTRL_EN_Pin,value)
#define GPIO_SET_OUT_SEL_ISO(value) HAL_GPIO_WritePin(OUT_SEL_ISO_GPIO_Port, OUT_SEL_ISO_Pin,value)
#define GPIO_SET_BMS_CTRL_WAKEUP(value) HAL_GPIO_WritePin(BMS_CTRL_WAKEUP_GPIO_Port, BMS_CTRL_WAKEUP_Pin,value)
#define GPIO_SET_SHUNT_EN(value) HAL_GPIO_WritePin(SHUNT_EN_GPIO_Port, SHUNT_EN_Pin,value)
#define GPIO_SET_OUT_SEL_HV(value) HAL_GPIO_WritePin(OUT_SEL_HV_GPIO_Port, OUT_SEL_HV_Pin,value)
#define GPIO_SET_OVP_RESET(value) HAL_GPIO_WritePin(OVP_RESET_GPIO_Port, OVP_RESET_Pin,value)
#define GPIO_SET_DISCHARGE(value) HAL_GPIO_WritePin(DISCHARGE_GPIO_Port, DISCHARGE_Pin,value)
#define GPIO_SET_HV_CTRL_EN(value) HAL_GPIO_WritePin(HV_CTRL_EN_GPIO_Port, HV_CTRL_EN_Pin,value)
#define GPIO_SET_SHUNT_ISO_EN(value) HAL_GPIO_WritePin(SHUNT_ISO_EN_GPIO_Port, SHUNT_ISO_EN_Pin,value)
#define GPIO_SET_ADC_SYNC_RESET(value) HAL_GPIO_WritePin(ADC_SYNC_RESET_GPIO_Port, ADC_SYNC_RESET_Pin,value)

static inline uint8_t gpio_readBtnOut(void) {
	return HAL_GPIO_ReadPin(BUTTON_OUT_GPIO_Port, BUTTON_OUT_Pin) == 0;
}
static inline uint8_t gpio_readBtnEsc(void) {
	return HAL_GPIO_ReadPin(BUTTON_ESC_GPIO_Port, BUTTON_ESC_Pin) == 0;
}
static inline uint8_t gpio_readBtnOk(void) {
	return HAL_GPIO_ReadPin(BUTTON_OK_GPIO_Port, BUTTON_OK_Pin) == 0;
}

static inline void gpio_shutdown(void){
	HAL_GPIO_WritePin(ON_REQ_GPIO_Port, ON_REQ_Pin,0);
}

void gpio_turnOn(void);
void gpio_turnOff(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif
#endif /*__ GPIO_H__ */

