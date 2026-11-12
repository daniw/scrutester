/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dma.c
  * @brief   This file provides code for the configuration
  *          of all the requested memory to memory DMA transfers.
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

/* Includes ------------------------------------------------------------------*/
#include "dma.h"

/* USER CODE BEGIN 0 */
#include "irq_priority.h"
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure DMA                                                              */
/*----------------------------------------------------------------------------*/

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/**
  * Enable DMA controller clock
  */
void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 4, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
  /* DMA2_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel2_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel2_IRQn);
  /* DMA1_Channel8_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel8_IRQn, 8, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel8_IRQn);

}

/* USER CODE BEGIN 2 */
/*
 * Overrides the priorities MX_DMA_Init() above just set, from
 * irq_priority.h -- see that file's "generated line + USER CODE override"
 * note. Unlike the per-peripheral MSP-init files, MX_DMA_Init() has no
 * USER CODE block of its own inside its body (CubeMX doesn't emit one here),
 * so this can't be a handful of bare statements dropped in place the way the
 * other files' overrides are -- it has to be a function, called once from
 * main.c's USER CODE BEGIN 2 right after MX_DMA_Init().
 */
void dma_irq_priority_override(void) {
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, IRQ_PRIO_CTRL_LOOP, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA2_Channel2_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
	HAL_NVIC_SetPriority(DMA1_Channel8_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
}
/* USER CODE END 2 */

