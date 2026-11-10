/*******************************************************************************/
/*
 * File: cli_flash_test.c
 * Created on: Nov 9, 2026
 * Author: ahorat
 */
/*******************************************************************************/
/*
 * Flash bench-test CLI commands (writeFlash, testFlash), moved out of cli.c.
 */
/*******************************************************************************/

#include "cli_flash_test.h"
#include "cli.h"

#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include "w25n01gv.h"

#ifdef CLI_ENABLED

extern w25n01gv_handle flash;

// Shared 512-byte scratch buffer for the flash CLI commands, replacing the
// stack buffers (up to 16 kB) they used to allocate against a 1 kB stack.
// A divisor of the 2048-byte W25N01GV_PAGE_SIZE; used only synchronously.
uint8_t cli_flash_buf[CLI_FLASH_BUF_SIZE];

void cmd_writeFlash(void){
	uint32_t address;
	uint32_t size;
	uint8_t *write_ptr;
	w25n01gv_status_t status = W25N01GV_OK;
#if CLI_FLASH_WRITE_LIPSUM != 1
	CLI_CHECK_ARG_CNT(3);
#else // CLI_FLASH_WRITE_LIPSUM == 1
	uint16_t page_addr;
	const char *lipsum_str;
	CLI_CHECK_ARG_CNT_MIN(2);
#endif // CLI_FLASH_WRITE_LIPSUM

	char *end;
	address = strtoul(arg_locs[1], &end, 10);
	size = strtoul(arg_locs[2], &end, 10);

#if CLI_FLASH_WRITE_LIPSUM != 1
	if (size > LINE_BUF_SIZE) {
		size = LINE_BUF_SIZE;
	}
	for (uint16_t i = 0; i < size; i++) {
		cli_flash_buf[i] = arg_locs[3][i];
	}
	write_ptr = cli_flash_buf;
#else // CLI_FLASH_WRITE_LIPSUM == 1
	page_addr = W25N01GV_ADDR_TO_PAGE_ADDR(address);
	if (page_addr == 0){
		static const char str[] = "Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Ut purus elit, vestibulum ut, placerat ac, adipiscing vitae, felis. Curabitur dictum gravida mauris. Nam arcu libero, nonummy eget, consectetuer id, vulputate a, magna. Donec vehicula augue eu neque. Pellentesque habitant morbi tristique senectus et netus et malesuada fames ac turpis egestas. Mauris ut leo. Cras viverra metus rhoncus sem. Nulla et lectus vestibulum urna fringilla ultrices. Phasellus eu tellus sit amet tortor gravida placerat. Integer sapien est, iaculis in, pretium quis, viverra ac, nunc. Praesent eget sem vel leo ultrices bibendum. Aenean faucibus. Morbi dolor nulla, malesuada eu, pulvinar at, mollis ac, nulla. Curabitur auctor semper nulla. Donec varius orci eget risus. Duis nibh mi, congue eu, accumsan eleifend, sagittis quis, diam. Duis eget orci sit amet orci dignissim rutrum. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 1) {
		static const char str[] = "Nam dui ligula, fringilla a, euismod sodales, sollicitudin vel, wisi. Morbi auctor lorem non justo. Nam lacus libero, pretium at, lobortis vitae, ultricies et, tellus. Donec aliquet, tortor sed accumsan bibendum, erat ligula aliquet magna, vitae ornare odio metus a mi. Morbi ac orci et nisl hendrerit mollis. Suspendisse ut massa. Cras nec ante. Pellentesque a nulla. Cum sociis natoque penatibus et magnis dis parturient montes, nascetur ridiculus mus. Aliquam tincidunt urna. Nulla ullamcorper vestibulum turpis. Pellentesque cursus luctus mauris. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 2) {
		static const char str[] = "Nulla malesuada porttitor diam. Donec felis erat, congue non, volutpat at, tincidunt tristique, libero. Vivamus viverra fermentum felis. Donec nonummy pellentesque ante. Phasellus adipiscing semper elit. Proin fermentum massa ac quam. Sed diam turpis, molestie vitae, placerat a, molestie nec, leo. Maecenas lacinia. Nam ipsum ligula, eleifend at, accumsan nec, suscipit a, ipsum. Morbi blandit ligula feugiat magna. Nunc eleifend consequat lorem. Sed lacinia nulla vitae enim. Pellentesque tincidunt purus vel magna. Integer non enim. Praesent euismod nunc eu purus. Donec bibendum quam in tellus. Nullam cursus pulvinar lectus. Donec et mi. Nam vulputate metus eu enim. Vestibulum pellentesque felis eu massa. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 3) {
		static const char str[] = "Quisque ullamcorper placerat ipsum. Cras nibh. Morbi vel justo vitae lacus tincidunt ultrices. Lorem ipsum dolor sit amet, consectetuer adipiscing elit. In hac habitasse platea dictumst. Integer tempus convallis augue. Etiam facilisis. Nunc elementum fermentum wisi. Aenean placerat. Ut imperdiet, enim sed gravida sollicitudin, felis odio placerat quam, ac pulvinar elit purus eget enim. Nunc vitae tortor. Proin tempus nibh sit amet nisl. Vivamus quis tortor vitae risus porta vehicula. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 4) {
		static const char str[] = "Fusce mauris. Vestibulum luctus nibh at lectus. Sed bibendum, nulla a faucibus semper, leo velit ultricies tellus, ac venenatis arcu wisi vel nisl. Vestibulum diam. Aliquam pellentesque, augue quis sagittis posuere, turpis lacus congue quam, in hendrerit risus eros eget felis. Maecenas eget erat in sapien mattis porttitor. Vestibulum porttitor. Nulla facilisi. Sed a turpis eu lacus commodo facilisis. Morbi fringilla, wisi in dignissim interdum, justo lectus sagittis dui, et vehicula libero dui cursus dui. Mauris tempor ligula sed lacus. Duis cursus enim ut augue. Cras ac magna. Cras nulla. Nulla egestas. Curabitur a leo. Quisque egestas wisi eget nunc. Nam feugiat lacus vel est. Curabitur consectetuer. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 5) {
		static const char str[] = "Suspendisse vel felis. Ut lorem lorem, interdum eu, tincidunt sit amet, laoreet vitae, arcu. Aenean faucibus pede eu ante. Praesent enim elit, rutrum at, molestie non, nonummy vel, nisl. Ut lectus eros, malesuada sit amet, fermentum eu, sodales cursus, magna. Donec eu purus. Quisque vehicula, urna sed ultricies auctor, pede lorem egestas dui, et convallis elit erat sed nulla. Donec luctus. Curabitur et nunc. Aliquam dolor odio, commodo pretium, ultricies non, pharetra in, velit. Integer arcu est, nonummy in, fermentum faucibus, egestas vel, odio. ";
		lipsum_str = str;
		size = sizeof(str);
	}
	else if (page_addr == 63) {
		static const char str[] = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789_123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789#";
		lipsum_str = str;
		if (size > sizeof(str)) {
			size = sizeof(str);
		}
	}
	else {
		static const char str[] = "None";
		lipsum_str = str;
		size = sizeof(str);
	}
	write_ptr = (uint8_t *) lipsum_str;
#endif // CLI_FLASH_WRITE_LIPSUM

	status = w25n01gv_write(&flash, write_ptr, address, size);
	if (status != W25N01GV_OK) {
		printf("Unable to write flash memory. \r\n");
	}
	return;
}

