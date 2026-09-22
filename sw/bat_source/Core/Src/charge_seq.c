/*
 * charge_seq.c
 *
 * See charge_seq.h. No HAL, HRTIM or GPIO access here on purpose: the ISR
 * (ctrl_main.c) and the state machine (statemachine.c) apply what this
 * computes, and tools/test_charge_seq.c exercises it on the host.
 */

#include "charge_seq.h"
#include "ctrl_param.h"

/* The constants in ctrl_param.h are mostly double literals; the control ISR
 * runs on a single-precision FPU, so fold everything to float once here. */
#define D_HI      ((float) CTRL_PARAM_CONST_DUTY_HIGH)
#define D_LO      ((float) CTRL_PARAM_CONST_DUTY_LOW)
#define BOOST_LO  ((float) CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW)
#define BOOST_HI  ((float) CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH)
#define CC_LO     ((float) CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_LOW)
#define CC_HI     ((float) CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH)

/* PRIM starts at the same fraction of its full duty as the 60V boost ramp. */
#define PRIM_START_FRACTION 0.1f

#define SECONDS_TO_TICKS(s) ((uint32_t) ((s) * CTRL_FREQ))
#define PRIM_TICKS  SECONDS_TO_TICKS(CTRL_PARAM_CHARGE_PRECHARGE_PRIM_RAMP_s)
#define SEK_TICKS   SECONDS_TO_TICKS(CTRL_PARAM_CHARGE_PRECHARGE_SEK_RAMP_s)
#define HOLD_TICKS  SECONDS_TO_TICKS(CTRL_PARAM_CHARGE_PRECHARGE_HOLD_s)
#define TOTAL_TICKS (PRIM_TICKS + SEK_TICKS + HOLD_TICKS)

static float clampf(float x, float lo, float hi) {
	if (x < lo)
		return lo;
	if (x > hi)
		return hi;
	return x;
}

float charge_seq_zero_current_duty(int32_t v_in_mV, int32_t v_term_mV) {
	if (v_term_mV <= 0)
		return D_HI; /* no usable charger reading: the least-current duty */
	return clampf(1.0f - D_HI * (float) v_in_mV / (float) v_term_mV, D_LO, D_HI);
}

float charge_seq_precharge_duty(int32_t v_in_mV, int32_t v_ref_mV) {
	if (v_ref_mV <= 0)
		return BOOST_LO;
	return clampf(1.0f - D_HI * (float) v_in_mV / (float) v_ref_mV, BOOST_LO, BOOST_HI);
}

int32_t charge_seq_precharge_target_mV(int32_t v_term_mV) {
	return (int32_t) ((float) v_term_mV * CTRL_PARAM_CHARGE_PRECHARGE_RATIO);
}

float charge_seq_cc_preload_action(float sek_duty) {
	return clampf(D_HI - sek_duty, CC_LO, CC_HI);
}

uint8_t charge_seq_v_term_in_window(int32_t v_term_mV) {
	return v_term_mV >= CTRL_PARAM_CHARGE_START_VIN_LOW_mV
			&& v_term_mV <= CTRL_PARAM_CHARGE_START_VIN_HIGH_mV;
}

void charge_precharge_start(charge_precharge_t *p, int32_t v_term_mV) {
	p->tick = 0;
	p->v_target_mV = charge_seq_precharge_target_mV(v_term_mV);
}

uint8_t charge_precharge_step(charge_precharge_t *p, int32_t v_in_mV,
		float *prim_duty, float *sek_duty) {
	const uint32_t t = p->tick;

	if (t < PRIM_TICKS) {
		/* Soft start of the battery-side bridge with SEK at minimum duty. */
		*prim_duty = D_HI * (PRIM_START_FRACTION
				+ (1.0f - PRIM_START_FRACTION) * (float) t / (float) PRIM_TICKS);
		*sek_duty = BOOST_LO;
	} else {
		const uint32_t ts = t - PRIM_TICKS;
		const float s = (ts >= SEK_TICKS) ? 1.0f : (float) ts / (float) SEK_TICKS;
		/* Voltage the converter makes at minimum SEK duty; the reference ramps
		 * from there to the target, so the first SEK duty equals BOOST_LO and
		 * the duty is continuous across the PRIM/SEK ramp boundary. */
		const float v_start = D_HI * (float) v_in_mV / (1.0f - BOOST_LO);
		const float v_ref = v_start + ((float) p->v_target_mV - v_start) * s;
		*prim_duty = D_HI;
		*sek_duty = charge_seq_precharge_duty(v_in_mV, (int32_t) v_ref);
	}

	if (t < TOTAL_TICKS)
		p->tick = t + 1;
	return t >= TOTAL_TICKS;
}

