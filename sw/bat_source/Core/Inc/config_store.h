/*
 * config_store.h
 *
 * Persistent configuration store on the external EEPROM: a versioned
 * header, a hardware_data block (factory ID data) and a calibration
 * block (ADC channel trims + PID tuning gains), each with its own
 * CRC32. Structure and workflow ported from the night-cones project's
 * config_store (Andreas Horat), with the night-cones-specific fields
 * (cone_id, lightmode, network settings, ...) replaced by fields that
 * are actually meaningful for BatSource.
 */

#ifndef SRC_CONFIG_STORE_H_
#define SRC_CONFIG_STORE_H_

#include <stdint.h>

#define CONFIG_STORE_PSVN 1 // Version of the persistent storage layout

#define CONFIG_STORE_HARDWARE_DATA_SIZE 16
#define CONFIG_STORE_CALIBRATION_SIZE   192

/**
 * Factory-programmed hardware identification. Hardware revision is
 * intentionally not duplicated here: it is already read live from the
 * PCA9554 GPIO expander at boot (see driver_pca9554.c), and keeping a
 * second copy here would risk the two drifting out of sync.
 */
typedef struct
{
	uint16_t serial_number;
} hardware_data_t;

/**
 * Field calibration data: ADC channel offset/gain trims for the
 * channels with known calibration needs (V_TERM, I_OUT, I_ISO - see
 * architecture.md known issue #5), and the PID tuning gains that are
 * otherwise hardcoded placeholders in ctrl_param.h (known issue #7).
 * Units and scaling match what's fed directly into adc_data and
 * ctrl_PID_controller_init()/PID_controller_t, so no re-scaling is
 * needed when loading or saving.
 *
 * v_sens/v_out/v_hv were added alongside v_term/i_out/i_iso once the
 * calibration.c module made all six channels (V_TERM, V_SENS, V_OUT,
 * V_HV, I_OUT, I_ISO) calibratable from the Settings menu / CLI -
 * offset is always in raw ADC counts (same convention as the
 * pre-existing three fields), subtracted before gain is applied.
 *
 * v_term_ext/i_out_ext/i_iso_ext trim the external ADS131M04's
 * counterparts of v_term/i_out/i_iso. They have no channel of their
 * own in calibration.c - calibrating V_TERM/I_OUT/I_ISO samples and
 * sets these in the same pass, since both readings come from the same
 * physical measurement at the same moment.
 */
typedef struct
{
	uint16_t v_term_offset_mv;
	float    v_term_gain;
	uint16_t i_out_offset_ma;
	float    i_out_gain;
	uint16_t i_iso_offset_ua;
	float    i_iso_gain;

	int32_t  v_sens_ext_offset;
	float    v_sens_ext_gain;
	uint16_t v_out_offset;
	float    v_out_gain;
	uint16_t v_hv_offset;
	float    v_hv_gain;

	int32_t  v_term_ext_offset;
	float    v_term_ext_gain;
	int32_t  i_out_ext_offset;
	float    i_out_ext_gain;
	int32_t  i_iso_ext_offset;
	float    i_iso_ext_gain;

	float voltage_buck_p;
	float voltage_buck_i;
	float voltage_boost_p;
	float voltage_boost_i;
	float current_p;
	float current_i;
	float boost_iout_limit_p;
	float boost_iout_limit_i;
	float charge_current_p;
	float charge_current_i;
	float charge_voltage_p;
	float charge_voltage_i;
	float voltage_hv_p;
	float voltage_hv_i;
	float flyback_current_p;
	float flyback_current_i;

	// Rated pack capacity in mA-seconds - see CTRL_PARAM_BATTERY_CAPACITY_mAs.
	uint32_t battery_capacity_mAs;

	// Raw 12-bit DAC code for the RESISTANCE_1A mode's pulsed I_1A_REF
	// reference - see CTRL_PARAM_1A_REF_DAC_VALUE. Adaptable via EEPROM so
	// it can be bench-calibrated instead of staying a fixed placeholder.
	uint16_t i_1a_ref_dac_value;
} calibration_t;

typedef struct
{
	uint8_t psvn;
	uint8_t header_reserved[7]; // Pads header to 8 bytes.

	hardware_data_t hardware_data;
	uint8_t hardware_data_reserved[CONFIG_STORE_HARDWARE_DATA_SIZE - sizeof(hardware_data_t)];
	uint32_t hardware_data_crc;

	calibration_t calibration;
	uint8_t calibration_reserved[CONFIG_STORE_CALIBRATION_SIZE - sizeof(calibration_t)];
	uint32_t calibration_crc;
} config_store_t;

_Static_assert(sizeof(hardware_data_t) <= CONFIG_STORE_HARDWARE_DATA_SIZE, "hardware_data_t too big for its reserved block");
_Static_assert(sizeof(calibration_t) <= CONFIG_STORE_CALIBRATION_SIZE, "calibration_t too big for its reserved block");

extern config_store_t config_store;

/**
 * Populates config_store with sane defaults and loads it from the
 * EEPROM, applying calibration to adc_data. Must run after
 * MX_I2C4_Init() and before adc_init()/ctrl_main_init() so both see
 * the loaded (or defaulted) calibration.
 */
void config_store_init(void);

/**
 * Re-reads config_store from the EEPROM.
 * @return 0 on success, negative error code otherwise (config_store
 *         still holds valid, defaulted data in that case).
 */
int8_t config_store_read(void);

/**
 * Persists the calibration block (and its CRC) to the EEPROM. Only
 * writes pages whose content actually changed.
 * @return 0 on success, negative error code otherwise.
 */
int8_t config_store_store(void);

/**
 * Persists the header and hardware_data block (and its CRC) to the
 * EEPROM. This is factory/provisioning data - only call it from a
 * deliberate commandline action, not as part of normal operation.
 * @return 0 on success, negative error code otherwise.
 */
int8_t config_store_storeHW(void);

#endif /* SRC_CONFIG_STORE_H_ */
