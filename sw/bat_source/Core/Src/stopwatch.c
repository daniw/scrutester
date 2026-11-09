/*
 * stopwatch.c
 *
 * See stopwatch.h. The voltage timer state is written by the ext ADC ISR
 * (stopwatch_sample()) and read by the main context: the ISR only moves
 * ARMED -> RUNNING -> DONE and writes each field before the state, so main
 * sees complete values whenever it reads the state first.
 *
 *  Created on: Nov 8, 2026
 *      Author: ahorat
 */

#include "stopwatch.h"

static struct {
	uint8_t running;
	uint32_t t0;
	uint32_t acc;
} manual;

static volatile struct {
	stopwatch_vt_state_t state;
	stopwatch_vt_kind_t kind;
	int32_t stop_mV;
	int32_t trigger_mV; // DISCHARGE start level
	uint32_t t_start;
	uint32_t t_stop;
	int32_t v_start;
	int32_t v_stop;
	uint8_t reported;
} vt;

void stopwatch_toggle(uint32_t now_ms) {
	if (manual.running) {
		manual.acc += now_ms - manual.t0;
		manual.running = 0;
	} else {
		manual.t0 = now_ms;
		manual.running = 1;
	}
}

void stopwatch_clear(void) {
	manual.running = 0;
	manual.acc = 0;
}

uint8_t stopwatch_running(void) {
	return manual.running;
}

uint32_t stopwatch_elapsed_ms(uint32_t now_ms) {
	return manual.running ? manual.acc + (now_ms - manual.t0) : manual.acc;
}

uint8_t stopwatch_vt_arm(stopwatch_vt_kind_t kind, int32_t stop_mV,
		int32_t v_now_mV, uint32_t now_ms) {
	if (kind == VT_KIND_DISCHARGE && v_now_mV <= stop_mV)
		return 0;
	vt.state = VT_IDLE; // ISR ignores the fields until the state is set below
	vt.kind = kind;
	vt.stop_mV = stop_mV;
	vt.trigger_mV = v_now_mV / 100 * 95;
	vt.reported = 0;
	if (kind == VT_KIND_IMD) {
		vt.t_start = now_ms;
		vt.v_start = v_now_mV;
		vt.state = VT_RUNNING;
	} else {
		vt.state = VT_ARMED;
	}
	return 1;
}

void stopwatch_vt_cancel(void) {
	vt.state = VT_IDLE;
}

stopwatch_vt_state_t stopwatch_vt_state(void) {
	return vt.state;
}

stopwatch_vt_kind_t stopwatch_vt_kind(void) {
	return vt.kind;
}

uint32_t stopwatch_vt_elapsed_ms(uint32_t now_ms) {
	stopwatch_vt_state_t s = vt.state;

	if (s == VT_DONE)
		return vt.t_stop - vt.t_start;
	return (s == VT_RUNNING) ? now_ms - vt.t_start : 0;
}

int32_t stopwatch_vt_start_mV(void) {
	return vt.v_start;
}

int32_t stopwatch_vt_stop_mV(void) {
	return vt.v_stop;
}

uint8_t stopwatch_vt_take_done(void) {
	if (vt.state != VT_DONE || vt.reported)
		return 0;
	vt.reported = 1;
	return 1;
}

void stopwatch_sample(int32_t v_mV, uint32_t now_ms) {
	if (vt.state == VT_ARMED && v_mV < vt.trigger_mV) {
		vt.t_start = now_ms;
		vt.v_start = v_mV;
		vt.state = VT_RUNNING;
	}
	if (vt.state == VT_RUNNING && v_mV < vt.stop_mV) {
		vt.t_stop = now_ms;
		vt.v_stop = v_mV;
		vt.state = VT_DONE;
	}
}

void stopwatch_reset(void) {
	stopwatch_clear();
	vt.state = VT_IDLE;
}

uint8_t stopwatch_busy(void) {
	return manual.running || vt.state == VT_ARMED || vt.state == VT_RUNNING;
}
