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
  /* DMA1_Channel1_IRQn interrupt configuration -- hdma_adc5 (adc.c): part of
   * the ADC measurement pipeline, not the ADC1 control-loop tick itself. */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_IRQn interrupt configuration -- hdma_adc1 (adc.c): this is
   * the ADC1 DMA-complete IRQ that drives HAL_ADC_ConvCpltCallback() ->
   * adc_convert_fast_data() -> ctrl_main_ctrl(), i.e. the ~25 kHz duty-cycle
   * control loop. This is the only IRQ in the project at IRQ_PRIO_CTRL_LOOP. */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, IRQ_PRIO_CTRL_LOOP, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel3_IRQn interrupt configuration -- hdma_adc2 (adc.c). */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  /* DMA1_Channel4_IRQn interrupt configuration -- hdma_adc3 (adc.c). */
  HAL_NVIC_SetPriority(DMA1_Channel4_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel4_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration -- hdma_adc4 (adc.c). */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  /* DMA1_Channel6_IRQn interrupt configuration -- hdma_spi3_rx (spi.c),
   * part of the ADS131M04 external-ADC pipeline. */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  /* DMA1_Channel7_IRQn interrupt configuration -- hdma_spi3_tx (spi.c),
   * part of the ADS131M04 external-ADC pipeline. */
  HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  /* DMA2_Channel1_IRQn interrupt configuration -- hdma_i2c4_tx (i2c.c). */
  HAL_NVIC_SetPriority(DMA2_Channel1_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA2_Channel1_IRQn);
  /* DMA2_Channel2_IRQn interrupt configuration -- hdma_spi4_tx (spi.c), LCD. */
  HAL_NVIC_SetPriority(DMA2_Channel2_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA2_Channel2_IRQn);
  /* DMA1_Channel8_IRQn interrupt configuration -- hdma_i2c4_rx (i2c.c). */
  HAL_NVIC_SetPriority(DMA1_Channel8_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
  HAL_NVIC_EnableIRQ(DMA1_Channel8_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */

