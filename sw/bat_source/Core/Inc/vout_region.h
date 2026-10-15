/*
 * vout_region.h
 *
 * Operating-region logic for the full-range (0..60 V) voltage source mode,
 * kept free of HAL/HRTIM so it can be compiled and tested on the host
 * (tools/test_vout_region.sh).
 *
 * The LV converter is a 4-switch buck-boost. With D_prim/D_sek the written
 * HRTIM compare duties (see charge_seq.h for the convention):
 *
 *     V_out = D_prim * V_in / (1 - D_sek)
 *
 * and three regions are used, chosen from the ratio m = V_ref / V_in:
 *
 *   BUCK         m low.   PRIM switches fast (1 MHz) and carries the control
 *                         duty; SEK is parked at a low duty, at the low freq.
 *   FOUR_SWITCH  m ~ 1.   Both legs switch fast, D_prim = D_sek = D, so
 *                         m = D / (1 - D).
 *   BOOST        m high.  SEK switches fast and carries the control duty;
 *                         PRIM is parked at a high duty, at the low freq.
 *
 * Region edges sit at m = 1 -/+ CTRL_PARAM_VOUT_REGION_MARGIN, with a
 * hysteresis on the way back (CTRL_PARAM_VOUT_REGION_HYSTERESIS) so a
 * reference sitting on an edge does not make the converter flip between
 * frequency sets every tick.
 *
 * The duty <-> ratio helpers use the ideal model above only to hand a
 * region's control variable over to the next one without an output step; the
 * PI loops do the actual regulation.
 */

#ifndef INC_VOUT_REGION_H_
#define INC_VOUT_REGION_H_

typedef enum {
	VOUT_REGION_BUCK = 0,
	VOUT_REGION_FOUR_SWITCH,
	VOUT_REGION_BOOST
} vout_region_t;

/* Next region for ratio m = V_ref / V_in, given the current one (hysteresis).
 * May skip a region if the reference jumps. */
vout_region_t vout_region_select(vout_region_t current, float ratio);

/* Region's control variable (the duty the PI of that region drives) for an
 * ideal conversion ratio m. BUCK: D_prim. FOUR_SWITCH: D (= D_prim = D_sek).
 * BOOST: D_sek. Not clamped to any PI limit. */
float vout_region_duty_for_ratio(vout_region_t region, float ratio);

/* Ideal ratio D_prim / (1 - D_sek) of a commanded duty pair. */
float vout_region_ratio_from_duties(float duty_prim, float duty_sek);

#endif /* INC_VOUT_REGION_H_ */
