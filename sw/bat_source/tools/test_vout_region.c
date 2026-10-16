/* Host test for vout_region.c, see tools/test_vout_region.sh. */
#include <stdio.h>
#include <math.h>
#include "vout_region.h"
#include "ctrl_param.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void test_select(void) {
	const float lo = 1.0F - CTRL_PARAM_VOUT_REGION_MARGIN;
	const float hi = 1.0F + CTRL_PARAM_VOUT_REGION_MARGIN;
	const float h = CTRL_PARAM_VOUT_REGION_HYSTERESIS;

	CHECK(vout_region_select(VOUT_REGION_BUCK, 0.1F) == VOUT_REGION_BUCK);
	CHECK(vout_region_select(VOUT_REGION_BUCK, lo + 0.01F) == VOUT_REGION_FOUR_SWITCH);
	CHECK(vout_region_select(VOUT_REGION_BUCK, hi + 0.01F) == VOUT_REGION_BOOST); // skips 4SW
	CHECK(vout_region_select(VOUT_REGION_BOOST, 0.1F) == VOUT_REGION_BUCK);       // skips 4SW

	// Hysteresis: on the way back the edge moves by h.
	CHECK(vout_region_select(VOUT_REGION_FOUR_SWITCH, lo - h / 2) == VOUT_REGION_FOUR_SWITCH);
	CHECK(vout_region_select(VOUT_REGION_FOUR_SWITCH, lo - h - 0.01F) == VOUT_REGION_BUCK);
	CHECK(vout_region_select(VOUT_REGION_BOOST, hi - h / 2) == VOUT_REGION_BOOST);
	CHECK(vout_region_select(VOUT_REGION_BOOST, hi - h - 0.01F) == VOUT_REGION_FOUR_SWITCH);

	// A ratio sitting just either side of an edge must not flap.
	vout_region_t r = VOUT_REGION_BUCK;
	for (int i = 0; i < 100; i++) {
		r = vout_region_select(r, lo + ((i & 1) ? 0.01F : -0.01F));
		if (i > 0)
			CHECK(r == VOUT_REGION_FOUR_SWITCH);
	}
}

static void test_duty_model(void) {
	// Round trip through the ideal model in each region.
	const float ms[] = { 0.2F, 0.7F, 1.0F, 1.2F, 2.0F, 4.0F };
	for (unsigned i = 0; i < sizeof(ms) / sizeof(ms[0]); i++) {
		float m = ms[i];
		float d = vout_region_duty_for_ratio(VOUT_REGION_FOUR_SWITCH, m);
		CHECK(fabsf(vout_region_ratio_from_duties(d, d) - m) < 1e-4F);

		d = vout_region_duty_for_ratio(VOUT_REGION_BUCK, m);
		CHECK(fabsf(vout_region_ratio_from_duties(d, CTRL_PARAM_CONST_DUTY_LOW) - m) < 1e-4F);

		if (m >= CTRL_PARAM_CONST_DUTY_HIGH) {
			d = vout_region_duty_for_ratio(VOUT_REGION_BOOST, m);
			CHECK(fabsf(vout_region_ratio_from_duties(CTRL_PARAM_CONST_DUTY_HIGH, d) - m) < 1e-3F);
		}
	}
	// 4SW at m = 1 is D = 0.5.
	CHECK(fabsf(vout_region_duty_for_ratio(VOUT_REGION_FOUR_SWITCH, 1.0F) - 0.5F) < 1e-6F);
}

static void test_pi_limits_reachable(void) {
	// The buck region's top edge must be reachable inside the buck PI's duty
	// ceiling, and the 4-switch region inside its own -- otherwise the loop
	// saturates before the region hands over.
	float d = vout_region_duty_for_ratio(VOUT_REGION_BUCK, 1.0F - CTRL_PARAM_VOUT_REGION_MARGIN);
	CHECK(d < CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_HIGH);
	d = vout_region_duty_for_ratio(VOUT_REGION_FOUR_SWITCH, 1.0F + CTRL_PARAM_VOUT_REGION_MARGIN);
	CHECK(d < CTRL_PARAM_VOLTAGE_4SW_DUTY_SAT_HIGH);
	// Boost region's low edge needs a positive SEK duty below its ceiling.
	d = vout_region_duty_for_ratio(VOUT_REGION_BOOST, 1.0F + CTRL_PARAM_VOUT_REGION_MARGIN - CTRL_PARAM_VOUT_REGION_HYSTERESIS);
	CHECK(d > CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW && d < CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH);
}

static void test_burst_plan(void) {
	const unsigned F = CTRL_PARAM_VOUT_FRAME_CYCLES, N = CTRL_PARAM_VOUT_SYNC_CYCLES;
	const float lo = CTRL_PARAM_VOUT_MIN_RUN_DUTY, hi = CTRL_PARAM_VOUT_MAX_RUN_DUTY;

	// Sync window is long enough for the bootstrap recharge: Q / I plus two
	// dead times (7.8 ns each) must fit in the idle block of 1/f_fast cycles.
	float t_us = (float) CTRL_PARAM_BOOT_CHARGE_PER_PERIOD_nC / CTRL_PARAM_BOOT_RECHARGE_mA;
	float cycle_us = 1e6F / CTRL_PARAM_SW_FREQ_HIGH;
	CHECK(N * cycle_us >= t_us + 0.016F + cycle_us); // one cycle of alignment margin
	CHECK(F == 20);

	// Normal operation: average duty preserved, sync window only.
	vout_burst_t b = vout_burst_plan(0.4F, F, N, 1, lo, hi);
	CHECK(b.idle_cycles == N);
	CHECK(fabsf(b.duty_run * (F - b.idle_cycles) / F - 0.4F) < 1e-4F);

	// No parked leg (4-switch): no idle at all, duty untouched.
	b = vout_burst_plan(0.5F, F, 0, 0, lo, hi);
	CHECK(b.idle_cycles == 0 && fabsf(b.duty_run - 0.5F) < 1e-6F);

	// Run duty capped.
	b = vout_burst_plan(0.99F, F, N, 1, lo, hi);
	CHECK(b.duty_run <= hi + 1e-6F);

	// Demand below the HRTIM floor: floor duty, fewer run cycles, average held.
	b = vout_burst_plan(0.01F, F, N, 1, lo, hi);
	CHECK(fabsf(b.duty_run - lo) < 1e-6F);
	CHECK(b.idle_cycles > N && b.idle_cycles <= F);
	CHECK(fabsf(b.duty_run * (F - b.idle_cycles) / F - 0.01F) < lo / F); // within one cycle
	// Zero demand (shorted output): everything skipped.
	b = vout_burst_plan(0.0F, F, N, 1, lo, hi);
	CHECK(b.idle_cycles == F);
	// Skipping is a buck-only feature.
	b = vout_burst_plan(0.0F, F, N, 0, lo, hi);
	CHECK(b.idle_cycles == N);
	// Monotonic: lower demand never gives more run cycles.
	unsigned prev = 0;
	for (int i = 0; i <= 100; i++) {
		b = vout_burst_plan(0.0F + 0.0005F * i, F, N, 1, lo, hi);
		CHECK(F - b.idle_cycles >= prev);
		prev = F - b.idle_cycles;
	}
}

int main(void) {
	test_burst_plan();
	test_select();
	test_duty_model();
	test_pi_limits_reachable();
	if (fails) { printf("%d failure(s)\n", fails); return 1; }
	printf("test_vout_region: OK\n");
	return 0;
}
