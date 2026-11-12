/*
 * ctrl_main.c
 *
 *  Created on: Jul 30, 2025
 *      Author: ahorat
 */

#include "ctrl_main.h"
#include "ctrl_param.h"
#include "ctrl_PID_control.h"
#include "adc.h"
#include "hrtim.h"
#include <stdio.h>
#include "aux_io_ctrl.h"
#include "config_store.h"
#include "bq76905.h"
#include "mode_table.h"
#include "vout_region.h"

extern BQ76905_handle bms;
extern ADC_MEAS_DATA adc_data;

// Main-context only (ctrl_main_charge_handover()/_take_peak()).
static uint8_t charge_peak_reported;

ctrl_main_t ctrl_main_handle;
PID_controller_t ctrl_pi_voltage_buck;
PID_controller_t ctrl_pi_voltage_boost;
PID_controller_t ctrl_pi_current;
PID_controller_t ctrl_pi_boost_iout_limit;
PID_controller_t ctrl_pi_charge_current;
PID_controller_t ctrl_pi_charge_voltage;
PID_controller_t ctrl_pi_voltage_hv;
PID_controller_t ctrl_pi_hv_iout_limit;
PID_controller_t ctrl_pi_voltage_4sw;
PID_controller_t ctrl_pi_10a_vlimit;

_Static_assert(CTRL_PARAM_CHARGE_CV_RATE_HZ * STATEMACHINE_STEP_PERIOD_mS == 1000,
		"CV loop runs at the statemachine tick");

// Full-range 60V mode state, ISR-owned (ctrl_voltage_wide()).
static vout_region_t vout_region;
static float vout_duty_prim; // last commanded, used to hand over between regions
static float vout_duty_sek;
// Bench diagnostics, printed by printADC.
static volatile uint32_t vout_region_changes;
static volatile float vout_last_ratio;
static float vout_vin_filt_mV;        // 0 = not yet seeded
static uint16_t vout_region_hold;     // ticks before the next region change
// Region change in steps (vout_transition_step()), hardware state as written.
static vout_region_t vout_trans_from;
static unsigned vout_trans_step;
static unsigned vout_burst_leg;       // VOUT_LEG_*, 0 = burst off
static uint32_t vout_freq_prim;
static uint32_t vout_freq_sek;

static const uint16_t ctrl_main_iso_values[4] = { 125, 250, 500, 1000 };

// CLI setPI IDs are positional (row index), see `help setPI`.
const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN] = { {
		&ctrl_pi_voltage_buck, &config_store.gains.voltage_buck_p,
		&config_store.gains.voltage_buck_i,
		CTRL_PARAM_VOLTAGE_BUCK_P, CTRL_PARAM_VOLTAGE_BUCK_I,
		CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_HIGH,
		CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_LOW, "Buck", CTRL_FREQ }, {
		&ctrl_pi_voltage_boost, &config_store.gains.voltage_boost_p,
		&config_store.gains.voltage_boost_i,
		CTRL_PARAM_VOLTAGE_BOOST_P, CTRL_PARAM_VOLTAGE_BOOST_I,
		CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH,
		CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW, "Boost", CTRL_FREQ }, { &ctrl_pi_current,
		&config_store.gains.current_p,
		&config_store.gains.current_i,
		CTRL_PARAM_CURRENT_P, CTRL_PARAM_CURRENT_I,
		CTRL_PARAM_CURRENT_DUTY_SAT_HIGH, CTRL_PARAM_CURRENT_DUTY_SAT_LOW,
		"Current", CTRL_FREQ }, { &ctrl_pi_boost_iout_limit,
		&config_store.gains.boost_iout_limit_p,
		&config_store.gains.boost_iout_limit_i,
		CTRL_PARAM_BOOST_IOUT_LIMIT_P, CTRL_PARAM_BOOST_IOUT_LIMIT_I,
		CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH,
		CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW, "BoostIoutLimit", CTRL_FREQ }, {
		&ctrl_pi_charge_current, &config_store.gains.charge_current_p,
		&config_store.gains.charge_current_i,
		CTRL_PARAM_CHARGE_CURRENT_P, CTRL_PARAM_CHARGE_CURRENT_I,
		CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH,
		CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_LOW, "ChargeCurrent", CTRL_FREQ }, {
		&ctrl_pi_charge_voltage, &config_store.gains.charge_voltage_p, &config_store.gains.charge_voltage_i,
		CTRL_PARAM_CHARGE_VOLTAGE_P, CTRL_PARAM_CHARGE_VOLTAGE_I,
		CTRL_PARAM_CHARGE_CURRENT_mA / 1000.0F, 0.0F, "ChargeVoltage", CTRL_PARAM_CHARGE_CV_RATE_HZ }, {
		&ctrl_pi_voltage_hv, &config_store.gains.voltage_hv_p,
		&config_store.gains.voltage_hv_i,
		CTRL_PARAM_HV_VOLTAGE_P, CTRL_PARAM_HV_VOLTAGE_I,
		CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_HIGH, CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_LOW,
		"HVVoltage", CTRL_FREQ }, { &ctrl_pi_hv_iout_limit,
		&config_store.gains.flyback_current_p,
		&config_store.gains.flyback_current_i,
		CTRL_PARAM_HV_CURRENT_P, CTRL_PARAM_HV_CURRENT_I,
		CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_HIGH, CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_LOW,
		"HVCurrent", CTRL_FREQ }, { &ctrl_pi_voltage_4sw, &config_store.gains.voltage_4sw_p,
		&config_store.gains.voltage_4sw_i,
		CTRL_PARAM_VOLTAGE_4SW_P, CTRL_PARAM_VOLTAGE_4SW_I,
		CTRL_PARAM_VOLTAGE_4SW_DUTY_SAT_HIGH,
		CTRL_PARAM_VOLTAGE_4SW_DUTY_SAT_LOW, "FourSwitch", CTRL_FREQ }, {
		&ctrl_pi_10a_vlimit, &config_store.gains.vlimit_10a_p, &config_store.gains.vlimit_10a_i,
		CTRL_PARAM_10A_VLIMIT_P, CTRL_PARAM_10A_VLIMIT_I,
		CTRL_PARAM_CURRENT_DUTY_SAT_HIGH, CTRL_PARAM_CURRENT_DUTY_SAT_LOW,
		"10AVoltLimit", CTRL_FREQ }, };

