/*
 * aux_io_ctrl.c
 *
 *  Created on: May 25, 2025
 *      Author: Andreas
 */

#include "aux_io_ctrl.h"
#include "driver_pca9554.h"
#include "gpio.h"
#include "statemachine.h"
#include "mode_table.h"

/* GPIO_MASK_* (which relay/shunt GPIOs a mode drives high) and the per-mode
 * mask values themselves now live in mode_table[] (mode_table.h/.c) - see
 * aux_io_ctrl_set_config() below. */

PCA9554_handle hpca_hw_rev;

void aux_io_ctrl_GPIO_Init(void) {
	pca9554_init(&hpca_hw_rev, PCA9554_ADDRESS_HW_REV, 0xFF);
}

uint8_t aux_io_ctrl_readHW_Revision(void) {
	return pca9554_read_input(&hpca_hw_rev);
}

/**
 * Function used to manually set the GPIOs from CLI
 * @param pin pin number as defined above
 * @param value value to set the pin (0,1)
 */
void aux_io_ctrl_manual_set_io(uint8_t pin, uint8_t value) {
	switch (pin) {
	case GPIO_DISPLAY_WR:
		HAL_GPIO_WritePin(DISPLAY_WR_GPIO_Port, DISPLAY_WR_Pin, value);
		return;
	case GPIO_CONV_CTRL_EN:
		HAL_GPIO_WritePin(CONV_CTRL_EN_GPIO_Port, CONV_CTRL_EN_Pin, value);
		return;
	case GPIO_ON_REQ:
		HAL_GPIO_WritePin(ON_REQ_GPIO_Port, ON_REQ_Pin, value);
		return;
	case GPIO_OUT_SEL_ISO:
		HAL_GPIO_WritePin(OUT_SEL_ISO_GPIO_Port, OUT_SEL_ISO_Pin, value);
		return;
	case GPIO_BMS_CTRL_WAKEUP:
		HAL_GPIO_WritePin(BMS_CTRL_WAKEUP_GPIO_Port, BMS_CTRL_WAKEUP_Pin,
				value);
		return;
	case GPIO_SHUNT_EN:
		HAL_GPIO_WritePin(SHUNT_EN_GPIO_Port, SHUNT_EN_Pin, value);
		return;
	case GPIO_OUT_SEL_HV:
		HAL_GPIO_WritePin(OUT_SEL_HV_GPIO_Port, OUT_SEL_HV_Pin, value);
		return;
	case GPIO_OVP_RESET:
		HAL_GPIO_WritePin(OVP_RESET_GPIO_Port, OVP_RESET_Pin, value);
		return;
	case GPIO_DISCHARGE:
		HAL_GPIO_WritePin(DISCHARGE_N_GPIO_Port, DISCHARGE_N_Pin, value);
		return;
	case GPIO_HV_CTRL_EN:
		HAL_GPIO_WritePin(HV_CTRL_EN_GPIO_Port, HV_CTRL_EN_Pin, value);
		return;
	case GPIO_SHUNT_ISO_EN:
		HAL_GPIO_WritePin(SHUNT_ISO_EN_GPIO_Port, SHUNT_ISO_EN_Pin, value);
		return;
	case GPIO_ADC_SYNC_RESET:
		HAL_GPIO_WritePin(ADC_SYNC_RESET_N_GPIO_Port, ADC_SYNC_RESET_N_Pin,
				value);
		return;
	default:
		return;
	}
}


void aux_io_ctrl_set_config(statemachine_modes_t mode){
	/* mode_table[] only covers the real statemachine modes (indices
	 * 0..STATEMACHINE_MODE_RESERVED-1). An out-of-range mode must not be
	 * allowed to index into it and drive the relay/output GPIOs from
	 * whatever garbage follows the table. */
	if (mode >= STATEMACHINE_MODE_RESERVED) {
		return;
	}

	uint8_t cfg = mode_table[mode].aux_io_mask;

	HAL_GPIO_WritePin(OUT_SEL_ISO_GPIO_Port, OUT_SEL_ISO_Pin,
			cfg & GPIO_MASK_OUT_SEL_ISO);
	HAL_GPIO_WritePin(OUT_SEL_HV_GPIO_Port, OUT_SEL_HV_Pin,
			cfg & GPIO_MASK_OUT_SEL_HV);
	HAL_GPIO_WritePin(SHUNT_EN_GPIO_Port, SHUNT_EN_Pin,
			cfg & GPIO_MASK_SHUNT_EN);
	HAL_GPIO_WritePin(SHUNT_ISO_EN_GPIO_Port, SHUNT_ISO_EN_Pin,
			cfg & GPIO_MASK_SHUNT_ISO_EN);
	HAL_GPIO_WritePin(DISCHARGE_N_GPIO_Port, DISCHARGE_N_Pin,
			cfg & GPIO_MASK_DISCHARGE);
}

