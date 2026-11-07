/*
 * config_store.c
 *
 * See config_store.h. Read/store logic and the CRC32 ported from the
 * night-cones project's config_store.cpp (Andreas Horat), adapted to
 * this project's blocking eeprom.c driver and re-targeted at
 * BatSource's own calibration data instead of night-cones' cone
 * settings.
 */

#include "config_store.h"
#include "eeprom.h"
#include "adc.h"
#include "ctrl_param.h"
#include "ctrl_main.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

config_store_t config_store;

#define CONFIG_STORE_HEADER_ADDR      0U
#define CONFIG_STORE_HARDWARE_ADDR    8U
#define CONFIG_STORE_CALIBRATION_ADDR (CONFIG_STORE_HARDWARE_ADDR + CONFIG_STORE_HARDWARE_DATA_SIZE + sizeof(uint32_t))
#define CONFIG_STORE_GAINS_ADDR       (CONFIG_STORE_CALIBRATION_ADDR + CONFIG_STORE_CALIBRATION_SIZE + sizeof(uint32_t))
#define CONFIG_STORE_USED_SIZE        (CONFIG_STORE_GAINS_ADDR + CONFIG_STORE_GAINS_SIZE + sizeof(uint32_t))

_Static_assert(offsetof(config_store_t, hardware_data) == CONFIG_STORE_HARDWARE_ADDR, "hardware block address");
_Static_assert(offsetof(config_store_t, calibration) == CONFIG_STORE_CALIBRATION_ADDR, "calibration block address");
_Static_assert(offsetof(config_store_t, gains) == CONFIG_STORE_GAINS_ADDR, "gains block address");
_Static_assert(CONFIG_STORE_USED_SIZE <= EEPROM_SIZE, "config_store does not fit the EEPROM");

// PSVN 2 calibration block (calibration and gains mixed), read for migration only.
#define CONFIG_STORE_V2_PSVN 2
#define CONFIG_STORE_V2_CALIBRATION_SIZE 192
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

	float gains[16];

	uint32_t battery_capacity_mAs;
	uint16_t i_1a_ref_dac_value;
	uint16_t v_in_offset_mv;
	float    v_in_gain;
} calibration_v2_t;

_Static_assert(sizeof(calibration_v2_t) <= CONFIG_STORE_V2_CALIBRATION_SIZE, "calibration_v2_t too big");

typedef enum
{
	CONFIG_LOAD_OK,       // same PSVN
	CONFIG_LOAD_MIGRATED, // older PSVN, calibration kept, gains default
	CONFIG_LOAD_UNKNOWN,  // blank or unknown old PSVN
	CONFIG_LOAD_NEWER     // stored by newer firmware
} config_load_t;

/**
 * CRC32 (same polynomial/algorithm as night-cones' config_store).
 * @param message pointer to the data to be CRCed.
 * @param length number of bytes to calculate.
 * @return the CRC value.
 */