void cmd_testFlash(void){
	uint8_t id[4];
	w25n01gv_status_t status = W25N01GV_OK;
	w25n01gv_prot_reg_t reg_prot = {0};
	w25n01gv_conf_reg_t reg_conf = {0};
	w25n01gv_status_reg_t reg_status = {0};
	status = w25n01gv_read_id(&flash, id);
	if (status == W25N01GV_OK){
		printf("Flash Manufacturer ID:        0x%02X\r\n", id[0]);
		printf("Flash Device ID:              0x%02X%02X\r\n", id[1], id[2]);
	}
	else{
		printf("Error: Unable to read flash JEDEC ID. \r\n");
		return;
	}
#if CLI_FLASH_TEST_STATUS_REG_READ == 1
	// Status register read test
	printf("\r\n= Flash status register read test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_prot(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash protection register:    0x%02X\r\n", flash.reg_prot.reg);
	}
	else{
		printf("Error: Unable to read flash protection register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
	else{
		printf("Error: Unable to read flash configuration register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash status register:        0x%02X\r\n", flash.reg_status.reg);
	}
	else{
		printf("Error: Unable to read flash status register. \r\n");
		return;
	}
#endif // CLI_FLASH_TEST_STATUS_REG_READ
#if CLI_FLASH_TEST_STATUS_REG_WRITE == 1
	// Status register write test
	printf("\r\n= Flash status register write test: \r\n");
	reg_conf.fields.buf_mode=0;
	if (status == W25N01GV_OK){
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X (after writing)\r\n", flash.reg_conf.reg);
		reg_conf.fields.buf_mode=1;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	else{
		printf("Error: Unable to read flash configuration register. \r\n");
		return;
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X (after resetting)\r\n", flash.reg_conf.reg);
	}
#endif // CLI_FLASH_TEST_STATUS_REG_WRITE
#if CLI_FLASH_TEST_BUF_EN_DIS == 1
	// Buffer mode enable / disable test
	printf("\r\n= Buffer mode enable / disable test: \r\n");
	printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	if (status == W25N01GV_OK){
		status = w25n01gv_buffer_mode_disable(&flash);
		printf("Disable buffer mode\r\n");
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_buffer_mode_enable(&flash);
		printf("Enable buffer mode\r\n");
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_conf(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Flash configuration register: 0x%02X\r\n", flash.reg_conf.reg);
	}
#endif // CLI_FLASH_TEST_BUF_EN_DIS
#if CLI_FLASH_TEST_WRITE_EN_DIS == 1
	// Write enable and disable test
	printf("\r\n= Flash write enable / disable test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register before write enable: 0x%02X\r\n", flash.reg_status.reg);
		status = w25n01gv_write_enable(&flash);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register after write enable:  0x%02X\r\n", flash.reg_status.reg);
		status = w25n01gv_write_disable(&flash);
	}
	if (status == W25N01GV_OK){
		status = w25n01gv_read_reg_status(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Status register after write disable: 0x%02X\r\n", flash.reg_status.reg);
	}
#endif // CLI_FLASH_TEST_WRITE_EN_DIS
#if CLI_FLASH_TEST_READ_BLOCK == 1
	// Read block test
	printf("\r\n= Read block test: \r\n");
	uint16_t test_read_block_nof_bytes = 16;
	for (uint16_t page_addr = 0; page_addr < 16; page_addr++) {
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(&flash);
		}
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, cli_flash_buf, 0x00, test_read_block_nof_bytes);
		}
		if (status == W25N01GV_OK){
			printf("Page: %4d Data:", page_addr);
			for (uint16_t i = 0; i < test_read_block_nof_bytes; i++) {
				if ((i % 16 == 0) && (i > 0)) {
					printf("\r\n                ");
				}
				printf(" 0x%02X", cli_flash_buf[i]);
			}
			if (cli_flash_buf[0] != 0xFF) {
				printf(" <= Bad Block present ==");
			}
			printf("\r\n");
		}
	}
#endif // CLI_FLASH_TEST_READ_BLOCK
#if CLI_FLASH_TEST_READ_OTP_BLOCK == 1
	// Read OTP block test
	printf("\r\n= Read OTP block test: \r\n");
	uint16_t test_read_block_otp_nof_bytes = 256;
	if (status == W25N01GV_OK){
		reg_conf.fields.otp_enter = 1;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
	for (uint16_t page_addr = 0; page_addr < 3; page_addr++) {
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, page_addr);
		}
		if (status == W25N01GV_OK) {
			status = w25n01gv_wait_busy(&flash);
		}
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, cli_flash_buf, 0x00, test_read_block_otp_nof_bytes);
		}
		if (status == W25N01GV_OK){
			printf("Page: %4d Data:", page_addr);
			for (uint16_t i = 0; i < test_read_block_otp_nof_bytes; i++) {
				if ((i % 16 == 0) && (i > 0)) {
					printf("\r\n                ");
				}
				printf(" 0x%02X", cli_flash_buf[i]);
			}
			printf("\r\n");
		}
	}
	if (status == W25N01GV_OK){
		reg_conf.fields.otp_enter = 0;
		status = w25n01gv_write_reg_conf(&flash, &reg_conf);
	}
