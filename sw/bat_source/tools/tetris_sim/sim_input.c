/*
 * tools/tetris_sim/sim_input.c
 *
 * See sim_input.h. tim_encoder_read() emulates TIM4 exactly as configured in
 * tim.c -- a counter that wraps at 127 with 2 counts per mechanical detent --
 * because that wrap is precisely what input_encoder_read_clamped(), and
 * therefore tetris.c's horizontal movement, has to get right.
 */

#include "sim_input.h"
#include "tim.h"
#include "gpio.h"

static uint8_t encoder_count;      /* 0..127, like TIM4's CNT with ARR = 127 */
static int btn_frames[3];
static int out_latched;

uint16_t tim_encoder_read(void) {
	return encoder_count;
}

void tim_encoder_reset(uint8_t value) {
	encoder_count = (uint8_t) (value & 0x7F);
}

uint8_t gpio_readBtnOk(void) {
	return (uint8_t) (btn_frames[SIM_BTN_OK] > 0);
}

uint8_t gpio_readBtnEsc(void) {
	return (uint8_t) (btn_frames[SIM_BTN_ESC] > 0);
}

uint8_t gpio_readBtnOut(void) {
	return (uint8_t) (out_latched || btn_frames[SIM_BTN_OUT] > 0);
}

void sim_input_encoder_turn(int detents) {
	encoder_count = (uint8_t) ((encoder_count + 2 * detents) & 0x7F);
}

void sim_input_tap(int button, int frames) {
	if (button >= 0 && button < 3 && frames > btn_frames[button])
		btn_frames[button] = frames;
}

void sim_input_toggle_out(void) {
	out_latched = !out_latched;
}

void sim_input_frame_done(void) {
	for (int i = 0; i < 3; i++)
		if (btn_frames[i] > 0)
			btn_frames[i]--;
}
