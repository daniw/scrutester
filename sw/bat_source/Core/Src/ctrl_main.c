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

extern BQ76905_handle bms;

ctrl_main_t ctrl_main_handle;
PID_controller_t ctrl_pi_voltage_buck;
PID_controller_t ctrl_pi_voltage_boost;
PID_controller_t ctrl_pi_current;
PID_controller_t ctrl_pi_boost_iout_limit;
PID_controller_t ctrl_pi_charge_current;
PID_controller_t ctrl_pi_charge_voltage;
PID_controller_t ctrl_pi_voltage_hv;
PID_controller_t ctrl_pi_hv_iout_limit;

const uint16_t ctrl_main_iso_values[4] = { 125, 250, 500, 1000 };

// CLI setPI IDs are positional: 0=Buck,1=Boost,2=Current,3=BoostIoutLimit,4=ChargeCurrent.
const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN] = {
	{ &ctrl_pi_voltage_buck,     &config_store.calibration.voltage_buck_p,     &config_store.calibration.voltage_buck_i,
	  CTRL_PARAM_VOLTAGE_BUCK_P,       CTRL_PARAM_VOLTAGE_BUCK_I,
	  CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_HIGH,  CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_LOW,  "Buck" },
	{ &ctrl_pi_voltage_boost,    &config_store.calibration.voltage_boost_p,    &config_store.calibration.voltage_boost_i,
	  CTRL_PARAM_VOLTAGE_BOOST_P,      CTRL_PARAM_VOLTAGE_BOOST_I,
	  CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH, CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW, "Boost" },
	{ &ctrl_pi_current,          &config_store.calibration.current_p,          &config_store.calibration.current_i,
	  CTRL_PARAM_CURRENT_P,            CTRL_PARAM_CURRENT_I,
	  CTRL_PARAM_CURRENT_DUTY_SAT_HIGH,       CTRL_PARAM_CURRENT_DUTY_SAT_LOW,       "Current" },
	{ &ctrl_pi_boost_iout_limit, &config_store.calibration.boost_iout_limit_p, &config_store.calibration.boost_iout_limit_i,
	  CTRL_PARAM_BOOST_IOUT_LIMIT_P,   CTRL_PARAM_BOOST_IOUT_LIMIT_I,
	  CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH, CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW, "BoostIoutLimit" },
	{ &ctrl_pi_charge_current,   &config_store.calibration.charge_current_p,   &config_store.calibration.charge_current_i,
	  CTRL_PARAM_CHARGE_CURRENT_P,     CTRL_PARAM_CHARGE_CURRENT_I,
	  CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH, CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_LOW, "ChargeCurrent" },
	{ &ctrl_pi_charge_voltage,   &config_store.calibration.charge_voltage_p,   &config_store.calibration.charge_voltage_i,
	  CTRL_PARAM_CHARGE_VOLTAGE_P,     CTRL_PARAM_CHARGE_VOLTAGE_I,
	  CTRL_PARAM_CHARGE_VOLTAGE_DUTY_SAT_HIGH, CTRL_PARAM_CHARGE_VOLTAGE_DUTY_SAT_LOW, "ChargeVoltage" },
	{ &ctrl_pi_voltage_hv,   &config_store.calibration.voltage_hv_p,   &config_store.calibration.voltage_hv_i,
	  CTRL_PARAM_HV_VOLTAGE_P,     CTRL_PARAM_HV_VOLTAGE_I,
	  CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_HIGH, CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_LOW, "HVVoltage" },
	{ &ctrl_pi_hv_iout_limit,   &config_store.calibration.flyback_current_p,   &config_store.calibration.flyback_current_i,
	  CTRL_PARAM_HV_CURRENT_P,     CTRL_PARAM_HV_CURRENT_I,
	  CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_HIGH, CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_LOW, "HVCurrent" },
};