/******************* Function Prototypes **************************/
static void ctrl_voltage_buck(int32_t voltage_meas_mV,
		int32_t voltage_meas_accurate);
static void ctrl_current(int16_t current_meas_mA,
		int16_t current_meas_accurate, int32_t voltage_meas_mV);
static void ctrl_charge_current(uint32_t target_mA,
		int16_t current_meas_mA, int16_t current_bms_mA);
static void ctrl_voltage_hv(int32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV, int32_t current_meas_iso_uA);
static void ctrl_charge(const ADC_CONVERTED_DATA *meas);
static void ctrl_voltage_wide(const ADC_CONVERTED_DATA *meas);

/* Start a limiter at its ceiling: from 0 it holds the duty back. */
static inline void ctrl_limiter_arm(PID_controller_t *lim) {
	lim->prev_I_action = lim->sat_limit_high;
}

/* Cap `duty` at the limiter output. Anti-windup both ways: the loop not in
 * control tracks the applied duty (the limiter stays just above it and takes
 * over at once when the limit is reached). */
static inline float ctrl_limited_duty(PID_controller_t *pi,
		PID_controller_t *lim, float duty) {
	if (lim->action < duty) {
		duty = lim->action;
		pi->prev_I_action = duty - pi->P_action;
	} else {
		lim->prev_I_action = duty - lim->P_action;
	}
	return duty;
}

/**
 * Initializes the controllers
 */
void ctrl_main_init(void) {

	// P/I gains come from config_store, which is loaded from EEPROM (or
	// defaulted to the CTRL_PARAM_* constants) before ctrl_main_init() runs -
	// see config_store_init() in main(). Duty saturation limits stay as
	// hardcoded safety limits rather than field-tunable values.
	for (int i = 0; i < CTRL_PID_TABLE_LEN; i++) {
		const ctrl_pid_entry_t *e = &ctrl_pid_table[i];
		ctrl_PID_controller_init(e->ctrl, *e->cal_p, *e->cal_i, e->sat_high,
				e->sat_low);
	}

	ctrl_pi_boost_iout_limit.ref = CTRL_PARAM_BOOST_IOUT_LIMIT_mA / 1000.0F;
	ctrl_pi_hv_iout_limit.ref = CTRL_PARAM_HV_IOUT_LIMIT_uA / 1000.0F;
	ctrl_pi_10a_vlimit.ref = CTRL_PARAM_10A_COMPLIANCE_mV / 1000.0F;
}

/* LV start of the full-range voltage loop (60V, EMI). */
static void ctrl_start_voltage_wide(void) {
	ctrl_PID_reset(&ctrl_pi_voltage_buck);
	ctrl_PID_reset(&ctrl_pi_voltage_4sw);
	ctrl_PID_reset(&ctrl_pi_voltage_boost);
	ctrl_PID_reset(&ctrl_pi_boost_iout_limit);
	// From 0 the limiter held the duty back while the PI wound up (5 V -> ~9 V).
	ctrl_limiter_arm(&ctrl_pi_boost_iout_limit);
	// Start as buck from 0 duty; the ramped reference walks the regions.
	vout_region = VOUT_REGION_BUCK;
	vout_vin_filt_mV = 0.0F;
	vout_region_hold = 0;
	vout_trans_from = VOUT_REGION_BUCK;
	vout_trans_step = 2;
	vout_burst_leg = VOUT_LEG_PRIM;
	vout_freq_prim = CTRL_PARAM_SW_FREQ_HIGH;
	vout_freq_sek = CTRL_PARAM_SW_FREQ_LOW;
	vout_duty_prim = 0.0F;
	vout_duty_sek = CTRL_PARAM_CONST_DUTY_LOW;
	hrtim_set_prim_sek(CTRL_PARAM_SW_FREQ_HIGH, vout_duty_prim,
	CTRL_PARAM_SW_FREQ_LOW, vout_duty_sek);
	// Parked-leg sync before the outputs are enabled (hrtim.c).
	hrtim_burst_set_idle_cycles(CTRL_PARAM_VOUT_SYNC_CYCLES);
	hrtim_sync_start(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_SEK);
}

/* Resets the charge loops and peak tracking (both charge starts). */
static void ctrl_charge_reset(void) {
	ctrl_PID_reset(&ctrl_pi_voltage_buck);
	ctrl_PID_reset(&ctrl_pi_charge_current);
	ctrl_PID_reset(&ctrl_pi_charge_voltage);
	ctrl_main_handle.charge_cv_phase = 0;
	ctrl_main_handle.charge_i_peak_mA = 0;
	ctrl_main_handle.charge_peak_ticks = 0;
}

/* CHARGE start: open-loop PRECHARGE boost (relay open, OUT_LV not
 * measurable), the ISR ramps PRIM/SEK from here. */
