/*
 * aux_io_ctrl.h
 *
 *  Created on: May 25, 2025
 *      Author: Andreas
 */

#ifndef SRC_AUX_IO_CTRL_H_
#define SRC_AUX_IO_CTRL_H_

#include "stdint.h"
#include "main.h"
#include "stdio.h"
#include "statemachine.h"

#define GPIO_DISPLAY_WR       0x00
#define GPIO_CONV_CTRL_EN     0x01
#define GPIO_ON_REQ           0x02
#define GPIO_OUT_SEL_ISO      0x03
#define GPIO_BMS_CTRL_WAKEUP  0x04
#define GPIO_SHUNT_EN         0x05
#define GPIO_OUT_SEL_HV       0x06
#define GPIO_OVP_RESET        0x07
#define GPIO_DISCHARGE        0x08
#define GPIO_HV_CTRL_EN       0x09
#define GPIO_SHUNT_ISO_EN     0x0a
#define GPIO_ADC_SYNC_RESET   0x0b

#define GPIO_I2C_ALERT        0x0c
#define GPIO_ADC_DRDY         0x0d
#define GPIO_BUTTON_ESC       0x0e
#define GPIO_UI_PRESENT       0x0f
#define GPIO_BOOT0            0x10
#define GPIO_BUTTON_OUT       0x11
#define GPIO_BUTTON_OK        0x12
#define GPIO_OVP              0x13
#define GPIO_OCP              0x14





void aux_io_ctrl_GPIO_Init(void);

uint8_t aux_io_ctrl_readHW_Revision(void);

/**
 * Function used to manually set the GPIOs from CLI
 * @param pin pin number as defined above
 * @param value value to set the pin (0,1)
 */
void aux_io_ctrl_manual_set_io(uint8_t pin, uint8_t value);

/**
 * Sets the GPIO Config to the specified mode
 * @param mode statemachine mode to be set.
 */
void aux_io_ctrl_set_config(statemachine_modes_t mode);

/**
 * Same as aux_io_ctrl_set_config(), but always drives OUT_SEL_HV low
 * (K1 closed), ignoring GPIO_MASK_OUT_SEL_HV in mode_table[mode].aux_io_mask
 * entirely. Used wherever K1 must not be opened regardless of the target
 * mode's normal relay config -- e.g. a deep-discharge/CUV recovery charge,
 * where opening K1 would cut the MCU's own power (see statemachine.c). All
 * other GPIOs in the mask (OUT_SEL_ISO, SHUNT_EN, SHUNT_ISO_EN, DISCHARGE)
 * are applied normally.
 * @param mode statemachine mode to be set.
 */
void aux_io_ctrl_set_config_keep_k1_closed(statemachine_modes_t mode);


#endif /* SRC_AUX_IO_CTRL_H_ */
