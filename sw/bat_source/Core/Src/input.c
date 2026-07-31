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

typedef struct {
	uint8_t raw_last;
	uint8_t stable_count;
	uint8_t debounced;
} btn_state_t;

static btn_state_t ok_state = { 0, 0, 0 };
static btn_state_t esc_state = { 0, 0, 0 };
static btn_state_t out_state = { 0, 0, 0 };

/*
 * Shared debounce state machine: requires BTN_DEBOUNCE_SAMPLES consecutive
 * identical raw reads before accepting a transition (see the comment above
 * BTN_DEBOUNCE_SAMPLES). Each of input_btn_ok/esc/out()'s hardware paths
 * calls this with its own btn_state_t instance, so the three buttons'
 * debounce histories stay independent despite sharing this one function.
 */
static uint8_t input_debounce(btn_state_t *st, uint8_t raw) {
	if (raw == st->raw_last) {
		if (st->stable_count < BTN_DEBOUNCE_SAMPLES)
			st->stable_count++;
	} else {
		st->raw_last = raw;
		st->stable_count = 1;
	}
	if (st->stable_count >= BTN_DEBOUNCE_SAMPLES)
		st->debounced = raw;
	return st->debounced;
}

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
	return input_debounce(&ok_state, gpio_readBtnOk());
}

uint8_t input_btn_esc(void) {
	if (current_source == INPUT_SOURCE_CLI) {
		uint8_t pulse = cli_pulse_esc;
		cli_pulse_esc = 0;
		return pulse;
	}
	return input_debounce(&esc_state, gpio_readBtnEsc());
}

uint8_t input_btn_out(void) {
	if (current_source == INPUT_SOURCE_CLI)
		return cli_out_held;
	return input_debounce(&out_state, gpio_readBtnOut());
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