static void ctrl_start_charge(void) {
	ctrl_main_apply_reference(CTRL_MODE_CHARGE, 0);
	ctrl_charge_reset();
	// The CC loop is NOT preloaded here: a preload with
	// CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH would write SEK duty ~0, i.e.
	// the maximum-current end. It is preloaded with the zero-current
	// duty at the CLOSE -> CC_RAMP hand-over instead
	// (ctrl_main_charge_handover()).
	float prim_duty, sek_duty;
	charge_precharge_start(&ctrl_main_handle.precharge,
			adc_data.converted.v_term_ext_mv, &prim_duty, &sek_duty);
	hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_LOW);
	hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_HIGH);
	hrtim_set_duty(HRTIM_CHANNEL_PRIM, prim_duty);
	hrtim_set_duty(HRTIM_CHANNEL_SEK, sek_duty);
	hrtim_enable(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_SEK);
	// Assigned before `mode` (caller), so the ISR never sees CTRL_MODE_CHARGE
	// with a stale phase.
	ctrl_main_handle.charge_phase = CHG_PHASE_PRECHARGE;
}

/* 10A / RESISTANCE start: buck current loop with voltage limiter. */
static void ctrl_start_current(void) {
	ctrl_PID_reset(&ctrl_pi_voltage_buck);
	ctrl_PID_reset(&ctrl_pi_current);
	ctrl_PID_reset(&ctrl_pi_10a_vlimit);
	ctrl_limiter_arm(&ctrl_pi_10a_vlimit);
	hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_HIGH);
	hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_LOW);
	hrtim_set_duty(HRTIM_CHANNEL_SEK, CTRL_PARAM_CONST_DUTY_LOW); // Needs to be low!!
	hrtim_set_duty(HRTIM_CHANNEL_PRIM, 0);
	// Parked-SEK sync and pulse skipping, else a shorted output sits on
	// the minimum pulse (>= 2.4 A).
	hrtim_burst_set_idle_cycles(CTRL_PARAM_VOUT_SYNC_CYCLES);
	hrtim_sync_start(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_SEK);
}

/* ISOMETER start: HV flyback voltage loop with current limiter. */
static void ctrl_start_isometer(void) {
	ctrl_PID_reset(&ctrl_pi_voltage_hv);
	ctrl_PID_reset(&ctrl_pi_hv_iout_limit);
	ctrl_limiter_arm(&ctrl_pi_hv_iout_limit);
	hrtim_set_freq(HRTIM_CHANNEL_HV, CTRL_PARAM_SW_FREQ_HV);
	hrtim_set_duty(HRTIM_CHANNEL_HV, 0);
	hrtim_enable(HRTIM_CHANNEL_HV);
}

void ctrl_main_start_ctrl(ctrl_mode_t mode) {
	switch (mode) {
	case CTRL_MODE_CHARGE:
		ctrl_start_charge();
		break;
	case CTRL_MODE_60V:
		ctrl_start_voltage_wide();
		break;
	case CTRL_MODE_RESISTANCE_1A:
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_apply_reference(mode, 0);
		/* fall through */
	case CTRL_MODE_10A:
		ctrl_start_current();
		break;
	case CTRL_MODE_ISOMETER:
		ctrl_start_isometer();
		break;
#ifdef EMI_DEBUG
	case CTRL_MODE_EMI:
		// LV side as in 60V; HV flyback open loop at a constant duty.
		ctrl_start_voltage_wide();
		hrtim_set_freq(HRTIM_CHANNEL_HV, CTRL_PARAM_SW_FREQ_HV);
		hrtim_set_duty(HRTIM_CHANNEL_HV, CTRL_PARAM_EMI_HV_DUTY);
		hrtim_enable(HRTIM_CHANNEL_HV);
		break;
#endif
	case CTRL_MODE_OFF:
	default:
		break;
	}
	ctrl_main_handle.ramp = 0.2F;
	__COMPILER_BARRIER(); // setup stores before the hand-over store
	ctrl_main_handle.mode = mode;
}

/* See ctrl_main.h. Mirrors CTRL_MODE_CHARGE above but skips PRECHARGE: V_IN
 * is ~0 (BMS DSG FET open, see statemachine.c), so there is nothing to boost
 * from, and K1 is already closed (kept that way by the caller), so there is
 * no relay to wait for either -- starts directly in CC_RAMP. */
void ctrl_main_start_ctrl_charge_low_current(void) {
	ctrl_charge_reset();
	ctrl_main_handle.current_reference_mA =
			CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA;
	ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_CHARGE_END_VOLTAGE_mV;

	hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_LOW);
	hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_HIGH);
	// PRIM jumps straight to its normal pass-through duty -- no soft-start
	// ramp (V_IN gives no usable feedback to ramp against anyway).
	hrtim_set_duty(HRTIM_CHANNEL_PRIM, CTRL_PARAM_CONST_DUTY_HIGH);
	// Same zero-current preload ctrl_main_charge_handover() uses at the
	// normal CLOSE -> CC_RAMP hand-over, so the first CC tick doesn't glitch.
	// v_in_mV is passed as 0 rather than read live: it is not meaningful
	// while the DSG FET is open, and charge_seq_zero_current_duty() already
	// treats a non-positive v_term_mV as "least-current duty", so this is
	// the same safe-minimum starting point either way.
	float d0 = charge_seq_zero_current_duty(0,
			adc_data.converted.v_term_ext_mv);
	ctrl_pi_charge_current.ref = 0.0F;
	ctrl_pi_charge_current.prev_I_action = charge_seq_cc_preload_action(d0);
	hrtim_set_duty(HRTIM_CHANNEL_SEK, d0);
	ctrl_main_handle.duty = d0 * 1000;
	hrtim_enable(HRTIM_CHANNEL_PRIM);
	hrtim_enable(HRTIM_CHANNEL_SEK);

	ctrl_main_handle.ramp = 0.0F; // ISR ramps current 0 -> the reduced target
	// Assigned before `mode` (below), so the ISR never sees CTRL_MODE_CHARGE
	// with a stale phase (same ordering rule as ctrl_main_start_ctrl()).
	__COMPILER_BARRIER(); // setup stores before the hand-over store
	ctrl_main_handle.charge_phase = CHG_PHASE_CC_RAMP;
	ctrl_main_handle.mode = CTRL_MODE_CHARGE;
}

