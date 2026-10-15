/*
 * vout_region.c -- see vout_region.h.
 */

#include "vout_region.h"
#include "ctrl_param.h"

#define EDGE_LOW  (1.0F - CTRL_PARAM_VOUT_REGION_MARGIN)
#define EDGE_HIGH (1.0F + CTRL_PARAM_VOUT_REGION_MARGIN)
#define HYST      CTRL_PARAM_VOUT_REGION_HYSTERESIS

vout_region_t vout_region_select(vout_region_t current, float ratio) {
	switch (current) {
	case VOUT_REGION_BUCK:
		if (ratio > EDGE_HIGH)
			return VOUT_REGION_BOOST;
		if (ratio > EDGE_LOW)
			return VOUT_REGION_FOUR_SWITCH;
		return VOUT_REGION_BUCK;

	case VOUT_REGION_FOUR_SWITCH:
		if (ratio < EDGE_LOW - HYST)
			return VOUT_REGION_BUCK;
		if (ratio > EDGE_HIGH)
			return VOUT_REGION_BOOST;
		return VOUT_REGION_FOUR_SWITCH;

	case VOUT_REGION_BOOST:
	default:
		if (ratio < EDGE_LOW - HYST)
			return VOUT_REGION_BUCK;
		if (ratio < EDGE_HIGH - HYST)
			return VOUT_REGION_FOUR_SWITCH;
		return VOUT_REGION_BOOST;
	}
}

float vout_region_duty_for_ratio(vout_region_t region, float ratio) {
	if (ratio < 0.0F)
		ratio = 0.0F;
	switch (region) {
	case VOUT_REGION_BUCK:
		// Parked SEK duty is CONST_DUTY_LOW, so D_prim = m * (1 - D_low).
		return ratio * (1.0F - (float) CTRL_PARAM_CONST_DUTY_LOW);
	case VOUT_REGION_FOUR_SWITCH:
		return ratio / (1.0F + ratio);
	case VOUT_REGION_BOOST:
	default:
		// Parked PRIM duty is CONST_DUTY_HIGH: m = D_high / (1 - D_sek).
		if (ratio < (float) CTRL_PARAM_CONST_DUTY_HIGH)
			ratio = (float) CTRL_PARAM_CONST_DUTY_HIGH;
		return 1.0F - (float) CTRL_PARAM_CONST_DUTY_HIGH / ratio;
	}
}

float vout_region_ratio_from_duties(float duty_prim, float duty_sek) {
	float den = 1.0F - duty_sek;
	if (den < 0.01F)
		den = 0.01F;
	return duty_prim / den;
}
