/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */
#include "irq_priority.h"
/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, DISPLAY_WR_Pin|DISPLAY_CS_Pin|CONV_CTRL_EN_Pin|OUT_SEL_ISO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BMS_CTRL_WAKEUP_GPIO_Port, BMS_CTRL_WAKEUP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, SHUNT_EN_Pin|DISCHARGE_N_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, OUT_SEL_HV_Pin|OVP_RESET_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(HV_CTRL_EN_GPIO_Port, HV_CTRL_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ON_REQ_GPIO_Port, ON_REQ_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, SHUNT_ISO_EN_Pin|ADC_SYNC_RESET_N_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI_CS_GPIO_Port, SPI_CS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : DISPLAY_WR_Pin */
  GPIO_InitStruct.Pin = DISPLAY_WR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(DISPLAY_WR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DISPLAY_CS_Pin CONV_CTRL_EN_Pin ON_REQ_Pin OUT_SEL_ISO_Pin */
  GPIO_InitStruct.Pin = DISPLAY_CS_Pin|CONV_CTRL_EN_Pin|ON_REQ_Pin|OUT_SEL_ISO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : RESTRICT__No_toggle_Pin */
  GPIO_InitStruct.Pin = RESTRICT__No_toggle_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RESTRICT__No_toggle_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BMS_CTRL_WAKEUP_Pin */
  GPIO_InitStruct.Pin = BMS_CTRL_WAKEUP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BMS_CTRL_WAKEUP_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SHUNT_EN_Pin OUT_SEL_HV_Pin OVP_RESET_Pin DISCHARGE_N_Pin */
  GPIO_InitStruct.Pin = SHUNT_EN_Pin|OUT_SEL_HV_Pin|OVP_RESET_Pin|DISCHARGE_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : OVP_N_Pin OCP_N_Pin */
  GPIO_InitStruct.Pin = OVP_N_Pin|OCP_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : HV_CTRL_EN_Pin */
  GPIO_InitStruct.Pin = HV_CTRL_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(HV_CTRL_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RESTRICT__No_toggleB10_Pin RESTRICT__No_toggleB11_Pin RESTRICT__No_toggleB13_Pin RESTRICT__No_toggleB14_Pin */
  GPIO_InitStruct.Pin = RESTRICT__No_toggleB10_Pin|RESTRICT__No_toggleB11_Pin|RESTRICT__No_toggleB13_Pin|RESTRICT__No_toggleB14_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SHUNT_ISO_EN_Pin */
  GPIO_InitStruct.Pin = SHUNT_ISO_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(SHUNT_ISO_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : RESTRICT__No_toggleD14_Pin */
  GPIO_InitStruct.Pin = RESTRICT__No_toggleD14_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(RESTRICT__No_toggleD14_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : I2C_ALERT_Pin BUTTON_ESC_Pin */
  GPIO_InitStruct.Pin = I2C_ALERT_Pin|BUTTON_ESC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : SPI_CS_Pin */
  GPIO_InitStruct.Pin = SPI_CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(SPI_CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ADC_SYNC_RESET_N_Pin */
  GPIO_InitStruct.Pin = ADC_SYNC_RESET_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(ADC_SYNC_RESET_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ADC_DRDY_N_Pin */
  GPIO_InitStruct.Pin = ADC_DRDY_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ADC_DRDY_N_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : UI_PRESENT_Pin BOOT0_Pin BUTTON_OUT_Pin */
  GPIO_InitStruct.Pin = UI_PRESENT_Pin|BOOT0_Pin|BUTTON_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BUTTON_OK_Pin */
  GPIO_InitStruct.Pin = BUTTON_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUTTON_OK_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI2_IRQn, 4, 0);

}

/* USER CODE BEGIN 2 */
void gpio_irq_priority_override(void) {
	HAL_NVIC_SetPriority(EXTI2_IRQn, IRQ_PRIO_EXT_ADC, IRQ_SUBPRIO_NONE);
}

/*
 * Sends on-request signal to on/off controller
 */
void gpio_turnOn(void){
	HAL_GPIO_WritePin(ON_REQ_GPIO_Port, ON_REQ_Pin,1);
}

/*
 * Sends turn-off signal to on/off controller
 */
void gpio_turnOff(void){
	HAL_GPIO_WritePin(ON_REQ_GPIO_Port, ON_REQ_Pin,0);
}
/* USER CODE END 2 */
