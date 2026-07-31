/*
 * mode_table.c
 *
 * See mode_table.h for the field/flag rationale. Each entry below is a
 * direct transcription of what the pre-refactor code did for that mode in
 * statemachine.c's statemachine_switchfromIdle(), ctrl_main.c's
 * statemachine_mode_to_ctrl_mode(), adc.c's adc_configure_mode() and
 * aux_io_ctrl.c's aux_io_ctrl_mode_config[] -- see the per-entry comments.
 *
 *  Created on: Jul 31, 2026
 */

#include "mode_table.h"
#include "aux_io_ctrl.h"
#include "adc.h"

const mode_descriptor_t mode_table[STATEMACHINE_MODE_RESERVED] = {

    /* Never actually reached through statemachine_switchfromIdle() (it isn't
     * a selectable menu entry and is instead special-cased there, redirecting
     * to statemachine_switchtoIdle()) - aux_io_mask/ctrl_mode are still real,
     * used by aux_io_ctrl_set_config(STATEMACHINE_IDLE) (called directly from
     * statemachine_switchtoIdle()) and by the statemachine_mode_to_ctrl_mode()
     * lookup while current_mode == STATEMACHINE_IDLE. */
    [STATEMACHINE_IDLE] = {
        .ctrl_mode   = CTRL_MODE_OFF,
        .aux_io_mask = GPIO_MASK_OUT_SEL_HV | GPIO_MASK_SHUNT_EN,
        .enable_gpio = 0,
        .flags       = 0,
        .adc_trigger = ADC_TRIGGER_NONE,
    },

    /* Hold-OUT-to-enable: ctrl_main_start_ctrl()/GPIO_CONV_CTRL_EN are
     * asserted from statemachine_step()'s per-tick OUT handling, not here
     * (no MODE_F_AUTOSTART_CTRL) - hence output_on is forced to 0 at entry. */
    [STATEMACHINE_MODE_60V_OUT] = {
        .ctrl_mode   = CTRL_MODE_60V,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_CONV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_OUTPUT_ON_ZERO,
        .adc_trigger = ADC_TRIGGER_HRTIM_SEK,
    },

    /* Same shape as 60V_OUT -- hold-OUT-to-enable, output_on forced to 0. */
    [STATEMACHINE_MODE_10A_OUT] = {
        .ctrl_mode   = CTRL_MODE_10A,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_CONV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_OUTPUT_ON_ZERO,
        .adc_trigger = ADC_TRIGGER_HRTIM_PRIM,
    },

    /* Auto-starts the control loop and the ~1A DAC test pulse immediately on
     * entry (not hold-OUT) - output_on is left untouched by design (matches
     * the pre-refactor case, which never assigned it either). */
    [STATEMACHINE_MODE_RESISTANCE_1A] = {
        .ctrl_mode   = CTRL_MODE_RESISTANCE_1A,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_CONV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_LED_SENSE_ON
                     | MODE_F_AUTOSTART_CTRL | MODE_F_DAC_SQWAVE,
        .adc_trigger = ADC_TRIGGER_HRTIM_PRIM,
    },

    /* Auto-starts like RESISTANCE_1A, but no DAC pulse/sense LED. adc_trigger
     * Triggers off PRIM, same as RESISTANCE_1A: ctrl_main_start_ctrl() puts
     * both resistance modes on the same buck path with PRIM at
     * CTRL_PARAM_SW_FREQ_HIGH, and the loop regulates converted.v_out. At
     * 1 MHz with TRG1's post-scaler of 20 and the halving in
     * HAL_ADC_ConvCpltCallback(), that is 25 kHz -- exactly CTRL_FREQ, which
     * is what this mode's PI gains and ramp are scaled by. */
    [STATEMACHINE_MODE_RESISTANCE_1mA] = {
        .ctrl_mode   = CTRL_MODE_RESISTANCE_1mA,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_CONV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_AUTOSTART_CTRL,
        .adc_trigger = ADC_TRIGGER_HRTIM_PRIM,
    },

    /* Hold-OUT-to-enable via GPIO_HV_CTRL_EN, output_on forced to 0.
     *
     * Triggers off HV (TRG2 / Timer C), the converter this mode actually
     * controls. It previously assigned no trigger at all, so hadc1 kept
     * whichever one the last mode left and the control-loop rate depended on
     * mode history.
     *
     * NOTE, and this is not yet resolved: HV runs at CTRL_PARAM_SW_FREQ_HV
     * (350 kHz), TRG2's post-scaler in MX_HRTIM1_Init() is 15, and
     * HAL_ADC_ConvCpltCallback() halves again -- so this loop executes at
     * about 11.7 kHz, NOT the 25 kHz CTRL_FREQ that
     * CTRL_PARAM_HV_VOLTAGE_I/CTRL_PARAM_HV_CURRENT_I and the startup ramp
     * are scaled by. Integral action is therefore roughly 2.1x weaker, and
     * the ramp roughly 2.1x slower, than those constants suggest. The rate
     * is now at least deterministic, which is what makes bench tuning
     * possible; closing the gap (retune the HV gains, or change TRG2's
     * post-scaler to 7 for an exact 25 kHz) is a tuning decision to take
     * with the converter in front of you. See ctrl_param.h. */
    [STATEMACHINE_MODE_ISOMETER] = {
        .ctrl_mode   = CTRL_MODE_ISOMETER,
        .aux_io_mask = GPIO_MASK_OUT_SEL_HV | GPIO_MASK_SHUNT_ISO_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_HV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_OUTPUT_ON_ZERO,
        .adc_trigger = ADC_TRIGGER_HRTIM_HV,
    },

    /* Passive readout, no control loop, output_on left untouched (matches
     * the pre-refactor case, which never assigned it). */
    [STATEMACHINE_MODE_VOLTMETER] = {
        .ctrl_mode   = CTRL_MODE_OFF,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_OUT_SEL_HV | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = 0,
        .flags       = MODE_F_LED_OUT_ON,
        .adc_trigger = ADC_TRIGGER_NONE,
    },

    /* Entry is entirely special-cased in statemachine_switchfromIdle()
     * (submenu state setup) rather than driven by this table - aux_io_mask
     * is still real (used by the shared aux_io_ctrl_set_config(mode) tail
     * call and by calibration.c's adc_configure_mode(STATEMACHINE_MODE_SETTINGS)
     * call). ctrl_mode/flags/adc_trigger are the inert "off" values a mode
     * with no control loop and no ADC reconfiguration would have. */
    [STATEMACHINE_MODE_SETTINGS] = {
        .ctrl_mode   = CTRL_MODE_OFF,
        .aux_io_mask = GPIO_MASK_OUT_SEL_HV | GPIO_MASK_SHUNT_EN,
        .enable_gpio = 0,
        .flags       = 0,
        .adc_trigger = ADC_TRIGGER_NONE,
    },

    /* Never entered through statemachine_switchfromIdle() (falls to its
     * default: no-op, same as the pre-refactor switch) - aux_io_mask is
     * still real, used by aux_io_ctrl_mode_config[]'s pre-refactor entry. */
    [STATEMACHINE_MODE_SHUTDOWN] = {
        .ctrl_mode   = CTRL_MODE_OFF,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = 0,
        .flags       = 0,
        .adc_trigger = ADC_TRIGGER_NONE,
    },

    /* Auto-starts immediately like RESISTANCE_1A/1mA, and is the only mode
     * that forces output_on to 1 at entry (charging starts "on" by
     * design -- see the comment on this case in statemachine.c). */
    [STATEMACHINE_MODE_CHARGE] = {
        .ctrl_mode   = CTRL_MODE_CHARGE,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = GPIO_CONV_CTRL_EN,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_OUTPUT_ON_ONE | MODE_F_AUTOSTART_CTRL,
        .adc_trigger = ADC_TRIGGER_HRTIM_SEK,
    },

    /* No control loop (CTRL_MODE_OFF); entry additionally requires the
     * v_term >= 500mV refusal check special-cased in
     * statemachine_switchfromIdle() before this table is consulted, and
     * hrtim_sek_force_short() afterwards. output_on left untouched (matches
     * the pre-refactor case, which never assigned it). adc_trigger is
     * ADC_TRIGGER_NONE, but -- unlike ISOMETER/RESISTANCE_1mA above -- this
     * is intentional, not a bug: AMPMETER is a passive readout of i_out,
     * which hadc1 already samples continuously regardless of mode. */
    [STATEMACHINE_MODE_AMPMETER] = {
        .ctrl_mode   = CTRL_MODE_OFF,
        .aux_io_mask = GPIO_MASK_OUT_SEL_ISO | GPIO_MASK_SHUNT_EN | GPIO_MASK_DISCHARGE,
        .enable_gpio = 0,
        .flags       = MODE_F_LED_OUT_ON | MODE_F_SEK_FORCE_SHORT,
        .adc_trigger = ADC_TRIGGER_NONE,
    },
};
