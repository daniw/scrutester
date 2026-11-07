#pragma once
#include <stdint.h>
#define EEPROM_SIZE 256U
#define EEPROM_PAGE_SIZE 8U
int8_t eeprom_read(uint16_t addr, uint8_t *data, uint16_t len);
int8_t eeprom_write(uint16_t addr, uint8_t *data, uint16_t len);
void eeprom_init(void);
