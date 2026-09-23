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

/* Remembers protection_get_error_mask() while a protection fault is forcing
 * the converter off, so that if CHARGE is ultimately left because of it (see
 * the protection_forced_output_off recovery branch in statemachine_step()),
 * the debug print below can still report which source(s) caused it -- by
 * the time the fault actually clears and the mode transition happens, the
 * live mask has already gone back to 0. */
static uint16_t charge_protection_exit_mask;

/* Main-context view of the multi-phase CHARGE sequence, see charge_seq.h. */
static charge_seq_t charge_seq;

/* Set when CHARGE is left for a reason that would simply recur (ESC,
 * protection, BMS fault, stuck-open abort): the IDLE auto-entry rule would
 * otherwise restart CHARGE on the very next tick while the charger is still
 * connected. Cleared once the charger is removed (see statemachine_step()). */
static uint8_t charge_lockout;

#ifdef CHARGE_DEBUG
/* One-shot "precharge is done, waiting for chargeNext" notice, since the
 * periodic debug print alone doesn't call out that the hold is deliberate.
 * Reset on every CHARGE entry. */
static uint8_t charge_await_notified;
#endif

// Auto power-off after this long with no user interaction, in ANY mode. In
// ticks (STATEMACHINE_STEP_PERIOD_mS each) rather than milliseconds so it
// doesn't need a wider type: 10 min / 20 ms = 30000, comfortably inside
// uint16_t (max 65535, i.e. up to ~21.8 min) with room to retune.
#define STATEMACHINE_INACTIVITY_TIMEOUT_TICKS ((10UL * 60UL * 1000UL) / STATEMACHINE_STEP_PERIOD_mS)

// Ticks since the encoder last moved, a button was last pressed/held, or a
// control loop was last actively running (see statemachine_step()'s use of
// this). Reset on every mode entry (statemachine_switchtoIdle() and
// statemachine_switchfromIdle()'s shared tail) so a near-expired count from
// a previous mode doesn't carry over into a freshly entered one.
static uint16_t inactivity_ticks;

// Raw encoder count as of the last tick, to detect movement independently of
// what any given mode does with it (menu index, setpoint, calibration
// selection, ...) -- see statemachine_step().
static uint16_t last_raw_encoder;

void statemachine_switchfromIdle(statemachine_modes_t mode);
void statemachine_switchtoIdle(void);
static void statemachine_apply_encoder_setpoint(void);
static void statemachine_step_calibration(void);
static void statemachine_enter_mode_generic(statemachine_modes_t mode);
static uint8_t statemachine_enter_charge(void);
static uint8_t statemachine_enter_charge_low_current(void);
static void statemachine_step_charge(void);
static void statemachine_charge_lockout(void);

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

	timer_add(STATEMACHINE_STEP_PERIOD_mS, TIMER_TYPE_TICK, EVENT_SM_STEP, 0);
	statemachine_switchtoIdle();
}

static input_encoder_clamp_t encoder_setpoint_clamp;

