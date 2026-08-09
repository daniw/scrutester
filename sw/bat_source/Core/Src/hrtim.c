/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    hrtim.c
 * @brief   This file provides code for the configuration
 *          of the HRTIM instances.
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
#include "hrtim.h"

/* USER CODE BEGIN 0 */
#include "stdio.h"
/* USER CODE END 0 */

HRTIM_HandleTypeDef hhrtim1;

/* HRTIM1 init function */
void MX_HRTIM1_Init(void)
{

  /* USER CODE BEGIN HRTIM1_Init 0 */

  /* USER CODE END HRTIM1_Init 0 */

  HRTIM_ADCTriggerCfgTypeDef pADCTriggerCfg = {0};
  HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
  HRTIM_TimerCtlTypeDef pTimerCtl = {0};
  HRTIM_TimerCfgTypeDef pTimerCfg = {0};
  HRTIM_CompareCfgTypeDef pCompareCfg = {0};
  HRTIM_DeadTimeCfgTypeDef pDeadTimeCfg = {0};
  HRTIM_OutputCfgTypeDef pOutputCfg = {0};

  /* USER CODE BEGIN HRTIM1_Init 1 */

  /* USER CODE END HRTIM1_Init 1 */
  hhrtim1.Instance = HRTIM1;
  hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
  hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
  if (HAL_HRTIM_Init(&hhrtim1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_DLLCalibrationStart(&hhrtim1, HRTIM_CALIBRATIONRATE_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_PollForDLLCalibration(&hhrtim1, 10) != HAL_OK)
  {
    Error_Handler();
  }
  pADCTriggerCfg.UpdateSource = HRTIM_ADCTRIGGERUPDATE_TIMER_B;
  pADCTriggerCfg.Trigger = HRTIM_ADCTRIGGEREVENT13_TIMERB_PERIOD;
  if (HAL_HRTIM_ADCTriggerConfig(&hhrtim1, HRTIM_ADCTRIGGER_1, &pADCTriggerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_ADCPostScalerConfig(&hhrtim1, HRTIM_ADCTRIGGER_1, 19) != HAL_OK)
  {
    Error_Handler();
  }
  pADCTriggerCfg.UpdateSource = HRTIM_ADCTRIGGERUPDATE_TIMER_E;
  pADCTriggerCfg.Trigger = HRTIM_ADCTRIGGEREVENT13_TIMERE_PERIOD;
  if (HAL_HRTIM_ADCTriggerConfig(&hhrtim1, HRTIM_ADCTRIGGER_3, &pADCTriggerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_ADCPostScalerConfig(&hhrtim1, HRTIM_ADCTRIGGER_3, 19) != HAL_OK)
  {
    Error_Handler();
  }
  pADCTriggerCfg.UpdateSource = HRTIM_ADCTRIGGERUPDATE_TIMER_C;
  pADCTriggerCfg.Trigger = HRTIM_ADCTRIGGEREVENT579_TIMERC_PERIOD;
  if (HAL_HRTIM_ADCTriggerConfig(&hhrtim1, HRTIM_ADCTRIGGER_5, &pADCTriggerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_ADCPostScalerConfig(&hhrtim1, HRTIM_ADCTRIGGER_5, 0xE) != HAL_OK)
  {
    Error_Handler();
  }
  pTimeBaseCfg.Period = 51200;
  pTimeBaseCfg.RepetitionCounter = 0x00;
  pTimeBaseCfg.PrescalerRatio = HRTIM_PRESCALERRATIO_MUL32;
  pTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCtl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UPDOWN;
  pTimerCtl.GreaterCMP1 = HRTIM_TIMERGTCMP1_EQUAL;
  pTimerCtl.DualChannelDacEnable = HRTIM_TIMER_DCDE_DISABLED;
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_RollOverModeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_TIM_FEROM_BOTH|HRTIM_TIM_BMROM_BOTH
                              |HRTIM_TIM_ADROM_CREST|HRTIM_TIM_OUTROM_BOTH
                              |HRTIM_TIM_ROM_BOTH) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.InterruptRequests = HRTIM_TIM_IT_NONE;
  pTimerCfg.DMARequests = HRTIM_TIM_DMA_NONE;
  pTimerCfg.DMASrcAddress = 0x0000;
  pTimerCfg.DMADstAddress = 0x0000;
  pTimerCfg.DMASize = 0x1;
  pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
  pTimerCfg.InterleavedMode = HRTIM_INTERLEAVED_MODE_DISABLED;
  pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
  pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
  pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_ENABLED;
  pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
  pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
  pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED;
  pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
  pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
  pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_A_B_C_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_TIMER_B;
  pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_ENABLED;
  pTimerCfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_CONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_DISABLED;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_DISABLED;
  pTimerCfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_UNCONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimerCfg.PreloadEnable = HRTIM_PRELOAD_ENABLED;
  pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_ENABLED;
  pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_D_E_DELAYEDPROTECTION_DISABLED;
  pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_TIMER_E;
  pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_ENABLED;
  pTimerCfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_CONDITIONAL;
  if (HAL_HRTIM_WaveformTimerConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimerCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 0x60;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pDeadTimeCfg.Prescaler = HRTIM_TIMDEADTIME_PRESCALERRATIO_MUL8;
  pDeadTimeCfg.RisingValue = 0x00A;
  pDeadTimeCfg.RisingSign = HRTIM_TIMDEADTIME_RISINGSIGN_POSITIVE;
  pDeadTimeCfg.RisingLock = HRTIM_TIMDEADTIME_RISINGLOCK_READONLY;
  pDeadTimeCfg.RisingSignLock = HRTIM_TIMDEADTIME_RISINGSIGNLOCK_READONLY;
  pDeadTimeCfg.FallingValue = 0x00A;
  pDeadTimeCfg.FallingSign = HRTIM_TIMDEADTIME_FALLINGSIGN_POSITIVE;
  pDeadTimeCfg.FallingLock = HRTIM_TIMDEADTIME_FALLINGLOCK_READONLY;
  pDeadTimeCfg.FallingSignLock = HRTIM_TIMDEADTIME_FALLINGSIGNLOCK_READONLY;
  if (HAL_HRTIM_DeadTimeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, &pDeadTimeCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_DeadTimeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pDeadTimeCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMCMP1;
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
  pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pOutputCfg.SetSource = HRTIM_OUTPUTSET_NONE;
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_OUTPUT_TC1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_OUTPUT_TE1, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_NONE;
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B, HRTIM_OUTPUT_TB2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_OUTPUT_TE2, &pOutputCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimeBaseCfg.Period = 0x3924;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_RollOverModeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_TIM_FEROM_BOTH|HRTIM_TIM_BMROM_BOTH
                              |HRTIM_TIM_ADROM_CREST|HRTIM_TIM_OUTROM_BOTH
                              |HRTIM_TIM_ROM_BOTH) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 0x1555;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  pTimeBaseCfg.Period = 0xC800;
  if (HAL_HRTIM_TimeBaseConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimeBaseCfg) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_WaveformTimerControl(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, &pTimerCtl) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_HRTIM_RollOverModeConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_TIM_FEROM_BOTH|HRTIM_TIM_BMROM_BOTH
                              |HRTIM_TIM_ADROM_CREST|HRTIM_TIM_OUTROM_BOTH
                              |HRTIM_TIM_ROM_BOTH) != HAL_OK)
  {
    Error_Handler();
  }
  pCompareCfg.CompareValue = 0x300;
  if (HAL_HRTIM_WaveformCompareConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E, HRTIM_COMPAREUNIT_1, &pCompareCfg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN HRTIM1_Init 2 */

  /* USER CODE END HRTIM1_Init 2 */
  HAL_HRTIM_MspPostInit(&hhrtim1);

}

void HAL_HRTIM_MspInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspInit 0 */

  /* USER CODE END HRTIM1_MspInit 0 */
    /* HRTIM1 clock enable */
    __HAL_RCC_HRTIM1_CLK_ENABLE();
  /* USER CODE BEGIN HRTIM1_MspInit 1 */

  /* USER CODE END HRTIM1_MspInit 1 */
  }
}

void HAL_HRTIM_MspPostInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspPostInit 0 */

  /* USER CODE END HRTIM1_MspPostInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**HRTIM1 GPIO Configuration
    PB12     ------> HRTIM1_CHC1
    PC8     ------> HRTIM1_CHE1
    PC9     ------> HRTIM1_CHE2
    PA10     ------> HRTIM1_CHB1
    PA11     ------> HRTIM1_CHB2
    */
    GPIO_InitStruct.Pin = HV_CTRL_PRIM_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;
    HAL_GPIO_Init(HV_CTRL_PRIM_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CONV_CTRL_SEC_L_Pin|CONV_CTRL_SEC_H_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF3_HRTIM1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CONV_CTRL_PRIM_L_Pin|CONV_CTRL_PRIM_H_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN HRTIM1_MspPostInit 1 */

  /* USER CODE END HRTIM1_MspPostInit 1 */
  }

}

void HAL_HRTIM_MspDeInit(HRTIM_HandleTypeDef* hrtimHandle)
{

  if(hrtimHandle->Instance==HRTIM1)
  {
  /* USER CODE BEGIN HRTIM1_MspDeInit 0 */

  /* USER CODE END HRTIM1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_HRTIM1_CLK_DISABLE();
  /* USER CODE BEGIN HRTIM1_MspDeInit 1 */

  /* USER CODE END HRTIM1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
/**
 * Starts all HRTIM Channel counters
 */
void hrtim_start_timer(void) {
	HRTIM1->sMasterRegs.MCR |= (HRTIM_MCR_TBCEN | HRTIM_MCR_TCCEN | HRTIM_MCR_TECEN);
}

/**
 * @brief Checks whether channel is a valid index into HRTIM1->sTimerxRegs[]
 *        for one of the channels this driver actually drives (PRIM/SEK/HV).
 *        Deliberately excludes HRTIM_CHANNEL_ALL (a software-only sentinel
 *        meaningful only to hrtim_disable()) and any other timer index, so
 *        callers can never make hrtim_set_freq()/hrtim_set_duty() write an
 *        unintended timer's registers.
 * @param channel Channel to validate.
 * @return 1 if valid, 0 otherwise.
 */
static uint8_t hrtim_channel_is_valid(uint8_t channel) {
	return (channel == HRTIM_CHANNEL_PRIM) || (channel == HRTIM_CHANNEL_SEK)
			|| (channel == HRTIM_CHANNEL_HV);
}

/**
 * Sets the frequency for the specified channel
 * @param channel Channel for which to set the frequency.
 * @param freq Frequency to set in Hz.
 */
void hrtim_set_freq(uint8_t channel, uint32_t freq) {
	uint32_t period;

	if (!hrtim_channel_is_valid(channel))
		return;

	// freq == 0 has no valid period (division by zero); there is no sane
	// substitute value to invent here, so refuse rather than guess.
	if (freq == 0)
		return;

	period = (0x98968000 / freq);
	// PERxR is a 16-bit register subject to the same HRTIM-imposed limits as
	// CMP1xR (see hrtim_set_duty() below): a too-low freq makes the quotient
	// overflow 16 bits and silently truncate to an arbitrary period, and a
	// too-high freq can undershoot HRTIM's minimum usable period. Clamp to
	// the same [0x60, 0xFFDF] window hrtim_set_duty() already enforces, so
	// the timer always ends up at a well-defined, bounded frequency instead
	// of an arbitrary truncated one - consistent with hrtim_set_duty()'s
	// existing silent-clamp behaviour.
	if (period < 0x60)
		period = 0x60;
	else if (period > 0xFFDF)
		period = 0xFFDF;

	HRTIM1->sTimerxRegs[channel].PERxR = period;
	//HRTIM1->sTimerxRegs[channel].TIMxCR = (HRTIM1->sTimerxRegs[channel].TIMxCR & (~ HRTIM_PRESCALERRATIO_DIV4)) | HRTIM_PRESCALERRATIO_MUL32;
}

/**
 * Set the duty cycle for the specified channel
 * @param channel Channel for which to set the frequency.
 * @param value duty in pu from 0 to 1
 */

void hrtim_set_duty(uint8_t channel, float value) {
	uint32_t duty;

	if (!hrtim_channel_is_valid(channel))
		return;

	if (value < 0.0F)
		value = 0.0F;
	else if (value > 1.0F)
		value = 1.0F;
	duty = (HRTIM1->sTimerxRegs[channel].PERxR * value);
	// Enforce more strict limit imposed by HRTIM
	if (duty < 0x60)
		duty = 0x60;
	else if (duty > 0xFFDF)
		duty = 0xFFDF;
	HRTIM1->sTimerxRegs[channel].CMP1xR = duty;


}

void hrtim_enable(uint8_t channel) {
	switch (channel) {
	case HRTIM_CHANNEL_PRIM:
		HRTIM1->sCommonRegs.OENR = (HRTIM_OENR_TB1OEN | HRTIM_OENR_TB2OEN);
		break;
	case HRTIM_CHANNEL_SEK:
		HRTIM1->sCommonRegs.OENR = (HRTIM_OENR_TE1OEN | HRTIM_OENR_TE2OEN);
		break;
	case HRTIM_CHANNEL_HV:
		HRTIM1->sCommonRegs.OENR = (HRTIM_OENR_TC1OEN | HRTIM_OENR_TC2OEN);
		break;
	}
}
void hrtim_disable(uint8_t channel) {
	switch (channel) {
	case HRTIM_CHANNEL_ALL:
		HRTIM1->sCommonRegs.ODISR = (HRTIM_OENR_TB1OEN | HRTIM_OENR_TB2OEN) | (HRTIM_OENR_TE1OEN | HRTIM_OENR_TE2OEN) | (HRTIM_OENR_TC1OEN | HRTIM_OENR_TC2OEN);
		break;
	case HRTIM_CHANNEL_PRIM:
		HRTIM1->sCommonRegs.ODISR = (HRTIM_OENR_TB1OEN | HRTIM_OENR_TB2OEN);
		break;
	case HRTIM_CHANNEL_SEK:
		HRTIM1->sCommonRegs.ODISR = (HRTIM_OENR_TE1OEN | HRTIM_OENR_TE2OEN);
		break;
	case HRTIM_CHANNEL_HV:
		HRTIM1->sCommonRegs.ODISR = (HRTIM_OENR_TC1OEN | HRTIM_OENR_TC2OEN);
		break;
	}
}

static uint8_t hrtim_sek_shorted = 0;

/**
 * @brief Forces the secondary-side (SEK/SEC) half-bridge into a deliberate
 *        shoot-through (both switches driven ON simultaneously) for
 *        AMPMETER mode, so current can flow through the shunt without any
 *        converter switching action in the way. Detaches
 *        CONV_CTRL_SEC_L_Pin/CONV_CTRL_SEC_H_Pin from HRTIM's alternate
 *        function and drives them as plain GPIO instead, since
 *        hrtim_set_duty()'s clamp (0x60-0xFFDF) can't reach a true static
 *        0%/100% duty.
 *
 *        CALLER MUST confirm the output is at ~0V before calling this -
 *        shoot-through is only safe here because there's no significant
 *        voltage across the half-bridge to short. Timer E's internal
 *        counter keeps running (only the physical output pins are
 *        detached), so ADC triggers tied to it are unaffected.
 *
 *        Which logic level is "ON" for these gate signals is not yet
 *        confirmed from the schematic - verify on the bench with the
 *        output current-limited/disconnected before trusting this under
 *        load.
 * @return none
 */
void hrtim_sek_force_short(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	hrtim_disable(HRTIM_CHANNEL_SEK);

	GPIO_InitStruct.Pin = CONV_CTRL_SEC_L_Pin | CONV_CTRL_SEC_H_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	// Drive both gates to the same ON level - deliberate shoot-through.
	// TODO: confirm ON polarity on the bench before relying on this under load.
	HAL_GPIO_WritePin(CONV_CTRL_SEC_L_GPIO_Port, CONV_CTRL_SEC_L_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONV_CTRL_SEC_H_GPIO_Port, CONV_CTRL_SEC_H_Pin, GPIO_PIN_SET);

	hrtim_sek_shorted = 1;
}

/**
 * @brief Reverses hrtim_sek_force_short(): restores
 *        CONV_CTRL_SEC_L_Pin/CONV_CTRL_SEC_H_Pin to HRTIM's alternate
 *        function (mirroring HAL_HRTIM_MspPostInit's original
 *        configuration exactly) and re-enables the SEK output. Safe to
 *        call even if the pins are already in their normal state.
 * @return none
 */
void hrtim_sek_restore(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	if (!hrtim_sek_shorted)
		return;

	GPIO_InitStruct.Pin = CONV_CTRL_SEC_L_Pin | CONV_CTRL_SEC_H_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF3_HRTIM1;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	hrtim_enable(HRTIM_CHANNEL_SEK);

	hrtim_sek_shorted = 0;
}
/* USER CODE END 1 */
