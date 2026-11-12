/*
 * eeprom.h
 *
 * Driver for the ZD24C02B I2C EEPROM (2 kbit / 256 bytes) used for
 * calibration and ID data storage. Ported from the i2cEeprom driver
 * in the night-cones project (Andreas Horat) to this project's
 * blocking I2C API.
 */

#ifndef SRC_EEPROM_H_
#define SRC_EEPROM_H_

#include <stdint.h>

#define EEPROM_I2C_ADDRESS    (0x50 << 1)
#define EEPROM_SIZE           256U  // ZD24C02B: 2 kbit
#define EEPROM_PAGE_SIZE      8U
#define EEPROM_WRITE_DELAY_MS 5U
#define EEPROM_READY_TIMEOUT_MS 50U

void eeprom_init(void);
uint8_t eeprom_isReady(void);
int8_t eeprom_read(uint16_t addr, uint8_t *data, uint16_t count);
int8_t eeprom_write(uint16_t addr, uint8_t *data, uint16_t count);
uint16_t eeprom_length(void);
void eeprom_erase(uint8_t value);

#endif /* SRC_EEPROM_H_ */