static void statemachine_apply_encoder_setpoint(void) {
	int32_t max = (statemachine_handle.current_mode == STATEMACHINE_MODE_ISOMETER) ? 3 : 127;
	ctrl_main_handle.poti_reference = (uint16_t) input_encoder_read_clamped(
			&encoder_setpoint_clamp, 0, max);
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
		uint8_t idx = (input_encoder_read()) % CAL_CH_COUNT;
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
				// stored.
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

	// Due to race conditions
	adc_snapshot_converted();
	adc_convert_data();
	ui_ctrl_step();

	// Update protection and turn off if necessary
	protection_update(statemachine_handle.current_mode);

	// Unconditional every tick (not just while STATEMACHINE_MODE_CHARGE is
	// active) -- see balancing.c: this guarantees the balancing mask gets
	// written back to 0 on the tick after charging stops for any reason.
	balancing_update();

	if (statemachine_mode_to_ctrl_mode(statemachine_handle.current_mode) != CTRL_MODE_OFF) {
		if (protection_get_worst_level() == PROTECTION_LEVEL_ERROR) {
			if (ctrl_main_handle.mode != CTRL_MODE_OFF) {
				ctrl_main_stop_control();
				aux_io_ctrl_manual_set_io(GPIO_HV_CTRL_EN , 0);
				aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
			}
			statemachine_handle.output_on = 0;
			statemachine_handle.protection_forced_output_off = 1;
			charge_protection_exit_mask = protection_get_error_mask();
		} else if (statemachine_handle.protection_forced_output_off) {
			statemachine_handle.protection_forced_output_off = 0;
			if (statemachine_handle.current_mode == STATEMACHINE_MODE_CHARGE) {
				static const protection_source_t PROTECTION_SOURCES[] = {
						PROTECTION_SRC_TEMP_SEC, PROTECTION_SRC_TEMP_TRAFO,
						PROTECTION_SRC_TEMP_CURRENT, PROTECTION_SRC_TEMP_PRIM,
						PROTECTION_SRC_OVP, PROTECTION_SRC_OCP };
				printf("CHARGE exit: protection cleared, was forced off by:");
				for (unsigned i = 0; i < sizeof(PROTECTION_SOURCES) / sizeof(PROTECTION_SOURCES[0]); i++) {
					if (charge_protection_exit_mask & PROTECTION_SOURCES[i])
						printf(" %s", protection_source_name(PROTECTION_SOURCES[i]));
				}
				printf(" (mask=0x%04X)\r\n", charge_protection_exit_mask);
				statemachine_charge_lockout();
				statemachine_switchtoIdle();
			}
		}
	} else {
		statemachine_handle.protection_forced_output_off = 0;
	}

	// Handle Buttons press
	if (input_btn_ok())
		ok_button_pressed += 1;
	else if (input_btn_esc())
		esc_button_pressed += 1;
	else {
		ok_button_pressed = 0;
		esc_button_pressed = 0;
	}
	if ((!statemachine_handle.protection_forced_output_off)
			&& (input_btn_out())) {
		if (out_button_pressed < 2)
			out_button_pressed += 1;
	} else
		out_button_pressed = 0;

	// Re-arm the CHARGE auto-entry once the charger has been unplugged.
	if (charge_lockout
			&& adc_data.converted.v_term_ext_mv < CTRL_PARAM_CHARGE_START_VIN_LOW_mV) {
		charge_lockout = 0;
	}

	// Auto power-off, in every mode: reset the timer on any encoder movement,
	// any button press/hold, or a control loop actively running (covers
	// CHARGE, an auto-started RESISTANCE measurement, and 60V/10A/ISOMETER
	// while OUT is held) -- deliberately NOT statemachine_handle.output_on,
	// which mode_table.c documents as left stale across a mode switch for
	// passive-readout modes (VOLTMETER's entry there), unlike
	// ctrl_main_handle.mode, which ctrl_main_stop_control() (called from
	// statemachine_switchtoIdle()) reliably clears to CTRL_MODE_OFF every
	// time IDLE is (re-)entered. STATEMACHINE_MODE_SHUTDOWN never becomes a
	// lasting current_mode (see statemachine_switchfromIdle()), so it needs
	// no case here.
	uint16_t raw_encoder = input_encoder_read();
	uint8_t active = (raw_encoder != last_raw_encoder)
			|| ok_button_pressed || esc_button_pressed || out_button_pressed
			|| ctrl_main_handle.mode != CTRL_MODE_OFF;
	last_raw_encoder = raw_encoder;
	if (active) {
		inactivity_ticks = 0;
	} else if (++inactivity_ticks >= STATEMACHINE_INACTIVITY_TIMEOUT_TICKS) {
		printf("Auto power-off: untouched for %lus in mode %d\r\n",
				(unsigned long) STATEMACHINE_INACTIVITY_TIMEOUT_TICKS
						* STATEMACHINE_STEP_PERIOD_mS / 1000UL,
				statemachine_handle.current_mode);
		gpio_power_off();
		// Only reached if power didn't actually cut (see gpio_power_off()):
		// wait out another full timeout instead of retrying every tick.
		inactivity_ticks = 0;
	}

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
			statemachine_switchfromIdle(STATEMACHINE_MODE_SHUTDOWN);

		// CHARGE is auto-entered, not menu/button-selected: whenever idle and
		// a 15-24V supply is detected at the output with no BMS fault
		// latched, start charging on our own -- unless the last charge was
		// ended by ESC/a fault and the charger has not been removed since
		// (charge_lockout), or the BMS has not delivered a plausible stack
		// voltage yet (it reads 0 before the first poll, which would pass the
		// "below end voltage" test below).
		if (!charge_lockout
				&& charge_seq_v_term_in_window(adc_data.converted.v_term_ext_mv)
				&& !(bms.SafetyRegisters.safetyStatusA || bms.SafetyRegisters.safetyStatusB)
				&& bms.VoltageRegisters.StackVoltage >= CTRL_PARAM_STACK_VOLTAGE_MIN_VALID_mV
				&& bms.VoltageRegisters.StackVoltage <= CTRL_PARAM_STACK_VOLTAGE_MAX_VALID_mV
				&& bms.VoltageRegisters.StackVoltage
						< CTRL_PARAM_CHARGE_END_VOLTAGE_mV - CTRL_PARAM_CHARGE_RESTART_MARGIN_mV) {
			statemachine_switchfromIdle(STATEMACHINE_MODE_CHARGE);
		} else if (!charge_lockout
				&& (bms.SafetyRegisters.safetyStatusA & BQ76905_SAFETY_STATUS_A_CUV)
				&& !(bms.SafetyRegisters.safetyStatusA & (uint8_t) ~BQ76905_SAFETY_STATUS_A_CUV)
				&& !bms.SafetyRegisters.safetyStatusB
				&& charge_seq_v_term_in_window(adc_data.converted.v_term_ext_mv)) {
			// Deep-discharge/CUV recovery: called directly, not through
			// statemachine_switchfromIdle() -- see
			// statemachine_enter_charge_low_current()'s comment.
			statemachine_enter_charge_low_current();
		}
		break;

	case STATEMACHINE_MODE_60V_OUT:
	case STATEMACHINE_MODE_10A_OUT:
	case STATEMACHINE_MODE_ISOMETER:
		statemachine_apply_encoder_setpoint();
		if (out_button_pressed == 1) {
			statemachine_handle.output_on = 1;
			ctrl_main_start_ctrl(
					statemachine_mode_to_ctrl_mode(
							statemachine_handle.current_mode));
			aux_io_ctrl_manual_set_io(
					mode_table[statemachine_handle.current_mode].enable_gpio,
					1);
		} // Second if necessary, as out_button is 2 while pressed.
		else if (out_button_pressed == 0) {
			statemachine_handle.output_on = 0;
			ctrl_main_stop_control();
			aux_io_ctrl_manual_set_io(
					mode_table[statemachine_handle.current_mode].enable_gpio,
					0);
		}
		/* fall through to update display !! */
	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
	case STATEMACHINE_MODE_VOLTMETER:
	case STATEMACHINE_MODE_AMPMETER:
		display_update_mode(statemachine_handle.current_mode, statemachine_handle.output_on);
		break;

	case STATEMACHINE_MODE_CHARGE:
		statemachine_step_charge();
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
		} else if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_CALIBRATION) {
			statemachine_step_calibration();
			return;
		} else {
			display_update_settings_detail(statemachine_handle.settings_mode);
			if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_BMS
					&& ok_button_pressed == 1) {
				balancing_toggle_manual_override();
			}
			if (esc_button_pressed == 1) {
				if (statemachine_handle.settings_mode == STATEMACHINE_SETTINGS_MODE_BMS) {
					balancing_clear_manual_override();
				}
				statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
				display_show_settings_list(statemachine_handle.current_menu_index);
				input_encoder_reset(statemachine_handle.settings_mode-STATEMACHINE_SETTINGS_MODE_BMS);
				return;
			}
		}
		break;

	default:
		break;
	}
	if (esc_button_pressed == 1) {
		if (statemachine_handle.current_mode == STATEMACHINE_MODE_CHARGE) {
			printf("CHARGE exit: ESC pressed by user\r\n");
			statemachine_charge_lockout();
		}
		statemachine_switchtoIdle();
	}

}

