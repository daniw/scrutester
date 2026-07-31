/*
 * calibration.h
 *
 * Shared zero/gain calibration logic for the six field-calibratable
 * ADC channels (V_TERM, V_SENS, V_OUT, V_HV, I_OUT, I_ISO), used by
 * both the Settings-menu UI (statemachine.c/display.c) and the CLI
 * (cli.c) so the sampling/math lives in exactly one place.
 *
 * Calibration model: converted = (raw - offset) * gain, offset always
 * in raw ADC counts. Two independent steps per channel:
 *   1. Zero (required): set the real-world input to zero (short/open
 *      as appropriate for the channel), call calibration_zero() --
 *      samples raw ADC counts and stores them as the offset.
 *   2. Gain (optional, second point): apply one known non-zero
 *      reference value, call calibration_set_gain() with that value
 *      -- samples raw again and computes gain = reference / (raw -
 *      offset).
 * Both steps persist to the EEPROM immediately (config_store_store()),
 * no separate save step.
 */

#ifndef SRC_CALIBRATION_H_
#define SRC_CALIBRATION_H_

#include <stdint.h>

typedef enum
{
	CAL_CH_V_TERM = 0,
	CAL_CH_V_SENS,
	CAL_CH_V_OUT,
	CAL_CH_V_HV,
	CAL_CH_I_OUT,
	CAL_CH_I_ISO,
	CAL_CH_COUNT
} calibration_channel_t;

/** Result of a gain-calibration commit (calibration_set_gain()/
 *  calibration_set_gain_raw()). CALIBRATION_STATUS_ERR covers an invalid
 *  channel as well as an invalid computed/supplied gain (zero,
 *  non-finite, or implausibly large -- see calibration.c); either way
 *  nothing is written to adc_data or config_store. */
typedef enum
{
	CALIBRATION_STATUS_OK = 0,
	CALIBRATION_STATUS_ERR,
} calibration_status_t;

const char *calibration_channel_name(calibration_channel_t ch);
const char *calibration_channel_unit(calibration_channel_t ch);

/** Points the ADC(s) at the mode needed to sample/display ch (see
 *  calibration.c) -- stops and reconfigures up to three ADC DMA streams,
 *  so it is relatively expensive (~ms) and must be called only when
 *  arming a channel for calibration (entering the calibration screen, or
 *  the selected channel changing), never once per draw/tick. Harmless to
 *  call repeatedly for the same channel. calibration_read_*() call this
 *  themselves since they are one-shot deliberate actions; calibration_peek_*()
 *  deliberately do not, and rely on the caller having armed the channel. */
void calibration_ensure_adc_mode(calibration_channel_t ch);

/** Live raw ADC counts for the channel, averaged over a few samples.
 *  Blocking (~8ms) and reconfigures the ADC via calibration_ensure_adc_mode() --
 *  fine for one-shot CLI/commit use, NOT for a per-tick UI redraw (see
 *  calibration_peek_raw()). */
int32_t calibration_read_raw(calibration_channel_t ch);

/** Live converted physical-unit value (same field the rest of the
 *  firmware reads), for on-screen/CLI feedback while calibrating.
 *  Reconfigures the ADC via calibration_ensure_adc_mode(); same caveat as
 *  calibration_read_raw() -- use calibration_peek_converted() for a
 *  per-tick redraw instead. */
int32_t calibration_read_converted(calibration_channel_t ch);

/** Non-blocking, no ADC reconfiguration: just the current raw ADC count
 *  already sitting in adc_data. Safe to call every statemachine tick from
 *  the calibration screen's live display, PROVIDED calibration_ensure_adc_mode(ch)
 *  was already called once when ch was armed (see display_calibration_enter()/
 *  statemachine_step_calibration()). */
int32_t calibration_peek_raw(calibration_channel_t ch);

/** Non-blocking counterpart to calibration_read_converted() -- same
 *  "must already be armed" requirement as calibration_peek_raw(). */
int32_t calibration_peek_converted(calibration_channel_t ch);

/** Samples raw ADC counts and stores them as the channel's offset,
 *  then persists to EEPROM. */
void calibration_zero(calibration_channel_t ch);

/** Samples raw ADC counts, computes gain = reference_value / (raw -
 *  offset), stores it, then persists to EEPROM. Returns
 *  CALIBRATION_STATUS_ERR (leaving both the live adc_data gain and the
 *  stored config_store gain untouched, and never persisting) if the
 *  computed gain -- or, for channels with an ext-ADC counterpart
 *  (calibration_has_ext()), the computed ext gain -- would be zero,
 *  non-finite, or implausibly large; see calibration.c for the exact
 *  bound. */
calibration_status_t calibration_set_gain(calibration_channel_t ch, float reference_value);

/** Directly stores a caller-supplied offset (raw ADC counts) for ch,
 *  skipping the ADC sampling calibration_zero() does, then persists to
 *  EEPROM. raw_ext_offset additionally sets the ext-ADC counterpart's
 *  offset (see calibration_has_ext()) when non-NULL; pass NULL to leave
 *  it untouched, which is also the correct/only choice for channels
 *  without an ext counterpart. */
void calibration_set_offset_raw(calibration_channel_t ch, int32_t raw_offset, const int32_t *raw_ext_offset);

/** Directly stores a caller-supplied gain for ch, skipping the ADC
 *  sampling calibration_set_gain() does, then persists to EEPROM.
 *  ext_gain additionally sets the ext-ADC counterpart's gain (see
 *  calibration_has_ext()) when non-NULL; pass NULL to leave it
 *  untouched, which is also the correct/only choice for channels
 *  without an ext counterpart. A typed-in gain is just as capable of
 *  being a zero/blank-field mistake as a sampled one, so it is subject to
 *  the same zero/non-finite/magnitude validation as calibration_set_gain() --
 *  see calibration.c. Returns CALIBRATION_STATUS_ERR, leaving both
 *  adc_data and config_store untouched and skipping the EEPROM write,
 *  if gain or (when supplied) *ext_gain fails that check. */
calibration_status_t calibration_set_gain_raw(calibration_channel_t ch, float gain, const float *ext_gain);

/** True for V_TERM/I_OUT/I_ISO: channels that have an external-ADC
 *  (ADS131M04) counterpart which calibration_zero()/calibration_set_gain()
 *  calibrate automatically alongside the internal channel, since both
 *  measure the same physical quantity at the same moment. */
uint8_t calibration_has_ext(calibration_channel_t ch);

/** Live converted value of ch's ext-ADC counterpart, for on-screen
 *  confirmation while calibrating. 0 if calibration_has_ext(ch) is false.
 *  Reconfigures the ADC via calibration_ensure_adc_mode(); use
 *  calibration_peek_ext_converted() for a per-tick redraw instead. */
int32_t calibration_read_ext_converted(calibration_channel_t ch);

/** Non-blocking counterpart to calibration_read_ext_converted() -- same
 *  "must already be armed" requirement as calibration_peek_raw(). */
int32_t calibration_peek_ext_converted(calibration_channel_t ch);

#endif /* SRC_CALIBRATION_H_ */
