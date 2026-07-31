/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.c
  * @brief   This file provides code for the configuration
  *          of the ADC instances.
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
#include "adc.h"

/* USER CODE BEGIN 0 */
#include "ctrl_main.h"
#include "aux_io_ctrl.h"
#include "config_store.h"
#include "dac.h"
#include "mode_table.h"

extern ADC_MEAS_DATA adc_data;

volatile uint8_t adc_interrupt_cnt;

// ISR-private copy of the converted measurements. adc_convert_fast_data()
// (ISR context, from HAL_ADC_ConvCpltCallback()) recomputes these every ADC
// interrupt from adc_data.raw plus adc_data's calibration, and
// ctrl_main_ctrl() -- called from that same ISR invocation, right after --
// reads them straight back, so the control loop always sees this tick's
// fresh values.
//
// adc_data.converted, which is what every main-context reader sees, is
// updated only by adc_snapshot_converted()'s critical-section copy of this,
// once per statemachine_step() tick. That stops main context and the ISR
// tearing each other's reads and writes of the same fields, without costing
// the control loop any freshness.
//
// This holds only the converted block, not a whole ADC_MEAS_DATA: the raw
// and calibration members of a duplicate struct would be permanently zero,
// and ctrl_main_ctrl() taking a const ADC_CONVERTED_DATA* makes reaching for
// one a compile error rather than a silent read of zeros in the control loop.
static ADC_CONVERTED_DATA adc_converted_isr;
/* USER CODE END 0 */

ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
ADC_HandleTypeDef hadc3;
ADC_HandleTypeDef hadc4;
ADC_HandleTypeDef hadc5;
DMA_HandleTypeDef hdma_adc1;
DMA_HandleTypeDef hdma_adc2;
DMA_HandleTypeDef hdma_adc3;
DMA_HandleTypeDef hdma_adc4;
DMA_HandleTypeDef hdma_adc5;

/* ADC1 init function */
void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */
  adc_interrupt_cnt = 0;
  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.GainCompensation = 0;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_HRTIM_TRG1;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}
/* ADC2 init function */
void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc2.Init.Resolution = ADC_RESOLUTION_12B;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.GainCompensation = 0;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc2.Init.LowPowerAutoWait = DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.NbrOfConversion = 1;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_EXTERNALTRIG_HRTIM_TRG1;
  hadc2.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc2.Init.DMAContinuousRequests = ENABLE;
  hadc2.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc2.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}
/* ADC3 init function */
void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc3.Init.Resolution = ADC_RESOLUTION_12B;
  hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc3.Init.GainCompensation = 0;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_EXTERNALTRIG_HRTIM_TRG2;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc3.Init.DMAContinuousRequests = ENABLE;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc3, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}