/*
 * The order of the two statements below is load-bearing, as is the fact that
 * ctrl_main_start_ctrl() assigns ctrl_main_handle.mode last.
 *
 * Both functions run in main context from statemachine_step() (TIM2 tick,
 * IRQ_PRIO_AUX), while ctrl_main_ctrl() runs from the ADC control-loop
 * interrupt at IRQ_PRIO_CTRL_LOOP -- which is higher, so it can preempt them
 * part-way through. Clearing mode first here means a preempting control-loop
 * tick sees CTRL_MODE_OFF and writes no duty, rather than driving a converter
 * whose PWM is about to be disabled underneath it. Setting mode last in
 * start_ctrl() is the mirror image: the loop only starts acting once the
 * frequencies, duties and PID state are fully set up.
 *
 * TIM2 and the ADC DMA interrupt have different priorities and can preempt
 * each other, so this ordering is what keeps the two contexts consistent --
 * do not reorder either function's tail without re-checking that.
 */
void ctrl_main_stop_control(void) {
	ctrl_main_handle.mode = CTRL_MODE_OFF;
	ctrl_main_handle.charge_phase = CHG_PHASE_OFF;
	// Disabled PRIM/SEK idle low side ON (IDLES1): CONV_CTRL_EN must be off.
	hrtim_sync_stop();
	hrtim_disable(HRTIM_CHANNEL_ALL);
}

/**
 * Main control task
 */
/*
 * Takes the converted measurements directly rather than the whole
 * ADC_MEAS_DATA. The caller (HAL_ADC_ConvCpltCallback() in adc.c) hands over
 * an ISR-private set of freshly converted values whose enclosing struct has
 * no populated raw/calibration members, so narrowing the parameter here makes
 * reaching for one a compile error instead of a silent read of zeros in the
 * control loop.
 */
void ctrl_main_ctrl(const ADC_CONVERTED_DATA *meas) {
	switch (ctrl_main_handle.mode) {

	case CTRL_MODE_60V:
#ifdef EMI_DEBUG
	case CTRL_MODE_EMI: // HV runs open loop
#endif
		ctrl_voltage_wide(meas);
		break;
	case CTRL_MODE_RESISTANCE_1A:
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_voltage_buck(meas->v_out, meas->v_out);
		break;
	case CTRL_MODE_10A:
		ctrl_current(meas->i_out, meas->i_out_ext_mA, meas->v_out);

		break;
	case CTRL_MODE_ISOMETER:
		ctrl_voltage_hv(meas->v_hv, meas->v_term_ext_mv,
				meas->i_iso_ext_uA);
		break;
	case CTRL_MODE_CHARGE:
		ctrl_charge(meas);
		break;

	case CTRL_MODE_OFF:
	default:
		break;
	}

}

/*
 * CHARGE, per phase (see charge_seq.h). Runs in the control ISR.
 *
 * PRECHARGE is fully open-loop and driven from here; CLOSE only holds the
 * duties (the state machine is switching the output relay and, on the
 * hand-over, writes the SEK duty and the PI state itself -- which is safe
 * because this ISR touches none of them in CLOSE); CC_RAMP and CV are the
 * closed loops.
 */
static void ctrl_charge(const ADC_CONVERTED_DATA *meas) {
	switch (ctrl_main_handle.charge_phase) {

	case CHG_PHASE_PRECHARGE: {
		float prim_duty, sek_duty;
		uint8_t done = charge_precharge_step(&ctrl_main_handle.precharge,
				meas->v_in, &prim_duty, &sek_duty);
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, prim_duty);
		hrtim_set_duty(HRTIM_CHANNEL_SEK, sek_duty);
		ctrl_main_handle.duty = sek_duty * 1000;
		if (done)
			ctrl_main_handle.charge_phase = CHG_PHASE_CLOSE;
		break;
	}

	case CHG_PHASE_CLOSE:
	case CHG_PHASE_CC_RAMP:
	case CHG_PHASE_CV: {
		// Peak |I_OUT| in the window after the relay-close command. The
		// window only starts counting once the state machine has issued the
		// command (ctrl_main_charge_handover() zeroes the counter), so the
		// PRECHARGE -> CLOSE wait for that command is not part of it.
		if (ctrl_main_handle.charge_peak_ticks
				< (uint32_t) (CTRL_PARAM_CHARGE_PEAK_WINDOW_s * CTRL_FREQ)) {
			int32_t a = meas->i_out < 0 ? -meas->i_out : meas->i_out;
			if (a > ctrl_main_handle.charge_i_peak_mA)
				ctrl_main_handle.charge_i_peak_mA = a;
			ctrl_main_handle.charge_peak_ticks++;
		}

		if (ctrl_main_handle.charge_phase == CHG_PHASE_CLOSE)
			break;

		uint16_t stack_mV = bms.VoltageRegisters.StackVoltage;
		uint8_t stack_valid =
				(stack_mV >= CTRL_PARAM_STACK_VOLTAGE_MIN_VALID_mV)
						&& (stack_mV <= CTRL_PARAM_STACK_VOLTAGE_MAX_VALID_mV);

		if (ctrl_main_handle.charge_phase == CHG_PHASE_CC_RAMP&& stack_valid
		&& stack_mV >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV) {
			// Hand the CV loop (main context from here on) the present
			// current, so the switch-over has no step.
			float ref_A = ctrl_pi_charge_current.ref;
			ctrl_main_handle.charge_cv_current_mA = (uint32_t) (ref_A * 1000.0F);
			ctrl_pi_charge_voltage.prev_I_action = ref_A;
			ctrl_main_handle.charge_cv_phase = 1;
			ctrl_main_handle.charge_phase = CHG_PHASE_CV;
		}

		// The CC loop runs in CV too, its target set by the CV loop.
		ctrl_charge_current(
				ctrl_main_handle.charge_phase == CHG_PHASE_CV ?
						ctrl_main_handle.charge_cv_current_mA :
						ctrl_main_handle.current_reference_mA,
				-meas->i_out, bms.CurrentRegisters.CC2Current);
		break;
	}

	case CHG_PHASE_OFF:
	default:
		break;
	}
}

