/*
 * statemachine.c
 *
 *  Created on: Aug 9, 2025
 *      Author: ahorat
 */

#include "statemachine.h"
#include "aux_io_ctrl.h"
#include "display.h"
#include "menu.h"
#include "input.h"
#include "timer.h"
#include "ctrl_main.h"
#include "stm32g4xx_hal.h"
#include "adc.h"
#include "ui_ctrl.h"
#include "tim.h"
#include "gpio.h"
#include "hrtim.h"
#include "bq76905.h"
#include "ctrl_param.h"
#include "calibration.h"
#include "protection.h"

statemachine_t statemachine_handle;
uint16_t ok_button_pressed;
uint16_t esc_button_pressed;
uint16_t out_button_pressed;
extern ctrl_main_t ctrl_main_handle;
extern ADC_MEAS_DATA adc_data;
extern BQ76905_handle bms;

void statemachine_switchfromIdle(statemachine_modes_t mode);
void statemachine_switchtoIdle(void);
static void statemachine_apply_encoder_setpoint(void);
static void statemachine_step_calibration(void);

/* Settings > Calibration sub-UI state -- 0=channel list, 1=zero step,
 * 2=optional gain step (see display_calibration_enter/update()). */
static uint8_t calibration_ui_state = 0;
static calibration_channel_t calibration_selected_channel = CAL_CH_V_TERM;

/* Encoder-dialed reference value step per channel (in the channel's native
 * unit) for the optional gain calibration step. V_TERM/I_OUT reuse the same
 * per-detent scale already used for their live setpoints (ctrl_main.c's
 * CTRL_MODE_60V/CTRL_MODE_10A); the others are new, sized to give a sensible
 * dial range for typical reference values on that channel. */
static const float CALIBRATION_REFERENCE_STEP[CAL_CH_COUNT] = {
		[CAL_CH_V_TERM] = 500.0f,   // mV/detent, max ~63V
		[CAL_CH_V_SENS] = 50000.0f, // uV/detent, max ~6.3V
		[CAL_CH_V_OUT] = 500.0f,    // mV/detent, max ~63V
		[CAL_CH_V_HV] = 10000.0f,   // mV/detent, max ~1270V
		[CAL_CH_I_OUT] = 100.0f,    // mA/detent, max ~12.7A
		[CAL_CH_I_ISO] = 100.0f,    // uA/detent, max ~12.7mA
};

void statemachine_init(void) {
	statemachine_handle.output_on = 0;
	statemachine_handle.current_mode = STATEMACHINE_IDLE;
	statemachine_handle.current_menu_index = 0;
	statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
	ok_button_pressed = 0;
	esc_button_pressed = 0;

	input_init();
	input_encoder_reset(62);
	display_init();

	timer_add(STATEMACHINE_STEP_PERIOD_mS, TIMER_TYPE_TICK, EVENT_SM_STEP, 0);
	statemachine_switchtoIdle();
}

/* Feeds the live encoder value into ctrl_main's poti_reference and
 * immediately recomputes the derived reference (voltage/current/iso target)
 * for whichever mode is currently showing -- this runs every tick regardless
 * of whether OUT is held, so the reference is already correct and ready the
 * moment the controller actually starts, not just eventually once the ADC
 * ISR happens to catch up. */
static void statemachine_apply_encoder_setpoint(void) {
	ctrl_main_handle.poti_reference = input_encoder_read();
	ctrl_main_apply_reference(statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode),
			ctrl_main_handle.poti_reference);
}

/* Drives the Settings > Calibration sub-UI (see calibration_ui_state above).
 * Zero and gain are each confirmed with their own OK press, so the two
 * calibration steps of calibration.h can be triggered independently; ESC on
 * the channel list backs out to the Settings list (handled by the caller),
 * ESC on the zero/gain steps only cancels that step and returns to the
 * channel list. */
