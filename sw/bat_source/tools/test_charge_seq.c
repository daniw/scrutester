/*
 * Host-side tests for Core/Src/charge_seq.c. Run with tools/test_charge_seq.sh.
 *
 * Only checks what can be checked without hardware: the duty formulas, the
 * PRECHARGE ramp shape, and the ordering of the phase stepper. Whether the
 * duty model matches the real converter (OUT_LV is unmeasured) and how long
 * the relay really takes are bench items.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "charge_seq.h"
#include "ctrl_param.h"

static int failures;

#define CHECK(cond) do { \
	if (!(cond)) { \
		printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
		failures++; \
	} \
} while (0)

#define CHECK_NEAR(a, b, tol) do { \
	double _a = (a), _b = (b); \
	if (fabs(_a - _b) > (tol)) { \
		printf("FAIL %s:%d: %s = %.6f, expected %.6f (+-%g)\n", __FILE__, __LINE__, #a, _a, _b, (double) (tol)); \
		failures++; \
	} \
} while (0)

static void test_duty_formulas(void) {
	/* V_in 13 V, charger 18 V: 1 - 0.985*13/18 */
	CHECK_NEAR(charge_seq_zero_current_duty(13000, 18000), 0.28861, 1e-4);
	CHECK_NEAR(charge_seq_zero_current_duty(14600, 15000), 0.04127, 1e-4);
	/* Pack above the charger: clamped to the minimum duty, not negative. */
	CHECK_NEAR(charge_seq_zero_current_duty(20000, 15000), 0.015, 1e-6);
	/* No charger reading: least-current duty. */
	CHECK_NEAR(charge_seq_zero_current_duty(13000, 0), 0.985, 1e-6);
	/* No 0.5 ceiling any more: a low pack on a 24 V charger gives D > 0.5. */
	CHECK(charge_seq_zero_current_duty(10000, 24000) > 0.5f);

	/* Tied to the tunable ratio, not hardcoded, since it has already changed
	 * once (0.95 -> 0.99) since this test was written. */
	CHECK_NEAR(charge_seq_precharge_target_mV(18000),
			18000 * (double) CTRL_PARAM_CHARGE_PRECHARGE_RATIO, 1);
	CHECK_NEAR(charge_seq_precharge_duty(13000, 17100), 0.25117, 1e-4);
	CHECK_NEAR(charge_seq_precharge_duty(13000, 5000), 0.015, 1e-6);   /* below V_in: minimum */
	CHECK_NEAR(charge_seq_precharge_duty(5000, 60000), 0.8, 1e-6);     /* clamped to boost limit */
	CHECK_NEAR(charge_seq_precharge_duty(13000, 0), 0.015, 1e-6);

	/* The preload writes the requested SEK duty through D = 0.985 - action. */
	float d0 = charge_seq_zero_current_duty(13000, 18000);
	CHECK_NEAR(0.985f - charge_seq_cc_preload_action(d0), d0, 1e-5);
	CHECK_NEAR(charge_seq_cc_preload_action(0.0f), 0.985, 1e-6);   /* saturates at the PI limit */
	CHECK_NEAR(charge_seq_cc_preload_action(2.0f), 0.0, 1e-6);

	CHECK(!charge_seq_v_term_in_window(14999));
	CHECK(charge_seq_v_term_in_window(15000));
	CHECK(charge_seq_v_term_in_window(24000));
	CHECK(!charge_seq_v_term_in_window(24001));
}

static void run_precharge(int32_t v_in, int32_t v_term, int32_t v_in_end) {
	charge_precharge_t p;
	charge_precharge_start(&p, v_term);
	const int32_t target = charge_seq_precharge_target_mV(v_term);

	/* Derived from ctrl_param.h rather than hardcoded, so retuning those
	 * constants (as happened once already) doesn't leave this test stale. */
	const uint32_t total_ticks = (uint32_t) ((CTRL_PARAM_CHARGE_PRECHARGE_PRIM_RAMP_s
			+ CTRL_PARAM_CHARGE_PRECHARGE_SEK_RAMP_s
			+ CTRL_PARAM_CHARGE_PRECHARGE_HOLD_s) * CTRL_FREQ);
	const uint32_t margin_ticks = total_ticks + 5000;

	float prim = 0, sek = 0, prev_prim = 0, prev_sek = 0;
	uint32_t done_at = 0;
	int done = 0;
	float max_prim_step = 0, max_sek_step = 0;

	for (uint32_t i = 0; i < margin_ticks; i++) {
		int32_t v = v_in + (int32_t) ((int64_t) (v_in_end - v_in) * i / total_ticks);
		if (i >= total_ticks)
			v = v_in_end;
		uint8_t d = charge_precharge_step(&p, v, &prim, &sek);
		if (i > 0) {
			if (prim + 1e-6f < prev_prim || sek + 1e-6f < prev_sek) {
				if (v_in_end == v_in) { /* monotonic only with a constant battery voltage */
					printf("FAIL non-monotonic ramp at tick %u\n", (unsigned) i);
					failures++;
				}
			}
			if (fabsf(prim - prev_prim) > max_prim_step) max_prim_step = fabsf(prim - prev_prim);
			if (fabsf(sek - prev_sek) > max_sek_step) max_sek_step = fabsf(sek - prev_sek);
		}
		prev_prim = prim;
		prev_sek = sek;
		if (d && !done) {
			done = 1;
			done_at = i;
		}
		CHECK(prim <= 0.985f + 1e-6f && prim >= 0.0f);
		CHECK(sek >= 0.015f - 1e-6f && sek <= 0.8f + 1e-6f);
	}

	CHECK(done);
	CHECK(done_at == total_ticks); /* PRIM_RAMP + SEK_RAMP + HOLD, in ticks */
	CHECK(max_prim_step < 3e-4f);
	CHECK(max_sek_step < 1e-3f);
	CHECK_NEAR(prim, 0.985, 1e-6);

	/* Unloaded synchronous boost: V_out = D_prim * V_in / (1 - D_sek). */
	double v_out = prim * (double) v_in_end / (1.0 - sek);
	if (target > (int32_t) (0.985 * v_in_end / (1.0 - 0.015)))
		CHECK_NEAR(v_out, target, 0.005 * target);
	else
		CHECK_NEAR(sek, 0.015, 1e-6); /* target below what minimum duty already gives */
}