void ctrl_main_charge_handover(int32_t v_in_mV, int32_t v_term_mV) {
	if (ctrl_main_handle.charge_phase != CHG_PHASE_CLOSE)
		return;

	float d0 = charge_seq_zero_current_duty(v_in_mV, v_term_mV);
	ctrl_PID_reset(&ctrl_pi_charge_current);
	ctrl_pi_charge_current.ref = 0.0F;
	// D = CONST_DUTY_HIGH - action is what ctrl_apply_inverted_sek_duty()
	// writes, so this makes the first CC tick write d0 again.
	ctrl_pi_charge_current.prev_I_action = charge_seq_cc_preload_action(d0);
	hrtim_set_duty(HRTIM_CHANNEL_SEK, d0);
	ctrl_main_handle.duty = d0 * 1000;

	ctrl_main_handle.charge_i_peak_mA = 0;
	ctrl_main_handle.charge_peak_ticks = 0;
	charge_peak_reported = 0;
}

/* See ctrl_main.h. */
void ctrl_main_vout_diag(uint8_t *region, uint32_t *changes, float *ratio) {
	*region = (uint8_t) vout_region;
	*changes = vout_region_changes;
	*ratio = vout_last_ratio;
}

void ctrl_main_charge_start_ramp(void) {
	if (ctrl_main_handle.charge_phase != CHG_PHASE_CLOSE)
		return;

	// ctrl_charge_current() ramps the reference with
	// ctrl_apply_ramped_ref(), which starts from whatever `ramp` holds;
	// start_ctrl() leaves it at 0.2 for the other modes' soft start.
	ctrl_main_handle.ramp = 0.0F;
	__COMPILER_BARRIER(); // setup stores before the hand-over store
	ctrl_main_handle.charge_phase = CHG_PHASE_CC_RAMP; // last: the ISR acts on it
}

int32_t ctrl_main_charge_reference_mA(void) {
	return (int32_t) (ctrl_pi_charge_current.ref * 1000.0F);
}

uint8_t ctrl_main_charge_take_peak(int32_t *peak_mA) {
	if (charge_peak_reported || ctrl_main_handle.charge_phase == CHG_PHASE_OFF
			|| ctrl_main_handle.charge_phase == CHG_PHASE_PRECHARGE
			|| ctrl_main_handle.charge_peak_ticks
					< (uint32_t) (CTRL_PARAM_CHARGE_PEAK_WINDOW_s * CTRL_FREQ))
		return 0;
	*peak_mA = ctrl_main_handle.charge_i_peak_mA;
	charge_peak_reported = 1;
	return 1;
}

/**
 * @brief Highest encoder count (poti count) valid for a mode.
 * @param mode Control mode
 */
uint16_t ctrl_main_poti_max(ctrl_mode_t mode) {
	switch (mode) {
	case CTRL_MODE_60V:
		return CTRL_PARAM_60V_VOLTAGE_MAX_mV / CTRL_MAIN_MV_PER_COUNT;
	case CTRL_MODE_10A:
		return CTRL_PARAM_10A_CURRENT_MAX_mA / CTRL_MAIN_MA_PER_COUNT;
	case CTRL_MODE_ISOMETER:
		return sizeof(ctrl_main_iso_values) / sizeof(ctrl_main_iso_values[0]) - 1;
#ifdef EMI_DEBUG
	case CTRL_MODE_EMI:
		return CTRL_PARAM_EMI_VOLTAGE_MAX_mV / CTRL_MAIN_MV_PER_COUNT;
#endif
	default:
		return 127;
	}
}

/**
 * Computes the reference value for the given (target) mode from the poti
 * count. Takes mode explicitly rather than reading ctrl_main_handle.mode, so
 * it can be called continuously from statemachine.c to keep the reference
 * live-computed for whichever screen is showing, even before
 * ctrl_main_start_ctrl() has actually activated the control loop.
 * @param mode Target control mode to compute the reference for.
 * @param reference_poti_count Reference value of the poti (in count)
 */
