/*
 * charge_seq.h
 *
 * Sequencing for the multi-phase CHARGE start, kept free of HAL/HRTIM/GPIO
 * so it can be compiled and tested on the host (tools/test_charge_seq.sh).
 *
 * The problem it solves: closing the output relay (K1, normally closed,
 * opened by driving OUT_SEL_HV high) onto a converter output node (OUT_LV,
 * ~33 uF) that is not at the charger voltage causes a large inrush, and the
 * old CC loop then started at maximum current. The sequence is
 *
 *   PRECHARGE  relay open, converter runs as an open-loop boost and brings
 *              OUT_LV to CTRL_PARAM_CHARGE_PRECHARGE_RATIO * V_term. OUT_LV
 *              has no ADC channel (V_OUT, V_TERM and I_OUT are all on the
 *              jack side of the relay), so the duty comes from V_IN alone.
 *   CLOSE      relay commanded closed while the SEK duty is stepped to the
 *              zero-current duty for the charger voltage; wait for the relay.
 *   CC_RAMP    current loop, reference ramps from 0 A, PI preloaded so the
 *              current starts at ~0.
 *   CV         existing constant-voltage tail.
 *
 * Single-step test mode (charge_seq_set_manual()): once PRECHARGE's ramp and
 * hold finish, the ISR parks in CLOSE (duty held, relay still open) and
 * charge_seq_step() keeps returning CHG_ACT_NONE instead of closing the relay
 * -- i.e. it holds open-loop boosting indefinitely for the bench to check
 * (TP12, or the "V_IN"/"V_OUT" fields of the periodic CLI print). The CLI's
 * `chargeNext` command (cli.c) calls charge_seq_request_advance() to release
 * it, which closes the relay and, once it has settled, starts the current
 * ramp -- steps 1 and 2 of a manual test are then this pause and that one
 * command, matching the two phases a bench check cares about.
 *
 * Duty conventions (HRTIM_CHANNEL_SEK, "D" below is the written compare duty):
 *   boost (battery -> OUT_LV):  V_OUT_LV = D_prim * V_in / (1 - D_sek)
 *   charge (OUT_LV -> battery): current ~ (V_OUT_LV * (1 - D_sek) - D_prim * V_bat) / R
 * so a larger SEK duty means less charge current, and the charge loop writes
 * D_sek = CTRL_PARAM_CONST_DUTY_HIGH - PI action.
 */

#ifndef INC_CHARGE_SEQ_H_
#define INC_CHARGE_SEQ_H_

#include <stdint.h>

/* Gates the CHARGE bench-debug surface: the periodic/peak CLI printouts in
 * statemachine.c and the `chargeStep`/`chargeNext` CLI commands (cli.c) built
 * on the single-step API below. Off by default, so none of it is compiled
 * into a normal build -- define it (e.g. here, uncommented) for bench work.
 * The single-step fields/functions themselves stay compiled in either way
 * (they're inert -- `manual` never becomes 1 -- with nothing left to set
 * them, and keeping them out of this #ifdef keeps this module buildable and
 * host-testable exactly as-is regardless of the flag). */
//#define CHARGE_DEBUG

typedef enum {
	CHG_PHASE_OFF = 0,     /* CHARGE not running */
	CHG_PHASE_PRECHARGE,   /* ISR ramps the open-loop boost */
	CHG_PHASE_CLOSE,       /* ISR holds the duty; main context owns the relay */
	CHG_PHASE_CC_RAMP,     /* charge-current loop, reference ramping */
	CHG_PHASE_CV           /* charge-voltage loop */
} charge_phase_t;

/* ---- Duty maths (pure) --------------------------------------------------- */

/* SEK duty that gives zero charge current with PRIM at its full duty:
 * V_term * (1 - D) = CONST_DUTY_HIGH * V_in. Clamped to the duty limits. */
float charge_seq_zero_current_duty(int32_t v_in_mV, int32_t v_term_mV);

/* Boost SEK duty that makes OUT_LV = v_ref_mV from v_in_mV (PRIM at full
 * duty), clamped to the boost duty saturation limits. */
float charge_seq_precharge_duty(int32_t v_in_mV, int32_t v_ref_mV);

int32_t charge_seq_precharge_target_mV(int32_t v_term_mV);