static void test_precharge(void) {
	run_precharge(13000, 18000, 13000);
	run_precharge(10000, 24000, 10000);
	run_precharge(14500, 15000, 14500);   /* target 14250 V < V_in: stays at minimum duty */
	run_precharge(13000, 18000, 12500);   /* battery sags during the ramp: duty follows */

	/* First tick: PRIM at PRIM_START_FRACTION of full duty, SEK at minimum.
	 * PRIM_START_FRACTION is private to charge_seq.c (not in charge_seq.h),
	 * so this 0.1 has to be kept in sync with it by hand if it's retuned
	 * again. */
	charge_precharge_t p;
	float prim, sek;
	charge_precharge_start(&p, 18000);
	charge_precharge_step(&p, 13000, &prim, &sek);
	CHECK_NEAR(prim, 0.985 * 0.1, 1e-6);
	CHECK_NEAR(sek, 0.015, 1e-6);
}

static charge_seq_action_t step(charge_seq_t *s, charge_phase_t isr, int32_t ref, int32_t i) {
	charge_seq_in_t in = { isr, ref, i };
	return charge_seq_step(s, &in, 20);
}

static void test_phase_stepper(void) {
	charge_seq_t s = {0}; // manual must start at 0: see charge_seq_init()'s doc comment
	charge_seq_init(&s);
	CHECK(!charge_seq_may_complete(&s));

	for (int i = 0; i < 100; i++)
		CHECK(step(&s, CHG_PHASE_PRECHARGE, 0, 0) == CHG_ACT_NONE);

	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_CLOSE_RELAY);
	CHECK(!charge_seq_may_complete(&s));
	/* 100 ms relay settle at 20 ms per tick: NONE x4, then START_RAMP */
	for (int i = 0; i < 4; i++)
		CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_NONE);
	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_START_RAMP);
	CHECK(charge_seq_may_complete(&s));

	/* Reference low or current flowing: never abort. */
	for (int i = 0; i < 500; i++)
		CHECK(step(&s, CHG_PHASE_CC_RAMP, 200, 0) == CHG_ACT_NONE);
	for (int i = 0; i < 500; i++)
		CHECK(step(&s, CHG_PHASE_CC_RAMP, 800, 400) == CHG_ACT_NONE);

	/* Reference up, no current: abort after 2 s, not before. */
	int n = 0;
	while (step(&s, CHG_PHASE_CC_RAMP, 400, 5) != CHG_ACT_ABORT_STUCK_OPEN)
		CHECK(++n < 1000);
	CHECK(n == 99);

	/* A single tick with current restarts the timer. */
	charge_seq_init(&s);
	step(&s, CHG_PHASE_CLOSE, 0, 0);
	for (int i = 0; i < 5; i++) step(&s, CHG_PHASE_CLOSE, 0, 0);
	for (int i = 0; i < 90; i++)
		CHECK(step(&s, CHG_PHASE_CC_RAMP, 400, 0) == CHG_ACT_NONE);
	CHECK(step(&s, CHG_PHASE_CC_RAMP, 400, 100) == CHG_ACT_NONE);
	for (int i = 0; i < 90; i++)
		CHECK(step(&s, CHG_PHASE_CC_RAMP, 400, 0) == CHG_ACT_NONE);

	/* CV: no stuck detection, may still complete. */
	CHECK(step(&s, CHG_PHASE_CV, 1000, 0) == CHG_ACT_NONE);
	for (int i = 0; i < 500; i++)
		CHECK(step(&s, CHG_PHASE_CV, 1000, 0) == CHG_ACT_NONE);
	CHECK(charge_seq_may_complete(&s));
}

