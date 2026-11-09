/*
 * input.h
 *
 * Abstracts the encoder/buttons so statemachine.c can be driven either from
 * real hardware (TIM4 quadrature encoder + GPIO buttons) or from a CLI
 * keyboard-control session (see cli.c), without caring which.
 */

#ifndef INC_INPUT_H_
#define INC_INPUT_H_

#include "stdint.h"

/* Raw encoder count range: 0..INPUT_ENCODER_RANGE-1. Must equal TIM4's
 * ARR+1 (tim.c, htim4.Init.Period = 127, set in CubeMX). */
#define INPUT_ENCODER_RANGE 128

typedef enum {
	INPUT_SOURCE_HW = 0, INPUT_SOURCE_CLI
} input_source_t;

void input_init(void);
void input_set_source(input_source_t src);
input_source_t input_get_source(void);

/* Same semantics as tim_encoder_read()/tim_encoder_reset(): raw 0-127 count,
 * 2 counts per mechanical detent. */
uint16_t input_encoder_read(void);
void input_encoder_reset(uint8_t value);

/* Per-session state for input_encoder_read_clamped(): tracks the last raw
 * count seen so consecutive reads can be turned into a signed step, plus the
 * running clamped position itself. Zero-initialise, or use
 * input_encoder_clamp_reset(). */
typedef struct {
	int32_t position;
	uint16_t prev_raw;
} input_encoder_clamp_t;

/* (Re-)synchronises `clamp` to the encoder's current raw count and sets its
 * tracked position to `value`. Call this once whenever the caller's own
 * frame of reference restarts (e.g. on entering a mode) and, in particular,
 * whenever the raw encoder itself is reset (input_encoder_reset()) -- if the
 * two get out of sync, the first subsequent input_encoder_read_clamped()
 * call sees a large, spurious step. */
void input_encoder_clamp_reset(input_encoder_clamp_t *clamp, int32_t value);

/* Turns the encoder's wrapping raw 0-127 count into a position clamped to
 * [min, max]: reaching either end and continuing to turn holds the position
 * there instead of wrapping to the other end. Used wherever the encoder
 * drives a live, physical setpoint (output voltage/current references,
 * the ISOMETER test-voltage selector) -- unlike the raw count, wrapping
 * there would mean turning the knob past one end snaps the output straight
 * to the other end of its range with no ramp, which is a safety hazard, not
 * just a UI quirk. Wrapping lists use
 * input_encoder_read_wrapped() instead.
 *
 * Correct only if this is called at least once per ~64 raw counts of actual
 * rotation (see the implementation) -- true for a human turning a knob
 * polled every 20 ms, false only if `clamp` is left stale for a long time
 * (e.g. a mode is entered and this isn't called every tick from then on). */
int32_t input_encoder_read_clamped(input_encoder_clamp_t *clamp, int32_t min, int32_t max);

/* Per-list state for input_encoder_read_wrapped(): last raw count seen and
 * the running list position. Set up with input_encoder_wrap_reset(). */
typedef struct {
	int32_t position;
	uint16_t prev_raw;
} input_encoder_wrap_t;

/* Syncs `wrap` to the current raw count and sets its position to `value`
 * (modulo the list length on the next read). Call when a list is entered or
 * redrawn at a given row, and after any input_encoder_reset(). */
void input_encoder_wrap_reset(input_encoder_wrap_t *wrap, int32_t value);

/* List index in [0, n) that wraps last <-> first. Follows the raw count by
 * deltas, so it stays continuous across the raw wrap (128 is no multiple of
 * n) and across a changing `n`. n must be > 0. Same call-rate limit as
 * input_encoder_read_clamped(). */
int32_t input_encoder_read_wrapped(input_encoder_wrap_t *wrap, int32_t n);

/* Same semantics as gpio_readBtnOk/Esc/Out(): level, non-zero while pressed. */
uint8_t input_btn_ok(void);
uint8_t input_btn_esc(void);
uint8_t input_btn_out(void);

/*
 * CLI-simulated backend. Only called from cli.c's keyboard-control-mode byte
 * parser; has no effect while INPUT_SOURCE_HW is active.
 */

/* Moves the simulated encoder by one detent (2 counts): +1 = CW/increment, -1 = CCW/decrement. */
void input_cli_encoder_step(int8_t direction);

/* OK/ESC are momentary keys (Enter/Esc) -> one-shot pulse, consumed by the next input_btn_ok/esc() call. */
void input_cli_pulse_ok(void);
void input_cli_pulse_esc(void);

/* OUT has no key-release event over a terminal, so Space toggles a held level instead of pulsing it. */
void input_cli_toggle_out(void);

#endif /* INC_INPUT_H_ */