void ctrl_main_apply_reference(ctrl_mode_t mode, uint16_t reference_poti_count) {
	switch (mode) {

	case CTRL_MODE_60V:
#ifdef EMI_DEBUG
	case CTRL_MODE_EMI:
#endif
		ctrl_main_handle.voltage_reference_mV = reference_poti_count * CTRL_MAIN_MV_PER_COUNT;
		break;
	case CTRL_MODE_10A:
		ctrl_main_handle.current_reference_mA = reference_poti_count * CTRL_MAIN_MA_PER_COUNT;
		break;
	case CTRL_MODE_RESISTANCE_1A:
		ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_IDLE_VOLTAGE_1A_mV;
		break;
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_IDLE_VOLTAGE_1mA_mV;
		break;
	case CTRL_MODE_ISOMETER:
		ctrl_main_handle.voltage_iso_reference_V =
				ctrl_main_iso_values[reference_poti_count];
		break;
	case CTRL_MODE_CHARGE:
		ctrl_main_handle.current_reference_mA = CTRL_PARAM_CHARGE_CURRENT_mA;
		ctrl_main_handle.voltage_reference_mV =
				CTRL_PARAM_CHARGE_END_VOLTAGE_mV;
		break;

	case CTRL_MODE_OFF:
	default:
		ctrl_main_handle.voltage_reference_mV = 0;
		ctrl_main_handle.current_reference_mA = 0;
		ctrl_main_handle.voltage_iso_reference_V = 0;
		break;
	}

}

/*
 * Startup ramp shared by every ctrl_*() control loop below.
 *
 * ctrl_main_handle.ramp is a SINGLE field, not one per controller: it is
 * zeroed once by ctrl_main_start_ctrl() and from then on incremented by
 * whichever ctrl_*() function ctrl_main_ctrl()'s switch calls that
 * tick. Exactly one of those functions runs per control-loop tick, so a
 * shared counter is correct -- do NOT change this into a per-controller
 * ramp, and do not call this helper more than once per tick.
 *
 * Returns the multiplier just applied to `target` (1.0F once the ramp has
 * saturated past 1.0F, otherwise the freshly-incremented
 * ctrl_main_handle.ramp).
 */
static float ctrl_apply_ramped_ref(PID_controller_t *c, float target, float div) {
	float multiplier;
	if (ctrl_main_handle.ramp > 1.0F) {
		multiplier = 1.0F;
		c->ref = target;
		ctrl_main_handle.ramp = 1.1F;
	} else {
		ctrl_main_handle.ramp += (div / CTRL_FREQ);
		multiplier = ctrl_main_handle.ramp;
		c->ref = target * multiplier;
	}
	return multiplier;
}

/*
 * Shared tail of CHARGE's CC and CV loops: HRTIM_CHANNEL_SEK is driven
 * inverted, D = CTRL_PARAM_CONST_DUTY_HIGH - action, so a larger PI action
 * means a smaller SEK duty and more charge current (see charge_seq.h for the
 * duty/current relation). The PI saturation limits bound the result; there
 * is no further ceiling on the SEK duty.
 */
static void ctrl_apply_inverted_sek_duty(float action) {
	float duty = (float) CTRL_PARAM_CONST_DUTY_HIGH - action;
	hrtim_set_duty(HRTIM_CHANNEL_SEK, duty);
	ctrl_main_handle.duty = duty * 1000;
}

/**
 * Writes a frame-average PRIM duty in buck (SEK parked), split into run duty
 * and idle cycles as in the voltage mode's buck region.
 * @param duty Frame-average PRIM duty (0..1).
 */
static void ctrl_apply_buck_burst(float duty) {
	float duty_run = duty;
#ifdef VOUT_SYNC_BURST_ENABLED
	vout_burst_t bp = vout_burst_plan(duty, CTRL_PARAM_VOUT_FRAME_CYCLES,
			CTRL_PARAM_VOUT_SYNC_CYCLES, 1, CTRL_PARAM_VOUT_MIN_RUN_DUTY,
			CTRL_PARAM_VOUT_MAX_RUN_DUTY);
	hrtim_burst_set_idle_cycles(bp.idle_cycles);
	duty_run = bp.duty_run;
#endif
	hrtim_set_duty(HRTIM_CHANNEL_PRIM, duty_run);
	ctrl_main_handle.duty = duty * 1000;
}

static void ctrl_voltage_buck(int32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV) {
	ctrl_apply_ramped_ref(&ctrl_pi_voltage_buck,
			ctrl_main_handle.voltage_reference_mV / 1000.0F, 10.0F);

	ctrl_PID_controller_execute(&ctrl_pi_voltage_buck,
			voltage_meas_mV / 1000.0F, voltage_meas_accurate_mV / 1000.0F);

	ctrl_apply_buck_burst(ctrl_pi_voltage_buck.action);
}

/**
 * Writes one 60V leg: freq + duty together when the freq changes, else duty.
 * @param channel HRTIM_CHANNEL_PRIM or _SEK.
 * @param freq Last written freq of that leg, updated.
 * @param fast Leg runs at the high freq now.
 * @param fast_in_region Leg is fast in the target region (else it waits to
 *        be parked: its parked duty is limited to a sane 1 MHz duty).
 * @param duty Duty for the leg.
 */
static void ctrl_vout_write_leg(uint8_t channel, uint32_t *freq, uint8_t fast,
		uint8_t fast_in_region, float duty) {
	uint32_t f = fast ? CTRL_PARAM_SW_FREQ_HIGH : CTRL_PARAM_SW_FREQ_LOW;
	if (fast && !fast_in_region) {
		if (duty < CTRL_PARAM_VOUT_MIN_RUN_DUTY)
			duty = CTRL_PARAM_VOUT_MIN_RUN_DUTY;
		else if (duty > CTRL_PARAM_VOUT_MAX_RUN_DUTY)
			duty = CTRL_PARAM_VOUT_MAX_RUN_DUTY;
	}
	if (f != *freq) {
		hrtim_set_freq_duty(channel, f, duty);
		*freq = f;
	} else
		hrtim_set_duty(channel, duty);
}