/* ADC4 init function */
void MX_ADC4_Init(void)
{

  /* USER CODE BEGIN ADC4_Init 0 */

  /* USER CODE END ADC4_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC4_Init 1 */

  /* USER CODE END ADC4_Init 1 */

  /** Common config
  */
  hadc4.Instance = ADC4;
  hadc4.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc4.Init.Resolution = ADC_RESOLUTION_12B;
  hadc4.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc4.Init.GainCompensation = 0;
  hadc4.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc4.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc4.Init.LowPowerAutoWait = DISABLE;
  hadc4.Init.ContinuousConvMode = DISABLE;
  hadc4.Init.NbrOfConversion = 1;
  hadc4.Init.DiscontinuousConvMode = DISABLE;
  hadc4.Init.ExternalTrigConv = ADC_EXTERNALTRIG_HRTIM_TRG1;
  hadc4.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc4.Init.DMAContinuousRequests = ENABLE;
  hadc4.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc4.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC4_Init 2 */

  /* USER CODE END ADC4_Init 2 */

}
/* ADC5 init function */
void MX_ADC5_Init(void)
{

  /* USER CODE BEGIN ADC5_Init 0 */

  /* USER CODE END ADC5_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};
  ADC_InjectionConfTypeDef sConfigInjected = {0};

  /* USER CODE BEGIN ADC5_Init 1 */

  /* USER CODE END ADC5_Init 1 */

  /** Common config
  */
  hadc5.Instance = ADC5;
  hadc5.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc5.Init.Resolution = ADC_RESOLUTION_12B;
  hadc5.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc5.Init.GainCompensation = 0;
  hadc5.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc5.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc5.Init.LowPowerAutoWait = DISABLE;
  hadc5.Init.ContinuousConvMode = ENABLE;
  hadc5.Init.NbrOfConversion = 12;
  hadc5.Init.DiscontinuousConvMode = DISABLE;
  hadc5.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc5.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc5.Init.DMAContinuousRequests = ENABLE;
  hadc5.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc5.Init.OversamplingMode = ENABLE;
  hadc5.Init.Oversampling.Ratio = ADC_OVERSAMPLING_RATIO_256;
  hadc5.Init.Oversampling.RightBitShift = ADC_RIGHTBITSHIFT_4;
  hadc5.Init.Oversampling.TriggeredMode = ADC_TRIGGEREDMODE_SINGLE_TRIGGER;
  hadc5.Init.Oversampling.OversamplingStopReset = ADC_REGOVERSAMPLING_CONTINUED_MODE;
  if (HAL_ADC_Init(&hadc5) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = ADC_REGULAR_RANK_3;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = ADC_REGULAR_RANK_4;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_12;
  sConfig.Rank = ADC_REGULAR_RANK_5;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = ADC_REGULAR_RANK_6;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_14;
  sConfig.Rank = ADC_REGULAR_RANK_7;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_15;
  sConfig.Rank = ADC_REGULAR_RANK_8;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_16;
  sConfig.Rank = ADC_REGULAR_RANK_9;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_TEMPSENSOR_ADC5;
  sConfig.Rank = ADC_REGULAR_RANK_10;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VBAT;
  sConfig.Rank = ADC_REGULAR_RANK_11;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_VREFINT;
  sConfig.Rank = ADC_REGULAR_RANK_12;
  if (HAL_ADC_ConfigChannel(&hadc5, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Injected Channel
  */
  sConfigInjected.InjectedChannel = ADC_CHANNEL_1;
  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_2CYCLES_5;
  sConfigInjected.InjectedSingleDiff = ADC_SINGLE_ENDED;
  sConfigInjected.InjectedOffsetNumber = ADC_OFFSET_NONE;
  sConfigInjected.InjectedOffset = 0;
  sConfigInjected.InjectedNbrOfConversion = 1;
  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
  sConfigInjected.AutoInjectedConv = DISABLE;
  sConfigInjected.QueueInjectedContext = DISABLE;
  sConfigInjected.ExternalTrigInjecConv = ADC_EXTERNALTRIGINJEC_HRTIM_TRG1;
  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
  sConfigInjected.InjecOversamplingMode = DISABLE;
  if (HAL_ADCEx_InjectedConfigChannel(&hadc5, &sConfigInjected) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC5_Init 2 */

  /* USER CODE END ADC5_Init 2 */

}

static uint32_t HAL_RCC_ADC12_CLK_ENABLED=0;
static uint32_t HAL_RCC_ADC345_CLK_ENABLED=0;

void HAL_ADC_MspInit(ADC_HandleTypeDef* adcHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspInit 0 */

  /* USER CODE END ADC1_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC1 clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC1 GPIO Configuration
    PA0     ------> ADC1_IN1
    */
    GPIO_InitStruct.Pin = I_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(I_OUT_GPIO_Port, &GPIO_InitStruct);

    /* ADC1 DMA Init */
    /* ADC1 Init */
    hdma_adc1.Instance = DMA1_Channel2;
    hdma_adc1.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc1.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc1.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc1.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc1.Init.Mode = DMA_CIRCULAR;
    hdma_adc1.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc1) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc1);

  /* USER CODE BEGIN ADC1_MspInit 1 */

  /* USER CODE END ADC1_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspInit 0 */

  /* USER CODE END ADC2_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC12;
    PeriphClkInit.Adc12ClockSelection = RCC_ADC12CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC2 clock enable */
    HAL_RCC_ADC12_CLK_ENABLED++;
    if(HAL_RCC_ADC12_CLK_ENABLED==1){
      __HAL_RCC_ADC12_CLK_ENABLE();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC2 GPIO Configuration
    PA1     ------> ADC2_IN2
    */
    GPIO_InitStruct.Pin = V_TERM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(V_TERM_GPIO_Port, &GPIO_InitStruct);

    /* ADC2 DMA Init */
    /* ADC2 Init */
    hdma_adc2.Instance = DMA1_Channel3;
    hdma_adc2.Init.Request = DMA_REQUEST_ADC2;
    hdma_adc2.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc2.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc2.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc2.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc2.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc2.Init.Mode = DMA_CIRCULAR;
    hdma_adc2.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc2) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc2);

  /* USER CODE BEGIN ADC2_MspInit 1 */

  /* USER CODE END ADC2_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC3)
  {
  /* USER CODE BEGIN ADC3_MspInit 0 */

  /* USER CODE END ADC3_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC345;
    PeriphClkInit.Adc345ClockSelection = RCC_ADC345CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC3 clock enable */
    HAL_RCC_ADC345_CLK_ENABLED++;
    if(HAL_RCC_ADC345_CLK_ENABLED==1){
      __HAL_RCC_ADC345_CLK_ENABLE();
    }

    __HAL_RCC_GPIOE_CLK_ENABLE();
    /**ADC3 GPIO Configuration
    PE13     ------> ADC3_IN3
    */
    GPIO_InitStruct.Pin = V_IN_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(V_IN_GPIO_Port, &GPIO_InitStruct);

    /* ADC3 DMA Init */
    /* ADC3 Init */
    hdma_adc3.Instance = DMA1_Channel4;
    hdma_adc3.Init.Request = DMA_REQUEST_ADC3;
    hdma_adc3.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc3.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc3.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc3.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc3.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc3.Init.Mode = DMA_CIRCULAR;
    hdma_adc3.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc3) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc3);

  /* USER CODE BEGIN ADC3_MspInit 1 */

  /* USER CODE END ADC3_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC4)
  {
  /* USER CODE BEGIN ADC4_MspInit 0 */

  /* USER CODE END ADC4_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC345;
    PeriphClkInit.Adc345ClockSelection = RCC_ADC345CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC4 clock enable */
    HAL_RCC_ADC345_CLK_ENABLED++;
    if(HAL_RCC_ADC345_CLK_ENABLED==1){
      __HAL_RCC_ADC345_CLK_ENABLE();
    }

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**ADC4 GPIO Configuration
    PE15     ------> ADC4_IN2
    PB15     ------> ADC4_IN5
    */
    GPIO_InitStruct.Pin = V_OUT_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(V_OUT_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = V_HV_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(V_HV_GPIO_Port, &GPIO_InitStruct);

    /* ADC4 DMA Init */
    /* ADC4 Init */
    hdma_adc4.Instance = DMA1_Channel5;
    hdma_adc4.Init.Request = DMA_REQUEST_ADC4;
    hdma_adc4.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc4.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc4.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc4.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc4.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc4.Init.Mode = DMA_CIRCULAR;
    hdma_adc4.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc4) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc4);

  /* USER CODE BEGIN ADC4_MspInit 1 */

  /* USER CODE END ADC4_MspInit 1 */
  }
  else if(adcHandle->Instance==ADC5)
  {
  /* USER CODE BEGIN ADC5_MspInit 0 */

  /* USER CODE END ADC5_MspInit 0 */

  /** Initializes the peripherals clocks
  */
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC345;
    PeriphClkInit.Adc345ClockSelection = RCC_ADC345CLKSOURCE_SYSCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
      Error_Handler();
    }

    /* ADC5 clock enable */
    HAL_RCC_ADC345_CLK_ENABLED++;
    if(HAL_RCC_ADC345_CLK_ENABLED==1){
      __HAL_RCC_ADC345_CLK_ENABLE();
    }

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**ADC5 GPIO Configuration
    PE8     ------> ADC5_IN6
    PE10     ------> ADC5_IN14
    PE11     ------> ADC5_IN15
    PE12     ------> ADC5_IN16
    PD8     ------> ADC5_IN12
    PD9     ------> ADC5_IN13
    PD10     ------> ADC5_IN7
    PD11     ------> ADC5_IN8
    PD12     ------> ADC5_IN9
    PA8     ------> ADC5_IN1
    PA9     ------> ADC5_IN2
    */
    GPIO_InitStruct.Pin = V_3V3_Pin|V_15V_Pin|V_VCC_Pin|V_5V_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TEMP_CURRENT_Pin|CONV_CTRL_TEMP_PRIM_Pin|CONV_CTRL_TEMP_SEC_Pin|V_3V3_A_Pin
                          |CONV_CTRL_TEMP_TRAFO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = BMS_CTRL_I_BAT_Pin|I_ISO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* ADC5 DMA Init */
    /* ADC5 Init */
    hdma_adc5.Instance = DMA1_Channel1;
    hdma_adc5.Init.Request = DMA_REQUEST_ADC5;
    hdma_adc5.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc5.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc5.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc5.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc5.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc5.Init.Mode = DMA_CIRCULAR;
    hdma_adc5.Init.Priority = DMA_PRIORITY_LOW;
    if (HAL_DMA_Init(&hdma_adc5) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(adcHandle,DMA_Handle,hdma_adc5);

  /* USER CODE BEGIN ADC5_MspInit 1 */

  /* USER CODE END ADC5_MspInit 1 */
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef* adcHandle)
{

  if(adcHandle->Instance==ADC1)
  {
  /* USER CODE BEGIN ADC1_MspDeInit 0 */

  /* USER CODE END ADC1_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC1 GPIO Configuration
    PA0     ------> ADC1_IN1
    */
    HAL_GPIO_DeInit(I_OUT_GPIO_Port, I_OUT_Pin);

    /* ADC1 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC1_MspDeInit 1 */

  /* USER CODE END ADC1_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC2)
  {
  /* USER CODE BEGIN ADC2_MspDeInit 0 */

  /* USER CODE END ADC2_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC12_CLK_ENABLED--;
    if(HAL_RCC_ADC12_CLK_ENABLED==0){
      __HAL_RCC_ADC12_CLK_DISABLE();
    }

    /**ADC2 GPIO Configuration
    PA1     ------> ADC2_IN2
    */
    HAL_GPIO_DeInit(V_TERM_GPIO_Port, V_TERM_Pin);

    /* ADC2 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC2_MspDeInit 1 */

  /* USER CODE END ADC2_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC3)
  {
  /* USER CODE BEGIN ADC3_MspDeInit 0 */

  /* USER CODE END ADC3_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC345_CLK_ENABLED--;
    if(HAL_RCC_ADC345_CLK_ENABLED==0){
      __HAL_RCC_ADC345_CLK_DISABLE();
    }

    /**ADC3 GPIO Configuration
    PE13     ------> ADC3_IN3
    */
    HAL_GPIO_DeInit(V_IN_GPIO_Port, V_IN_Pin);

    /* ADC3 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC3_MspDeInit 1 */

  /* USER CODE END ADC3_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC4)
  {
  /* USER CODE BEGIN ADC4_MspDeInit 0 */

  /* USER CODE END ADC4_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC345_CLK_ENABLED--;
    if(HAL_RCC_ADC345_CLK_ENABLED==0){
      __HAL_RCC_ADC345_CLK_DISABLE();
    }

    /**ADC4 GPIO Configuration
    PE15     ------> ADC4_IN2
    PB15     ------> ADC4_IN5
    */
    HAL_GPIO_DeInit(V_OUT_GPIO_Port, V_OUT_Pin);

    HAL_GPIO_DeInit(V_HV_GPIO_Port, V_HV_Pin);

    /* ADC4 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC4_MspDeInit 1 */

  /* USER CODE END ADC4_MspDeInit 1 */
  }
  else if(adcHandle->Instance==ADC5)
  {
  /* USER CODE BEGIN ADC5_MspDeInit 0 */

  /* USER CODE END ADC5_MspDeInit 0 */
    /* Peripheral clock disable */
    HAL_RCC_ADC345_CLK_ENABLED--;
    if(HAL_RCC_ADC345_CLK_ENABLED==0){
      __HAL_RCC_ADC345_CLK_DISABLE();
    }

    /**ADC5 GPIO Configuration
    PE8     ------> ADC5_IN6
    PE10     ------> ADC5_IN14
    PE11     ------> ADC5_IN15
    PE12     ------> ADC5_IN16
    PD8     ------> ADC5_IN12
    PD9     ------> ADC5_IN13
    PD10     ------> ADC5_IN7
    PD11     ------> ADC5_IN8
    PD12     ------> ADC5_IN9
    PA8     ------> ADC5_IN1
    PA9     ------> ADC5_IN2
    */
    HAL_GPIO_DeInit(GPIOE, V_3V3_Pin|V_15V_Pin|V_VCC_Pin|V_5V_Pin);

    HAL_GPIO_DeInit(GPIOD, TEMP_CURRENT_Pin|CONV_CTRL_TEMP_PRIM_Pin|CONV_CTRL_TEMP_SEC_Pin|V_3V3_A_Pin
                          |CONV_CTRL_TEMP_TRAFO_Pin);

    HAL_GPIO_DeInit(GPIOA, BMS_CTRL_I_BAT_Pin|I_ISO_Pin);

    /* ADC5 DMA DeInit */
    HAL_DMA_DeInit(adcHandle->DMA_Handle);
  /* USER CODE BEGIN ADC5_MspDeInit 1 */

  /* USER CODE END ADC5_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void adc_init(int32_t* ext_adc_data)
{
	adc_data.ext_adc_data = ext_adc_data;

	HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
	HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED);

	  adc_data.v_in_offset   = ADC_VIN_OFFSET_MV  ;
	  adc_data.i_bat_offset  = ADC_IBAT_OFFSET_MA ;

	  // V_TERM/I_OUT/I_ISO/V_SENS/V_OUT/V_HV offset and gain come from
	  // config_store, which is loaded from EEPROM (or defaulted to the
	  // ADC_* constants) before adc_init() runs - see config_store_init()
	  // in main().
	  adc_data.v_term_offset = config_store.calibration.v_term_offset_mv;
	  adc_data.v_term_gain   = config_store.calibration.v_term_gain;
	  adc_data.i_out_offset  = config_store.calibration.i_out_offset_ma;
	  adc_data.i_out_gain    = config_store.calibration.i_out_gain;
	  adc_data.i_iso_offset  = config_store.calibration.i_iso_offset_ua;
	  adc_data.i_iso_gain    = config_store.calibration.i_iso_gain;
	  adc_data.v_sens_ext_offset = config_store.calibration.v_sens_ext_offset;
	  adc_data.v_sens_ext_gain   = config_store.calibration.v_sens_ext_gain;
	  adc_data.v_out_offset  = config_store.calibration.v_out_offset;
	  adc_data.v_out_gain    = config_store.calibration.v_out_gain;
	  adc_data.v_hv_offset   = config_store.calibration.v_hv_offset;
	  adc_data.v_hv_gain     = config_store.calibration.v_hv_gain;
	  adc_data.v_term_ext_offset = config_store.calibration.v_term_ext_offset;
	  adc_data.v_term_ext_gain   = config_store.calibration.v_term_ext_gain;
	  adc_data.i_out_ext_offset  = config_store.calibration.i_out_ext_offset;
	  adc_data.i_out_ext_gain    = config_store.calibration.i_out_ext_gain;
	  adc_data.i_iso_ext_offset  = config_store.calibration.i_iso_ext_offset;
	  adc_data.i_iso_ext_gain    = config_store.calibration.i_iso_ext_gain;

}


void adc_start(void){

	ADC345_COMMON->CCR |= ADC_CCR_VBATSEL;
	ADC12_COMMON->CCR |= ADC_CCR_VBATSEL;
	// The injected group (I_BAT/I_ISO) only exists on hadc5 (see
	// MX_ADC5_Init()/adc_configure_mode()'s HAL_ADCEx_InjectedConfigChannel()
	// calls) -- hadc1 has no injected channels configured on it at all, so
	// starting it there was always dead/wrong. Arm the real injected group.
	HAL_ADCEx_InjectedStart_IT(&hadc5);
	HAL_ADC_Start_DMA(&hadc5, (uint32_t*)&adc_data.raw.v_3v3, 12);

}

// Which mode's injected channel is currently configured on hadc5 - both
// ISOMETER (I_ISO) and CHARGE (I_BAT) share the same injected rank/callback,
// so HAL_ADCEx_InjectedConvCpltCallback() needs this to know which raw field
// the conversion result belongs to.
static statemachine_modes_t adc_injected_mode = STATEMACHINE_IDLE;

// Applies mode_table[mode].adc_trigger to the shared hadc1..4 ExternalTrigConv
// field -- the part of adc_configure_mode() that used to be four repeated
// assignment lines per case. ADC_TRIGGER_NONE means "leave hadc1..4 exactly
// as whichever mode ran before this one left them" (see mode_table.h) --
// this is how ISOMETER and RESISTANCE_1mA's pre-existing, deliberately
// preserved missing-trigger bugs (deferred to a separate, bench-validated
// fix) come through unchanged: their table entries are ADC_TRIGGER_NONE, so
// this function is a no-op for them, exactly like the pre-refactor switch
// cases that never assigned the trigger at all.
static void adc_apply_shared_trigger(statemachine_modes_t mode) {
	if (mode >= STATEMACHINE_MODE_RESERVED) {
		return;
	}
	uint32_t trigger = mode_table[mode].adc_trigger;
	if (trigger == ADC_TRIGGER_NONE) {
		return;
	}
	hadc1.Init.ExternalTrigConv = trigger;
	hadc2.Init.ExternalTrigConv = trigger;
	hadc3.Init.ExternalTrigConv = trigger;
	hadc4.Init.ExternalTrigConv = trigger;
}

void adc_configure_mode(statemachine_modes_t mode) {

	ADC_ChannelConfTypeDef sConfig = { 0 };
	ADC_InjectionConfTypeDef sConfigInjected = {0};

	adc_injected_mode = mode;

	HAL_ADC_Stop_DMA(&hadc1);
	HAL_ADC_Stop_DMA(&hadc2);
	HAL_ADC_Stop_DMA(&hadc3);
	HAL_ADC_Stop_DMA(&hadc4);
	HAL_ADC_Stop_DMA(&hadc5);

	adc_apply_shared_trigger(mode);

	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;

	switch (mode) {
	case STATEMACHINE_MODE_60V_OUT:
		sConfig.Channel = ADC_CHANNEL_2;
		if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
			Error_Handler();
		HAL_ADC_Init(&hadc4);
		HAL_ADC_Start_DMA(&hadc4, (uint32_t*) &adc_data.raw.v_out, 1);
		break;
		// STATEMACHINE_MODE_RESISTANCE_1mA is deliberately NOT a case here
		// (pre-existing, preserved bug, deferred to a separate,
		// bench-validated fix -- see adc_apply_shared_trigger()'s doc
		// comment above and mode_table.h's ADC_TRIGGER_NONE comment). It
		// falls through to `default:` below, getting neither the shared
		// trigger nor this hadc4 v_out channel config/DMA start, even
		// though ctrl_main_ctrl() runs its buck voltage loop on
		// converted.v_out, which hadc4 supplies. Do NOT add a case for it.
	case STATEMACHINE_MODE_RESISTANCE_1A:
		sConfig.Channel = ADC_CHANNEL_2;
		if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
			Error_Handler();
		HAL_ADC_Init(&hadc4);
		HAL_ADC_Start_DMA(&hadc4, (uint32_t*) &adc_data.raw.v_out, 1);
		break;


	case STATEMACHINE_MODE_ISOMETER:
		// Pre-existing, preserved bug: mode_table[STATEMACHINE_MODE_ISOMETER]
		// .adc_trigger is ADC_TRIGGER_NONE, so adc_apply_shared_trigger()
		// above left hadc1..4's shared trigger untouched -- ISOMETER's
		// control loop rate ends up depending on whichever mode ran before
		// it. Deferred to a separate, bench-validated fix; do NOT "fix" it
		// here. See mode_table.h's ADC_TRIGGER_NONE comment.
		HAL_ADC_Stop_DMA(&hadc4);
		sConfig.Channel = ADC_CHANNEL_5;
		if (HAL_ADC_ConfigChannel(&hadc4, &sConfig) != HAL_OK)
			Error_Handler();


		  sConfigInjected.InjectedChannel = ADC_CHANNEL_2;
		  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
		  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_2CYCLES_5;
		  sConfigInjected.InjectedSingleDiff = ADC_SINGLE_ENDED;
		  sConfigInjected.InjectedOffsetNumber = ADC_OFFSET_NONE;
		  sConfigInjected.InjectedOffset = 0;
		  sConfigInjected.InjectedNbrOfConversion = 1;
		  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
		  sConfigInjected.AutoInjectedConv = DISABLE;
		  sConfigInjected.QueueInjectedContext = DISABLE;
		  sConfigInjected.ExternalTrigInjecConv = ADC_TRIGGER_HRTIM_HV;
		  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
		  sConfigInjected.InjecOversamplingMode = DISABLE;
		  if (HAL_ADCEx_InjectedConfigChannel(&hadc5, &sConfigInjected) != HAL_OK)
		  {
		    Error_Handler();
		  }
		  // HAL_ADC_Stop_DMA(&hadc5) above (top of this function) stops BOTH
		  // the regular and injected groups on hadc5 and disables the ADC
		  // (see ADC_ConversionStop(hadc, ADC_REGULAR_INJECTED_GROUP) +
		  // ADC_Disable() inside HAL_ADC_Stop_DMA()). Reconfiguring the
		  // injected channel here does NOT resume triggering by itself, so
		  // the injected group must be re-armed every time we (re-)enter
		  // ISOMETER mode. JADSTART is guaranteed clear at this point (we
		  // just stopped it above), so this cannot return HAL_BUSY.
		  HAL_ADCEx_InjectedStart_IT(&hadc5);
		HAL_ADC_Start_DMA(&hadc4, (uint32_t*) &adc_data.raw.v_hv, 1);

		break;
	case STATEMACHINE_MODE_CHARGE:
		sConfigInjected.InjectedChannel = ADC_CHANNEL_1;
				  sConfigInjected.InjectedRank = ADC_INJECTED_RANK_1;
				  sConfigInjected.InjectedSamplingTime = ADC_SAMPLETIME_2CYCLES_5;
				  sConfigInjected.InjectedSingleDiff = ADC_SINGLE_ENDED;
				  sConfigInjected.InjectedOffsetNumber = ADC_OFFSET_NONE;
				  sConfigInjected.InjectedOffset = 0;
				  sConfigInjected.InjectedNbrOfConversion = 1;
				  sConfigInjected.InjectedDiscontinuousConvMode = DISABLE;
				  sConfigInjected.AutoInjectedConv = DISABLE;
				  sConfigInjected.QueueInjectedContext = DISABLE;
				  sConfigInjected.ExternalTrigInjecConv = ADC_TRIGGER_HRTIM_SEK;
				  sConfigInjected.ExternalTrigInjecConvEdge = ADC_EXTERNALTRIGINJECCONV_EDGE_RISING;
				  sConfigInjected.InjecOversamplingMode = DISABLE;
				  if (HAL_ADCEx_InjectedConfigChannel(&hadc5, &sConfigInjected) != HAL_OK)
				  {
				    Error_Handler();
				  }
				  // Same requirement as ISOMETER above: HAL_ADC_Stop_DMA(&hadc5) at
				  // the top of this function stops the injected group too, so it
				  // must be re-armed every time we (re-)enter CHARGE mode.
				  HAL_ADCEx_InjectedStart_IT(&hadc5);
				  break;
	case STATEMACHINE_MODE_10A_OUT:
	case STATEMACHINE_MODE_VOLTMETER:
	case STATEMACHINE_MODE_AMPMETER:
	case STATEMACHINE_MODE_SETTINGS:
	case STATEMACHINE_MODE_SHUTDOWN:
	case STATEMACHINE_MODE_RESERVED:
	case STATEMACHINE_IDLE:
	default:
		/* Ampmeter is a passive readout of i_out, which hadc1 already
		 * samples continuously below regardless of mode -- nothing extra
		 * to configure here. */
		break;
	}



	HAL_ADC_Init(&hadc1);
	HAL_ADC_Init(&hadc2);
	HAL_ADC_Init(&hadc3);

	HAL_ADC_Start_DMA(&hadc1, (uint32_t*) &adc_data.raw.i_out,1);
	HAL_ADC_Start_DMA(&hadc2, (uint32_t*) &adc_data.raw.v_term,1);
	HAL_ADC_Start_DMA(&hadc3, (uint32_t*) &adc_data.raw.v_in,1);
	HAL_ADC_Start_DMA(&hadc5, (uint32_t*) &adc_data.raw.v_3v3, 12);
}

