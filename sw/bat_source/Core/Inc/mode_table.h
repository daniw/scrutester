/*
 * mode_table.h
 *
 * Single source of truth for per-statemachine_modes_t data that used to be
 * spread (and kept in lockstep by hand) across four places:
 *   - statemachine.c's statemachine_switchfromIdle() switch
 *   - ctrl_main.c's statemachine_mode_to_ctrl_mode() switch
 *   - adc.c's adc_configure_mode() switch (the shared hadc1..4 trigger part)
 *   - aux_io_ctrl.c's aux_io_ctrl_mode_config[] table (the model this follows)
 *
 * mode_table[] is indexed 0..STATEMACHINE_MODE_RESERVED-1, i.e. every real
 * statemachine_modes_t value (STATEMACHINE_IDLE..STATEMACHINE_MODE_AMPMETER).
 * STATEMACHINE_MODE_RESERVED itself is not a real mode and is never indexed;
 * callers must bounds-check `mode < STATEMACHINE_MODE_RESERVED` first (see
 * aux_io_ctrl_set_config()/statemachine_mode_to_ctrl_mode() for the pattern).
 *
 *  Created on: Jul 31, 2026
 */

#ifndef INC_MODE_TABLE_H_
#define INC_MODE_TABLE_H_

#include <stdint.h>
#include "statemachine.h"
#include "ctrl_main.h"

/* aux_io_ctrl_mode_config[]'s former bit layout -- which relay/shunt GPIOs
 * aux_io_ctrl_set_config() drives high for a given mode. Shared between
 * mode_table.c (builds the per-mode masks) and aux_io_ctrl.c (decodes them
 * back into individual pin writes). */
#define GPIO_MASK_OUT_SEL_ISO   (1 << 0)
#define GPIO_MASK_OUT_SEL_HV    (1 << 1)
#define GPIO_MASK_SHUNT_EN      (1 << 2)
#define GPIO_MASK_SHUNT_ISO_EN  (1 << 3)
#define GPIO_MASK_DISCHARGE     (1 << 4)

/* statemachine_switchfromIdle()'s per-mode entry-sequence flags. Derived by
 * enumerating every case of the original hand-written switch -- see
 * mode_table.c for the per-mode rationale this was built from. */

/* ui_ctrl_ledOutOn() at entry -- every real entry mode except SETTINGS
 * (which is handled as its own special case, see statemachine.c). */
#define MODE_F_LED_OUT_ON       (1u << 0)
/* ui_ctrl_ledSenseOn() at entry -- RESISTANCE_1A only. */
#define MODE_F_LED_SENSE_ON     (1u << 1)
/* Start the control loop and assert enable_gpio immediately on entry
 * (RESISTANCE_1A/1mA, CHARGE). Without this flag, the mode instead waits for
 * OUT to be held down -- ctrl_main_start_ctrl()/enable_gpio are asserted from
 * statemachine_step()'s per-tick OUT-button handling instead (60V_OUT,
 * 10A_OUT, ISOMETER). */
#define MODE_F_AUTOSTART_CTRL   (1u << 2)
/* output_on = 0 at entry. */
#define MODE_F_OUTPUT_ON_ZERO   (1u << 3)
/* output_on = 1 at entry (CHARGE only). */
#define MODE_F_OUTPUT_ON_ONE    (1u << 4)
/* dac_sqwave_start() at entry -- RESISTANCE_1A only (drives the ~1A test
 * pulse). */
#define MODE_F_DAC_SQWAVE       (1u << 5)
/* hrtim_sek_force_short() at entry -- AMPMETER only. */
#define MODE_F_SEK_FORCE_SHORT  (1u << 6)

/* adc_trigger value meaning "leave hadc1..4's ExternalTrigConv exactly as
 * whichever mode ran before it left it" -- i.e. adc_configure_mode() does NOT
 * touch the shared fast-loop trigger for this mode.
 *
 * This is only ever correct for modes that drive no control loop: IDLE,
 * SETTINGS, SHUTDOWN, VOLTMETER and AMPMETER. Those are passive readouts of
 * channels hadc1..3 sample regardless of trigger source, so which converter
 * happens to be pacing the conversions does not matter to them.
 *
 * Every mode that DOES run a control loop must name its converter's trigger
 * explicitly, because the loop executes once per conversion and its PI gains
 * and startup ramp are scaled by an assumed rate. ISOMETER and RESISTANCE_1mA
 * both used to be ADC_TRIGGER_NONE by omission rather than by intent, which
 * made their loop rate depend on mode history; both now name a trigger. */
#define ADC_TRIGGER_NONE 0u

typedef struct {
    ctrl_mode_t ctrl_mode;   /* CTRL_MODE_OFF if the mode drives no control loop */
    uint8_t     aux_io_mask; /* GPIO_MASK_* bits, see above */
    uint8_t     enable_gpio; /* GPIO_CONV_CTRL_EN / GPIO_HV_CTRL_EN, 0 if unused */
    uint8_t     flags;       /* MODE_F_* bits, see above */
    uint32_t    adc_trigger; /* ADC_TRIGGER_HRTIM_{PRIM,SEK}, or ADC_TRIGGER_NONE */
} mode_descriptor_t;

extern const mode_descriptor_t mode_table[STATEMACHINE_MODE_RESERVED];

#endif /* INC_MODE_TABLE_H_ */
