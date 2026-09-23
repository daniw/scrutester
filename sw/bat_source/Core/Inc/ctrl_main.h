/*
 * ctrl_main.h
 *
 *  Created on: Jul 30, 2025
 *      Author: ahorat
 */

#ifndef INC_CTRL_MAIN_H_
#define INC_CTRL_MAIN_H_

#include "ctrl_PID_control.h"
#include "adc.h"
#include "statemachine.h"
#include "charge_seq.h"

typedef enum {
	CTRL_MODE_OFF,
	CTRL_MODE_60V,
	CTRL_MODE_10A,
	CTRL_MODE_RESISTANCE_1A,
	CTRL_MODE_RESISTANCE_1mA,
	CTRL_MODE_ISOMETER,
	CTRL_MODE_CHARGE
} ctrl_mode_t;


typedef struct{
	uint16_t poti_reference;
	uint16_t voltage_iso_reference_V;
	uint32_t voltage_reference_mV;
	uint32_t current_reference_mA;
	float ramp;
	ctrl_mode_t mode;
	int32_t duty;
	// One-way latch: set once CHARGE's CC phase reaches end-voltage and
	// switches to CV, cleared only when CHARGE is (re-)started.
	uint8_t charge_cv_phase;

	// CHARGE sub-phase, see charge_seq.h. Written by the control ISR
	// (PRECHARGE -> CLOSE -> ... CC_RAMP -> CV) and, while the ISR is parked
	// in CLOSE, by ctrl_main_charge_start_ramp() from main context. Like
	// `mode`, whoever changes it assigns it LAST, after everything the new
	// phase needs is set up.
	volatile charge_phase_t charge_phase;
	charge_precharge_t precharge;   // ISR-owned while charge_phase == PRECHARGE

	// Peak |I_OUT| seen by the ISR from the relay-close command for
	// CTRL_PARAM_CHARGE_PEAK_WINDOW_s, printed once as a bench tuning aid.
	int32_t charge_i_peak_mA;
	uint32_t charge_peak_ticks;

} ctrl_main_t;

/**
 * One entry per tunable PID control loop: the live controller instance, the
 * EEPROM-backed calibration fields it's saved to/loaded from, its default
 * gains and (fixed, non-tunable) duty saturation limits, and a CLI-facing
 * name. ctrl_main_init() and the CLI's setPI/saveEEPROM/readEEPROM commands
 * all iterate ctrl_pid_table instead of hand-listing each controller, so
 * adding a new tunable loop only means adding one row here.
 */
typedef struct {
	PID_controller_t *ctrl;
	float *cal_p;
	float *cal_i;
	float default_p;
	float default_i;
	float sat_high;
	float sat_low;
	const char *name;
} ctrl_pid_entry_t;

#define CTRL_PID_TABLE_LEN 8
extern const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN];

void ctrl_main_init(void);
void ctrl_main_start_ctrl(ctrl_mode_t mode);
/* Deep-discharge/CUV recovery variant of ctrl_main_start_ctrl(CTRL_MODE_CHARGE):
 * V_IN reads ~0 (BMS DSG FET open), so there is nothing to boost from --
 * skips PRECHARGE entirely and starts directly in CC_RAMP, at
 * CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA rather than the normal full
 * charge current. K1 must already be closed before this is called (see
 * statemachine_enter_charge_low_current() in statemachine.c) -- this
 * function does not touch any relay GPIO itself. */
void ctrl_main_start_ctrl_charge_low_current(void);
void ctrl_main_ctrl(const ADC_CONVERTED_DATA *meas);
void ctrl_main_stop_control(void);
void ctrl_main_apply_reference(ctrl_mode_t mode, uint16_t reference_poti_count);
ctrl_mode_t statemachine_mode_to_ctrl_mode(statemachine_modes_t mode);

/* CHARGE phase hand-offs, main context only (see charge_seq.h). Both require
 * the ISR to be parked in CHG_PHASE_CLOSE and do nothing otherwise. */

/* Called right after the output relay was commanded closed: steps the SEK
 * duty to the zero-current duty for the present V_in/V_term and preloads the
 * charge-current PI so the current starts at ~0. */
void ctrl_main_charge_handover(int32_t v_in_mV, int32_t v_term_mV);
/* Called once the relay has settled: the current reference starts ramping
 * from 0 A. */
void ctrl_main_charge_start_ramp(void);

/* Present charge-current reference in mA (for the stuck-open check). */
int32_t ctrl_main_charge_reference_mA(void);
/* Returns 1 exactly once per charge start, when the peak-current window after
 * the relay-close command has elapsed, and stores the peak in *peak_mA. */
uint8_t ctrl_main_charge_take_peak(int32_t *peak_mA);


#endif /* INC_CTRL_MAIN_H_ */