/* Single-step bench test mode: holds after precharge until released. */
static void test_manual_step(void) {
	charge_seq_t s = {0}; // manual must start at 0: see charge_seq_init()'s doc comment
	charge_seq_init(&s);
	charge_seq_set_manual(&s, 1);
	CHECK(!charge_seq_awaiting_advance(&s));

	/* Still ramping: not "awaiting" yet, and step() must not act on its own. */
	for (int i = 0; i < 100; i++) {
		CHECK(step(&s, CHG_PHASE_PRECHARGE, 0, 0) == CHG_ACT_NONE);
		CHECK(!charge_seq_awaiting_advance(&s));
	}

	/* ISR reaches CLOSE: manual mode holds, indefinitely, relay still open. */
	for (int i = 0; i < 500; i++) {
		CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_NONE);
		CHECK(charge_seq_awaiting_advance(&s));
		CHECK(!charge_seq_may_complete(&s));
	}

	/* Released: behaves exactly like the automatic path from here. */
	charge_seq_request_advance(&s);
	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_CLOSE_RELAY);
	CHECK(!charge_seq_awaiting_advance(&s));
	for (int i = 0; i < 4; i++)
		CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_NONE);
	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_START_RAMP);
	CHECK(charge_seq_may_complete(&s));

	/* Turning manual mode off before the pause point: runs straight through. */
	charge_seq_init(&s);
	charge_seq_set_manual(&s, 0);
	for (int i = 0; i < 100; i++)
		step(&s, CHG_PHASE_PRECHARGE, 0, 0);
	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_CLOSE_RELAY);

	/* A fresh charge_seq_init() clears any stale advance request and re-arms
	 * the hold; `manual` itself survives the init (see charge_seq_init()). */
	charge_seq_set_manual(&s, 1);
	step(&s, CHG_PHASE_PRECHARGE, 0, 0);
	charge_seq_request_advance(&s);
	charge_seq_init(&s);
	CHECK(step(&s, CHG_PHASE_CLOSE, 0, 0) == CHG_ACT_NONE);
	CHECK(charge_seq_awaiting_advance(&s));
}

/* Random ISR timing: the relay is only ever commanded after the ISR parked in
 * CLOSE, at most once, and current control starts no earlier than the settle
 * time later. */
static void test_ordering(void) {
	srand(1);
	for (int run = 0; run < 500; run++) {
		charge_seq_t s = {0}; // manual must start at 0: see charge_seq_init()'s doc comment
		charge_seq_init(&s);
		int close_at = rand() % 300;
		int closes = 0, ramps = 0, close_tick = -1, ramp_tick = -1;
		for (int t = 0; t < 700; t++) {
			charge_phase_t isr = t < close_at ? CHG_PHASE_PRECHARGE : CHG_PHASE_CLOSE;
			if (ramps)
				isr = CHG_PHASE_CC_RAMP;
			charge_seq_action_t a = step(&s, isr, 100, 500);
			if (a == CHG_ACT_CLOSE_RELAY) {
				closes++;
				close_tick = t;
				CHECK(t >= close_at);
			}
			if (a == CHG_ACT_START_RAMP) {
				ramps++;
				ramp_tick = t;
				CHECK(closes == 1);
			}
		}
		CHECK(closes == 1 && ramps == 1);
		CHECK((ramp_tick - close_tick) * 20 >= CTRL_PARAM_CHARGE_RELAY_SETTLE_ms);
	}
}

/* Deep-discharge/CUV recovery entry: skips PRECHARGE/CLOSE, starts directly
 * in CC_RAMP, same stuck-open behavior as a normal charge from there. */
static void test_low_current_recovery(void) {
	charge_seq_t s = {0}; // manual must start at 0: see charge_seq_init()'s doc comment
	charge_seq_init_low_current_recovery(&s);
	CHECK(s.phase == CHG_PHASE_CC_RAMP);
	CHECK(s.low_current_recovery);
	CHECK(charge_seq_may_complete(&s)); // no need to wait for PRECHARGE/CLOSE first

	/* Reference low or current flowing: never abort, same as a normal charge
	 * already past CLOSE. */
	for (int i = 0; i < 500; i++)
		CHECK(step(&s, CHG_PHASE_CC_RAMP, 200, 0) == CHG_ACT_NONE);

	/* Stuck-open detection still applies (relay stuck open, or charger gone
	 * mid-recovery, is just as real a fault here). */
	int n = 0;
	while (step(&s, CHG_PHASE_CC_RAMP, 400, 5) != CHG_ACT_ABORT_STUCK_OPEN)
		CHECK(++n < 1000);
	CHECK(n == 99);

	/* A fresh charge_seq_init() (the normal-charge path) clears the flag. */
	charge_seq_init(&s);
	CHECK(!s.low_current_recovery);
	CHECK(s.phase == CHG_PHASE_PRECHARGE);
}

int main(void) {
	test_duty_formulas();
	test_precharge();
	test_phase_stepper();
	test_manual_step();
	test_ordering();
	test_low_current_recovery();
	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("charge_seq: all checks passed\n");
	return 0;
}
