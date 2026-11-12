/*
 * w25n01gv.c
 *
 *  Created on: Apr 20, 2026
 *      Author: daniw
 */

#include "w25n01gv.h"
#include "stdio.h"
#include <string.h>

#if W25N01GV_WRITE_CHECK_EMPTY
uint8_t buffer[W25N01GV_PAGE_SIZE];
#endif //W25N01GV_WRITE_CHECK_EMPTY

/*
 * Initialize flash memory W25N01GV
 * @param hw25n01gv W25N01GV handle to the device.
 * @param hqspi handle for QSPI peripheral
 * @param timeout timeout for QSPI operation
 */
w25n01gv_status_t w25n01gv_init(w25n01gv_handle *hw25n01gv, QSPI_HandleTypeDef *hqspi, uint32_t timeout) {
#ifdef W25N01GV_TEST_UNION_STRUCT
	/* test orientation of bitfields */
	test_bitfield_t test_bitfield;      /* testvariable */
	test_bitfield.byte[0] = 0x00;
	test_bitfield.bitfield.low = 0x01;  /* write testvalue to lower nibble */
	if (test_bitfield.byte[0] != 0x01) {   /* test if lower nibble has been written */
		while (1) {
			/* Hey Programmer,
			 * It seems that your compiler uses a
			 * different order for bitfields than mine.
			 * If you still want to use this library,
			 * change, if possible, the order of bitfields in
			 * your compiler or in this library!
			 */
		}
	}
	test_bitfield.word = 0x0000;
	test_bitfield.byte[0] = 0x55;  /* write testvalue to lower byte */
	if (test_bitfield.word != 0x0055) {   /* test if lower byte has been written */
		while (1) {
			/* Hey Programmer,
			 * It seems that your compiler uses a
			 * different order for bitfields than mine.
			 * If you still want to use this library,
			 * change, if possible, the order of bitfields in
			 * your compiler or in this library!
			 */
		}
	}
#endif // W25N01GV_TEST_UNION_STRUCT
	w25n01gv_status_t status;
	hw25n01gv->hqspi = hqspi;
	hw25n01gv->timeout = timeout;
	hw25n01gv->reg_prot.reg = W25N01GV_REG_PROT_INIT;
	hw25n01gv->reg_conf.reg = W25N01GV_REG_CONF_INIT;
	hw25n01gv->reg_status.reg = W25N01GV_REG_STATUS_INIT;
	hw25n01gv->reg_conf.fields.buf_mode = W25N01GV_BUF;
	status = w25n01gv_reset(hw25n01gv);
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg(hw25n01gv);
	}
	return status;
}

/*
 * Read data from flash memory
 * @param hw25n01gv W25N01GV handle to the device
 * @param data pointer to data
 * @param address address
 * @param size size of data to be read
 * Todo: Untested, must be tested before usage
 */
