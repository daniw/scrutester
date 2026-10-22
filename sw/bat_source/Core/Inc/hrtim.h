/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    hrtim.h
  * @brief   This file contains all the function prototypes for
  *          the hrtim.c file
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
#ifndef __HRTIM_H__
#define __HRTIM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern HRTIM_HandleTypeDef hhrtim1;

/* USER CODE BEGIN Private defines */
/* HRTIM_CHANNEL_PRIM/SEK/HV are indices into HRTIM1->sTimerxRegs[] (Timer
 * B/E/C respectively). HRTIM_CHANNEL_ALL is a software-only sentinel used by
 * hrtim_disable() to mean "every channel at once" - it must NOT collide with
 * any real sTimerxRegs[] index (0..5, Timers A..F), otherwise a function
 * that doesn't special-case ALL would read/write Timer A's registers. */
#define HRTIM_CHANNEL_ALL 0xFF
#define HRTIM_CHANNEL_PRIM 1
#define HRTIM_CHANNEL_SEK 4
#define HRTIM_CHANNEL_HV 2

/* PERxR for a switching frequency (up/down counting, MUL32 prescaler:
 * 2.56 GHz / f). Not clamped -- see hrtim_set_freq(). */
#define HRTIM_PERIOD_FROM_FREQ(freq) (0x98968000U / (uint32_t) (freq))
/**
 * @brief Checks whether channel is a valid index into HRTIM1->sTimerxRegs[]
 *        for one of the channels this driver actually drives (PRIM/SEK/HV).*/
#define HRTIM_CHANNEL_IS_VALID(channel) (((channel) == HRTIM_CHANNEL_PRIM) \
		|| ((channel) == HRTIM_CHANNEL_SEK) || ((channel) == HRTIM_CHANNEL_HV))
/* USER CODE END Private defines */

void MX_HRTIM1_Init(void);

void HAL_HRTIM_MspPostInit(HRTIM_HandleTypeDef *hhrtim);

/* USER CODE BEGIN Prototypes */
void hrtim_start_timer(void);
void hrtim_set_freq(uint8_t channel, uint32_t freq);
void hrtim_set_duty(uint8_t channel, float value);
void hrtim_sync_start(uint8_t fast_channel);
void hrtim_burst_select_leg(uint8_t fast_channel);
void hrtim_sync_stop(void);
void hrtim_burst_enable(uint8_t enable);
void hrtim_burst_set_idle_cycles(unsigned cycles);
void hrtim_set_prim_sek(uint32_t prim_freq, float prim_duty, uint32_t sek_freq, float sek_duty);
void hrtim_enable(uint8_t channel);
void hrtim_disable(uint8_t channel);
void hrtim_sek_force_short(void);
void hrtim_sek_restore(void);
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __HRTIM_H__ */