/******************* Function Prototypes **************************/
void ctrl_main_ctrl_voltage_buck(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate);
void ctrl_main_ctrl_voltage_boost(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate, int32_t current_meas_ext_mA);
void ctrl_main_ctrl_current(int16_t current_meas_mA,
		int16_t current_meas_accurate);
void ctrl_main_ctrl_charge_current(int16_t current_meas_mA,
		int16_t current_meas_accurate);
void ctrl_main_ctrl_charge_voltage(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV);
void ctrl_main_ctrl_voltage_hv(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV, int32_t current_meas_iso_uA);

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
		ctrl_PID_controller_init(e->ctrl, *e->cal_p, *e->cal_i, 0, e->sat_high, e->sat_low);
	}

	ctrl_pi_boost_iout_limit.ref = CTRL_PARAM_BOOST_IOUT_LIMIT_mA / 1000.0F;
	ctrl_pi_hv_iout_limit.ref = CTRL_PARAM_HV_IOUT_LIMIT_mA / 1000.0F;
}

/**
 * Maps a statemachine_modes_t to its ctrl_mode_t counterpart, via a
 * mode_table[] lookup - see mode_table.h/.c. The two enums are positionally
 * identical only up through ISOMETER and diverge after that
 * (STATEMACHINE_MODE_VOLTMETER=6 vs CTRL_MODE_CHARGE=6) - callers must go
 * through this instead of passing statemachine_handle.current_mode directly
 * to ctrl_main_start_ctrl(). Modes with no ctrl_mode_t counterpart
 * (AMPMETER, VOLTMETER, SETTINGS, SHUTDOWN, IDLE) correctly map to
 * CTRL_MODE_OFF, since none of them drive the control loop.
 * @param mode statemachine mode to translate.
 * @return the matching ctrl_mode_t, or CTRL_MODE_OFF if none applies (also
 *   the out-of-range/mode >= STATEMACHINE_MODE_RESERVED fallback).
 */
ctrl_mode_t statemachine_mode_to_ctrl_mode(statemachine_modes_t mode) {
	if (mode >= STATEMACHINE_MODE_RESERVED) {
		return CTRL_MODE_OFF;
	}
	return mode_table[mode].ctrl_mode;
}

void ctrl_main_start_ctrl(ctrl_mode_t mode) {
	switch (mode) {

	case CTRL_MODE_CHARGE:
		ctrl_main_apply_reference(mode, 0);
		ctrl_PID_reset(&ctrl_pi_voltage_buck);
		ctrl_PID_reset(&ctrl_pi_charge_current);
		ctrl_PID_reset(&ctrl_pi_charge_voltage);
		ctrl_main_handle.charge_cv_phase = 0;
		// Preload for smoother turn on
		ctrl_pi_charge_current.prev_I_action = CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH;
		hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_LOW);
		hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_HIGH);
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, CTRL_PARAM_CONST_DUTY_HIGH);
		hrtim_set_duty(HRTIM_CHANNEL_SEK, CTRL_PARAM_CONST_DUTY_LOW);
		hrtim_enable(HRTIM_CHANNEL_PRIM);
		hrtim_enable(HRTIM_CHANNEL_SEK);
		break;
	case CTRL_MODE_60V:
		ctrl_PID_reset(&ctrl_pi_voltage_boost);
		ctrl_PID_reset(&ctrl_pi_boost_iout_limit);
		hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_LOW);
		hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_HIGH);
		// This is set to low, because it is part of the voltage ramp.
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, CTRL_PARAM_CONST_DUTY_LOW);
		hrtim_set_duty(HRTIM_CHANNEL_SEK, CTRL_PARAM_CONST_DUTY_LOW);
		hrtim_enable(HRTIM_CHANNEL_PRIM);
		hrtim_enable(HRTIM_CHANNEL_SEK);
		break;
	case CTRL_MODE_RESISTANCE_1A:
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_apply_reference(mode, 0);
	case CTRL_MODE_10A:
		ctrl_PID_reset(&ctrl_pi_voltage_buck);
		ctrl_PID_reset(&ctrl_pi_current);
		hrtim_set_freq(HRTIM_CHANNEL_PRIM, CTRL_PARAM_SW_FREQ_HIGH);
		hrtim_set_freq(HRTIM_CHANNEL_SEK, CTRL_PARAM_SW_FREQ_LOW);
		hrtim_set_duty(HRTIM_CHANNEL_SEK, CTRL_PARAM_CONST_DUTY_LOW); // Needs to be low!!
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, 0);
		hrtim_enable(HRTIM_CHANNEL_PRIM);
		hrtim_enable(HRTIM_CHANNEL_SEK);
		break;
	case CTRL_MODE_ISOMETER:
		ctrl_PID_reset(&ctrl_pi_voltage_hv);
		ctrl_PID_reset(&ctrl_pi_hv_iout_limit);
		hrtim_set_freq(HRTIM_CHANNEL_HV, CTRL_PARAM_SW_FREQ_HV);
		hrtim_set_duty(HRTIM_CHANNEL_HV, 0);
		hrtim_enable(HRTIM_CHANNEL_HV);
		break;

	case CTRL_MODE_OFF:
	default:
		break;
	}
	ctrl_main_handle.ramp = 0.0F;
	ctrl_main_handle.mode = mode;
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
 * Before the interrupt priorities were split, TIM2 and the ADC DMA interrupt
 * shared priority 0 and could not preempt each other, so this ordering was
 * merely tidy. It is now what keeps the two contexts consistent -- do not
 * reorder either function's tail without re-checking that.
 */