void adc_convert_fast_data(void){
	// Runs in ISR context (HAL_ADC_ConvCpltCallback()) -- writes the
	// ISR-private instance only. ctrl_main_ctrl() (called right after, same
	// ISR) reads this same instance for its control loop. Main context never
	// touches adc_converted_isr directly; it only ever sees a coherent copy via
	// adc_snapshot_converted() (see statemachine_step()).
	adc_converted_isr.v_in   = (adc_data.raw.v_in   - adc_data.v_in_offset  )* ADC_VIN_GAIN_MV;
	adc_converted_isr.v_out  = (adc_data.raw.v_out  - adc_data.v_out_offset )* adc_data.v_out_gain;
	adc_converted_isr.v_term = (adc_data.raw.v_term - adc_data.v_term_offset)* adc_data.v_term_gain;
	adc_converted_isr.v_hv   = (adc_data.raw.v_hv   - adc_data.v_hv_offset  )* adc_data.v_hv_gain;
	adc_converted_isr.i_bat  = (adc_data.raw.i_bat  - adc_data.i_bat_offset )* ADC_IBAT_GAIN_MA;
	adc_converted_isr.i_out  = (adc_data.raw.i_out  - adc_data.i_out_offset ) * adc_data.i_out_gain;
	adc_converted_isr.i_iso  = (adc_data.raw.i_iso  - adc_data.i_iso_offset ) * adc_data.i_iso_gain;
	adc_converted_isr.v_term_ext_mv = (adc_data.ext_adc_data[0] - adc_data.v_term_ext_offset) * adc_data.v_term_ext_gain;
	// v_term_ext_mv_filt was previously never assigned (the only write to it was
	// inside a commented-out legacy callback using a pre-refactor field name),
	// so Voltmeter/60V readouts that display this field always read zero.
	adc_converted_isr.v_term_ext_mv_filt = (int32_t) ((1.0f - ADC_R_MOHM_FILT_ALPHA)* adc_converted_isr.v_term_ext_mv_filt
			+ ADC_R_MOHM_FILT_ALPHA * adc_converted_isr.v_term_ext_mv);
	adc_converted_isr.i_out_ext_mA  = (adc_data.ext_adc_data[1] - adc_data.i_out_ext_offset) * adc_data.i_out_ext_gain;
	adc_converted_isr.v_sens_ext_uv = (adc_data.ext_adc_data[2] - adc_data.v_sens_ext_offset) * adc_data.v_sens_ext_gain;
	adc_converted_isr.i_iso_ext_uA  = (adc_data.ext_adc_data[3] - adc_data.i_iso_ext_offset) * adc_data.i_iso_ext_gain;
}

