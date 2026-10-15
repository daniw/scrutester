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

int main(void) {
	test_select();
	test_duty_model();
	test_pi_limits_reachable();
	if (fails) { printf("%d failure(s)\n", fails); return 1; }
	printf("test_vout_region: OK\n");
	return 0;
}