void ctrl_main_stop_control(void) {
	ctrl_main_handle.mode = CTRL_MODE_OFF;
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
		ctrl_main_ctrl_voltage_boost(meas->v_out,
				meas->v_term_ext_mv, meas->i_out_ext_mA);
		break;
	case CTRL_MODE_RESISTANCE_1A:
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_ctrl_voltage_buck(meas->v_out,
				meas->v_out);
		break;
	case CTRL_MODE_10A:
		ctrl_main_ctrl_current(meas->i_out,
				meas->i_out_ext_mA);

		break;
	case CTRL_MODE_ISOMETER:
		ctrl_main_ctrl_voltage_hv(meas->v_hv,
				meas->v_term_ext_mv,
				meas->i_iso_ext_uA);
		break;
	case CTRL_MODE_CHARGE:
		/* CC/CV: hold charge current until the end voltage is reached, then
		 * latch into CV and hold that voltage (statemachine.c decides when
		 * the cycle is actually done, once current has also tapered off).
		 * One-way latch so a small voltage sag once CV is holding doesn't
		 * bounce the loop back into CC. Both phases drive HRTIM_CHANNEL_SEK
		 * (set up in ctrl_main_start_ctrl()), unlike ctrl_main_ctrl_voltage_buck()
		 * which drives HRTIM_CHANNEL_PRIM. */
		if (!ctrl_main_handle.charge_cv_phase
				&& meas->v_in >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV) {
			ctrl_main_handle.charge_cv_phase = 1;
			// Bumpless transfer: seed the CV loop's integrator from the CC
			// loop's last output duty, so the switchover doesn't jerk the
			// duty cycle - same preload trick ctrl_main_start_ctrl() uses
			// for smooth turn-on.
			ctrl_pi_charge_voltage.prev_I_action = ctrl_pi_charge_current.action;
		}

		if (!ctrl_main_handle.charge_cv_phase)
			ctrl_main_ctrl_charge_current(-meas->i_out,
					-meas->i_out_ext_mA);
		else
			ctrl_main_ctrl_charge_voltage(meas->v_in,
					bms.VoltageRegisters.StackVoltage);
		break;

	case CTRL_MODE_OFF:
	default:
		break;
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
		ctrl_main_handle.voltage_reference_mV = reference_poti_count * 500; // 500mV Resolution, Max range 63V
		break;
	case CTRL_MODE_10A:
		ctrl_main_handle.current_reference_mA = reference_poti_count * 100; // 100mA Resolution, Max Range 10A
		break;
	case CTRL_MODE_RESISTANCE_1A:
		ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_IDLE_VOLTAGE_1A_mV;
		break;
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_IDLE_VOLTAGE_1mA_mV;
		break;
	case CTRL_MODE_ISOMETER:
		ctrl_main_handle.voltage_iso_reference_V =
				ctrl_main_iso_values[reference_poti_count & 0x3];
		break;
	case CTRL_MODE_CHARGE:
		ctrl_main_handle.current_reference_mA = CTRL_PARAM_CHARGE_CURRENT_mA;
		ctrl_main_handle.voltage_reference_mV = CTRL_PARAM_CHARGE_END_VOLTAGE_mV;
		break;

	case CTRL_MODE_OFF:
	default:
		ctrl_main_handle.voltage_reference_mV = 0;
		ctrl_main_handle.current_reference_mA = 0;
		ctrl_main_handle.voltage_iso_reference_V = 0;
		break;
	}

}

