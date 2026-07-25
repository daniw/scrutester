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

ctrl_main_t ctrl_main_handle;
PID_controller_t ctrl_pi_voltage_buck;
PID_controller_t ctrl_pi_voltage_boost;
PID_controller_t ctrl_pi_current;
PID_controller_t ctrl_pi_boost_iout_limit;
PID_controller_t ctrl_pi_charge_current;

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

	// Fixed reference for the boost current-limit loop; not part of the
	// generic per-entry init since it's not a gain/saturation value.
	ctrl_pi_boost_iout_limit.ref = CTRL_PARAM_BOOST_IOUT_LIMIT_mA / 1000.0F;
}

/**
 * Maps a statemachine_modes_t to its ctrl_mode_t counterpart. The two enums
 * are positionally identical only up through ISOMETER and diverge after
 * that (STATEMACHINE_MODE_VOLTMETER=6 vs CTRL_MODE_CHARGE=6) - callers must
 * go through this instead of passing statemachine_handle.current_mode
 * directly to ctrl_main_start_ctrl(). Modes with no ctrl_mode_t counterpart
 * (AMPMETER, VOLTMETER, SETTINGS, SHUTDOWN, IDLE) correctly map to
 * CTRL_MODE_OFF, since none of them drive the control loop.
 * @param mode statemachine mode to translate.
 * @return the matching ctrl_mode_t, or CTRL_MODE_OFF if none applies.
 */
ctrl_mode_t statemachine_mode_to_ctrl_mode(statemachine_modes_t mode) {
	switch (mode) {
	case STATEMACHINE_MODE_60V_OUT:
		return CTRL_MODE_60V;
	case STATEMACHINE_MODE_10A_OUT:
		return CTRL_MODE_10A;
	case STATEMACHINE_MODE_RESISTANCE_1A:
		return CTRL_MODE_RESISTANCE_1A;
	case STATEMACHINE_MODE_RESISTANCE_1mA:
		return CTRL_MODE_RESISTANCE_1mA;
	case STATEMACHINE_MODE_ISOMETER:
		return CTRL_MODE_ISOMETER;
	case STATEMACHINE_MODE_CHARGE:
		return CTRL_MODE_CHARGE;
	default:
		return CTRL_MODE_OFF;
	}
}

void ctrl_main_start_ctrl(ctrl_mode_t mode) {
	switch (mode) {

	case CTRL_MODE_CHARGE:
		ctrl_main_apply_reference(mode, 0);
		ctrl_PID_reset(&ctrl_pi_voltage_buck);
		ctrl_PID_reset(&ctrl_pi_charge_current);
		// Pre-load the integrator at the saturation boundary so the very
		// first cycle's action==sat_limit_high, which (via the inverted
		// duty math in ctrl_main_ctrl_charge_current()) yields SEK duty
		// 0 at startup instead of jumping straight to ~sat_limit_high.
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
		break;

	case CTRL_MODE_OFF:
	default:
		break;
	}
	ctrl_main_handle.ramp = 0.0F;
	ctrl_main_handle.mode = mode;
}

void ctrl_main_stop_control(void) {
	ctrl_main_handle.mode = CTRL_MODE_OFF;
	hrtim_disable(HRTIM_CHANNEL_ALL);
}

/**
 * Main control task
 */
void ctrl_main_ctrl(ADC_MEAS_DATA *adc_data) {
	switch (ctrl_main_handle.mode) {

	case CTRL_MODE_60V:
		ctrl_main_ctrl_voltage_boost(adc_data->converted.v_out,
				adc_data->converted.v_term_ext_mv, adc_data->converted.i_out_ext_mA);
		break;
	case CTRL_MODE_RESISTANCE_1A:
	case CTRL_MODE_RESISTANCE_1mA:
		ctrl_main_ctrl_voltage_buck(adc_data->converted.v_out,
				adc_data->converted.v_term_ext_mv);
		break;
	case CTRL_MODE_10A:
		ctrl_main_ctrl_current(adc_data->converted.i_out,
				adc_data->converted.i_out_ext_mA);

		break;
	case CTRL_MODE_ISOMETER:
		//ctrl_main_ctrl_hv(adc_data->)
		break;
	case CTRL_MODE_CHARGE:
		/* Simple CC/CV: hold charge current until the end voltage is reached,
		 * then hold that voltage. Reuses the buck path set up in
		 * ctrl_main_start_ctrl() (same HRTIM channel as 10A/Resistance). */
		if (adc_data->converted.v_in < CTRL_PARAM_CHARGE_END_VOLTAGE_mV)
			ctrl_main_ctrl_charge_current(-adc_data->converted.i_out,
					-adc_data->converted.i_out_ext_mA);
		//else
			/*ctrl_main_ctrl_voltage_buck(adc_data->converted.v_in,
					adc_data->converted.v_in);*/
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
		ctrl_main_handle.voltage_reference_mV = reference_poti_count * 500; // 500mV Auflösung, Max range 63V
		break;
	case CTRL_MODE_10A:
		ctrl_main_handle.current_reference_mA = reference_poti_count * 100; // 100mA Auflösung, Max Range 10A
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
		/* Charge current/end-voltage are fixed, not user-adjustable via the encoder. */
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

	// Current-limit foldback: independent PI loop on the accurate external
	// ADC current reading, min-selected against the voltage loop's duty
	// output (standard CV/CC crossover). No anti-windup interaction between
	// the two loops - each runs and saturates independently.
	ctrl_PID_controller_execute(&ctrl_pi_boost_iout_limit,
			current_meas_ext_mA / 1000.0F, current_meas_ext_mA / 1000.0F, 0);

	float duty_limited = ctrl_pi_voltage_boost.action;
	if (ctrl_pi_boost_iout_limit.action < duty_limited)
		duty_limited = ctrl_pi_boost_iout_limit.action;

	// Apply Duty
	hrtim_set_duty(HRTIM_CHANNEL_SEK, duty_limited);
	ctrl_main_handle.duty = duty_limited*1000;
	//if(ctrl_pi_voltage.action > 0.24F || ctrl_pi_voltage.action<0.24F)
	//	printf("%d\r\n", duty);
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