static uint32_t config_store_crc32b(const uint8_t *message, uint16_t length)
{
	uint32_t byte, crc, mask;

	crc = 0xFFFFFFFF;
	for (uint16_t i = 0; i < length; i++)
	{
		byte = message[i];
		crc = crc ^ byte;
		for (int8_t j = 7; j >= 0; j--)
		{
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	return ~crc;
}

/**
 * Fills config_store with the values used today as hardcoded
 * constants in adc.h/ctrl_param.h, so a blank or corrupt EEPROM
 * reproduces today's behavior exactly instead of leaving zeroed-out
 * calibration in place.
 */
static void config_store_setDefaults(void)
{
	memset(&config_store, 0, sizeof(config_store));
	config_store.psvn = CONFIG_STORE_PSVN;

	config_store.hardware_data.serial_number = 0;

	config_store.calibration.v_term_offset_mv = ADC_VTERM_OFFSET_MV;
	config_store.calibration.v_term_gain      = ADC_VTERM_GAIN_MV;
	config_store.calibration.i_out_offset_ma  = ADC_IOUT_OFFSET;
	config_store.calibration.i_out_gain       = ADC_IOUT_GAIN_MA;
	config_store.calibration.i_iso_offset_ua  = ADC_IISO_OFFSET_UA;
	config_store.calibration.i_iso_gain       = ADC_IISO_GAIN_MA;

	config_store.calibration.v_in_offset_mv = ADC_VIN_OFFSET_MV;
	config_store.calibration.v_in_gain      = ADC_VIN_GAIN_MV;

	config_store.calibration.v_out_offset  = ADC_VOUT_OFFSET_MV;
	config_store.calibration.v_out_gain    = ADC_VOUT_GAIN_MV;
	config_store.calibration.v_hv_offset   = ADC_VHV_OFFSET_MV;
	config_store.calibration.v_hv_gain     = ADC_VHV_GAIN_MV;

	config_store.calibration.v_sens_ext_offset = 0;
	config_store.calibration.v_sens_ext_gain   = ADC_EXT_VSENS_GAIN_UV;
	config_store.calibration.v_term_ext_offset = 0;
	config_store.calibration.v_term_ext_gain   = ADC_EXT_VTERM_GAIN_MV;
	config_store.calibration.i_out_ext_offset  = 0;
	config_store.calibration.i_out_ext_gain    = ADC_EXT_IOUT_GAIN_mA;
	config_store.calibration.i_iso_ext_offset  = 0;
	config_store.calibration.i_iso_ext_gain    = ADC_EXT_IISO_GAIN_UA;

	// ctrl_pid_table (ctrl_main.c) is the single source of truth for PID
	// defaults: every tunable loop has exactly one row there, so looping
	// over it here means a newly added row can't be forgotten in this
	// function the way voltage_hv_p/i and flyback_current_p/i once were.
	for (int i = 0; i < CTRL_PID_TABLE_LEN; i++)
	{
		const ctrl_pid_entry_t *e = &ctrl_pid_table[i];
		*e->cal_p = e->default_p;
		*e->cal_i = e->default_i;
	}

	config_store.calibration.battery_capacity_mAs = CTRL_PARAM_BATTERY_CAPACITY_mAs;
	config_store.calibration.i_1a_ref_dac_value = CTRL_PARAM_1A_REF_DAC_VALUE;
}

/**
 * Adopts the stored image img into cfg, which already holds defaults.
 * hardware_data and calibration are kept if their CRC is valid, gains
 * are only adopted for the current PSVN (otherwise they stay default).
 * A PSVN 2 image is converted from the old mixed calibration layout.
 * @param result set to a negative code on a CRC mismatch (-3 hardware,
 *        -4 calibration, -5 gains), left alone otherwise.
 * @return what was found, see config_load_t.
 */
static config_load_t config_store_parse(const config_store_t *img, config_store_t *cfg, int8_t *result)
{
	uint8_t psvn = img->psvn;

	if (psvn == 0xFF || psvn < CONFIG_STORE_V2_PSVN)
		return CONFIG_LOAD_UNKNOWN;
	if (psvn > CONFIG_STORE_PSVN)
		return CONFIG_LOAD_NEWER;

	if (config_store_crc32b((const uint8_t *) &img->hardware_data, CONFIG_STORE_HARDWARE_DATA_SIZE) == img->hardware_data_crc)
	{
		cfg->hardware_data = img->hardware_data;
	}
	else
	{
		printf("EEPROM: hardware data CRC mismatch, using defaults\r\n");
		*result = -3;
	}

	if (psvn == CONFIG_STORE_V2_PSVN)
	{
		const uint8_t *raw = (const uint8_t *) img + CONFIG_STORE_CALIBRATION_ADDR;
		uint32_t crc;
		calibration_v2_t old;
		calibration_t *c = &cfg->calibration;

		memcpy(&crc, raw + CONFIG_STORE_V2_CALIBRATION_SIZE, sizeof(crc));
		if (config_store_crc32b(raw, CONFIG_STORE_V2_CALIBRATION_SIZE) == crc)
		{
			memcpy(&old, raw, sizeof(old));
			c->v_term_offset_mv = old.v_term_offset_mv;
			c->v_term_gain = old.v_term_gain;
			c->i_out_offset_ma = old.i_out_offset_ma;
			c->i_out_gain = old.i_out_gain;
			c->i_iso_offset_ua = old.i_iso_offset_ua;
			c->i_iso_gain = old.i_iso_gain;
			c->v_sens_ext_offset = old.v_sens_ext_offset;
			c->v_sens_ext_gain = old.v_sens_ext_gain;
			c->v_out_offset = old.v_out_offset;
			c->v_out_gain = old.v_out_gain;
			c->v_hv_offset = old.v_hv_offset;
			c->v_hv_gain = old.v_hv_gain;
			c->v_term_ext_offset = old.v_term_ext_offset;
			c->v_term_ext_gain = old.v_term_ext_gain;
			c->i_out_ext_offset = old.i_out_ext_offset;
			c->i_out_ext_gain = old.i_out_ext_gain;
			c->i_iso_ext_offset = old.i_iso_ext_offset;
			c->i_iso_ext_gain = old.i_iso_ext_gain;
			c->battery_capacity_mAs = old.battery_capacity_mAs;
			c->i_1a_ref_dac_value = old.i_1a_ref_dac_value;
			c->v_in_offset_mv = old.v_in_offset_mv;
			c->v_in_gain = old.v_in_gain;
		}
		else
		{
			printf("EEPROM: calibration CRC mismatch, using defaults\r\n");
			*result = -4;
		}
		return CONFIG_LOAD_MIGRATED;
	}

	if (config_store_crc32b((const uint8_t *) &img->calibration, CONFIG_STORE_CALIBRATION_SIZE) == img->calibration_crc)
	{
		cfg->calibration = img->calibration;
	}
	else
	{
		printf("EEPROM: calibration CRC mismatch, using defaults\r\n");
		*result = -4;
	}

	if (psvn != CONFIG_STORE_PSVN)
		return CONFIG_LOAD_MIGRATED;

	if (config_store_crc32b((const uint8_t *) &img->gains, CONFIG_STORE_GAINS_SIZE) == img->gains_crc)
	{
		cfg->gains = img->gains;
	}
	else
	{
		printf("EEPROM: gains CRC mismatch, using defaults\r\n");
		*result = -5;
	}

	return CONFIG_LOAD_OK;
}

/**
 * Re-reads config_store from the EEPROM. Always leaves config_store
 * fully populated with valid data: defaults are applied up front,
 * then each block (hardware_data, calibration, gains) is individually
 * adopted from the EEPROM only if its own CRC checks out, so a
 * corrupt block never leaves garbage in a live calibration value.
 * After a PSVN upgrade the calibration is kept, gains are reset, and
 * the new layout is written back.
 */
int8_t config_store_read(void)
{
	config_store_t temp;
	int8_t result = 0;

	config_store_setDefaults();

	if (eeprom_read(CONFIG_STORE_HEADER_ADDR, (uint8_t *) &temp, CONFIG_STORE_USED_SIZE) != 0)
	{
		printf("EEPROM: read failed, using defaults\r\n");
		return -1;
	}

	switch (config_store_parse(&temp, &config_store, &result))
	{
	case CONFIG_LOAD_UNKNOWN:
		printf("EEPROM: storage version 0x%02X unknown, writing defaults\r\n", temp.psvn);
		config_store_storeHW();
		config_store_store();
		return -2;
	case CONFIG_LOAD_NEWER:
		printf("EEPROM: storage version %u is newer than firmware (%u), using defaults, EEPROM untouched\r\n",
				temp.psvn, CONFIG_STORE_PSVN);
		return -2;
	case CONFIG_LOAD_MIGRATED:
		config_store_storeHW();
		config_store_store();
		printf("EEPROM: migrated PSVN %u -> %u, calibration kept, gains reset\r\n",
				temp.psvn, CONFIG_STORE_PSVN);
		break;
	default:
		break;
	}

	return result;
}

/**
 * Writes [src, src+len) to the EEPROM starting at addr, one
 * EEPROM_PAGE_SIZE page at a time, skipping any page whose EEPROM content
 * already matches src (a failed read is treated as "does not match", so
 * that page is written anyway). Returns 0 if every page that needed writing
 * wrote successfully, otherwise the eeprom_write()/eeprom_read() result of
 * the last page that failed -- callers accumulate this the same way the
 * pre-extraction inline loops did: only overwrite their own running result
 * when this returns non-zero, never reset it back to 0.
 */
static int8_t config_store_write_block(uint16_t addr, const uint8_t *src, uint16_t len)
{
	int8_t result = 0;
	uint8_t buffer[EEPROM_PAGE_SIZE];

	for (uint16_t i = 0; i < len; i += EEPROM_PAGE_SIZE)
	{
		int8_t r = eeprom_read(addr + i, buffer, EEPROM_PAGE_SIZE);
		if (r != 0 || memcmp(buffer, src + i, EEPROM_PAGE_SIZE) != 0)
		{
			r = eeprom_write(addr + i, (uint8_t *) (src + i), EEPROM_PAGE_SIZE);
			if (r != 0)
				result = r;
		}
	}

	return result;
}

/**
 * Computes the CRC of a block and writes block and CRC (CRC directly
 * behind the block), only touching pages whose content changed.
 * @return 0 on success, negative error code otherwise.
 */
static int8_t config_store_store_block(uint16_t addr, uint8_t *block, uint16_t size, uint32_t *crc)
{
	int8_t result = 0;
	int8_t r;

	*crc = config_store_crc32b(block, size);

	r = config_store_write_block(addr, block, size);
	if (r != 0)
		result = r;

	r = eeprom_write(addr + size, (uint8_t *) crc, sizeof(*crc));
	if (r != 0)
		result = r;

	return result;
}

/**
 * Persists the calibration and gains blocks (and their CRCs) to the
 * EEPROM, writing only pages whose content actually changed.
 */
int8_t config_store_store(void)
{
	int8_t result = 0;
#ifdef ENABLE_CALIBRATION
	int8_t r;

	r = config_store_store_block(CONFIG_STORE_CALIBRATION_ADDR,
			(uint8_t *) &config_store.calibration, CONFIG_STORE_CALIBRATION_SIZE,
			&config_store.calibration_crc);
	if (r != 0)
		result = r;

	r = config_store_store_block(CONFIG_STORE_GAINS_ADDR,
			(uint8_t *) &config_store.gains, CONFIG_STORE_GAINS_SIZE,
			&config_store.gains_crc);
	if (r != 0)
		result = r;
#endif
	return result;
}

/**
 * Persists the header and hardware_data block (and its CRC) to the
 * EEPROM, writing only pages whose content actually changed.
 */
int8_t config_store_storeHW(void)
{
	int8_t result = 0;
	int8_t r;

	config_store.hardware_data_crc = config_store_crc32b((uint8_t *) &config_store.hardware_data, CONFIG_STORE_HARDWARE_DATA_SIZE);

	r = config_store_write_block(CONFIG_STORE_HEADER_ADDR,
			(uint8_t *) &config_store, EEPROM_PAGE_SIZE);
	if (r != 0)
		result = r;

	r = config_store_write_block(CONFIG_STORE_HARDWARE_ADDR,
			(uint8_t *) &config_store.hardware_data, CONFIG_STORE_HARDWARE_DATA_SIZE);
	if (r != 0)
		result = r;

	r = eeprom_write(CONFIG_STORE_HARDWARE_ADDR + CONFIG_STORE_HARDWARE_DATA_SIZE,
			(uint8_t *) &config_store.hardware_data_crc, sizeof(config_store.hardware_data_crc));
	if (r != 0)
		result = r;

	return result;
}

/**
 * Populates config_store with defaults and loads it from the EEPROM.
 * Must run after MX_I2C4_Init() and before adc_init()/ctrl_main_init().
 */
void config_store_init(void)
{
	eeprom_init();
	config_store_read();
}