static void statemachine_step_calibration(void) {
	switch (calibration_ui_state) {
	case 0: { // channel list
		uint8_t idx = (input_encoder_read() / 2) % CAL_CH_COUNT;
		if (idx != calibration_selected_channel) {
			calibration_selected_channel = (calibration_channel_t) idx;
		}
		display_calibration_update(calibration_selected_channel, 0, 0.0f);
		if (ok_button_pressed == 1) {
			calibration_ui_state = 1;
			display_calibration_enter(calibration_selected_channel, 1);
		}
		if (esc_button_pressed == 1) {
			statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
			display_show_settings_list(statemachine_handle.current_menu_index);
		}
		break;
	}
	case 1: // zero step
		display_calibration_update(calibration_selected_channel, 1, 0.0f);
		if (ok_button_pressed == 1) {
			calibration_zero(calibration_selected_channel);
			calibration_ui_state = 2;
			input_encoder_reset(0);
			display_calibration_enter(calibration_selected_channel, 2);
		}
		if (esc_button_pressed == 1) {
			calibration_ui_state = 0;
			display_calibration_enter(calibration_selected_channel, 0);
		}
		break;
	case 2: { // optional gain step
		float reference_value = input_encoder_read()
				* CALIBRATION_REFERENCE_STEP[calibration_selected_channel];
		display_calibration_update(calibration_selected_channel, 2, reference_value);
		if (ok_button_pressed == 1) {
			calibration_set_gain(calibration_selected_channel, reference_value);
			calibration_ui_state = 0;
			display_calibration_enter(calibration_selected_channel, 0);
		}
		if (esc_button_pressed == 1) {
			calibration_ui_state = 0;
			display_calibration_enter(calibration_selected_channel, 0);
		}
		break;
	}
	default:
		break;
	}
}