#endif // CLI_FLASH_TEST_READ_OTP_BLOCK
#if CLI_FLASH_TEST_BAD_BLOCK_LUT == 1
	// Read bad block look up table test
	printf("\r\n= Read bad block look up table test: \r\n");
	if (status == W25N01GV_OK){
		status = w25n01gv_read_badblock(&flash);
	}
	if (status == W25N01GV_OK){
		printf("Bad block LUT: \r\n");
		printf("ID | Logical | Physical\r\n");
		printf("---+---------+---------\r\n");
		for (uint8_t i = 0; i < W25N01GV_BADBLOCK_LUT_SIZE; i++) {
			printf("%2d | 0x%04X  | 0x%04X\r\n", i, flash.badblock_lut[i].addr.logic_block_addr, flash.badblock_lut[i].addr.phys_block_addr);
		}
	}
#endif // CLI_FLASH_TEST_BAD_BLOCK_LUT
#if CLI_FLASH_TEST_BAD_BLOCK == 1
	// Bad block test
	printf("\r\n= Bad block test: \r\n");
	uint16_t bad_block_present = 0;
	uint16_t bb_loop_count = 0;
	uint8_t data_page;
	uint8_t data_spare;
	test_read_block_nof_bytes = 1;
	for (uint32_t page_addr = 0; page_addr < 0xffff; page_addr+=0x40) {
		bb_loop_count++;
		if (status == W25N01GV_OK){
			status = w25n01gv_page_read(&flash, (uint16_t) page_addr);
		}
		do {
			status = w25n01gv_read_reg_status(&flash);
		} while(status == W25N01GV_OK && flash.reg_status.fields.busy != 0);
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, &data_page, 0x00, 1);
		}
		if (status == W25N01GV_OK){
			status = w25n01gv_read_data(&flash, &data_spare, 0x800, 1);
		}
		if (status == W25N01GV_OK){
			if ((data_page != 0xFF) || (data_spare != 0xFF)) {
				bad_block_present = 1;
				printf("Bad block on page %4d, Page: 0x%02X  Spare: 0x%02X\r\n", (uint16_t) (page_addr / 0x40), data_page, data_spare);
			}
		}
	}
	printf("%d blocks scanned\r\n", bb_loop_count);
	if (bad_block_present == 0) {
		printf("No bad blocks\r\n");
	}
#endif // CLI_FLASH_TEST_BAD_BLOCK
#if CLI_FLASH_TEST_LAST_ECC_FAIL == 1
	// Last ECC failure page address test
	printf("\r\n= Last ECC failure page address test: \r\n");
	uint16_t ecc_fail_page_addr = 0xffff;
	if (status == W25N01GV_OK){
		status = w25n01gv_last_ecc_failure(&flash, &ecc_fail_page_addr);
	}
	if (status == W25N01GV_OK){
		printf("Page address of last ECC failure: %X\r\n", ecc_fail_page_addr);
	}
#endif // CLI_FLASH_TEST_LAST_ECC_FAIL
}

#endif // CLI_ENABLED