w25n01gv_status_t w25n01gv_read(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size) {
	w25n01gv_status_t status;
	uint16_t page_addr;
	uint16_t column_addr;
	uint32_t size_preread;

	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	column_addr = W25N01GV_ADDR_TO_COLUMN_ADDR(address);
	size_preread = (size > W25N01GV_PAGE_SIZE - column_addr) ? W25N01GV_PAGE_SIZE - column_addr : size;

	status = w25n01gv_wait_busy(hw25n01gv);
	if (column_addr != 0) {
		if (status == W25N01GV_OK) {
			status = w25n01gv_buffer_mode_enable(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_page_read(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK && hw25n01gv->reg_status.fields.ecc_status == 2) {
			status = W25N01GV_ECC_UNCORRECTABLE;
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_read_data(hw25n01gv, data, column_addr, size_preread);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		data += (W25N01GV_PAGE_SIZE - column_addr);
		page_addr++;
		size -= size_preread;
		column_addr = 0;
	}
	if (size > 0) {
		if (status == W25N01GV_OK) {
			status = w25n01gv_buffer_mode_disable(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_page_read(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK && hw25n01gv->reg_status.fields.ecc_status == 2) {
			status = W25N01GV_ECC_UNCORRECTABLE;
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_read_data(hw25n01gv, data, column_addr, size);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
	}
	return status;
}

/*
 * Read otp data from flash memory
 * @param hw25n01gv W25N01GV handle to the device
 * @param data pointer to data
 * @param address address
 * @param size size of data to be read
 * Todo: Untested, must be tested before usage
 */
w25n01gv_status_t w25n01gv_read_otp(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size) {
	w25n01gv_status_t status;
	uint16_t page_addr;
	uint16_t column_addr;
	uint32_t size_preread;
	w25n01gv_conf_reg_t reg_conf;

	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	column_addr = W25N01GV_ADDR_TO_COLUMN_ADDR(address);
	size_preread = (size > W25N01GV_PAGE_SIZE - column_addr) ? W25N01GV_PAGE_SIZE - column_addr : size;

	status = w25n01gv_wait_busy(hw25n01gv);
	if (status == W25N01GV_OK) {
		status = w25n01gv_buffer_mode_enable(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_conf(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		reg_conf = hw25n01gv->reg_conf;
		reg_conf.fields.otp_enter = 1;
		status = w25n01gv_write_reg_conf(hw25n01gv, &reg_conf);
	}
	if (column_addr != 0) {
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_page_read(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_read_data(hw25n01gv, data, column_addr, size_preread);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		data += (W25N01GV_PAGE_SIZE - column_addr);
		page_addr++;
		size -= size_preread;
		column_addr = 0;
	}
	if (size > 0) {
		if (status == W25N01GV_OK) {
			status = w25n01gv_page_read(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_read_data(hw25n01gv, data, column_addr, size);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_conf(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		reg_conf = hw25n01gv->reg_conf;
		reg_conf.fields.otp_enter = 0;
		status = w25n01gv_write_reg_conf(hw25n01gv, &reg_conf);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	return status;
}

/*
 * Erase data from flash memory
 * @param hw25n01gv W25N01GV handle to the device
 * @param address address
 * @param size size of data to be erased
 * Todo: Untested, must be tested before usage
 */
w25n01gv_status_t w25n01gv_erase(w25n01gv_handle *hw25n01gv, uint32_t start_address, uint32_t end_address) {
	w25n01gv_status_t status;
	w25n01gv_prot_reg_t reg_prot;

	status = w25n01gv_wait_busy(hw25n01gv);
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_prot(hw25n01gv);
	}
	reg_prot = hw25n01gv->reg_prot;
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_set_prot(hw25n01gv, W25N01GV_PROT_NONE, W25N01GV_PROT_LOWER);
	}
	for (uint32_t page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(start_address); page_addr <= W25N01GV_ADDR_TO_PAGE_ADDR(end_address); page_addr += W25N01GV_PAGES_PER_BLOCK) {
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_write_enable(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			//printf("Erasing block at page address: 0x%04lX\r\n", page_addr); // Todo: remove after debugging
			status = w25n01gv_block_erase(hw25n01gv, page_addr);
		}
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_set_prot(hw25n01gv, reg_prot.fields.block_prot, reg_prot.fields.top_bot_prot);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	return status;
}

/*
 * Write data to flash memory
 * @param hw25n01gv W25N01GV handle to the device
 * @param data pointer to data
 * @param address address
 * @param size size of data to be read
 * Todo: Untested, must be tested before usage
 */
w25n01gv_status_t w25n01gv_write(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size) {
	w25n01gv_status_t status;
	w25n01gv_prot_reg_t reg_prot;
	uint16_t page_addr;
	uint16_t column_addr;
	uint16_t write_size;

	if ((address + size) > W25N01GV_SIZE) {
		return W25N01GV_ADDR_OVERRUN;
	}

	status = w25n01gv_wait_busy(hw25n01gv);
	if (status == W25N01GV_OK) {
		status = w25n01gv_buffer_mode_enable(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_prot(hw25n01gv);
	}
	reg_prot = hw25n01gv->reg_prot;
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_set_prot(hw25n01gv, W25N01GV_PROT_NONE, W25N01GV_PROT_LOWER);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	while (size > 0) {
		page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
		column_addr = W25N01GV_ADDR_TO_COLUMN_ADDR(address);
		write_size = (column_addr + size >= W25N01GV_PAGE_SIZE) ? W25N01GV_PAGE_SIZE - column_addr : size;

		//printf("Writing, page address: 0x%04X, column address: 0x%04X, size: %u\r\n", page_addr, column_addr, write_size); // Todo: remove after debugging

#if W25N01GV_WRITE_CHECK_EMPTY
		if (status == W25N01GV_OK) {
			status = w25n01gv_page_read(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		//printf("2 Writing, page address: 0x%04X, column address: 0x%04X, size: %u\r\n", page_addr, column_addr, write_size); // Todo: remove after debugging
		if (status == W25N01GV_OK) {
			status = w25n01gv_read_data(hw25n01gv, buffer, column_addr, write_size);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		//printf("3 Writing, page address: 0x%04X, column address: 0x%04X, size: %u\r\n", page_addr, column_addr, write_size); // Todo: remove after debugging
		for (uint16_t i = 0; i < write_size; i++) {
			if (buffer[i] != 0xFF) {
				printf("Writing aborted, tried writing to non-empty page: 0x%04X\r\n", page_addr); // Todo: remove after debugging
				if (status == W25N01GV_OK) {
					status = w25n01gv_set_prot(hw25n01gv, reg_prot.fields.block_prot, reg_prot.fields.top_bot_prot);
				}
				if (status == W25N01GV_OK) {
					status = w25n01gv_wait_busy(hw25n01gv);
				}
				return W25N01GV_WRITE_NON_EMPTY;
			}
		}
		//printf("4 Writing, page address: 0x%04X, column address: 0x%04X, size: %u\r\n", page_addr, column_addr, write_size); // Todo: remove after debugging
#endif //W25N01GV_WRITE_CHECK_EMPTY
		if (status == W25N01GV_OK) {
			status = w25n01gv_write_enable(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_prog_load(hw25n01gv, data, column_addr, write_size);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (hw25n01gv->reg_status.fields.program_failure != 0) {
			printf("Writing aborted, program failure (prog load), page address: 0x%04X\r\n", page_addr); // Todo: remove after debugging
			if (status == W25N01GV_OK) {
				status = w25n01gv_set_prot(hw25n01gv, reg_prot.fields.block_prot, reg_prot.fields.top_bot_prot);
			}
			if (status == W25N01GV_OK) {
				status = w25n01gv_wait_busy(hw25n01gv);
			}
			return W25N01GV_PROG_FAILURE;
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_write_enable(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_prog_exec(hw25n01gv, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(hw25n01gv);
		}
		if (hw25n01gv->reg_status.fields.program_failure != 0) {
			printf("Writing aborted, program failure (prog execute), page address: 0x%04X\r\n", page_addr); // Todo: remove after debugging
			if (status == W25N01GV_OK) {
				status = w25n01gv_set_prot(hw25n01gv, reg_prot.fields.block_prot, reg_prot.fields.top_bot_prot);
			}
			if (status == W25N01GV_OK) {
				status = w25n01gv_wait_busy(hw25n01gv);
			}
			return W25N01GV_PROG_FAILURE;
		}
		address += write_size;
		data += write_size;
		size -= write_size;
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_set_prot(hw25n01gv, reg_prot.fields.block_prot, reg_prot.fields.top_bot_prot);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_wait_busy(hw25n01gv);
	}
	return status;
}

#if W25N01GV_WRITE_CHECK_EMPTY
/*
 * Verify data previously written to flash memory by reading it back and
 * comparing it against the expected data.
 * @param hw25n01gv W25N01GV handle to the device
 * @param data pointer to the expected data
 * @param address address
 * @param size size of data to be verified
 */
w25n01gv_status_t w25n01gv_verify(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size) {
	w25n01gv_status_t status;
	uint32_t chunk;
	while (size > 0) {
		chunk = (size > W25N01GV_PAGE_SIZE) ? W25N01GV_PAGE_SIZE : size;
		status = w25n01gv_read(hw25n01gv, buffer, address, chunk);
		if (status != W25N01GV_OK) {
			return status; // propagates W25N01GV_ECC_UNCORRECTABLE etc. as-is
		}
		if (memcmp(buffer, data, chunk) != 0) {
			return W25N01GV_VERIFY_FAILED;
		}
		address += chunk;
		data += chunk;
		size -= chunk;
	}
	return W25N01GV_OK;
}
#endif //W25N01GV_WRITE_CHECK_EMPTY

/*
 * Reset flash memory
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_reset(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_RESET;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_NONE;
	cmd.NbData              = 0;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = w25n01gv_wait_busy(hw25n01gv);
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	/*if (status == W25N01GV_OK) {
		uint8_t pData;
		status = HAL_QSPI_Transmit(hw25n01gv->hqspi, &pData, hw25n01gv->timeout);
	}*/
	return status;
}

/*
 * Read JEDEC ID
 * @param hw25n01gv W25N01GV handle to the device.
 * @param id pointer to array to write JEDEC ID into
 */
w25n01gv_status_t w25n01gv_read_id(w25n01gv_handle *hw25n01gv, uint8_t *id) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_ID;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 3;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, id, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Read all status registers
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_read_reg(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	status = w25n01gv_read_reg_prot(hw25n01gv);
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_conf(hw25n01gv);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_read_reg_status(hw25n01gv);
	}
	return status;
}

/*
 * Read protection register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_read_reg_prot(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_STATUS_REG;
	cmd.Address             = W25N01GV_REG_PROT_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, &hw25n01gv->reg_prot.reg, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Read configuration register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_read_reg_conf(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_STATUS_REG;
	cmd.Address             = W25N01GV_REG_CONF_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, &hw25n01gv->reg_conf.reg, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Read status register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_read_reg_status(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_STATUS_REG;
	cmd.Address             = W25N01GV_REG_STATUS_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, &hw25n01gv->reg_status.reg, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Write all status registers
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_write_reg(w25n01gv_handle *hw25n01gv, w25n01gv_reg_t *regs) {
	w25n01gv_status_t status;
	status = w25n01gv_write_reg_prot(hw25n01gv, &regs->reg_prot);
	if (status == W25N01GV_OK) {
		status = w25n01gv_write_reg_conf(hw25n01gv, &regs->reg_conf);
	}
	if (status == W25N01GV_OK) {
		status = w25n01gv_write_reg_status(hw25n01gv, &regs->reg_status);
	}
	return status;
}

/*
 * Write protection register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_write_reg_prot(w25n01gv_handle *hw25n01gv, w25n01gv_prot_reg_t *reg) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_WRITE_STATUS_REG;
	cmd.Address             = W25N01GV_REG_PROT_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, &reg->reg, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		hw25n01gv->reg_prot.reg = reg->reg;
	}
	return status;
}

/*
 * Write configuration register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_write_reg_conf(w25n01gv_handle *hw25n01gv, w25n01gv_conf_reg_t *reg) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_WRITE_STATUS_REG;
	cmd.Address             = W25N01GV_REG_CONF_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, &reg->reg, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		hw25n01gv->reg_conf.reg = reg->reg;
	}
	return status;
}

/*
 * Write status register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_write_reg_status(w25n01gv_handle *hw25n01gv, w25n01gv_status_reg_t *reg) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_WRITE_STATUS_REG;
	cmd.Address             = W25N01GV_REG_STATUS_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, &reg->reg, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		hw25n01gv->reg_status.reg = reg->reg;
	}
	return status;
}

/*
 * Write protection register
 * @param hw25n01gv W25N01GV handle to the device.
 * @param reg pointer to register variable
 */
w25n01gv_status_t w25n01gv_set_prot(w25n01gv_handle *hw25n01gv, uint8_t block_prot, uint8_t upper_lower) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_STATUS_REG;
	cmd.Address             = W25N01GV_REG_PROT_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, &hw25n01gv->reg_prot.reg, hw25n01gv->timeout);
		hw25n01gv->reg_prot.fields.block_prot = block_prot & 0xF;
		hw25n01gv->reg_prot.fields.top_bot_prot = upper_lower & 0x1;
	}
	cmd.Instruction         = W25N01GV_CMD_WRITE_STATUS_REG;
	cmd.Address             = W25N01GV_REG_PROT_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, &hw25n01gv->reg_prot.reg, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Wait until the busy flag in the status register has cleared
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_wait_busy(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_STATUS_REG;
	cmd.Address             = W25N01GV_REG_STATUS_ADDR;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 1;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	do {
		status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
		if (status == W25N01GV_OK) {
			status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, &hw25n01gv->reg_status.reg, hw25n01gv->timeout);
		}
	} while ((status == W25N01GV_OK) && (hw25n01gv->reg_status.fields.busy != 0));
	return status;
}

/*
 * Enable buffer mode
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_buffer_mode_enable(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	status = w25n01gv_read_reg_conf(hw25n01gv);
	if (status == W25N01GV_OK) {
		hw25n01gv->reg_conf.fields.buf_mode = 1;
		status = w25n01gv_write_reg_conf(hw25n01gv, &hw25n01gv->reg_conf);
	}
	return status;
}

/*
 * Disable buffer mode
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_buffer_mode_disable(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	status = w25n01gv_read_reg_conf(hw25n01gv);
	if (status == W25N01GV_OK) {
		hw25n01gv->reg_conf.fields.buf_mode = 0;
		status = w25n01gv_write_reg_conf(hw25n01gv, &hw25n01gv->reg_conf);
	}
	return status;
}

/*
 * Enable write
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_write_enable(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_WRITE_ENABLE;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_NONE;
	cmd.NbData              = 0;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	return status;
}

/*
 * Disable write
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_write_disable(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_WRITE_DISABLE;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_NONE;
	cmd.NbData              = 0;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	return status;
}

/*
 * Bad block management
 * @param hw25n01gv W25N01GV handle to the device.
 * @param badblock  Pointer to bad block to be written to bad block look up table
 * Todo: Untested, must be tested before usage
 */
w25n01gv_status_t w25n01gv_badblock_management(w25n01gv_handle *hw25n01gv, w25n01gv_badblock_t badblock) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_BB_MANAGEMENT;
	cmd.Address             = badblock.word;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_32_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 0;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_NONE;
	cmd.NbData              = 0;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		uint8_t pData;
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, &pData, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Read bad block look up table
 * @param hw25n01gv W25N01GV handle to the device.
 */
w25n01gv_status_t w25n01gv_read_badblock(w25n01gv_handle *hw25n01gv) {
	w25n01gv_status_t status;
	uint8_t pData[4*W25N01GV_BADBLOCK_LUT_SIZE];
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_READ_BBM_LUT;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 4*W25N01GV_BADBLOCK_LUT_SIZE;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, pData, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		for (uint8_t i = 0; i < W25N01GV_BADBLOCK_LUT_SIZE; i++) {
			for (uint8_t j = 0; j < 4; j++) {
				hw25n01gv->badblock_lut->bytes[3-j] = pData[i*4 + j];
			}
		}
	}
	return status;
}

/*
 * Read page address of the last ECC failure
 * @param hw25n01gv W25N01GV handle to the device.
 * @param page_addr pointer for page address of last ECC failure
 */
w25n01gv_status_t w25n01gv_last_ecc_failure(w25n01gv_handle *hw25n01gv, uint16_t *page_addr) {
	w25n01gv_status_t status;
	uint8_t pData[2];
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_LAST_ECC_FAIL_PAGE_ADDR;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 2;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, pData, hw25n01gv->timeout);
	}
	if (status == W25N01GV_OK) {
		*page_addr = (((uint16_t) pData[0]) << 8) | pData[1];
	}
	return status;
}

/*
 * Block erase
 * @param hw25n01gv W25N01GV handle to the device.
 * @param page_addr Page address
 */
w25n01gv_status_t w25n01gv_block_erase(w25n01gv_handle *hw25n01gv, uint16_t page_addr) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_BLOCK_ERASE;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 2;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		uint8_t pData[2];
		pData[0] = (uint8_t) (page_addr>>8);
		pData[1] = (uint8_t) (page_addr & 0x00FF);
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, pData, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Load data into data buffer for preparation to program execution
 * @param hw25n01gv W25N01GV handle to the device
 * @param col_addr column address
 * @param data pointer to data
 * @param col_addr column address
 * @param size size of data to be loaded
 */
w25n01gv_status_t w25n01gv_prog_load(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
#if (W25N01GV_WRITE_DATA_LINES == 1)
		cmd.Instruction         = W25N01GV_CMD_PROG_DATA_LOAD;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_WRITE_DATA_LINES == 4)
		cmd.Instruction         = W25N01GV_CMD_QUAD_PROG_DATA_LOAD;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#else
	#error This amount of data lines is not supported! Check the definition of W25N01GV_WRITE_DATA_LINES.
#endif // W25N01GV_WRITE_DATA_LINES
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, data, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Load data into data buffer for preparation to program execution
 * @param hw25n01gv W25N01GV handle to the device
 * @param col_addr column address
 * @param data pointer to data
 * @param col_addr column address
 * @param size size of data to be loaded
 */
w25n01gv_status_t w25n01gv_prog_load_random(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
#if (W25N01GV_WRITE_DATA_LINES == 1)
		cmd.Instruction         = W25N01GV_CMD_RAND_PROG_DATA_LOAD;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_WRITE_DATA_LINES == 4)
		cmd.Instruction         = W25N01GV_CMD_RAND_QUAD_PROG_DATA_LOAD;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#else
	#error This amount data lines is not supported! Check the definition of W25N01GV_WRITE_DATA_LINES.
#endif // W25N01GV_WRITE_DATA_LINES
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, data, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Program execute
 * @param hw25n01gv W25N01GV handle to the device.
 * @param page_addr Page address
 */
w25n01gv_status_t w25n01gv_prog_exec(w25n01gv_handle *hw25n01gv, uint16_t page_addr) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_PROG_EXEC;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 2;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		uint8_t pData[2];
		pData[0] = (uint8_t) (page_addr>>8);
		pData[1] = (uint8_t) (page_addr & 0x00FF);
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, pData, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Page data read from memory page into data buffer
 * @param hw25n01gv W25N01GV handle to the device
 * @param addr page address
 */
w25n01gv_status_t w25n01gv_page_read(w25n01gv_handle *hw25n01gv, uint16_t page_addr) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	cmd.Instruction         = W25N01GV_CMD_PAGE_DATA_READ;
	cmd.Address             = 0x00;
	cmd.AlternateBytes      = 0x00;
	cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
	cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
	cmd.DummyCycles         = 8;
	cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
	cmd.AddressMode         = QSPI_ADDRESS_NONE;
	cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
	cmd.DataMode            = QSPI_DATA_1_LINE;
	cmd.NbData              = 2;
	cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
	cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
	cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		uint8_t pData[2];
		pData[0] = (uint8_t) (page_addr>>8);
		pData[1] = (uint8_t) (page_addr & 0x00FF);
		status = (w25n01gv_status_t) HAL_QSPI_Transmit(hw25n01gv->hqspi, pData, hw25n01gv->timeout);
	}
	return status;
}

/*
 * Read data from data buffer
 * @param hw25n01gv W25N01GV handle to the device
 * @param data pointer to data
 * @param col_addr column address
 * @param size size of data to be read
 */
w25n01gv_status_t w25n01gv_read_data(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size) {
	w25n01gv_status_t status;
	QSPI_CommandTypeDef cmd = {0};
	if ((hw25n01gv->reg_conf.fields.buf_mode == 1) || (hw25n01gv->reg_conf.fields.otp_enter == 1)) {
#if (W25N01GV_FAST_READ == 0)
		cmd.Instruction         = W25N01GV_CMD_READ;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 1) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 1) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_4_BYTE_ADDR;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 24;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_OUT;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_OUT_4_BYTE_ADDR;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 24;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_OUT;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_OUT_4_BYTE_ADDR;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 24;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_1_LINE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 2) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_IO;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 4;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_2_LINES;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 2) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_IO_4_BYTE_ADDR;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 12;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_2_LINES;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 4) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_IO;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 4;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_4_LINES;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 4) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_IO_4_BYTE_ADDR;
		cmd.Address             = col_addr;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 10;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_4_LINES;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#else
	#error This combination of address and data lines is not supported! Check the definition of W25N01GV_READ_ADDR_LINES, W25N01GV_READ_DATA_LINES, W25N01GV_READ_4_BYTE_ADDR.
#endif // W25N01GV_READ_ADDR_LINES, W25N01GV_READ_DATA_LINES, W25N01GV_READ_4_BYTE_ADDR
	}
	else { // ((hw25n01gv->reg_conf.fields.buf_mode == 0) && (hw25n01gv->reg_conf.fields.otp_enter == 0))
#if (W25N01GV_FAST_READ == 0)
		cmd.Instruction         = W25N01GV_CMD_READ;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 24;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 1) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_8_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 1) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_4_BYTE_ADDR;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_1_LINE;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_OUT;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_OUT_4_BYTE_ADDR;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_OUT;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 0;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 1) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_OUT_4_BYTE_ADDR;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_32_BITS;
		cmd.DummyCycles         = 8;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_1_LINE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 2) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_IO;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 16;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 2) && (W25N01GV_READ_DATA_LINES == 2) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_DUAL_IO_4_BYTE_ADDR;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 20;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_2_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 4) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 0)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_IO;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 12;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#elif (W25N01GV_READ_ADDR_LINES == 4) && (W25N01GV_READ_DATA_LINES == 4) && (W25N01GV_READ_4_BYTE_ADDR == 1)
		cmd.Instruction         = W25N01GV_CMD_FAST_READ_QUAD_IO_4_BYTE_ADDR;
		cmd.Address             = 0x00;
		cmd.AlternateBytes      = 0x00;
		cmd.AddressSize         = QSPI_ADDRESS_16_BITS;
		cmd.AlternateBytesSize  = QSPI_ALTERNATE_BYTES_8_BITS;
		cmd.DummyCycles         = 14;
		cmd.InstructionMode     = QSPI_INSTRUCTION_1_LINE;
		cmd.AddressMode         = QSPI_ADDRESS_NONE;
		cmd.AlternateByteMode   = QSPI_ALTERNATE_BYTES_NONE;
		cmd.DataMode            = QSPI_DATA_4_LINES;
		cmd.NbData              = size;
		cmd.DdrMode             = QSPI_DDR_MODE_DISABLE;
		cmd.DdrHoldHalfCycle    = QSPI_DDR_HHC_ANALOG_DELAY;
		cmd.SIOOMode            = QSPI_SIOO_INST_EVERY_CMD;
#else
	#error This combination of address and data lines is not supported! Check the definition of W25N01GV_READ_ADDR_LINES, W25N01GV_READ_DATA_LINES, W25N01GV_READ_4_BYTE_ADDR.
#endif // W25N01GV_READ_ADDR_LINES, W25N01GV_READ_DATA_LINES, W25N01GV_READ_4_BYTE_ADDR
	}
	status = (w25n01gv_status_t) HAL_QSPI_Command(hw25n01gv->hqspi, &cmd, hw25n01gv->timeout);
	if (status == W25N01GV_OK) {
		status = (w25n01gv_status_t) HAL_QSPI_Receive(hw25n01gv->hqspi, data, hw25n01gv->timeout);
	}
	return status;
}