void ctrl_main_ctrl_voltage_buck(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV) {
uint8_t duty;
	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_voltage_buck.ref = ctrl_main_handle.voltage_reference_mV / 1000.0F;
		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);
		ctrl_pi_voltage_buck.ref = (ctrl_main_handle.voltage_reference_mV / 1000.0F)
				* ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_voltage_buck, voltage_meas_mV / 1000.0F,
			voltage_meas_accurate_mV / 1000.0F, 0);

	// Apply Duty
	hrtim_set_duty(HRTIM_CHANNEL_PRIM, ctrl_pi_voltage_buck.action);
	ctrl_main_handle.duty = ctrl_pi_voltage_buck.action*1000;
	//if(ctrl_pi_voltage.action > 0.24F || ctrl_pi_voltage.action<0.24F)
	//	printf("%d\r\n", duty);
}


void ctrl_main_ctrl_voltage_boost(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV, int32_t current_meas_ext_mA) {
uint8_t duty;
	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_voltage_boost.ref = ctrl_main_handle.voltage_reference_mV / 1000.0F;
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, CTRL_PARAM_CONST_DUTY_HIGH);
		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);
		hrtim_set_duty(HRTIM_CHANNEL_PRIM, CTRL_PARAM_CONST_DUTY_HIGH*ctrl_main_handle.ramp);
		ctrl_pi_voltage_boost.ref = (ctrl_main_handle.voltage_reference_mV / 1000.0F)
				* ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_voltage_boost, voltage_meas_mV / 1000.0F,
			voltage_meas_accurate_mV / 1000.0F, 0);

	ctrl_PID_controller_execute(&ctrl_pi_boost_iout_limit,
			current_meas_ext_mA / 1000.0F, current_meas_ext_mA / 1000.0F, 0);

	float duty_limited = ctrl_pi_voltage_boost.action;
	if (ctrl_pi_boost_iout_limit.action < duty_limited)
		duty_limited = ctrl_pi_boost_iout_limit.action;

	// Apply Duty
	hrtim_set_duty(HRTIM_CHANNEL_SEK, duty_limited);
	ctrl_main_handle.duty = duty_limited*1000;
}

void ctrl_main_ctrl_current(int16_t current_meas_mA,
		int16_t current_meas_accurate) {

	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_current.ref = ctrl_main_handle.current_reference_mA / 1000.0F;
		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);
		ctrl_pi_current.ref = (ctrl_main_handle.current_reference_mA / 1000.0F)
				* ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_current, current_meas_mA / 1000.0F,
			current_meas_accurate / 1000.0F, 0);

	hrtim_set_duty(HRTIM_CHANNEL_PRIM, ctrl_pi_current.action);
	ctrl_main_handle.duty = ctrl_pi_current.action*1000;

}

