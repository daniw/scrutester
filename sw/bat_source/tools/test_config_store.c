/* Host test for config_store.c with a fake EEPROM, see tools/test_config_store.sh. */
#include <stdio.h>
#include <string.h>
#include "config_store.h"
#include "eeprom.h"
#include "ctrl_main.h"

static uint8_t rom[EEPROM_SIZE];
int8_t eeprom_read(uint16_t addr, uint8_t *data, uint16_t len) { memcpy(data, rom + addr, len); return 0; }
int8_t eeprom_write(uint16_t addr, uint8_t *data, uint16_t len) { memcpy(rom + addr, data, len); return 0; }
void eeprom_init(void) {}

extern config_store_t config_store;
#include "../Core/Src/config_store.c"

const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN] = {
#define ROW(n) { &config_store.gains.n##_p, &config_store.gains.n##_i, 1.5F, 2.5F }
	ROW(voltage_buck), ROW(voltage_boost), ROW(current), ROW(boost_iout_limit),
	ROW(charge_current), ROW(charge_voltage), ROW(voltage_hv), ROW(flyback_current),
	ROW(voltage_4sw), ROW(vlimit_10a) };

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void put_crc(uint16_t addr, uint16_t size)
{
	uint32_t crc = config_store_crc32b(rom + addr, size);
	memcpy(rom + addr + size, &crc, sizeof(crc));
}

/* Image as written by PSVN 2 firmware, all gains tuned to 9. */
static void make_v2(void)
{
	calibration_v2_t old;
	memset(rom, 0, sizeof(rom));
	memset(&old, 0, sizeof(old));
	rom[0] = 2;
	rom[8] = 0x34; rom[9] = 0x12;
	put_crc(8, 16);
	old.v_term_offset_mv = 11; old.v_term_gain = 2.0F;
	old.v_hv_gain = 3.0F; old.i_iso_ext_offset = -7;
	for (int i = 0; i < 16; i++) old.gains[i] = 9.0F;
	old.battery_capacity_mAs = 123456; old.i_1a_ref_dac_value = 77;
	old.v_in_offset_mv = 5; old.v_in_gain = 4.0F;
	memcpy(rom + CONFIG_STORE_CALIBRATION_ADDR, &old, sizeof(old));
	put_crc(CONFIG_STORE_CALIBRATION_ADDR, 192);
}

static void test_v2_migration(void)
{
	make_v2();
	CHECK(config_store_read() == 0);
	CHECK(config_store.hardware_data.serial_number == 0x1234);
	CHECK(config_store.calibration.v_term_offset_mv == 11);
	CHECK(config_store.calibration.v_term_gain == 2.0F);
	CHECK(config_store.calibration.v_hv_gain == 3.0F);
	CHECK(config_store.calibration.i_iso_ext_offset == -7);
	CHECK(config_store.calibration.battery_capacity_mAs == 123456);
	CHECK(config_store.calibration.i_1a_ref_dac_value == 77);
	CHECK(config_store.calibration.v_in_offset_mv == 5);
	CHECK(config_store.calibration.v_in_gain == 4.0F);
	for (int i = 0; i < CTRL_PID_TABLE_LEN; i++)
	{
		CHECK(*ctrl_pid_table[i].cal_p == 1.5F);
		CHECK(*ctrl_pid_table[i].cal_i == 2.5F);
	}
	/* Written back in the new layout: a second read is a plain same-PSVN load. */
	CHECK(rom[0] == CONFIG_STORE_PSVN);
	config_store_t before = config_store;
	CHECK(config_store_read() == 0);
	CHECK(memcmp(&before.calibration, &config_store.calibration, sizeof(calibration_t)) == 0);
	CHECK(memcmp(&before.gains, &config_store.gains, sizeof(gains_t)) == 0);
}

static void test_same_psvn(void)
{
	config_store_t keep;
	config_store_setDefaults();
	config_store.calibration.v_term_offset_mv = 42;
	config_store.gains.current_p = 8.0F;
	config_store.hardware_data.serial_number = 99;
	config_store_storeHW();
	config_store_store();
	keep = config_store;

	CHECK(config_store_read() == 0);
	CHECK(config_store.calibration.v_term_offset_mv == 42);
	CHECK(config_store.gains.current_p == 8.0F);
	CHECK(config_store.hardware_data.serial_number == 99);

	/* Corrupt gains only: calibration still adopted. */
	rom[CONFIG_STORE_GAINS_ADDR] ^= 0xFF;
	CHECK(config_store_read() == -5);
	CHECK(config_store.calibration.v_term_offset_mv == 42);
	CHECK(config_store.gains.voltage_buck_p == 1.5F);
	(void) keep;
}

static void test_future_upgrade(void)
{
	/* Stored one version older than the code (pretend code is newer). */
	config_store_t img;
	config_store_setDefaults();
	config_store.calibration.v_in_gain = 6.0F;
	config_store.gains.current_p = 8.0F;
	config_store_storeHW();
	config_store_store();
	rom[0] = CONFIG_STORE_PSVN - 1 >= 3 ? CONFIG_STORE_PSVN - 1 : CONFIG_STORE_PSVN;
	if (rom[0] == CONFIG_STORE_PSVN) return; /* only meaningful from PSVN 4 on */
	memcpy(&img, rom, sizeof(img));
	config_store_setDefaults();
	int8_t res = 0;
	CHECK(config_store_parse(&img, &config_store, &res) == CONFIG_LOAD_MIGRATED);
	CHECK(config_store.calibration.v_in_gain == 6.0F);
	CHECK(config_store.gains.current_p == 1.5F);
}

static void test_downgrade_and_blank(void)
{
	config_store_setDefaults();
	config_store_storeHW();
	config_store_store();
	rom[0] = CONFIG_STORE_PSVN + 1;
	uint8_t snap[EEPROM_SIZE];
	memcpy(snap, rom, sizeof(rom));
	CHECK(config_store_read() == -2);
	CHECK(memcmp(snap, rom, sizeof(rom)) == 0); /* EEPROM untouched */
	CHECK(config_store.gains.voltage_buck_p == 1.5F);

	memset(rom, 0xFF, sizeof(rom));
	CHECK(config_store_read() == -2);
	CHECK(rom[0] == CONFIG_STORE_PSVN); /* defaults written */
	CHECK(config_store_read() == 0);
}

static void test_soc(void)
{
	config_store_setDefaults();
	config_store.calibration.v_term_offset_mv = 42;
	config_store.gains.current_p = 8.0F;
	config_store_storeHW();
	config_store_store();
	CHECK(config_store_read() == 0);
	CHECK(config_store.soc.valid == 0);

	CHECK(config_store_store_soc(-123456, -1000) == 0);
	CHECK(config_store_read() == 0);
	CHECK(config_store.soc.valid == 1);
	CHECK(config_store.soc.charge_mAs == -123456);
	CHECK(config_store.soc.offset_mAs == -1000);
	CHECK(config_store.calibration.v_term_offset_mv == 42);

	/* Corrupt soc only: invalid, calibration and gains still load. */
	rom[CONFIG_STORE_SOC_ADDR] ^= 0xFF;
	CHECK(config_store_read() == 0);
	CHECK(config_store.soc.valid == 0);
	CHECK(config_store.calibration.v_term_offset_mv == 42);
	CHECK(config_store.gains.current_p == 8.0F);
}

int main(void)
{
	test_v2_migration();
	test_same_psvn();
	test_future_upgrade();
	test_downgrade_and_blank();
	test_soc();
	if (fails) { printf("%d FAILED\n", fails); return 1; }
	printf("test_config_store: OK\n");
	return 0;
}
