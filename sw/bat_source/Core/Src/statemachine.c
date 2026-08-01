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
#include "balancing.h"
#include "dac.h"
#include "config_store.h"
#include "mode_table.h"

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
static void statemachine_enter_mode_generic(statemachine_modes_t mode);

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
		[CAL_CH_V_IN] = 500.0f,     // mV/detent, max ~63V (battery stack
		                            // rail, nominal ~14V, ADC full scale
		                            // ~51V per ADC_VIN_GAIN_MV)
};

void statemachine_init(void) {
	statemachine_handle.output_on = 0;
	statemachine_handle.current_mode = STATEMACHINE_IDLE;
	statemachine_handle.current_menu_index = 0;
	statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
	ok_button_pressed = 0;
	esc_button_pressed = 0;
	statemachine_handle.protection_forced_output_off = 0;

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
 * channel list.
 *
 * ADC arming: calibration_ensure_adc_mode() is relatively expensive (stops/
 * reconfigures/restarts up to three ADC DMA streams) and must run only when
 * the *armed* channel actually changes, never once per tick -- this
 * function is the only place that both owns calibration_selected_channel
 * and can tell when that happens (initial entry into the calibration UI,
 * and the encoder-driven selection change in case 0), so it -- not
 * display.c, which only renders whatever calibration_peek_*() already
 * finds in adc_data -- is where the calls belong. Symmetrically, leaving
 * the calibration UI entirely (case 0's ESC) restores current_mode's own
 * ADC routing (STATEMACHINE_MODE_SETTINGS's, unchanged for as long as the
 * calibration UI is showing -- see statemachine_switchfromIdle()'s
 * STATEMACHINE_MODE_SETTINGS case and statemachine_step()'s SETTINGS case,
 * neither of which reassign current_mode while navigating Settings/
 * Calibration) so the Settings list, and whatever real mode is picked
 * next, aren't left with calibration's V_OUT/ISOMETER routing. */
static void statemachine_step_calibration(void) {
	switch (calibration_ui_state) {
	case 0: { // channel list
		uint8_t idx = (input_encoder_read() / 2) % CAL_CH_COUNT;
		if (idx != calibration_selected_channel) {
			calibration_selected_channel = (calibration_channel_t) idx;
			calibration_ensure_adc_mode(calibration_selected_channel);
		}
		display_calibration_update(calibration_selected_channel, 0, 0.0f);
		if (ok_button_pressed == 1) {
			calibration_ui_state = 1;
			display_calibration_enter(calibration_selected_channel, 1);
		}
		if (esc_button_pressed == 1) {
			statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
			adc_configure_mode(statemachine_handle.current_mode);
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
			if (calibration_set_gain(calibration_selected_channel, reference_value)
					== CALIBRATION_STATUS_OK) {
				calibration_ui_state = 0;
				display_calibration_enter(calibration_selected_channel, 0);
			} else {
				// Invalid gain (see calibration_set_gain()) -- nothing was
				// stored. Stay on the gain step (calibration_ui_state
				// unchanged) so the user can dial in a real reference and
				// retry, rather than silently returning to the channel
				// list as if it had succeeded. ui_state 3 is a one-shot
				// failure message occupying the same lines display_calibration_enter()'s
				// ui_state 2 uses for its static instructions; the next
				// tick's case 2 above keeps calling display_calibration_update(...,
				// 2, ...), which only ever touches the live-reading lines
				// further down, so the message stays up until the user
				// retries or backs out.
				display_calibration_enter(calibration_selected_channel, 3);
			}
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

	// Coherent, race-free copy of the ISR-written "converted" fast fields
	// into adc_data.converted, taken once per tick under a short critical
	// section (see adc_snapshot_converted()). Must run before
	// adc_convert_data(), since that also reads some of those fast fields
	// (i_out_ext_mA/v_sens_ext_uv/v_term_ext_mv) while computing r_mOhmx10 /
	// r_Ohmx10, and everything below this line (protection_update(),
	// display_*()) relies on adc_data.converted being self-consistent too.
	adc_snapshot_converted();
	adc_convert_data();
	protection_update(statemachine_handle.current_mode);

	// Unconditional every tick (not just while STATEMACHINE_MODE_CHARGE is
	// active) -- see balancing.c: this guarantees the balancing mask gets
	// written back to 0 on the tick after charging stops for any reason.
	balancing_update();
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
   if ((!statemachine_handle.protection_forced_output_off)&&(input_btn_out())) {
		if (out_button_pressed < 2)
			out_button_pressed += 1;
	} else
		out_button_pressed = 0;

	// Select transition to next state
	switch (statemachine_handle.current_mode) {
	case STATEMACHINE_IDLE:
		temp = (input_encoder_read()) % MENU_ORDER_LENGTH;
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
		if (adc_data.converted.v_term_ext_mv >= CTRL_PARAM_CHARGE_START_VIN_LOW_mV
				&& adc_data.converted.v_term_ext_mv <= CTRL_PARAM_CHARGE_START_VIN_HIGH_mV
				&& !(bms.SafetyRegisters.safetyStatusA || bms.SafetyRegisters.safetyStatusB)
				&& bms.VoltageRegisters.StackVoltage
						< CTRL_PARAM_CHARGE_END_VOLTAGE_mV - CTRL_PARAM_CHARGE_RESTART_MARGIN_mV) {
			statemachine_switchfromIdle(STATEMACHINE_MODE_CHARGE);
		}
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
		display_update_mode(statemachine_handle.current_mode, statemachine_handle.output_on);
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_ISOMETER:
		statemachine_apply_encoder_setpoint();
		if (out_button_pressed == 1) {
					statemachine_handle.output_on = 1;
					ctrl_main_start_ctrl(statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode));
					aux_io_ctrl_manual_set_io(GPIO_HV_CTRL_EN, 1);
				} else if (out_button_pressed == 0) {
					statemachine_handle.output_on = 0;
					ctrl_main_stop_control();
					aux_io_ctrl_manual_set_io(GPIO_HV_CTRL_EN, 0);
				}
		display_update_mode(statemachine_handle.current_mode, statemachine_handle.output_on);
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
		display_update_mode(statemachine_handle.current_mode,
				statemachine_handle.output_on);
		if (bms.VoltageRegisters.StackVoltage >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV) {
			BQ76905_resetChargeAccumulator(&bms);
			bms.Accumulator.accumulatedCharge = 0;
			bms.charge_percentage = 100;
		}
		// Successfully finished once voltage has reached the end voltage AND
		// current has tapered off (charge current is negated elsewhere, e.g.
		// -adc_data.converted.i_out_ext_mA, so mirror that sign convention
		// here). Safety-fault and supply-removed conditions still stop
		// immediately on their own, regardless of taper.
		if ((bms.VoltageRegisters.StackVoltage >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV
					&& -adc_data.converted.i_out_ext_mA <= CTRL_PARAM_CHARGE_TAPER_CURRENT_mA)
				|| bms.SafetyRegisters.safetyStatusA || bms.SafetyRegisters.safetyStatusB
				|| adc_data.converted.v_term_ext_mv < CTRL_PARAM_CHARGE_STOP_VIN_mV) {
			statemachine_switchtoIdle();
		}
		if (esc_button_pressed == 1) {
			statemachine_switchtoIdle();
		}
		break;

	case STATEMACHINE_MODE_SETTINGS:
		if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_MENU) {
			temp = STATEMACHINE_SETTINGS_MODE_BMS
					+ (input_encoder_read())
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
					calibration_ensure_adc_mode(calibration_selected_channel);
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

	ctrl_mode_t active_ctrl_mode = statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode);

	if (active_ctrl_mode != CTRL_MODE_OFF) {
		if (protection_get_worst_level() == PROTECTION_LEVEL_ERROR) {
			if (ctrl_main_handle.mode != CTRL_MODE_OFF) {
				ctrl_main_stop_control();
				aux_io_ctrl_manual_set_io(GPIO_HV_CTRL_EN , 0);
				aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
			}
			statemachine_handle.output_on = 0;
			statemachine_handle.protection_forced_output_off = 1;
		} else if (statemachine_handle.protection_forced_output_off) {
			statemachine_handle.protection_forced_output_off = 0;
			if (statemachine_handle.current_mode == STATEMACHINE_MODE_CHARGE) {
				statemachine_switchtoIdle();
			}
		}
	} else {
		statemachine_handle.protection_forced_output_off = 0;
	}
}

/* Runs the shared, mode_table[]-driven entry sequence used by every real
 * mode except IDLE (special-cased: redirects to statemachine_switchtoIdle()
 * instead), SETTINGS (special-cased: sets up its own submenu state) and
 * AMPMETER's refusal check (handled by the caller *before* this runs - the
 * rest of AMPMETER's entry, including MODE_F_SEK_FORCE_SHORT below, is
 * ordinary table-driven behaviour). This is a direct transcription of what
 * each pre-refactor case in this switch did; see mode_table.h for the
 * per-flag rationale and mode_table.c for the per-mode data it was built
 * from. Only ever invoked from case labels naming a real mode, so mode is
 * always < STATEMACHINE_MODE_RESERVED here -- the check below is pure
 * defense-in-depth for mode_table[]'s indexing contract, not a reachable
 * path. */
static void statemachine_enter_mode_generic(statemachine_modes_t mode) {
	if (mode >= STATEMACHINE_MODE_RESERVED) {
		return;
	}
	const mode_descriptor_t *d = &mode_table[mode];

	if (d->flags & MODE_F_LED_OUT_ON)
		ui_ctrl_ledOutOn();
	if (d->flags & MODE_F_LED_SENSE_ON)
		ui_ctrl_ledSenseOn();

	statemachine_handle.current_mode = mode;
	adc_configure_mode(mode);

	// Must run after adc_configure_mode(mode): every pre-refactor case that
	// auto-starts the control loop configured the ADC first.
	if (d->flags & MODE_F_AUTOSTART_CTRL) {
		ctrl_main_start_ctrl(d->ctrl_mode);
		aux_io_ctrl_manual_set_io(d->enable_gpio, 1);
	}
	if (d->flags & MODE_F_DAC_SQWAVE)
		dac_sqwave_start(DAC_CHANNEL_2, config_store.calibration.i_1a_ref_dac_value);

	if (d->flags & MODE_F_OUTPUT_ON_ZERO)
		statemachine_handle.output_on = 0;
	if (d->flags & MODE_F_OUTPUT_ON_ONE)
		statemachine_handle.output_on = 1;

	if (d->flags & MODE_F_SEK_FORCE_SHORT)
		hrtim_sek_force_short();

	display_enter_mode(mode);
}

void statemachine_switchfromIdle(statemachine_modes_t mode) {
	printf("Switch from Idle to %d\r\n", mode);

	switch (mode) {
	case STATEMACHINE_IDLE:
		// mode 0 (== STATEMACHINE_IDLE) is never actually reached in
		// practice - it isn't a selectable menu entry - but redirects to
		// statemachine_switchtoIdle() instead of the generic sequence,
		// matching the pre-refactor switch's "case 0: ...; break;". Still
		// falls through to the shared tail below, same as before.
		statemachine_switchtoIdle();
		break;

	case STATEMACHINE_MODE_AMPMETER:
		// Shoot-through on the SEK half-bridge is only safe if there's no
		// significant voltage across it already - refuse to enter otherwise.
		// return (not break) so the shared tail below doesn't reconfigure
		// the relays for a mode we just refused to actually enter.
		if (adc_data.converted.v_term >= 500) { // mV
			return;
		}
		statemachine_enter_mode_generic(mode);
		break;

	case STATEMACHINE_MODE_60V_OUT:
	case STATEMACHINE_MODE_10A_OUT:
	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
	case STATEMACHINE_MODE_ISOMETER:
	case STATEMACHINE_MODE_VOLTMETER:
	case STATEMACHINE_MODE_CHARGE:
		// CHARGE's interlock (refuse if a fault is already latched) lives in
		// the auto-detect check in statemachine_step()'s IDLE case, since
		// entry here *is* the auto-detect firing.
		statemachine_enter_mode_generic(mode);
		break;

	case STATEMACHINE_MODE_SETTINGS:
		statemachine_handle.current_mode = mode;
		statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
		statemachine_handle.current_menu_index = STATEMACHINE_SETTINGS_MODE_BMS;
		display_show_settings_list(statemachine_handle.current_menu_index);
		break;

	default:
		// Covers STATEMACHINE_MODE_SHUTDOWN/STATEMACHINE_MODE_RESERVED and
		// any out-of-range value - matches the pre-refactor switch's
		// default: no-op (SHUTDOWN is never entered through here in
		// practice; statemachine_step()'s ESC handling assigns
		// current_mode directly instead).
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
	dac_sqwave_stop(); // no-op unless RESISTANCE_1A left the DAC square wave running
	input_encoder_reset(62);
	statemachine_handle.current_mode = STATEMACHINE_IDLE;
	statemachine_handle.current_menu_index = 0xFF; /* force a redraw on the next tick */
	ui_ctrl_ledOutOff();
	ui_ctrl_ledSenseOff();
}