/*
 * Full-range voltage source (0..60 V): buck, 4-switch or boost by
 * V_ref / V_in (vout_region.h), one PI per region. Only the active PI runs, so
 * a region change preloads the incoming one with the duty that gives the same
 * conversion ratio, so the output does not step. The legs and the burst
 * change over three ticks (vout_transition_step()), so no parked pulse runs
 * outside an idle block. The 1 A limiter caps the duty in every region.
 */
static void ctrl_voltage_wide(const ADC_CONVERTED_DATA *meas) {
	ctrl_apply_ramped_ref(&ctrl_pi_voltage_buck,
			ctrl_main_handle.voltage_reference_mV / 1000.0F, 10.0F);
	float ref_V = ctrl_pi_voltage_buck.ref;
	ctrl_pi_voltage_4sw.ref = ref_V;
	ctrl_pi_voltage_boost.ref = ref_V;

	float v_out_V = meas->v_out / 1000.0F;
	float v_acc_V = meas->v_term_ext_mv / 1000.0F;

	// Filtered V_IN and a dwell time keep the region from toggling at an edge.
	float v_in_mV = (float) meas->v_in;
	if (vout_vin_filt_mV <= 0.0F)
		vout_vin_filt_mV = v_in_mV;
	else
		vout_vin_filt_mV += (v_in_mV - vout_vin_filt_mV)
				* (1.0F / (CTRL_PARAM_VOUT_VIN_FILTER_s * CTRL_FREQ));
	v_in_mV = vout_vin_filt_mV;
	if (v_in_mV < CTRL_PARAM_VOUT_VIN_MIN_mV)
		v_in_mV = CTRL_PARAM_VOUT_VIN_MIN_mV;
	float ratio = (ref_V * 1000.0F) / v_in_mV;

	vout_last_ratio = ratio;
	vout_region_t next = vout_region;
	if (vout_region_hold > 0)
		vout_region_hold--;
	else
		next = vout_region_select(vout_region, ratio);
	PID_controller_t *pi;
	switch (next) {
	case VOUT_REGION_BUCK:
		pi = &ctrl_pi_voltage_buck;
		break;
	case VOUT_REGION_FOUR_SWITCH:
		pi = &ctrl_pi_voltage_4sw;
		break;
	default:
		pi = &ctrl_pi_voltage_boost;
		break;
	}

	uint8_t changed = (next != vout_region);
	if (changed) {
		float m = vout_region_ratio_from_duties(vout_duty_prim, vout_duty_sek);
		float d = vout_region_duty_for_ratio(next, m);
		// action = P + I, so preload I with what is left after this tick's P.
		pi->prev_I_action = d - pi->P_gain * (pi->ref - v_out_V);
		// The limiter's duty scale differs per region.
		ctrl_limiter_arm(&ctrl_pi_boost_iout_limit);
		vout_trans_from = vout_region;
		vout_trans_step = 0;
		vout_region = next;
		vout_region_changes++;
		vout_region_hold = (uint16_t) (CTRL_PARAM_VOUT_REGION_DWELL_s
				* CTRL_FREQ);
	}

	ctrl_PID_controller_execute(pi, v_out_V, v_acc_V);
	ctrl_PID_controller_execute(&ctrl_pi_boost_iout_limit,
			meas->i_out / 1000.0F, meas->i_out / 1000.0F);

	float duty = ctrl_limited_duty(pi, &ctrl_pi_boost_iout_limit, pi->action);

	// `duty` is the average over a 20 us frame; the burst plan splits it into
	// run duty and idle cycles (sync block; buck also skips below the HRTIM
	// minimum duty). The 4-switch region parks nothing. In boost the idle
	// block holds SEC_L on, i.e. counts as SEK duty 1.
	vout_burst_t bp = { 0, duty };
#ifdef VOUT_SYNC_BURST_ENABLED
	if (vout_region == VOUT_REGION_BOOST)
		bp = vout_burst_plan_low_side(duty, CTRL_PARAM_VOUT_FRAME_CYCLES,
				CTRL_PARAM_VOUT_SYNC_CYCLES, CTRL_PARAM_VOUT_MAX_RUN_DUTY);
	else
		bp = vout_burst_plan(duty, CTRL_PARAM_VOUT_FRAME_CYCLES,
				vout_region == VOUT_REGION_FOUR_SWITCH ?
						0 : CTRL_PARAM_VOUT_SYNC_CYCLES,
				vout_region == VOUT_REGION_BUCK,
				CTRL_PARAM_VOUT_MIN_RUN_DUTY, CTRL_PARAM_VOUT_MAX_RUN_DUTY);
#endif

	switch (vout_region) {
	case VOUT_REGION_BUCK:
		vout_duty_prim = duty;
		vout_duty_sek = CTRL_PARAM_CONST_DUTY_LOW;
		break;
	case VOUT_REGION_FOUR_SWITCH:
		vout_duty_prim = duty;
		vout_duty_sek = duty;
		break;
	case VOUT_REGION_BOOST:
	default:
		vout_duty_prim = CTRL_PARAM_CONST_DUTY_HIGH;
		vout_duty_sek = duty;
		break;
	}

	vout_legs_t legs = vout_transition_step(vout_trans_from, vout_region,
			vout_trans_step);
	vout_legs_t target = vout_region_legs(vout_region);
#ifdef VOUT_SYNC_BURST_ENABLED
	// Only the fast leg follows the burst: PRIM in buck, SEK in boost.
	if (legs.burst != vout_burst_leg) {
		if (legs.burst == 0)
			hrtim_burst_stop();
		else
			hrtim_burst_select_leg(
					legs.burst == VOUT_LEG_SEK ?
							HRTIM_CHANNEL_SEK : HRTIM_CHANNEL_PRIM);
		vout_burst_leg = legs.burst;
	}
	// Old burst still on (step 0): keep the sync block for the parked pulses.
	hrtim_burst_set_idle_cycles(
			legs.burst == target.burst ?
					bp.idle_cycles : CTRL_PARAM_VOUT_SYNC_CYCLES);
#endif
	ctrl_vout_write_leg(HRTIM_CHANNEL_PRIM, &vout_freq_prim,
			(legs.fast & VOUT_LEG_PRIM) != 0,
			(target.fast & VOUT_LEG_PRIM) != 0,
			(legs.burst == VOUT_LEG_PRIM && target.burst == VOUT_LEG_PRIM) ?
					bp.duty_run : vout_duty_prim);
	ctrl_vout_write_leg(HRTIM_CHANNEL_SEK, &vout_freq_sek,
			(legs.fast & VOUT_LEG_SEK) != 0,
			(target.fast & VOUT_LEG_SEK) != 0,
			(legs.burst == VOUT_LEG_SEK && target.burst == VOUT_LEG_SEK) ?
					bp.duty_run : vout_duty_sek);
	if (vout_trans_step < 2)
		vout_trans_step++;
	ctrl_main_handle.duty = duty * 1000;
}