static void statemachine_charge_lockout(void) {
	charge_lockout = 1;
	printf("CHARGE locked out until the charger is removed\r\n");
}

#ifdef CHARGE_DEBUG
/* charge_seq's `manual` flag lives on the static `charge_seq` instance below,
 * which charge_seq_init() (called on every CHARGE entry) does not touch -- so
 * this persists across CHARGE (re-)starts on its own; no separate flag needed
 * here. */
void statemachine_charge_test_set_manual(uint8_t enable) {
	charge_seq_set_manual(&charge_seq, enable);
	printf("CHARGE test mode: single-step %s\r\n", enable
			? "enabled -- will hold after precharge (relay still open) until chargeNext"
			: "disabled");
}

uint8_t statemachine_charge_test_advance(void) {
	if (statemachine_handle.current_mode != STATEMACHINE_MODE_CHARGE) {
		printf("Not charging\r\n");
		return 0;
	}
	if (!charge_seq_awaiting_advance(&charge_seq)) {
		printf("CHARGE: nothing to advance right now (phase=%s)\r\n",
				charge_seq_phase_name(ctrl_main_handle.charge_phase));
		return 0;
	}
	charge_seq_request_advance(&charge_seq);
	printf("CHARGE: advancing -- closing the output relay and starting the current ramp\r\n");
	return 1;
}
#endif /* CHARGE_DEBUG */

