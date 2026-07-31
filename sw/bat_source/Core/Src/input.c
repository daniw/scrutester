/*
 * input.c
 *
 * See input.h. Two backends behind one interface: real hardware (tim.h/gpio.h)
 * and a CLI-simulated state driven by cli.c's keyboard-control-mode parser.
 */

#include "input.h"
#include "tim.h"
#include "gpio.h"

static input_source_t current_source = INPUT_SOURCE_HW;

static uint16_t cli_encoder_count = 0;
static uint8_t cli_pulse_ok = 0;
static uint8_t cli_pulse_esc = 0;
static uint8_t cli_out_held = 0;

// Hardware buttons are read once per statemachine tick (20ms) with no
// debounce below this layer. Requiring BTN_DEBOUNCE_SAMPLES consecutive
// identical raw reads before accepting a transition rejects contact bounce
// while adding only ~BTN_DEBOUNCE_SAMPLES*20ms of latency - well under a
// normal tap. CLI-simulated input never reaches this (returns earlier).
#define BTN_DEBOUNCE_SAMPLES 2

static uint8_t ok_raw_last = 0, ok_stable_count = 0, ok_debounced = 0;
static uint8_t esc_raw_last = 0, esc_stable_count = 0, esc_debounced = 0;
static uint8_t out_raw_last = 0, out_stable_count = 0, out_debounced = 0;

void input_init(void) {
	current_source = INPUT_SOURCE_HW;
	cli_encoder_count = 0;
	cli_pulse_ok = 0;
	cli_pulse_esc = 0;
	cli_out_held = 0;
}

void input_set_source(input_source_t src) {
	/* Carry the encoder position across the switch so navigation doesn't jump. */
	if (src == INPUT_SOURCE_CLI)
		cli_encoder_count = tim_encoder_read();
	else
		tim_encoder_reset((uint8_t) cli_encoder_count);
	current_source = src;
}

input_source_t input_get_source(void) {
	return current_source;
}

uint16_t input_encoder_read(void) {
	if (current_source == INPUT_SOURCE_CLI)
		return cli_encoder_count;
	return tim_encoder_read();
}

void input_encoder_reset(uint8_t value) {
	if (current_source == INPUT_SOURCE_CLI)
		cli_encoder_count = value;
	else
		tim_encoder_reset(value);
}

uint8_t input_btn_ok(void) {
	if (current_source == INPUT_SOURCE_CLI) {
		uint8_t pulse = cli_pulse_ok;
		cli_pulse_ok = 0;
		return pulse;
	}
	uint8_t raw = gpio_readBtnOk();
	if (raw == ok_raw_last) {
		if (ok_stable_count < BTN_DEBOUNCE_SAMPLES)
			ok_stable_count++;
	} else {
		ok_raw_last = raw;
		ok_stable_count = 1;
	}
	if (ok_stable_count >= BTN_DEBOUNCE_SAMPLES)
		ok_debounced = raw;
	return ok_debounced;
}

uint8_t input_btn_esc(void) {
	if (current_source == INPUT_SOURCE_CLI) {
		uint8_t pulse = cli_pulse_esc;
		cli_pulse_esc = 0;
		return pulse;
	}
	uint8_t raw = gpio_readBtnEsc();
	if (raw == esc_raw_last) {
		if (esc_stable_count < BTN_DEBOUNCE_SAMPLES)
			esc_stable_count++;
	} else {
		esc_raw_last = raw;
		esc_stable_count = 1;
	}
	if (esc_stable_count >= BTN_DEBOUNCE_SAMPLES)
		esc_debounced = raw;
	return esc_debounced;
}

uint8_t input_btn_out(void) {
	if (current_source == INPUT_SOURCE_CLI)
		return cli_out_held;
	uint8_t raw = gpio_readBtnOut();
	if (raw == out_raw_last) {
		if (out_stable_count < BTN_DEBOUNCE_SAMPLES)
			out_stable_count++;
	} else {
		out_raw_last = raw;
		out_stable_count = 1;
	}
	if (out_stable_count >= BTN_DEBOUNCE_SAMPLES)
		out_debounced = raw;
	return out_debounced;
}

void input_cli_encoder_step(int8_t direction) {
	if (direction > 0)
		cli_encoder_count = (cli_encoder_count + 2) & 0x7F;
	else
		cli_encoder_count = (cli_encoder_count - 2) & 0x7F;
}

void input_cli_pulse_ok(void) {
	cli_pulse_ok = 1;
}

void input_cli_pulse_esc(void) {
	cli_pulse_esc = 1;
}

void input_cli_toggle_out(void) {
	cli_out_held = !cli_out_held;
}
