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

const char *calibration_channel_name(calibration_channel_t ch);
const char *calibration_channel_unit(calibration_channel_t ch);

/** Live raw ADC counts for the channel, averaged over a few samples. */
int32_t calibration_read_raw(calibration_channel_t ch);

/** Live converted physical-unit value (same field the rest of the
 *  firmware reads), for on-screen/CLI feedback while calibrating. */
int32_t calibration_read_converted(calibration_channel_t ch);

/** Samples raw ADC counts and stores them as the channel's offset,
 *  then persists to EEPROM. */
void calibration_zero(calibration_channel_t ch);

/** Samples raw ADC counts, computes gain = reference_value / (raw -
 *  offset), stores it, then persists to EEPROM. */
void calibration_set_gain(calibration_channel_t ch, float reference_value);

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
 *  without an ext counterpart. */
void calibration_set_gain_raw(calibration_channel_t ch, float gain, const float *ext_gain);

/** True for V_TERM/I_OUT/I_ISO: channels that have an external-ADC
 *  (ADS131M04) counterpart which calibration_zero()/calibration_set_gain()
 *  calibrate automatically alongside the internal channel, since both
 *  measure the same physical quantity at the same moment. */
uint8_t calibration_has_ext(calibration_channel_t ch);

/** Live converted value of ch's ext-ADC counterpart, for on-screen
 *  confirmation while calibrating. 0 if calibration_has_ext(ch) is false. */
int32_t calibration_read_ext_converted(calibration_channel_t ch);

#endif /* SRC_CALIBRATION_H_ */