/* Per-tick CHARGE handling: phase sequencing (charge_seq.h), debug output and
 * the exit conditions. Runs from statemachine_step()'s CHARGE case. */
static void statemachine_step_charge(void) {
	display_update_mode(statemachine_handle.current_mode,
			statemachine_handle.output_on);

	// Phase sequencing. Not while a protection fault has the converter forced
	// off: control is stopped then, and the recovery branch at the top of
	// statemachine_step() ends CHARGE once the fault clears.
	if (!statemachine_handle.protection_forced_output_off) {
		charge_seq_in_t in = {
			.isr_phase = ctrl_main_handle.charge_phase,
			.ref_mA = ctrl_main_charge_reference_mA(),
			.charge_mA = -adc_data.converted.i_out_ext_mA,
		};
		switch (charge_seq_step(&charge_seq, &in, STATEMACHINE_STEP_PERIOD_mS)) {
		case CHG_ACT_CLOSE_RELAY:
			// The relays are normally closed: OUT_SEL_HV low = K1 closed, which
			// connects the converter output to the charger. The hand-over
			// steps the SEK duty to the zero-current duty at the same moment,
			// so OUT_LV is already at the charger voltage when the contacts
			// meet (and the current loop starts from ~0 A).
			aux_io_ctrl_manual_set_io(GPIO_OUT_SEL_HV, 0);
			ctrl_main_charge_handover(adc_data.converted.v_in,
					adc_data.converted.v_term_ext_mv_filt);
#ifdef CHARGE_DEBUG
			printf("CHARGE: precharge done (V_IN=%ldmV, V_TERM=%ldmV), relay closing\r\n",
					adc_data.converted.v_in, adc_data.converted.v_term_ext_mv_filt);
#endif
			break;
		case CHG_ACT_START_RAMP:
			ctrl_main_charge_start_ramp();
#ifdef CHARGE_DEBUG
			printf("CHARGE: relay settled, current ramp started\r\n");
#endif
			break;
		case CHG_ACT_ABORT_STUCK_OPEN:
			printf("CHARGE exit: no charge current although the reference is up "
					"(relay stuck open, or charger gone)\r\n");
			statemachine_charge_lockout();
			statemachine_switchtoIdle();
			return;
		case CHG_ACT_NONE:
		default:
			break;
		}

#ifdef CHARGE_DEBUG
		if (charge_seq_awaiting_advance(&charge_seq)) {
			if (!charge_await_notified) {
				charge_await_notified = 1;
				printf("CHARGE: precharge complete, holding (relay still open) "
						"-- send 'chargeNext' to close it and start the current ramp\r\n");
			}
		} else {
			charge_await_notified = 0;
		}
#endif

		// Deep-discharge recovery: once the BMS clears CUV, ramp the current
		// reference from the reduced recovery target back up to the normal
		// one, in place -- K1 never opened, so no relay recycle or restart
		// is needed. ctrl_main_handle.ramp (the ISR's own start-of-charge
		// ramp) is already saturated past 1.0 by now, so it won't do this on
		// its own -- this steps current_reference_mA directly, once per 20ms
		// tick, over CTRL_PARAM_CHARGE_RAMP_s.
		if (charge_seq.low_current_recovery
				&& !(bms.SafetyRegisters.safetyStatusA & BQ76905_SAFETY_STATUS_A_CUV)) {
			uint32_t step_mA = (uint32_t) ((CTRL_PARAM_CHARGE_CURRENT_mA
					- CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA)
					* STATEMACHINE_STEP_PERIOD_mS / (CTRL_PARAM_CHARGE_RAMP_s * 1000.0F));
			if (step_mA == 0)
				step_mA = 1; // guard against a ramp time too short for this current delta
			if (ctrl_main_handle.current_reference_mA + step_mA >= CTRL_PARAM_CHARGE_CURRENT_mA) {
				ctrl_main_handle.current_reference_mA = CTRL_PARAM_CHARGE_CURRENT_mA;
				charge_seq.low_current_recovery = 0;
				printf("CHARGE: BMS undervoltage cleared, ramped to full current\r\n");
			} else {
				ctrl_main_handle.current_reference_mA += step_mA;
			}
		}
	}

	// Only the wired cells: CellVoltages[4] is unused and reads 0, which
	// would make the spread check below fail forever.
	uint16_t cell_min = 0xFFFF;
	uint16_t cell_max = 0;
	for (uint8_t i = 0; i < BQ76905_PACK_CELL_COUNT; i++) {
		if (bms.CellVoltageRegisters.CellVoltages[i] > cell_max) {
			cell_max = bms.CellVoltageRegisters.CellVoltages[i];
		}
		if (bms.CellVoltageRegisters.CellVoltages[i] < cell_min) {
			cell_min = bms.CellVoltageRegisters.CellVoltages[i];
		}
	}

#ifdef CHARGE_DEBUG
	// Debug output. It used to be printed from the control ISR, where the
	// blocking UART write stalled the 25 kHz loop for several milliseconds.
	int32_t peak_mA;
	if (ctrl_main_charge_take_peak(&peak_mA)) {
		printf("CHARGE: peak |I_OUT| after the relay-close command: %ldmA\r\n", peak_mA);
	}
	static uint8_t print_div;
	if (++print_div >= 5) { // ~10 Hz at the 20 ms tick
		print_div = 0;
		printf("Charging: %s, I_OUT: %5d, I_OUT_ext_mA: %5ld, V_IN: %5ld, V_OUT: %5ld, "
				"V_TERM: %5ld, Stack: %5u, max(cell): %4u\r\n",
				charge_seq_phase_name(ctrl_main_handle.charge_phase),
				-adc_data.converted.i_out, -adc_data.converted.i_out_ext_mA,
				adc_data.converted.v_in, adc_data.converted.v_out,
				adc_data.converted.v_term_ext_mv,
				bms.VoltageRegisters.StackVoltage, cell_max);
	}
#endif /* CHARGE_DEBUG */

	// Exit conditions. End of charge only counts once current is actually
	// flowing (CC_RAMP/CV); everything else applies in every phase.
	uint8_t charge_completed = charge_seq_may_complete(&charge_seq)
			&& bms.VoltageRegisters.StackVoltage >= CTRL_PARAM_CHARGE_END_VOLTAGE_mV
			&& -adc_data.converted.i_out_ext_mA <= CTRL_PARAM_CHARGE_TAPER_CURRENT_mA
			&& (cell_max - cell_min <= 50);
	// While recovering from a deep-discharge CUV fault, CUV is exactly the
	// condition this run exists to clear -- it must not itself abort the
	// charge that's trying to recover from it (see
	// statemachine_enter_charge_low_current()). Any OTHER fault bit still
	// aborts immediately, same as a normal charge.
	uint8_t fault_mask_a = bms.SafetyRegisters.safetyStatusA;
	if (charge_seq.low_current_recovery) {
		fault_mask_a &= (uint8_t) ~BQ76905_SAFETY_STATUS_A_CUV;
	}
	uint8_t bms_fault = fault_mask_a || bms.SafetyRegisters.safetyStatusB;
	uint8_t supply_low = adc_data.converted.v_term_ext_mv < CTRL_PARAM_CHARGE_STOP_VIN_mV;

	if (charge_completed || bms_fault || supply_low) {
		if (charge_completed) {
			printf("CHARGE exit: end voltage reached and current tapered off "
					"(stack=%umV >= end=%dmV, i_out=%ldmA <= taper=%dmA)\r\n",
					bms.VoltageRegisters.StackVoltage, CTRL_PARAM_CHARGE_END_VOLTAGE_mV,
					-adc_data.converted.i_out_ext_mA, CTRL_PARAM_CHARGE_TAPER_CURRENT_mA);
		}
		if (fault_mask_a) {
			printf("CHARGE exit: BMS safetyStatusA=0x%02X\r\n", fault_mask_a);
		}
		if (bms.SafetyRegisters.safetyStatusB) {
			printf("CHARGE exit: BMS safetyStatusB=0x%02X\r\n", bms.SafetyRegisters.safetyStatusB);
		}
		if (supply_low) {
			printf("CHARGE exit: supply voltage dropped (v_term_ext_mv=%ldmV < stop=%dmV)\r\n",
					adc_data.converted.v_term_ext_mv, CTRL_PARAM_CHARGE_STOP_VIN_mV);
		}
		// Only a finished charge means "full"; a fault, a supply drop or
		// ESC must not declare the pack 100% or discard the accumulated charge.
		if (charge_completed) {
			BQ76905_resetChargeAccumulator(&bms);
			bms.Accumulator.accumulatedCharge = 0;
			bms.charge_percentage = 100;
		}
		if (bms_fault) {
			statemachine_charge_lockout();
		}
		statemachine_switchtoIdle();
	}
}

