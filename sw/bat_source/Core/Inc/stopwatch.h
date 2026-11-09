/*
 * stopwatch.h
 *
 * Timing helpers, HAL-free (times are passed in, ms from HAL_GetTick()):
 *  - manual stopwatch (STATEMACHINE_MODE_STOPWATCH)
 *  - voltage-triggered timer in VOLTMETER, fed per ext ADC sample
 *
 *  Created on: Nov 8, 2026
 *      Author: ahorat
 */

#ifndef INC_STOPWATCH_H_
#define INC_STOPWATCH_H_

#include "stdint.h"

/* ---- Manual stopwatch (main context only) ---- */

/** Start if stopped, stop if running. */
void stopwatch_toggle(uint32_t now_ms);
/** Set to 0 (stopped). */
void stopwatch_clear(void);
uint8_t stopwatch_running(void);
uint32_t stopwatch_elapsed_ms(uint32_t now_ms);

/* ---- Voltage-triggered timer ---- */

typedef enum {
	VT_KIND_IMD = 0,   // manual start, stop below the stop level
	VT_KIND_DISCHARGE, // start below 95 % of the arming voltage, stop below the stop level
	VT_KIND_COUNT
} stopwatch_vt_kind_t;

typedef enum {
	VT_IDLE = 0,
	VT_ARMED,
	VT_RUNNING,
	VT_DONE
} stopwatch_vt_state_t;

/**
 * @brief Starts the timer (IMD: running now, DISCHARGE: armed).
 * @param stop_mV Stop level, falling edge.
 * @param v_now_mV Present voltage (DISCHARGE reference).
 * @return 0 if refused (DISCHARGE with v_now <= stop level).
 */
uint8_t stopwatch_vt_arm(stopwatch_vt_kind_t kind, int32_t stop_mV,
		int32_t v_now_mV, uint32_t now_ms);
/** Back to VT_IDLE, from any state. */
void stopwatch_vt_cancel(void);
stopwatch_vt_state_t stopwatch_vt_state(void);
stopwatch_vt_kind_t stopwatch_vt_kind(void);
/** Elapsed time: running -> since start, done -> start to stop, else 0. */
uint32_t stopwatch_vt_elapsed_ms(uint32_t now_ms);
int32_t stopwatch_vt_start_mV(void);
int32_t stopwatch_vt_stop_mV(void);
/** 1 once per finished measurement (to report it). */
uint8_t stopwatch_vt_take_done(void);

/** ISR-safe: call for every new V_TERM sample (mV). */
void stopwatch_sample(int32_t v_mV, uint32_t now_ms);

/** Resets both timers. */
void stopwatch_reset(void);
/** True while a time is running or armed (keeps the auto power-off away). */
uint8_t stopwatch_busy(void);

#endif /* INC_STOPWATCH_H_ */
