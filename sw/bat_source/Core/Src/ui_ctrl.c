/*
 * ui_ctrl.c
 *
 *  Created on: 11.04.2026
 *      Author: Andreas
 */

#include "ui_ctrl.h"
#include "opt3004.h"
#include "lp581x.h"

opt3004_handle hamb;
lp581x_handle hled;
lp581x_handle hbacklight;

#define BACKLIGHT_CURRENT_MIN 20  /* 2.0 mA floor used by ui_ctrl_Dim() */
#define BACKLIGHT_CURRENT_MAX 400 /* 40.0 mA ceiling, matches cmd_setBacklight's cap */
#define BACKLIGHT_LUX_MAX 2000

static uint16_t backlight_current_x0_1mA = BACKLIGHT_CURRENT_MIN;

void ui_ctrl_init(void) {
	/*
	 * LED test setup
	 */
	uint8_t led_pwms[] = { 0xFF, 0xFF, 0xFF};
	uint8_t led_current[] = { 0x01, 0x01, 0x01};
	lp581x_init(&hled, LP5817_ADDRESS, LP5817_CHANNELS);
	lp581x_setPWMDimming(&hled, led_pwms);
	lp581x_setAnalogDimming(&hled, led_current);
	lp581x_setMaxCurrent51mA(&hled);

	uint8_t backlight_pwms[] = { 0xFF, 0xFF, 0xFF, 0xFF };
	uint8_t backlight_current[] = { 0x70, 0x70, 0x70, 0x70};
	lp581x_init(&hbacklight, LP5816_ADDRESS, LP5816_CHANNELS);
	lp581x_setPWMDimming(&hbacklight, backlight_pwms);
	lp581x_setAnalogDimming(&hbacklight, backlight_current);
	lp581x_setMaxCurrent51mA(&hbacklight);

	/*
	 * Ambient light sensor test setup
	 */
	opt3004_init(&hamb, OPT3004_DEVICE_ADDRESS_GND);
}

#define BACKLIGHT_FADE_DIVISOR 5 /* larger = slower/smoother fade */

void ui_ctrl_Dim(void) {
	uint32_t lux = opt3004_readLux(&hamb) / 100; // readLux() returns centi-lux
	uint16_t target; // 0.1mA units, same scale as cli.c's setBacklight command
	int32_t diff, step;
	uint8_t backlight_currents[4];

	// Linear-map ambient lux to backlight current: dim in the dark, full
	// brightness in daylight. Bounds match cmd_setBacklight's 40mA (400) cap.
	if (lux < 10)
		target = BACKLIGHT_CURRENT_MIN;
	else if (lux > BACKLIGHT_LUX_MAX)
		target = BACKLIGHT_CURRENT_MAX;
	else
		target =  (uint16_t) (BACKLIGHT_CURRENT_MIN
				+ (lux * (BACKLIGHT_CURRENT_MAX - BACKLIGHT_CURRENT_MIN)) / BACKLIGHT_LUX_MAX);

	// Fade toward the target instead of jumping straight to it, so ambient
	// light changes (e.g. walking into shadow) ramp the screen smoothly
	// rather than stepping abruptly. Step-of-1 fallback avoids an integer
	// IIR filter's classic dead zone, where a small remaining diff rounds
	// to a 0 step and the value gets stuck short of the target forever.
	diff = (int32_t) target - (int32_t) backlight_current_x0_1mA;
	step = diff / BACKLIGHT_FADE_DIVISOR;
	if (step == 0 && diff != 0)
		step = (diff > 0) ? 1 : -1;
	backlight_current_x0_1mA = (uint16_t) (backlight_current_x0_1mA + step);

	for (uint8_t i = 0; i < 4; i++)
		backlight_currents[i] = (uint8_t) (backlight_current_x0_1mA / 2);
	lp581x_setAnalogDimming(&hbacklight, backlight_currents);
}

uint8_t ui_ctrl_readBacklightPercent(void) {
	if (backlight_current_x0_1mA <= BACKLIGHT_CURRENT_MIN)
		return 0;
	if (backlight_current_x0_1mA >= BACKLIGHT_CURRENT_MAX)
		return 100;
	return (uint8_t) (((uint32_t) (backlight_current_x0_1mA - BACKLIGHT_CURRENT_MIN) * 100)
			/ (BACKLIGHT_CURRENT_MAX - BACKLIGHT_CURRENT_MIN));
}

void ui_ctrl_step(void) {
	// ui_ctrl_Dim() does an I2C transaction on the shared I2C4 bus; called
	// from statemachine_step() (20ms tick), throttle it to roughly once a
	// second rather than every tick.
	static uint8_t tick = 0;

	if (++tick >= 50) {
		tick = 0;
		ui_ctrl_Dim();
	}
}