/* CHARGE entry, called from statemachine_switchfromIdle(). Unlike the other
 * modes the relays are set BEFORE the converter starts: the sequence begins
 * with K1 (converter output to the jacks) open. Returns 0 (and does nothing)
 * if there is no charger in the allowed window, which also covers the CLI's
 * `state` command forcing CHARGE without one. */
static uint8_t statemachine_enter_charge(void) {
	if (!charge_seq_v_term_in_window(adc_data.converted.v_term_ext_mv)) {
		printf("Unable to switch to CHARGE: charger voltage %ldmV outside %d..%dmV\r\n",
				adc_data.converted.v_term_ext_mv,
				CTRL_PARAM_CHARGE_START_VIN_LOW_mV, CTRL_PARAM_CHARGE_START_VIN_HIGH_mV);
		return 0;
	}

	ui_ctrl_ledOutOn();
	statemachine_handle.current_mode = STATEMACHINE_MODE_CHARGE;
	adc_configure_mode(STATEMACHINE_MODE_CHARGE);

	aux_io_ctrl_set_config(STATEMACHINE_MODE_CHARGE);   // relays open, before any PWM
	charge_seq_init(&charge_seq);
#ifdef CHARGE_DEBUG
	charge_await_notified = 0;
#endif
	ctrl_main_start_ctrl(CTRL_MODE_CHARGE);             // PRECHARGE
	aux_io_ctrl_manual_set_io(mode_table[STATEMACHINE_MODE_CHARGE].enable_gpio, 1);
	statemachine_handle.output_on = 1;

	display_enter_mode(STATEMACHINE_MODE_CHARGE);
	printf("CHARGE: precharging the converter output (V_TERM=%ldmV)\r\n",
			adc_data.converted.v_term_ext_mv);
	return 1;
}