void statemachine_step(void) {
	uint8_t temp;

	adc_convert_data();
	protection_update();
	ui_ctrl_step();

	// Handle Buttons press
	if (input_btn_ok())
		ok_button_pressed += 1;
	else if (input_btn_esc())
		esc_button_pressed += 1;
	else {
		ok_button_pressed = 0;
		esc_button_pressed = 0;
	}
	if (input_btn_out()) {
		if (out_button_pressed < 2)
			out_button_pressed += 1;
	} else
		out_button_pressed = 0;

	// Select transition to next state
	switch (statemachine_handle.current_mode) {
	case STATEMACHINE_IDLE:
		temp = (input_encoder_read() / 2) % MENU_ORDER_LENGTH;
		if (temp != statemachine_handle.current_menu_index) {
			statemachine_handle.current_menu_index = temp;
			display_show_idle(temp);
		}
		if (ok_button_pressed == 1) {
			statemachine_switchfromIdle(menu_entry_at(temp)->mode);
		}

		if (esc_button_pressed == 1)
			statemachine_handle.current_mode = STATEMACHINE_MODE_SHUTDOWN;

		// CHARGE is auto-entered, not menu/button-selected: whenever idle and
		// a 15-25V supply is detected at the output with no BMS fault
		// latched, start charging on our own.
		/*if (adc_data.converted.v_term >= 15000 && adc_data.converted.v_term <= 25000
				&& !(bms.SafetyRegisters.safetyStatusA || bms.SafetyRegisters.safetyStatusB)) {
			statemachine_switchfromIdle(STATEMACHINE_MODE_CHARGE);
		}*/
		break;

	case STATEMACHINE_MODE_60V_OUT:
	case STATEMACHINE_MODE_10A_OUT:
		statemachine_apply_encoder_setpoint();
		if (out_button_pressed == 1) {
			statemachine_handle.output_on = 1;
			ctrl_main_start_ctrl(statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode));
			aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
		} else if (out_button_pressed == 0) {
			statemachine_handle.output_on = 0;
			ctrl_main_stop_control();
			aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
		}
		display_update_mode(statemachine_handle.current_mode,
				statemachine_handle.output_on);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
		display_update_mode(statemachine_handle.current_mode, 1);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_ISOMETER:
		statemachine_apply_encoder_setpoint();
		display_update_mode(statemachine_handle.current_mode, 1);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_VOLTMETER:
		display_update_mode(statemachine_handle.current_mode, 1);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_AMPMETER:
		display_update_mode(statemachine_handle.current_mode, 1);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_CHARGE:
		// Charging starts immediately on auto-detected entry (see the IDLE
		// case above) and stops the same way, without a manual OK toggle.
		// End-of-charge, BMS fault, or the external supply being removed
		// (14V exit vs. the 15V entry threshold - hysteresis so the mode
		// doesn't flap right at the boundary) all stop charging the same
		// way. BMS data only refreshes once a second (EVENT_BMS_TIMER)
		// regardless, so checking it here at the 100ms tick loses nothing
		// vs. checking in the fast ADC-ISR-driven control loop.
		display_update_mode(statemachine_handle.current_mode,
				statemachine_handle.output_on);
		if (bms.VoltageRegisters.StackVoltage >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV
				|| bms.SafetyRegisters.safetyStatusA || bms.SafetyRegisters.safetyStatusB
				|| adc_data.converted.v_term < 14000) {
			statemachine_switchtoIdle();
		}
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_SETTINGS:
		if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_MENU) {
			temp = STATEMACHINE_SETTINGS_MODE_BMS
					+ (input_encoder_read() / 2)
							% (STATEMACHINE_SETTINGS_MODE_LENGTH - 1);
			if (temp != statemachine_handle.current_menu_index) {
				statemachine_handle.current_menu_index = temp;
				display_show_settings_list(temp);
			}
			if (ok_button_pressed == 1) {
				statemachine_handle.settings_mode =
						(statemachine_settings_modes_t) statemachine_handle.current_menu_index;
				if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_CALIBRATION) {
					calibration_ui_state = 0;
					calibration_selected_channel = CAL_CH_V_TERM;
					display_calibration_enter(calibration_selected_channel, 0);
				} else {
					display_enter_settings_detail(statemachine_handle.settings_mode);
				}
			}
			if (esc_button_pressed == 1)
				statemachine_switchtoIdle();
		} else if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_CALIBRATION) {
			statemachine_step_calibration();
		} else {
			display_update_settings_detail(statemachine_handle.settings_mode);
			if (esc_button_pressed == 1) {
				statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
				display_show_settings_list(statemachine_handle.current_menu_index);
			}
		}
		break;

	case STATEMACHINE_MODE_SHUTDOWN:
		gpio_shutdown();

	default:
		break;
	}

	// Generic protection interlock, runs every tick regardless of mode: force
	// the converter output off the instant an ERROR-level fault (over-temp,
	// OVP, OCP) is latched, and automatically resume once it clears -- OUT
	// modes wait for a fresh OUT press (mirrors their normal button-toggle
	// behaviour), the auto-started modes just resume on their own.
	static uint8_t protection_forced_output_off = 0;
	ctrl_mode_t active_ctrl_mode = statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode);

	if (active_ctrl_mode != CTRL_MODE_OFF) {
		if (protection_get_worst_level() == PROTECTION_LEVEL_ERROR) {
			if (ctrl_main_handle.mode != CTRL_MODE_OFF) {
				ctrl_main_stop_control();
				aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
			}
			statemachine_handle.output_on = 0;
			protection_forced_output_off = 1;
		} else if (protection_forced_output_off) {
			if (statemachine_handle.current_mode == STATEMACHINE_MODE_60V_OUT ||
					statemachine_handle.current_mode == STATEMACHINE_MODE_10A_OUT) {
				if (out_button_pressed) {
					statemachine_handle.output_on = 1;
					ctrl_main_start_ctrl(active_ctrl_mode);
					aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
				}
			} else {
				ctrl_main_start_ctrl(active_ctrl_mode);
				if (statemachine_handle.current_mode == STATEMACHINE_MODE_CHARGE) {
					statemachine_handle.output_on = 1;
				}
				if (statemachine_handle.current_mode == STATEMACHINE_MODE_RESISTANCE_1A ||
						statemachine_handle.current_mode == STATEMACHINE_MODE_RESISTANCE_1mA) {
					aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
				}
			}
			protection_forced_output_off = 0;
		}
	} else {
		protection_forced_output_off = 0;
	}
}

