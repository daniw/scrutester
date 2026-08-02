/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dac.c
  * @brief   This file provides code for the configuration
  *          of the DAC instances.
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
#include "dac.h"

/* USER CODE BEGIN 0 */
#include "irq_priority.h"
static volatile uint8_t sqwave_enabled;
static volatile uint8_t sqwave_phase;
static volatile uint16_t sqwave_high_value;
/* USER CODE END 0 */

DAC_HandleTypeDef hdac1;

/* DAC1 init function */
void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_AUTOMATIC;
  sConfig.DAC_DMADoubleDataMode = DISABLE;
  sConfig.DAC_SignedFormat = DISABLE;
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
  sConfig.DAC_Trigger2 = DAC_TRIGGER_NONE;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_EXTERNAL;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT2 config
  */
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);

  /* USER CODE END DAC1_Init 2 */

}

void HAL_DAC_MspInit(DAC_HandleTypeDef* dacHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(dacHandle->Instance==DAC1)
  {
  /* USER CODE BEGIN DAC1_MspInit 0 */

  /* USER CODE END DAC1_MspInit 0 */
    /* DAC1 clock enable */
    __HAL_RCC_DAC1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**DAC1 GPIO Configuration
    PA4     ------> DAC1_OUT1
    PA5     ------> DAC1_OUT2
    */
    GPIO_InitStruct.Pin = VREF_2_UC_Pin|I_1A_REF_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* DAC1 interrupt Init -- TIM6_DAC_IRQn is also registered in tim.c's
     * HAL_TIM_Base_MspInit() (duplicate registration for the same vector);
     * kept consistent here (IRQ_PRIO_AUX) so whichever MspInit runs last
     * doesn't change the outcome. */
    HAL_NVIC_SetPriority(TIM6_DAC_IRQn, IRQ_PRIO_AUX, IRQ_SUBPRIO_NONE);
    HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
  /* USER CODE BEGIN DAC1_MspInit 1 */

  /* USER CODE END DAC1_MspInit 1 */
  }
}

void HAL_DAC_MspDeInit(DAC_HandleTypeDef* dacHandle)
{

  if(dacHandle->Instance==DAC1)
  {
  /* USER CODE BEGIN DAC1_MspDeInit 0 */

  /* USER CODE END DAC1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_DAC1_CLK_DISABLE();

    /**DAC1 GPIO Configuration
    PA4     ------> DAC1_OUT1
    PA5     ------> DAC1_OUT2
    */
    HAL_GPIO_DeInit(GPIOA, VREF_2_UC_Pin|I_1A_REF_Pin);

    /* DAC1 interrupt Deinit */
  /* USER CODE BEGIN DAC1:TIM6_DAC_IRQn disable */
    /**
    * Uncomment the line below to disable the "TIM6_DAC_IRQn" interrupt
    * Be aware, disabling shared interrupt may affect other IPs
    */
    /* HAL_NVIC_DisableIRQ(TIM6_DAC_IRQn); */
  /* USER CODE END DAC1:TIM6_DAC_IRQn disable */

  /* USER CODE BEGIN DAC1_MspDeInit 1 */

  /* USER CODE END DAC1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/**
  * Sets the DAC Value of the DAC used as reference / 2
  * @param value 12 bit value for the DAC Output
  */
void dac_setValueRef2(uint16_t value)
{
	  HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value);
}

/**
  * Sets the DAC Value of the DAC used as reference for the 1A current source
  * @param value 12 bit value for the DAC Output
  */
void dac_setValue1ARef(uint16_t value)
{
	  HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, value);
}

/**
  * Starts a 1Hz/50% duty square wave on the given DAC channel, toggling
  * between 0 and high_value_12bit once per dac_sqwave_tick() call.
  * @param dac_channel DAC_CHANNEL_1 or DAC_CHANNEL_2
  * @param high_value_12bit 12 bit value for the "high" half of the wave
  */
void dac_sqwave_start(uint32_t dac_channel, uint16_t high_value_12bit)
{
	sqwave_high_value = high_value_12bit;
	sqwave_phase = 0;
	sqwave_enabled = 1;
}

/**
  * Stops the square wave started by dac_sqwave_start().
  */
void dac_sqwave_stop(void)
{
	sqwave_enabled = 0;
	// Reset the channel to 0 so it isn't left mid-toggle at the high value
	// when stopped - 0 is the same idle state the channel had before this
	// feature existed.
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, 0);
}

/**
  * Advances the square wave by one half-period. Call once per TIM6
  * period-elapsed interrupt (500ms), so a full on/off cycle = 1s = 1Hz,
  * 50% duty by construction.
  */
void dac_sqwave_tick(void)
{
	if (!sqwave_enabled)
	{
		return;
	}
	sqwave_phase = !sqwave_phase;
	HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, sqwave_phase ? sqwave_high_value : 0);
}

/**
  * Returns whether the square wave is currently in its "high" (on) phase.
  * Always 0 if not running (dac_sqwave_stop() was called, or
  * dac_sqwave_start() never was). See dac_sqwave_tick() for phase semantics.
  */
uint8_t dac_sqwave_is_high(void)
{
	return sqwave_enabled && sqwave_phase;
}

/* USER CODE END 1 */