/* Deep-discharge/CUV recovery entry: the BMS has latched a cell undervoltage
 * fault (DSG FET open, V_IN ~0) and the device is running on charger power
 * fed through K1 into OUT_LV -- see statemachine_switchtoIdle(). Called
 * directly from the IDLE auto-entry check below rather than through
 * statemachine_switchfromIdle(), because that function's shared tail ends
 * with a plain aux_io_ctrl_set_config(mode) call which would reopen K1 right
 * after this function closes it off deliberately. Mirrors
 * statemachine_enter_charge() otherwise. */
static uint8_t statemachine_enter_charge_low_current(void) {
	if (!charge_seq_v_term_in_window(adc_data.converted.v_term_ext_mv)) {
		printf("Unable to switch to low-current CHARGE: charger voltage %ldmV "
				"outside %d..%dmV\r\n", adc_data.converted.v_term_ext_mv,
				CTRL_PARAM_CHARGE_START_VIN_LOW_mV, CTRL_PARAM_CHARGE_START_VIN_HIGH_mV);
		return 0;
	}

	ui_ctrl_ledOutOn();
	statemachine_handle.current_mode = STATEMACHINE_MODE_CHARGE;
	adc_configure_mode(STATEMACHINE_MODE_CHARGE);

	aux_io_ctrl_set_config_keep_k1_closed(STATEMACHINE_MODE_CHARGE); // K1 stays closed
	charge_seq_init_low_current_recovery(&charge_seq);
#ifdef CHARGE_DEBUG
	charge_await_notified = 0;
#endif
	ctrl_main_start_ctrl_charge_low_current();          // straight to CC_RAMP
	aux_io_ctrl_manual_set_io(mode_table[STATEMACHINE_MODE_CHARGE].enable_gpio, 1);
	statemachine_handle.output_on = 1;

	input_encoder_reset(0);
	input_encoder_clamp_reset(&encoder_setpoint_clamp, 0);
	inactivity_ticks = 0;

	display_enter_mode(STATEMACHINE_MODE_CHARGE);
	printf("CHARGE: deep-discharge recovery at %dmA (BMS reports CUV, V_TERM=%ldmV)\r\n",
			CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA, adc_data.converted.v_term_ext_mv);
	return 1;
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

	if (d->flags & MODE_F_SEK_FORCE_SHORT) {
		hrtim_sek_force_short();
		aux_io_ctrl_manual_set_io(d->enable_gpio, 1);
	}

	display_enter_mode(mode);
}

