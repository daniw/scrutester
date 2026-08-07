/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dac.h
  * @brief   This file contains all the function prototypes for
  *          the dac.c file
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
#ifndef __DAC_H__
#define __DAC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern DAC_HandleTypeDef hdac1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_DAC1_Init(void);

/* USER CODE BEGIN Prototypes */

/**
  * Sets the DAC Value of the DAC used as reference / 2
  * @param value 12 bit value for the DAC Output
  */
void dac_setValueRef2(uint16_t value);

/**
  * Sets the DAC Value of the DAC used as reference for the 1A current source
  * @param value 12 bit value for the DAC Output
  */
void dac_setValue1ARef(uint16_t value);

/**
  * Starts a 1Hz/50% duty square wave on the given DAC channel, toggling
  * between 0 and high_value_12bit once per dac_sqwave_tick() call.
  * @param dac_channel DAC_CHANNEL_1 or DAC_CHANNEL_2
  * @param high_value_12bit 12 bit value for the "high" half of the wave
  */
void dac_sqwave_init(uint32_t dac_channel, uint16_t high_value_12bit);

/**
  * Starts a 1Hz/50% duty square wave on the given DAC channel, toggling
  * between 0 and high_value_12bit once per dac_sqwave_tick() call. Meant to
  * be ticked from the TIM6 500ms period-elapsed interrupt (two ticks = one
  * full on/off cycle = 1 second).
  * @param dac_channel DAC_CHANNEL_1 or DAC_CHANNEL_2
  * @param high_value_12bit 12 bit value for the "high" half of the wave
  */
void dac_sqwave_start(uint32_t dac_channel, uint16_t high_value_12bit);

/**
  * Stops the square wave started by dac_sqwave_start() and resets the
  * DAC channel back to 0 (its idle state before this feature existed).
  */
void dac_sqwave_stop(void);

/**
  * Advances the square wave by one half-period. Call once per TIM6
  * period-elapsed interrupt. No-op if the square wave isn't running.
  */
void dac_sqwave_tick(void);

/**
  * Returns whether the square wave started by dac_sqwave_start() is
  * currently in its "high" (test-current-on) half-cycle. Always 0 if the
  * square wave isn't running.
  * @return 1 if currently in the "on"/high phase, 0 otherwise
  */
uint8_t dac_sqwave_is_high(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __DAC_H__ */

