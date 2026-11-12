/*
 * eeprom.c
 *
 * Driver for the ZD24C02B I2C EEPROM (2 kbit / 256 bytes) used for
 * calibration and ID data storage. Ported from the i2cEeprom driver
 * in the night-cones project (Andreas Horat) to this project's
 * blocking I2C API (i2c_WriteBlocking / i2c_ReadBlocking), since
 * EEPROM access only ever happens at boot or on rare CLI-triggered
 * saves.
 */

#include "eeprom.h"
#include "i2c.h"
#include "error.h"
#include "main.h"
#include <string.h>

/**
 * Waits (bounded) for the EEPROM to acknowledge its address, i.e. for
 * an internal write cycle to complete.
 * @return 1 if the EEPROM became ready, 0 on timeout.
 */
static uint8_t eeprom_waitReady(void)
{
	uint32_t start = HAL_GetTick();
	while (!eeprom_isReady())
	{
		if ((HAL_GetTick() - start) > EEPROM_READY_TIMEOUT_MS)
		{
			error_Add(ERROR_EEPROM_TIMEOUT, EEPROM_I2C_ADDRESS);
			return 0;
		}
	}
	return 1;
}

/**
 * Initializes the EEPROM driver. The bus itself is brought up by
 * MX_I2C4_Init(); this only checks that the chip is present so a
 * missing/unpopulated EEPROM is logged once at boot instead of
 * surfacing as a timeout on the first real access.
 */
void eeprom_init(void)
{
	eeprom_isReady();
}

/**
 * Checks if the EEPROM is ready to receive commands.
 * @return Returns 1, if the EEPROM acknowledges its address.
 */
uint8_t eeprom_isReady(void)
{
	return i2c_IsDeviceReadyBlocking(EEPROM_I2C_ADDRESS);
}

/**
 * Read consecutive bytes from the EEPROM.
 * @param addr address of the first byte.
 * @param data buffer to store the read data.
 * @param count number of bytes to read.
 * @return Returns 0 on success, negative error code otherwise.
 */
int8_t eeprom_read(uint16_t addr, uint8_t *data, uint16_t count)
{
	if (count == 0 || (uint32_t) addr + count > EEPROM_SIZE)
		return -1;

	if (!eeprom_waitReady())
		return -2;

	uint8_t addr_byte = (uint8_t) addr;
	if (i2c_WriteBlocking(EEPROM_I2C_ADDRESS, &addr_byte, 1) != HAL_OK)
		return -3;

	if (i2c_ReadBlocking(EEPROM_I2C_ADDRESS, data, count) != HAL_OK)
		return -4;

	return 0;
}

/**
 * Write consecutive bytes to the EEPROM. Splits the transfer on page
 * boundaries (EEPROM_PAGE_SIZE) as required by the chip, and waits
 * out the write cycle after every page.
 * @param addr address of the first byte to write.
 * @param data data to write.
 * @param count number of bytes to write.
 * @return Returns 0 on success, negative error code otherwise.
 */
int8_t eeprom_write(uint16_t addr, uint8_t *data, uint16_t count)
{
	if ((uint32_t) addr + count > EEPROM_SIZE)
		count = EEPROM_SIZE - addr;

	uint16_t transferred = 0;
	while (transferred < count)
	{
		uint16_t write_count = count - transferred;
		if (write_count > EEPROM_PAGE_SIZE)
			write_count = EEPROM_PAGE_SIZE;

		// Clip to the current page boundary so a write never spans two pages.
		uint16_t page_end = ((addr / EEPROM_PAGE_SIZE) + 1) * EEPROM_PAGE_SIZE;
		if (addr + write_count > page_end)
			write_count = page_end - addr;

		if (!eeprom_waitReady())
			return -2;

		uint8_t buf[1 + EEPROM_PAGE_SIZE];
		buf[0] = (uint8_t) addr;
		memcpy(&buf[1], data, write_count);
		if (i2c_WriteBlocking(EEPROM_I2C_ADDRESS, buf, write_count + 1) != HAL_OK)
			return -3;

		addr += write_count;
		data += write_count;
		transferred += write_count;

		HAL_Delay(EEPROM_WRITE_DELAY_MS);
	}

	return 0;
}

/**
 * @return Returns the number of bytes in the EEPROM.
 */
uint16_t eeprom_length(void)
{
	return EEPROM_SIZE;
}

/**
 * Erases the whole EEPROM with the specified value.
 * @param value Default value to write to all memory locations.
 */
void eeprom_erase(uint8_t value)
{
	uint8_t page[EEPROM_PAGE_SIZE];
	memset(page, value, sizeof(page));

	for (uint16_t addr = 0; addr < EEPROM_SIZE; addr += EEPROM_PAGE_SIZE)
		eeprom_write(addr, page, EEPROM_PAGE_SIZE);
}