/**
 * 10A current source with a compliance limit: a PI on v_out caps the duty at
 * CTRL_PARAM_10A_COMPLIANCE_mV, so open terminals do not wind the current
 * PI up to its duty ceiling.
 * @param current_meas_mA Fast internal i_out.
 * @param current_meas_accurate External ADC current (I part).
 * @param voltage_meas_mV Fast internal v_out.
 */
static void ctrl_current(int16_t current_meas_mA,
		int16_t current_meas_accurate, int32_t voltage_meas_mV) {

	ctrl_apply_ramped_ref(&ctrl_pi_current,
			ctrl_main_handle.current_reference_mA / 1000.0F, 10.0F);

	ctrl_PID_controller_execute(&ctrl_pi_current, current_meas_mA / 1000.0F,
			current_meas_accurate / 1000.0F);
	ctrl_PID_controller_execute(&ctrl_pi_10a_vlimit, voltage_meas_mV / 1000.0F,
			voltage_meas_mV / 1000.0F);

	float duty = ctrl_limited_duty(&ctrl_pi_current, &ctrl_pi_10a_vlimit,
			ctrl_pi_current.action);
	ctrl_apply_buck_burst(duty);
}

/**
 * @brief Charge CC loop.
 * @param target_mA Current setpoint (ramped).
 * @param current_meas_mA Internal current, positive = charging (P part).
 * @param current_bms_mA BMS battery current, positive = charging (I part).
 */
static void ctrl_charge_current(uint32_t target_mA,
		int16_t current_meas_mA, int16_t current_bms_mA) {

	// Reference 0 -> setpoint over CTRL_PARAM_CHARGE_RAMP_s (ramp starts at 0,
	// see ctrl_main_charge_start_ramp()); once the ramp has passed 1.0 the
	// helper latches the full setpoint.
	ctrl_apply_ramped_ref(&ctrl_pi_charge_current, target_mA / 1000.0F,
			1.0F / CTRL_PARAM_CHARGE_RAMP_s);

	ctrl_PID_controller_execute(&ctrl_pi_charge_current,
			current_meas_mA / 1000.0F, current_bms_mA / 1000.0F);

	// Apply Duty inverted, as the secondary Duty is inversed
	ctrl_apply_inverted_sek_duty(ctrl_pi_charge_current.action);

}

/* See ctrl_main.h. The upper limit follows current_reference_mA, so the
 * deep-discharge stepper and the normal target both cap the CV current. */
void ctrl_main_charge_cv_step(uint16_t cell_max_mV) {
	if (ctrl_main_handle.mode != CTRL_MODE_CHARGE
			|| ctrl_main_handle.charge_phase != CHG_PHASE_CV)
		return;

	float v_V = (BQ76905_PACK_CELL_COUNT * (uint32_t) cell_max_mV) / 1000.0F;
	ctrl_pi_charge_voltage.ref = ctrl_main_handle.voltage_reference_mV / 1000.0F;
	ctrl_pi_charge_voltage.sat_limit_high =
			ctrl_main_handle.current_reference_mA / 1000.0F;
	ctrl_PID_controller_execute(&ctrl_pi_charge_voltage, v_V, v_V);
	ctrl_main_handle.charge_cv_current_mA =
			(uint32_t) (ctrl_pi_charge_voltage.action * 1000.0F);
}

static void ctrl_voltage_hv(int32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV, int32_t current_meas_iso_uA) {

	ctrl_apply_ramped_ref(&ctrl_pi_voltage_hv,
			ctrl_main_handle.voltage_iso_reference_V, 10.0F);

	ctrl_PID_controller_execute(&ctrl_pi_voltage_hv, voltage_meas_mV / 1000.0F,
			voltage_meas_accurate_mV / 1000.0F);

	ctrl_PID_controller_execute(&ctrl_pi_hv_iout_limit,
			current_meas_iso_uA / 1000.0F, current_meas_iso_uA / 1000.0F);

	float duty_limited = ctrl_limited_duty(&ctrl_pi_voltage_hv,
			&ctrl_pi_hv_iout_limit, ctrl_pi_voltage_hv.action);

	// Apply Duty
	hrtim_set_duty(HRTIM_CHANNEL_HV, duty_limited);
	ctrl_main_handle.duty = duty_limited * 1000;

}