void charge_seq_init(charge_seq_t *s) {
	s->phase = CHG_PHASE_PRECHARGE;
	s->settle_ms = 0;
	s->stuck_ms = 0;
	s->advance_requested = 0; // a stale request must not release this run's pause
	s->precharge_ready = 0;
	// `manual` is deliberately left untouched -- see charge_seq_set_manual().
}

void charge_seq_set_manual(charge_seq_t *s, uint8_t manual) {
	s->manual = manual;
}

void charge_seq_request_advance(charge_seq_t *s) {
	s->advance_requested = 1;
}

uint8_t charge_seq_awaiting_advance(const charge_seq_t *s) {
	return s->manual && s->precharge_ready && !s->advance_requested;
}

charge_seq_action_t charge_seq_step(charge_seq_t *s, const charge_seq_in_t *in,
		uint16_t tick_ms) {
	switch (s->phase) {
	case CHG_PHASE_PRECHARGE:
		/* The ISR finishes the ramp and parks in CLOSE, holding its duties. */
		if (in->isr_phase == CHG_PHASE_CLOSE) {
			s->precharge_ready = 1;
			// In manual mode, hold here (relay stays open, duty held) until
			// the bench operator releases it -- see charge_seq_set_manual().
			if (s->manual && !s->advance_requested)
				break;
			s->advance_requested = 0;
			s->precharge_ready = 0;
			s->phase = CHG_PHASE_CLOSE;
			s->settle_ms = 0;
			return CHG_ACT_CLOSE_RELAY;
		}
		break;

	case CHG_PHASE_CLOSE:
		if (s->settle_ms < CTRL_PARAM_CHARGE_RELAY_SETTLE_ms)
			s->settle_ms = (uint16_t) (s->settle_ms + tick_ms);
		if (s->settle_ms >= CTRL_PARAM_CHARGE_RELAY_SETTLE_ms) {
			s->phase = CHG_PHASE_CC_RAMP;
			s->stuck_ms = 0;
			return CHG_ACT_START_RAMP;
		}
		break;

	case CHG_PHASE_CC_RAMP:
		if (in->isr_phase == CHG_PHASE_CV) {
			s->phase = CHG_PHASE_CV;
			s->stuck_ms = 0;
			break;
		}
		if (in->ref_mA > CTRL_PARAM_CHARGE_STUCK_OPEN_REF_mA
				&& in->charge_mA < CTRL_PARAM_CHARGE_STUCK_OPEN_MEAS_mA) {
			if (s->stuck_ms < CTRL_PARAM_CHARGE_STUCK_OPEN_ms)
				s->stuck_ms = (uint16_t) (s->stuck_ms + tick_ms);
			if (s->stuck_ms >= CTRL_PARAM_CHARGE_STUCK_OPEN_ms)
				return CHG_ACT_ABORT_STUCK_OPEN;
		} else {
			s->stuck_ms = 0;
		}
		break;

	case CHG_PHASE_OFF:
	case CHG_PHASE_CV:
	default:
		break;
	}
	return CHG_ACT_NONE;
}

uint8_t charge_seq_may_complete(const charge_seq_t *s) {
	return s->phase == CHG_PHASE_CC_RAMP || s->phase == CHG_PHASE_CV;
}

const char *charge_seq_phase_name(charge_phase_t phase) {
	switch (phase) {
	case CHG_PHASE_PRECHARGE: return "PRE";
	case CHG_PHASE_CLOSE:     return "CLOSE";
	case CHG_PHASE_CC_RAMP:   return "CC";
	case CHG_PHASE_CV:        return "CV";
	case CHG_PHASE_OFF:
	default:                  return "OFF";
	}
}