// Copies the ISR-private "converted" fields (adc_converted_isr, see
// above) into adc_data.converted -- the copy every main-context reader
// (adc_convert_data(), protection_update(), display_*(), CLI commands) uses
// -- as one atomic snapshot. Must run once per statemachine_step() tick,
// before adc_convert_data(), so main context always sees a self-consistent
// set of fields from the same sample instant instead of a torn mix of two.
//
// Critical section cost: sizeof(ADC_CONVERTED_DATA) is 68 bytes (measured
// with the project's arm-none-eabi-gcc, including struct padding), copied as
// a straight struct assignment with interrupts fully disabled and nothing
// else in the section. At -Os that's on the order of 20-30 word/halfword
// load-store pairs -- tens of cycles, well under 100ns at the 170MHz core
// clock -- against the ~40us ADC period, so it cannot cause a missed
// conversion.
void adc_snapshot_converted(void) {
	uint32_t primask = __get_PRIMASK();
	__disable_irq();
	adc_data.converted = adc_converted_isr;
	__set_PRIMASK(primask);
}

void adc_convert_data(void){

	// Fast ADC Data are automatically converted in the step function


	//// Calculate resistance
	// RESISTANCE_1A drives the ~1A test current as a 1Hz/50% duty pulse
	// (dac_sqwave_start(), started in statemachine_switchfromIdle()), so
	// i_out_ext_mA is only a valid, settled ~1A reading during the pulse's
	// "high" half - during "low" it's near-zero/noise, and a fresh ratio
	// computed from it is garbage. While in RESISTANCE_1A, gate the
	// calculation to the "high" phase and hold the last good values through
	// "low" instead of overwriting them. Any other mode (notably
	// RESISTANCE_1mA/Ohmmeter, which uses a continuous, unpulsed reference)
	// keeps the original unconditional, per-tick computation unchanged.
	if (adc_injected_mode != STATEMACHINE_MODE_RESISTANCE_1A || dac_sqwave_is_high()) {
		if (adc_data.converted.i_out_ext_mA != 0) {
			uint32_t r_mOhmx10_raw = 10*adc_data.converted.v_sens_ext_uv / adc_data.converted.i_out_ext_mA;
			if (adc_injected_mode == STATEMACHINE_MODE_RESISTANCE_1A) {
				if (adc_data.r_mOhmx10 == UINT32_MAX) {
					// No valid filter history yet - seed with the fresh
					// sample instead of blending with the sentinel.
					adc_data.r_mOhmx10 = r_mOhmx10_raw;
				} else {
					// One-pole low-pass, fc ~= 2Hz @ 50Hz - see
					// ADC_R_MOHM_FILT_ALPHA.
					adc_data.r_mOhmx10 = (uint32_t) (ADC_R_MOHM_FILT_ALPHA * r_mOhmx10_raw
							+ (1.0f - ADC_R_MOHM_FILT_ALPHA) * adc_data.r_mOhmx10);
				}
				if(adc_data.r_mOhmx10>ADC_R_MOHMX10_MAX_VALUE)
					adc_data.r_mOhmx10 = UINT32_MAX;
			} else {
				adc_data.r_mOhmx10 = r_mOhmx10_raw;
			}
			adc_data.r_Ohmx10 = 10*adc_data.converted.v_term_ext_mv / adc_data.converted.i_out_ext_mA ;
		}
		else {
			adc_data.r_mOhmx10 = UINT32_MAX;
			adc_data.r_Ohmx10 = UINT32_MAX;
		}
	}
	// else: RESISTANCE_1A, pulse currently "low" - hold last computed
	// r_mOhmx10/r_Ohmx10 rather than recompute from a near-zero current.

	// Calculate / Estimate Temperatures
	 adc_data.converted.temp_trafo   =  temp_deg_int(adc_data.raw.temp_trafo   >> 4);
	 adc_data.converted.temp_current =  temp_deg_int(adc_data.raw.temp_current >> 4);
	 adc_data.converted.temp_prim    =  temp_deg_int(adc_data.raw.temp_prim    >> 4);
	 adc_data.converted.temp_sec     =  temp_deg_int(adc_data.raw.temp_sec     >> 4);
	 adc_data.converted.int_temp     = __LL_ADC_CALC_TEMPERATURE(2500, (adc_data.raw.int_temp/16), LL_ADC_RESOLUTION_12B);

	 adc_data.converted.v_3v3        = adc_data.raw.v_3v3     *   ADC_GAIN_V_3V3;
	 adc_data.converted.v_3v3a       = adc_data.raw.v_3v3a    *   ADC_GAIN_V_3V3A;
	 adc_data.converted.v_15v        = adc_data.raw.v_15v     *   ADC_GAIN_V_15V;
	 adc_data.converted.v_vcc        = adc_data.raw.v_vcc     *   ADC_GAIN_V_VCC;
	 adc_data.converted.v_5v         = adc_data.raw.v_5v      *   ADC_GAIN_V_5V;
	 adc_data.converted.v_bat        = adc_data.raw.v_bat     *   ADC_GAIN_V_BAT;
	 adc_data.converted.v_ref_int    = __LL_ADC_CALC_VREFANALOG_VOLTAGE(( adc_data.raw.v_ref_int/16),LL_ADC_RESOLUTION_12B);

}



