/*
 * debug.h
 *
 * All debug / bench feature flags in one place. Uncomment to enable, or pass
 * -D<FLAG> to the compiler. No HAL includes: also used by host tests.
 *
 *  Created on: Oct 22, 2026
 *      Author: ahorat
 */

#ifndef INC_DEBUG_H_
#define INC_DEBUG_H_

// CHARGE bench output (periodic/peak printouts) and chargeStep/chargeNext.
//#define CHARGE_DEBUG

// EMI test mode (both converters switching, live terminals). Icon store:
// flash_icon_seed.py --emi for an EMI_DEBUG build.
#define EMI_DEBUG

// CLI commands that bypass all interlocks: setGPIO, setDAC, setDuty, enPWM,
// wdtTest. Without it they only print a notice.
//#define CLI_DEBUG

// Serial command line interface on USART2.
#define CLI_ENABLED

// Free-running parked leg, no pulse skipping in 60V/10A (bench comparison).
//#define VOUT_SYNC_BURST_DISABLE

// CLI flash test options (testFlash/readFlash/eraseFlash/writeFlash).
#define CLI_FLASH_TEST_STATUS_REG_READ	1 // Status register read test
#define CLI_FLASH_TEST_STATUS_REG_WRITE	1 // Status register write test
#define CLI_FLASH_TEST_BUF_EN_DIS		1 // Buffer mode enable / disable test
#define CLI_FLASH_TEST_WRITE_EN_DIS		1 // Write enable and disable test
#define CLI_FLASH_TEST_READ_BLOCK		1 // Read block test
#define CLI_FLASH_TEST_READ_OTP_BLOCK	1 // Read OTP block test
#define CLI_FLASH_TEST_BAD_BLOCK_LUT	1 // Read bad block look up table test
#define CLI_FLASH_TEST_BAD_BLOCK		1 // Bad block test
#define CLI_FLASH_TEST_LAST_ECC_FAIL	1 // Last ECC failure page address test

#define CLI_FLASH_READ_OTP				0 // Read OTP section
#define CLI_FLASH_READ_BUFFER_MODE_DIS	1 // Disable buffer mode for reading flash memory
#define CLI_FLASH_READ_SHOW_ASCII		1 // Show data read from memory as ascii string
#define CLI_FLASH_READ_HEX_LINEWIDTH	16 // Number of characters printed on one line in ascii mode
#define CLI_FLASH_READ_ASCII_LINEWIDTH	128 // Number of characters printed on one line in ascii mode

#define CLI_FLASH_ERASE_DISABLE			0 // Disable flash erase operation

#define CLI_FLASH_WRITE_LIPSUM			0 // Write Lorem Ipsum sample data to
#define CLI_FLASH_WRITE_EXEC			1 // enables program execute after loading buffer to write data to flash memory

#endif /* INC_DEBUG_H_ */