void statemachine_switchfromIdle(statemachine_modes_t mode) {
	printf("Switch from Idle to %d\r\n", mode);

	switch (mode) {
	case STATEMACHINE_MODE_60V_OUT:
		ui_ctrl_ledOutOn();

		adc_configure_mode(mode);
		statemachine_handle.current_mode = mode;
		statemachine_handle.output_on = 0;
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_10A_OUT:
		ui_ctrl_ledOutOn();
		adc_configure_mode(mode);
		statemachine_handle.current_mode = mode;
		statemachine_handle.output_on = 0;
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_RESISTANCE_1A:
		ui_ctrl_ledOutOn();
		ui_ctrl_ledSenseOn();

		statemachine_handle.current_mode = mode;
		adc_configure_mode(mode);
		ctrl_main_start_ctrl(statemachine_mode_to_ctrl_mode(mode));
		aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_RESISTANCE_1mA:
		ui_ctrl_ledOutOn();

		statemachine_handle.current_mode = mode;
		adc_configure_mode(mode);
		ctrl_main_start_ctrl(statemachine_mode_to_ctrl_mode(mode));
		aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_ISOMETER:
		ui_ctrl_ledOutOn();

		statemachine_handle.current_mode = mode;
		adc_configure_mode(mode);
		ctrl_main_start_ctrl(CTRL_MODE_ISOMETER);
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_VOLTMETER:
		ui_ctrl_ledOutOn();

		statemachine_handle.current_mode = mode;
		adc_configure_mode(mode);
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_AMPMETER:
		// Shoot-through on the SEK half-bridge is only safe if there's no
		// significant voltage across it already - refuse to enter otherwise.
		// return (not break) so the shared tail below doesn't reconfigure
		// the relays for a mode we just refused to actually enter.
		if (adc_data.converted.v_term >= 500) { // mV
			return;
		}
		ui_ctrl_ledOutOn();
		statemachine_handle.current_mode = mode;
		adc_configure_mode(mode);
		hrtim_sek_force_short();
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_CHARGE:
		// Interlock (refuse if a fault is already latched) lives in the
		// auto-detect check in statemachine_step()'s IDLE case, since entry
		// here *is* the auto-detect firing - starts immediately, like
		// RESISTANCE_1A/1mA, since charging isn't a "hold a button" action.
		ui_ctrl_ledOutOn();
		statemachine_handle.current_mode = mode;
		statemachine_handle.output_on = 1;
		adc_configure_mode(mode);
		ctrl_main_start_ctrl(statemachine_mode_to_ctrl_mode(mode));
		aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 1);
		display_enter_mode(mode);
		break;

	case STATEMACHINE_MODE_SETTINGS:
		statemachine_handle.current_mode = mode;
		statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
		statemachine_handle.current_menu_index = STATEMACHINE_SETTINGS_MODE_BMS;
		display_show_settings_list(statemachine_handle.current_menu_index);
		break;

	case 0:
		statemachine_switchtoIdle();
		break;
	default:
		break;
	}
	aux_io_ctrl_set_config(mode);
	input_encoder_reset(0);

}

void statemachine_switchtoIdle(void) {

	ctrl_main_stop_control();
	printf("Switch to Idle\r\n");
	aux_io_ctrl_set_config(STATEMACHINE_IDLE);
	aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
	hrtim_sek_restore(); // no-op unless AMPMETER left the SEK half-bridge shorted
	input_encoder_reset(62);
	statemachine_handle.current_mode = STATEMACHINE_IDLE;
	statemachine_handle.current_menu_index = 0xFF; /* force a redraw on the next tick */
	ui_ctrl_ledOutOff();
	ui_ctrl_ledSenseOff();
}