/* Charge-current PI action that makes ctrl_apply_inverted_sek_duty() write
 * `sek_duty` (the inverse of D = CONST_DUTY_HIGH - action). */
float charge_seq_cc_preload_action(float sek_duty);

/* Charger voltage inside the window CHARGE may start in. */
uint8_t charge_seq_v_term_in_window(int32_t v_term_mV);

/* ---- PRECHARGE ramp, stepped from the 25 kHz control ISR ------------------ */

typedef struct {
	uint32_t tick;
	int32_t v_target_mV;
} charge_precharge_t;

void charge_precharge_start(charge_precharge_t *p, int32_t v_term_mV);

/* One ISR tick. Writes the PRIM and SEK duty to apply; returns 1 once the
 * whole ramp and hold have elapsed. v_in_mV is read live so the duty follows
 * a sagging battery. */
uint8_t charge_precharge_step(charge_precharge_t *p, int32_t v_in_mV,
		float *prim_duty, float *sek_duty);

/* ---- Phase stepper, run from the 20 ms state-machine tick ---------------- */

typedef enum {
	CHG_ACT_NONE = 0,
	CHG_ACT_CLOSE_RELAY,        /* command the relay closed and hand over the duty */
	CHG_ACT_START_RAMP,         /* relay has settled: start current control */
	CHG_ACT_ABORT_STUCK_OPEN    /* no current although the reference is well up */
} charge_seq_action_t;

typedef struct {
	charge_phase_t phase;       /* main-context view of the sequence */
	uint16_t settle_ms;
	uint16_t stuck_ms;
	uint8_t manual;             /* single-step test mode, see charge_seq_set_manual() */
	uint8_t advance_requested;  /* one-shot, set by charge_seq_request_advance() */
	uint8_t precharge_ready;    /* PRECHARGE's ramp+hold has finished (isr_phase == CLOSE) */
	uint8_t low_current_recovery; /* see charge_seq_init_low_current_recovery() */
} charge_seq_t;

typedef struct {
	charge_phase_t isr_phase;   /* ctrl_main_handle.charge_phase as the ISR publishes it */
	int32_t ref_mA;             /* current reference in force */
	int32_t charge_mA;          /* measured charge current, positive while charging */
} charge_seq_in_t;

/* `*s` must be zero-initialised before the FIRST call (static/global storage
 * -- as statemachine.c's `charge_seq` is -- already is; a local/heap instance
 * needs an explicit `= {0}`), because this deliberately leaves `manual`
 * untouched on every call after that, see charge_seq_set_manual(). */
void charge_seq_init(charge_seq_t *s);

/* Deep-discharge/CUV recovery variant of charge_seq_init(): the battery side
 * (V_IN) reads ~0 (BMS DSG FET open), so PRECHARGE's boost-from-V_IN and
 * CLOSE (K1 is already closed and must stay that way -- see
 * statemachine_enter_charge_low_current() in statemachine.c) are both
 * skipped; starts directly in CC_RAMP. Same zero-initialisation requirement
 * as charge_seq_init(). */
void charge_seq_init_low_current_recovery(charge_seq_t *s);

charge_seq_action_t charge_seq_step(charge_seq_t *s, const charge_seq_in_t *in,
		uint16_t tick_ms);

/* Enables/disables single-step test mode. Does NOT reset `phase` etc. (safe
 * to call at any time, including mid-charge or before CHARGE is even
 * entered) and is NOT reset by charge_seq_init() -- once set from the CLI it
 * stays set across CHARGE (re-)starts until explicitly turned off. */
void charge_seq_set_manual(charge_seq_t *s, uint8_t manual);

/* Releases a manual-mode pause. No effect if not currently paused (see
 * charge_seq_awaiting_advance()) or not in manual mode. */
void charge_seq_request_advance(charge_seq_t *s);

/* True while PRECHARGE has finished and manual mode is holding it there. */
uint8_t charge_seq_awaiting_advance(const charge_seq_t *s);

/* The end-of-charge exit only makes sense once current is actually flowing. */
uint8_t charge_seq_may_complete(const charge_seq_t *s);

/* Short label for the display and the debug print ("PRE", "CLOSE", ...). */
const char *charge_seq_phase_name(charge_phase_t phase);

#endif /* INC_CHARGE_SEQ_H_ */