void statemachine_switchfromIdle(statemachine_modes_t mode) {
	printf("Switch from Idle to %d\r\n", mode);

	switch (mode) {
	case STATEMACHINE_IDLE:
		statemachine_switchtoIdle();
		break;

	case STATEMACHINE_MODE_AMPMETER:
		// Shoot-through on the SEK half-bridge is only safe if there's no
		// significant voltage across it already - refuse to enter otherwise.
		// return (not break) so the shared tail below doesn't reconfigure
		// the relays for a mode we just refused to actually enter.
		if (adc_data.converted.v_term >= 500) { // mV
			printf("Unable to switch to %d, voltage present at terminals, returning to Idle\r\n", mode);
			display_show_idle_error("ERROR: Remove external Voltage");
			return;
		}
        /* fall through */
	case STATEMACHINE_MODE_ISOMETER:
	case STATEMACHINE_MODE_VOLTMETER:
		// Disable isometer and voltmeter on device #0006, due to welded relay.
		/*if (config_store.hardware_data.serial_number == 6) {
			if (mode == STATEMACHINE_MODE_ISOMETER || mode == STATEMACHINE_MODE_VOLTMETER) {
				printf("Unable to switch to %d due to welded relay, returning to Idle\r\n", mode);
				return;
			}
		}*/
        /* fall through */
	case STATEMACHINE_MODE_60V_OUT:
	case STATEMACHINE_MODE_10A_OUT:
	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
		statemachine_enter_mode_generic(mode);
		break;

	case STATEMACHINE_MODE_CHARGE:
		if (!statemachine_enter_charge())
			return; // refused: skip the shared tail, like AMPMETER above
		break;

	case STATEMACHINE_MODE_SETTINGS:
		statemachine_handle.current_mode = mode;
		statemachine_handle.settings_mode = STATEMACHINE_SETTINGS_MODE_MENU;
		statemachine_handle.current_menu_index = STATEMACHINE_SETTINGS_MODE_BMS;
		display_show_settings_list(statemachine_handle.current_menu_index);
		break;

	case STATEMACHINE_MODE_SHUTDOWN:
		gpio_shutdown();
		break;

	default:
		break;
	}
	aux_io_ctrl_set_config(mode);
	input_encoder_reset(0);
	input_encoder_clamp_reset(&encoder_setpoint_clamp, 0);
	inactivity_ticks = 0;

}

