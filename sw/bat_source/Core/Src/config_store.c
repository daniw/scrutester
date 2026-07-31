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
#include <string.h>
#include <stdio.h>

config_store_t config_store;

#define CONFIG_STORE_HEADER_ADDR      0U
#define CONFIG_STORE_HARDWARE_ADDR    8U
#define CONFIG_STORE_CALIBRATION_ADDR (CONFIG_STORE_HARDWARE_ADDR + CONFIG_STORE_HARDWARE_DATA_SIZE + sizeof(uint32_t))
#define CONFIG_STORE_USED_SIZE        (CONFIG_STORE_CALIBRATION_ADDR + CONFIG_STORE_CALIBRATION_SIZE + sizeof(uint32_t))

/**
 * CRC32 (same polynomial/algorithm as night-cones' config_store).
 * @param message pointer to the data to be CRCed.
 * @param length number of bytes to calculate.
 * @return the CRC value.
 */
static uint32_t config_store_crc32b(uint8_t *message, uint16_t length)
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

	config_store.calibration.voltage_buck_p  = CTRL_PARAM_VOLTAGE_BUCK_P;
	config_store.calibration.voltage_buck_i  = CTRL_PARAM_VOLTAGE_BUCK_I;
	config_store.calibration.voltage_boost_p = CTRL_PARAM_VOLTAGE_BOOST_P;
	config_store.calibration.voltage_boost_i = CTRL_PARAM_VOLTAGE_BOOST_I;
	config_store.calibration.current_p       = CTRL_PARAM_CURRENT_P;
	config_store.calibration.current_i       = CTRL_PARAM_CURRENT_I;
	config_store.calibration.boost_iout_limit_p = CTRL_PARAM_BOOST_IOUT_LIMIT_P;
	config_store.calibration.boost_iout_limit_i = CTRL_PARAM_BOOST_IOUT_LIMIT_I;
	config_store.calibration.charge_current_p = CTRL_PARAM_CHARGE_CURRENT_P;
	config_store.calibration.charge_current_i = CTRL_PARAM_CHARGE_CURRENT_I;
	config_store.calibration.battery_capacity_mAs = CTRL_PARAM_BATTERY_CAPACITY_mAs;
	config_store.calibration.i_1a_ref_dac_value = CTRL_PARAM_1A_REF_DAC_VALUE;
}

/**
 * Re-reads config_store from the EEPROM. Always leaves config_store
 * fully populated with valid data: defaults are applied up front,
 * then each block (hardware_data, calibration) is individually
 * adopted from the EEPROM only if its own CRC checks out, so a
 * corrupt block never leaves garbage in a live calibration value.
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

	if (temp.psvn != CONFIG_STORE_PSVN)
	{
		printf("EEPROM: storage version 0x%02X unknown, writing defaults\r\n", temp.psvn);
		config_store_storeHW();
		config_store_store();
		return -2;
	}

	if (config_store_crc32b((uint8_t *) &temp.hardware_data, CONFIG_STORE_HARDWARE_DATA_SIZE) == temp.hardware_data_crc)
	{
		config_store.hardware_data = temp.hardware_data;
	}
	else
	{
		printf("EEPROM: hardware data CRC mismatch, using defaults\r\n");
		result = -3;
	}

	if (config_store_crc32b((uint8_t *) &temp.calibration, CONFIG_STORE_CALIBRATION_SIZE) == temp.calibration_crc)
	{
		config_store.calibration = temp.calibration;
	}
	else
	{
		printf("EEPROM: calibration CRC mismatch, using defaults\r\n");
		result = -4;
	}

	return result;
}

/**
 * Persists the calibration block (and its CRC) to the EEPROM,
 * writing only pages whose content actually changed.
 */
int8_t config_store_store(void)
{
	int8_t result = 0;
	uint8_t buffer[EEPROM_PAGE_SIZE];

	config_store.calibration_crc = config_store_crc32b((uint8_t *) &config_store.calibration, CONFIG_STORE_CALIBRATION_SIZE);

	for (uint16_t i = 0; i < CONFIG_STORE_CALIBRATION_SIZE; i += EEPROM_PAGE_SIZE)
	{
		uint8_t *src = ((uint8_t *) &config_store.calibration) + i;
		int8_t r = eeprom_read(CONFIG_STORE_CALIBRATION_ADDR + i, buffer, EEPROM_PAGE_SIZE);
		if (r != 0 || memcmp(buffer, src, EEPROM_PAGE_SIZE) != 0)
		{
			r = eeprom_write(CONFIG_STORE_CALIBRATION_ADDR + i, src, EEPROM_PAGE_SIZE);
			if (r != 0)
				result = r;
		}
	}

	int8_t r = eeprom_write(CONFIG_STORE_CALIBRATION_ADDR + CONFIG_STORE_CALIBRATION_SIZE,
			(uint8_t *) &config_store.calibration_crc, sizeof(config_store.calibration_crc));
	if (r != 0)
		result = r;

	return result;
}

/**
 * Persists the header and hardware_data block (and its CRC) to the
 * EEPROM, writing only pages whose content actually changed.
 */
int8_t config_store_storeHW(void)
{
	int8_t result = 0;
	uint8_t buffer[EEPROM_PAGE_SIZE];

	config_store.hardware_data_crc = config_store_crc32b((uint8_t *) &config_store.hardware_data, CONFIG_STORE_HARDWARE_DATA_SIZE);

	int8_t r = eeprom_read(CONFIG_STORE_HEADER_ADDR, buffer, EEPROM_PAGE_SIZE);
	if (r != 0 || memcmp(buffer, &config_store, EEPROM_PAGE_SIZE) != 0)
	{
		r = eeprom_write(CONFIG_STORE_HEADER_ADDR, (uint8_t *) &config_store, EEPROM_PAGE_SIZE);
		if (r != 0)
			result = r;
	}

	for (uint16_t i = 0; i < CONFIG_STORE_HARDWARE_DATA_SIZE; i += EEPROM_PAGE_SIZE)
	{
		uint8_t *src = ((uint8_t *) &config_store.hardware_data) + i;
		r = eeprom_read(CONFIG_STORE_HARDWARE_ADDR + i, buffer, EEPROM_PAGE_SIZE);
		if (r != 0 || memcmp(buffer, src, EEPROM_PAGE_SIZE) != 0)
		{
			r = eeprom_write(CONFIG_STORE_HARDWARE_ADDR + i, src, EEPROM_PAGE_SIZE);
			if (r != 0)
				result = r;
		}
	}

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