void ctrl_main_ctrl_charge_current(int16_t current_meas_mA,
		int16_t current_meas_accurate) {

	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_charge_current.ref = ctrl_main_handle.current_reference_mA / 1000.0F;
		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);
		ctrl_pi_charge_current.ref = (ctrl_main_handle.current_reference_mA / 1000.0F)
				* ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_charge_current, current_meas_mA / 1000.0F,
			current_meas_accurate / 1000.0F, 0);

	// Apply Duty
	// Apply Duty inverted, as the secondary Duty is inversed
	if((CTRL_PARAM_CONST_DUTY_HIGH- ctrl_pi_charge_current.action)> 0.5)
		hrtim_set_duty(HRTIM_CHANNEL_SEK, 0.5);
	else
		hrtim_set_duty(HRTIM_CHANNEL_SEK,CTRL_PARAM_CONST_DUTY_HIGH- ctrl_pi_charge_current.action);
	ctrl_main_handle.duty = (CTRL_PARAM_CONST_DUTY_HIGH-ctrl_pi_charge_current.action)*1000;

}

/**
 * CV tail of the charge cycle: P-term from the fast internal ADC
 * (voltage_meas_mV), I-term from the accurate but slow (~1Hz) BMS
 * StackVoltage reading (voltage_meas_accurate_mV) - the PID just holds the
 * last value between BMS refreshes, same as elsewhere in this file.
 */
void ctrl_main_ctrl_charge_voltage(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV) {

	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_charge_voltage.ref = ctrl_main_handle.voltage_reference_mV / 1000.0F;
		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);
		ctrl_pi_charge_voltage.ref = (ctrl_main_handle.voltage_reference_mV / 1000.0F)
				* ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_charge_voltage, voltage_meas_mV / 1000.0F,
			voltage_meas_accurate_mV / 1000.0F, 0);

	// Apply Duty
	// Apply Duty inverted, as the secondary Duty is inversed
	if((CTRL_PARAM_CONST_DUTY_HIGH- ctrl_pi_charge_voltage.action)> 0.5)
		hrtim_set_duty(HRTIM_CHANNEL_SEK, 0.5);
	else
		hrtim_set_duty(HRTIM_CHANNEL_SEK,CTRL_PARAM_CONST_DUTY_HIGH- ctrl_pi_charge_voltage.action);
	ctrl_main_handle.duty = (CTRL_PARAM_CONST_DUTY_HIGH-ctrl_pi_charge_voltage.action)*1000;

}

void ctrl_main_ctrl_voltage_hv(uint32_t voltage_meas_mV,
		int32_t voltage_meas_accurate_mV, int32_t current_meas_iso_uA) {
	uint8_t duty;

	// Startup Ramp
	if (ctrl_main_handle.ramp > 1.0F) {
		ctrl_pi_voltage_hv.ref = ctrl_main_handle.voltage_iso_reference_V;

		ctrl_main_handle.ramp = 1.1F;
	}

	else {
		ctrl_main_handle.ramp += (10.0F / CTRL_FREQ);

		ctrl_pi_voltage_hv.ref = (ctrl_main_handle.voltage_iso_reference_V
				) * ctrl_main_handle.ramp;
	}

	ctrl_PID_controller_execute(&ctrl_pi_voltage_hv,
			voltage_meas_mV / 1000.0F, voltage_meas_accurate_mV / 1000.0F, 0);


	ctrl_PID_controller_execute(&ctrl_pi_hv_iout_limit,
			current_meas_iso_uA / 1000000.0F, current_meas_iso_uA / 1000000.0F, 0);

	float duty_limited = ctrl_pi_voltage_hv.action;
	if (ctrl_pi_hv_iout_limit.action < duty_limited)
		duty_limited = ctrl_pi_hv_iout_limit.action;

	// Apply Duty
	hrtim_set_duty(HRTIM_CHANNEL_HV, duty_limited);
	ctrl_main_handle.duty = duty_limited * 1000;

}