void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef* hadc)
{
	// ISOMETER and CHARGE both inject on ADC5 rank 1 (see
	// adc_configure_mode()), but onto different pins (I_ISO vs I_BAT), so
	// the result has to be routed by whichever mode configured the injected
	// group last - adc_injected_mode tracks that. The rest below predates
	// the current refactor and stays commented out/dead.
	if (hadc->Instance == ADC5) {
		if (adc_injected_mode == STATEMACHINE_MODE_ISOMETER) {
			adc_data.raw.i_iso = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
		} else if (adc_injected_mode == STATEMACHINE_MODE_CHARGE) {
			adc_data.raw.i_bat = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
		}
	}

	//adc_data.v_in_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
	//adc_data.i_bat_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
	//adc_data.v_out_raw = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_3);
	////adc_data.v_in_mV   = (adc_data.v_in_raw   - adc_data.v_in_offset  ) * ADC_VIN_GAIN_MV;
	//adc_data.i_bat_mA  = (adc_data.i_bat_raw  - adc_data.i_bat_offset ) * ADC_IBAT_GAIN_MA;
	//adc_data.v_out_mV  = (adc_data.v_out_raw  - adc_data.v_out_offset ) * ADC_VOUT_GAIN_MV;
	//adc_data.converted.v_term_ext_mv = adc_data.ext_adc_data[0] * ADC_EXT_VTERM_GAIN_MV;
	//adc_data.v_term_ext_mv_filt = 0.9f*adc_data.v_term_ext_mv_filt + 0.1f*adc_data.v_term_ext_mv;
	//adc_data.converted.i_out_ext_mA  = adc_data.ext_adc_data[1] * ADC_EXT_IOUT_GAIN_mA;

	//adc_data.reference_poti = tim_encoder_read();
	//ctrl_main_ctrl_60v(0, adc_data.v_out_mV, adc_data.v_term_ext_mv, adc_data.i_bat_mA);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
	if (hadc == &hadc1) {
		adc_interrupt_cnt++;
		if (adc_interrupt_cnt > 1) {
			adc_interrupt_cnt = 0;
			adc_convert_fast_data();
			// Pass the ISR-private instance, not &adc_data: ctrl_main_ctrl()
			// only reads ->converted.* (v_out/v_term_ext_mv/i_out_ext_mA/...),
			// and it needs THIS tick's freshly-computed values, not whatever
			// main context's adc_snapshot_converted() last copied into
			// adc_data.converted up to 20ms ago.
			ctrl_main_ctrl(&adc_converted_isr);
		}
	}
}


/* USER CODE END 1 */