void statemachine_switchtoIdle(void) {

	ctrl_main_stop_control();
	printf("Switch to Idle\r\n");
	// A latched CUV (cell undervoltage) fault means the BMS has opened its
	// DSG FET -- the device may be running purely on charger power fed
	// through K1 (OUT_SEL_HV closed) into OUT_LV, with no battery path at
	// all (see statemachine_enter_charge_low_current()). Opening K1 here,
	// as the normal IDLE relay config does, would cut that power. This is
	// the one chokepoint every CHARGE exit path (fault, stuck-open, ESC,
	// boot) funnels through, so guarding it here covers all of them.
	if (bms.SafetyRegisters.safetyStatusA & BQ76905_SAFETY_STATUS_A_CUV) {
		aux_io_ctrl_set_config_keep_k1_closed(STATEMACHINE_IDLE);
	} else {
		aux_io_ctrl_set_config(STATEMACHINE_IDLE);
	}
	aux_io_ctrl_manual_set_io(GPIO_CONV_CTRL_EN, 0);
	aux_io_ctrl_manual_set_io(GPIO_HV_CTRL_EN, 0);
	hrtim_sek_restore(); // no-op unless AMPMETER left the SEK half-bridge shorted
	dac_sqwave_stop(); // no-op unless RESISTANCE_1A left the DAC square wave running
	input_encoder_reset(63+statemachine_handle.current_mode);
	adc_configure_mode(STATEMACHINE_IDLE);
	statemachine_handle.current_mode = STATEMACHINE_IDLE;
	statemachine_handle.current_menu_index = 0xFF; /* force a redraw on the next tick */
	ui_ctrl_ledOutOff();
	ui_ctrl_ledSenseOff();
	inactivity_ticks = 0;
}
