/*
 * LP581X.h
 *
 *  Created on: October 10, 2025
 *      Author: ahorat
 */

#ifndef INC_LP581X_H_
#define INC_LP581X_H_

#include <stdint.h>
#include "i2c.h"

// Local register state
typedef struct {
	uint8_t address;
	uint8_t config1;
	uint8_t channels;
} lp581x_handle;

// LP581X I2C Address
#define LP5814_ADDRESS (0x2C <<1)
#define LP5815_ADDRESS (0x2D <<1)
#define LP5816_ADDRESS (0x2C <<1)
#define LP5817_ADDRESS (0x2D <<1)

// LP581X number of channels
#define LP5814_CHANNELS 4
#define LP5815_CHANNELS 3
#define LP5816_CHANNELS 4
#define LP5817_CHANNELS 3

// LP581X Commands
#define LP581X_REG_CMD_SHUTDOWN 0x0D
#define LP581X_VAL_CMD_SHUTDOWN 0x33
#define LP581X_REG_CMD_RESET    0x0E
#define LP581X_VAL_CMD_RESET    0xCC
#define LP581X_REG_CMD_UPDATE   0x0F
#define LP581X_VAL_CMD_UPDATE   0x55

// LP581X Register Map
#define LP581X_REG_CHIP_EN       0x00
#define LP581X_REG_DEV_CONFIG0   0x01
#define LP581X_REG_DEV_CONFIG1   0x02
#define LP581X_REG_DEV_CONFIG2   0x03
#define LP581X_REG_DEV_CONFIG3   0x04
#define LP581X_REG_FLAG_CLR      0x13
#define LP581X_REG_OUT0_DC       0x14
#define LP581X_REG_OUT1_DC       0x15
#define LP581X_REG_OUT2_DC       0x16
#define LP581X_REG_OUT3_DC       0x17
#define LP581X_REG_OUT0_PWM      0x18
#define LP581X_REG_OUT1_PWM      0x19
#define LP581X_REG_OUT2_PWM      0x1A
#define LP581X_REG_OUT3_PWM      0x1B
#define LP581X_REG_FLAG          0x40

// LP581X Register Flags
#define LP581X_FLAG_CHIP_EN                  0x01

#define LP581X_FLAG_DEV_CONFIG0_25MA5        0x00
#define LP581X_FLAG_DEV_CONFIG0_51MA         0x01

#define LP581X_FLAG_DEV_CONFIG1_OUT0_EN      0x01
#define LP581X_FLAG_DEV_CONFIG1_OUT1_EN      0x02
#define LP581X_FLAG_DEV_CONFIG1_OUT2_EN      0x04
#define LP581X_FLAG_DEV_CONFIG1_OUT3_EN      0x08
#define LP581X_FLAG_DEV_CONFIG1_OUT_EN_ALL   (LP581X_FLAG_DEV_CONFIG1_OUT0_EN | LP581X_FLAG_DEV_CONFIG1_OUT1_EN | LP581X_FLAG_DEV_CONFIG1_OUT2_EN | LP581X_FLAG_DEV_CONFIG1_OUT3_EN)

#define LP581X_FLAG_DEV_CONFIG2_FADE0_EN     0x01
#define LP581X_FLAG_DEV_CONFIG2_FADE1_EN     0x02
#define LP581X_FLAG_DEV_CONFIG2_FADE2_EN     0x04
#define LP581X_FLAG_DEV_CONFIG2_FADE3_EN     0x08
#define LP581X_FLAG_DEV_CONFIG2_FADE_EN_ALL  (LP581X_FLAG_DEV_CONFIG2_FADE0_EN | LP581X_FLAG_DEV_CONFIG2_FADE1_EN | LP581X_FLAG_DEV_CONFIG2_FADE2_EN | LP581X_FLAG_DEV_CONFIG2_FADE3_EN)

#define LP581X_LED_FADE_TIME_MS_TO_REG(x) (x < 1000 ? ((x/50)<<4) : ((x==1000) ? 0xB<<4: x < 8000 ? (0xB+x/2000)<<4: 0xF<<4))
#define LP581X_LED_FADE_TIME_MIN 0
#define LP581X_LED_FADE_TIME_MAX 0xF

#define LP581X_FLAG_DEV_CONFIG3_OUT0_EXP_EN     0x10
#define LP581X_FLAG_DEV_CONFIG3_OUT1_EXP_EN     0x20
#define LP581X_FLAG_DEV_CONFIG3_OUT2_EXP_EN     0x40
#define LP581X_FLAG_DEV_CONFIG3_OUT3_EXP_EN     0x80
#define LP581X_FLAG_DEV_CONFIG3_OUT_EXP_EN_ALL  (LP581X_FLAG_DEV_CONFIG3_OUT0_EXP_EN | LP581X_FLAG_DEV_CONFIG3_OUT1_EXP_EN | LP581X_FLAG_DEV_CONFIG3_OUT2_EXP_EN | LP581X_FLAG_DEV_CONFIG3_OUT3_EXP_EN)

#define LP581X_FLAG_FLAG_TSD_CLR      0x02
#define LP581X_FLAG_FLAG_POR_CLR      0x01
#define LP581X_FLAG_FLAG_TSD      0x02
#define LP581X_FLAG_FLAG_POR      0x01

/**
 * Set direction for all pins at once
 * @param hlp581x lp581x handle to the device. 
 * @param address i2c device address
 */
uint8_t lp581x_init(lp581x_handle *hlp581x, uint8_t address, uint8_t channels);

/**
 * Makes the modified config values take effect.
 * @param hlp581x lp581x handle to the device. 
 */
uint8_t lp581x_updateConfig(lp581x_handle *hlp581x);

/**
 * Reset the LP581X Device
 * @param hlp581x lp581x handle to the device. 
 */
uint8_t lp581x_reset(lp581x_handle *hlp581x);

/**
 * Switches the LP581X device to shutdown mode.
 * @param hlp581x lp581x handle to the device. 
 */
uint8_t lp581x_shutdown(lp581x_handle *hlp581x);

/**
 * Enables specific channel
 * @param hlp581x lp581x handle to the device.
 * @param channel channel number
 */
uint8_t lp581x_enableChannel(lp581x_handle *hlp581x, uint8_t channel);

/**
 * Disables specific channel
 * @param hlp581x lp581x handle to the device.
 * @param channel channel number
 */
uint8_t lp581x_disableChannel(lp581x_handle *hlp581x, uint8_t channel);

/**
 * Set the analog Current value
 * @param hlp581x lp581x handle to the device.
 * @param values pointer to 8bit 4 element array containing all Analog Values
 */
uint8_t lp581x_setAnalogDimming(lp581x_handle *hlp581x, uint8_t* value);

/**
 * Set the analog Current value
 * @param hlp581x lp581x handle to the device. 
 * @param channel channel number 
 * @param value 8-bit analog dimming value
 */
uint8_t lp581x_setAnalogDimmingChannel(lp581x_handle *hlp581x, uint8_t channel, uint8_t value);

/**
 * Set the digital PWM value
 * @param hlp581x lp581x handle to the device. 
 * @param channel channel number 
 * @param value 8-bit pwm dimming value
 */
uint8_t lp581x_setPWMDimmingChannel(lp581x_handle *hlp581x, uint8_t channel, uint8_t value);

/**
 * Set the digital PWM value
 * @param hlp581x lp581x handle to the device.
 * @param values pointer to 8bit 4 element array containing all PWM Values
 */
uint8_t lp581x_setPWMDimming(lp581x_handle *hlp581x, uint8_t* value);

/**
 * Configure fading time and enable for all channels at once. 
 * @param hlp581x lp581x handle to the device. 
 * @param fade_time_ms fade time in ms 
 * @param mask fading enable channel mask 
 */
uint8_t lp581x_configureFading(lp581x_handle *hlp581x, uint16_t fade_time_ms, uint8_t mask);

/**
 * Configure eye-friendly exponential dimming curve for all channels. 
 * @param hlp581x lp581x handle to the device. 
 * @param mask exponential dimming channel mask 
 */
uint8_t lp581x_configureExpDimming(lp581x_handle *hlp581x, uint8_t mask);

/**
 * Sets the maximum output current to 25.5mA
 * @param hlp581x lp581x handle to the device.
 */
uint8_t lp581x_setMaxCurrent25mA5(lp581x_handle *hlp581x);

/**
 * Sets the maximum output current to 51mA
 * @param hlp581x lp581x handle to the device.
 */
uint8_t lp581x_setMaxCurrent51mA(lp581x_handle *hlp581x);

/**
 * Set the maximum current
 * @param hlp581x lp581x handle to the device. 
 */
#define lp581x_setMaxCurrent(handle, value) i2c_write(handle->address, value)

#endif /* INC_LP581X_H_ */
