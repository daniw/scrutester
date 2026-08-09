/*
 * w25n01gv.h
 *
 *  Created on: Apr 20, 2026
 *      Author: daniw
 */

#ifndef INC_W25N01GV_H_
#define INC_W25N01GV_H_

#ifndef W25N01GV_VARIANT_G
#ifndef W25N01GV_VARIANT_T
#ifndef W25N01GV_VARIANT_R
#define W25N01GV_VARIANT_G
#endif // ndef W25N01GV_VARIANT_R
#endif // ndef W25N01GV_VARIANT_T
#endif // ndef W25N01GV_VARIANT_G

#include "quadspi.h"

#define W25N01GV_FAST_READ								1
#define W25N01GV_READ_ADDR_LINES						4
#define W25N01GV_READ_DATA_LINES						4
#define W25N01GV_READ_4_BYTE_ADDR						0 // Not recommended, due to higher dummy clock count

#define W25N01GV_WRITE_DATA_LINES						1

#define W25N01GV_WRITE_CHECK_EMPTY						1

//#define W25N01GV_TEST_UNION_STRUCT

#define W25N01GV_PAGE_SIZE								(2048U)
#define W25N01GV_PAGES_PER_BLOCK						(64U) // Number of pages in a block
#define W25N01GV_NOF_BLOCKS								(1024U)
#define W25N01GV_NOF_PAGES								(W25N01GV_PAGES_PER_BLOCK * W25N01GV_NOF_BLOCKS)
#define W25N01GV_PAGE_MASK								(W25N01GV_PAGE_SIZE-1)
#define W25N01GV_SIZE									(W25N01GV_PAGE_SIZE * W25N01GV_NOF_PAGES)
#define W25N01GV_ADDR_TO_COLUMN_ADDR(addr)				((uint16_t) (addr & W25N01GV_PAGE_MASK))
#define W25N01GV_ADDR_TO_PAGE_ADDR(addr)				((uint16_t) ((addr >> 11) & 0xFFFF)) // Todo: Compute mask and shift width from paramaters

#define W25N01GV_MANUF_ID								(0xEF)
#define W25N01GV_DEVICE_ID								(0xAA21)

#ifdef W25N01GV_VARIANT_T
	#define W25N01GV_CMD_RESET							(0xFF)
	#define W25N01GV_CMD_READ_ID						(0x9F)
	#define W25N01GV_CMD_READ_STATUS_REG				(0x0F)
	//#define W25N01GV_CMD_READ_STATUS_REG				(0x05) // alternative command
	#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F)
	//#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F) // alternative command
	#define W25N01GV_CMD_WRITE_ENABLE					(0x06)
	#define W25N01GV_CMD_WRITE_DISABLE					(0x04)
	#define W25N01GV_CMD_BB_MANAGEMENT					(0xA1)
	#define W25N01GV_CMD_READ_BBM_LUT					(0xA5)
	#define W25N01GV_CMD_LAST_ECC_FAIL_PAGE_ADDR		(0xA9)
	#define W25N01GV_CMD_BLOCK_ERASE					(0xD8)
	#define W25N01GV_CMD_PROG_DATA_LOAD					(0x02)
	#define W25N01GV_CMD_RAND_PROG_DATA_LOAD			(0x84)
	#define W25N01GV_CMD_QUAD_PROG_DATA_LOAD			(0x32)
	#define W25N01GV_CMD_RAND_QUAD_PROG_DATA_LOAD		(0x34)
	#define W25N01GV_CMD_PROG_EXEC						(0x10)
	#define W25N01GV_CMD_PAGE_DATA_READ					(0x13)
	#define W25N01GV_CMD_READ							(0x03)
	#define W25N01GV_CMD_FAST_READ						(0x0B)
	#define W25N01GV_CMD_FAST_READ_4_BYTE_ADDR			(0x0C)
	#define W25N01GV_CMD_FAST_READ_DUAL_OUT				(0x3B)
	#define W25N01GV_CMD_FAST_READ_DUAL_OUT_4_BYTE_ADDR	(0x3C)
	#define W25N01GV_CMD_FAST_READ_QUAD_OUT				(0x6B)
	#define W25N01GV_CMD_FAST_READ_QUAD_OUT_4_BYTE_ADDR	(0x6C)
	#define W25N01GV_CMD_FAST_READ_DUAL_IO				(0xBB)
	#define W25N01GV_CMD_FAST_READ_DUAL_IO_4_BYTE_ADDR	(0xBC)
	#define W25N01GV_CMD_FAST_READ_QUAD_IO				(0xEB)
	#define W25N01GV_CMD_FAST_READ_QUAD_IO_4_BYTE_ADDR	(0xEC)

	#define W25N01GV_BUF								(0)
#endif // def W25N01GV_VARIANT_T

#ifdef W25N01GV_VARIANT_G
	#define W25N01GV_CMD_RESET							(0xFF)
	#define W25N01GV_CMD_READ_ID						(0x9F)
	#define W25N01GV_CMD_READ_STATUS_REG				(0x0F)
	//#define W25N01GV_CMD_READ_STATUS_REG				(0x05) // alternative command
	#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F)
	//#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F) // alternative command
	#define W25N01GV_CMD_WRITE_ENABLE					(0x06)
	#define W25N01GV_CMD_WRITE_DISABLE					(0x04)
	#define W25N01GV_CMD_BB_MANAGEMENT					(0xA1)
	#define W25N01GV_CMD_READ_BBM_LUT					(0xA5)
	#define W25N01GV_CMD_LAST_ECC_FAIL_PAGE_ADDR		(0xA9)
	#define W25N01GV_CMD_BLOCK_ERASE					(0xD8)
	#define W25N01GV_CMD_PROG_DATA_LOAD					(0x02)
	#define W25N01GV_CMD_RAND_PROG_DATA_LOAD			(0x84)
	#define W25N01GV_CMD_QUAD_PROG_DATA_LOAD			(0x32)
	#define W25N01GV_CMD_RAND_QUAD_PROG_DATA_LOAD		(0x34)
	#define W25N01GV_CMD_PROG_EXEC						(0x10)
	#define W25N01GV_CMD_PAGE_DATA_READ					(0x13)
	#define W25N01GV_CMD_READ							(0x03)
	#define W25N01GV_CMD_FAST_READ						(0x0B)
	#define W25N01GV_CMD_FAST_READ_4_BYTE_ADDR			(0x0C)
	#define W25N01GV_CMD_FAST_READ_DUAL_OUT				(0x3B)
	#define W25N01GV_CMD_FAST_READ_DUAL_OUT_4_BYTE_ADDR	(0x3C)
	#define W25N01GV_CMD_FAST_READ_QUAD_OUT				(0x6B)
	#define W25N01GV_CMD_FAST_READ_QUAD_OUT_4_BYTE_ADDR	(0x6C)
	#define W25N01GV_CMD_FAST_READ_DUAL_IO				(0xBB)
	#define W25N01GV_CMD_FAST_READ_DUAL_IO_4_BYTE_ADDR	(0xBC)
	#define W25N01GV_CMD_FAST_READ_QUAD_IO				(0xEB)
	#define W25N01GV_CMD_FAST_READ_QUAD_IO_4_BYTE_ADDR	(0xEC)

	#define W25N01GV_BUF								(1)
#endif // def W25N01GV_VARIANT_G

#ifdef W25N01GV_VARIANT_R
	#define W25N01GV_CMD_RESET							(0xFF)
	#define W25N01GV_CMD_READ_ID						(0x9F)
	#define W25N01GV_CMD_READ_STATUS_REG				(0x0F)
	//#define W25N01GV_CMD_READ_STATUS_REG				(0x05) // alternative command
	#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F)
	//#define W25N01GV_CMD_WRITE_STATUS_REG				(0x1F) // alternative command
	#define W25N01GV_CMD_WRITE_ENABLE					(0x06)
	#define W25N01GV_CMD_WRITE_DISABLE					(0x04)
	#define W25N01GV_CMD_BLOCK_ERASE					(0xD8)
	#define W25N01GV_CMD_PROG_DATA_LOAD					(0x02)
	#define W25N01GV_CMD_RAND_PROG_DATA_LOAD			(0x84)
	#define W25N01GV_CMD_QUAD_PROG_DATA_LOAD			(0x32)
	#define W25N01GV_CMD_RAND_QUAD_PROG_DATA_LOAD		(0x34)
	#define W25N01GV_CMD_PROG_EXEC						(0x10)
	#define W25N01GV_CMD_PAGE_DATA_READ					(0x13)
	#define W25N01GV_CMD_READ							(0x03)
	#define W25N01GV_CMD_FAST_READ						(0x0B)
	#define W25N01GV_CMD_FAST_READ_DUAL_OUT				(0x3B)
	#define W25N01GV_CMD_FAST_READ_QUAD_OUT				(0x6B)
	#define W25N01GV_CMD_FAST_READ_DUAL_IO				(0xBB)
	#define W25N01GV_CMD_FAST_READ_QUAD_IO				(0xEB)

	#define W25N01GV_BUF								(1)
#endif // def W25N01GV_VARIANT_R

#define W25N01GV_REG_PROT_INIT							(0x7C)
#define W25N01GV_REG_CONF_INIT							(0x10 | W25N01GV_BUF<<3)
#define W25N01GV_REG_STATUS_INIT						(0x01)

#define W25N01GV_REG_PROT_ADDR							(0xA0)
#define W25N01GV_REG_CONF_ADDR							(0xB0)
#define W25N01GV_REG_STATUS_ADDR						(0xC0)

#define W25N01GV_BADBLOCK_LUT_SIZE						(20)

#define W25N01GV_PROT_NONE								(0x0)
#define W25N01GV_PROT_1_512								(0x1)
#define W25N01GV_PROT_1_256								(0x2)
#define W25N01GV_PROT_1_128								(0x3)
#define W25N01GV_PROT_1_64								(0x4)
#define W25N01GV_PROT_1_32								(0x5)
#define W25N01GV_PROT_1_16								(0x6)
#define W25N01GV_PROT_1_8								(0x7)
#define W25N01GV_PROT_1_4								(0x8)
#define W25N01GV_PROT_1_2								(0x9)
#define W25N01GV_PROT_ALL								(0xF)

#define W25N01GV_PROT_UPPER								(0x0)
#define W25N01GV_PROT_LOWER								(0x1)

#ifdef W25N01GV_TEST_UNION_STRUCT
/*! \union test_bitfield_t
 *  \brief Type for testing the implementation of bitfields in the current compiler
 */
typedef union {
	struct {
		uint8_t low        : 4; /*!< Low nibble */
		uint8_t high       : 4; /*!< High nibble */
		uint8_t upper_low  : 4; /*!< low nibble, upper byte */
		uint8_t upper_high : 4; /*!< low nibble, upper byte */
	} bitfield;                 /*!< Nibbles */
	struct {
		uint8_t low; /*!< Low byte */
		uint8_t high; /*!< High byte */
	} byte_named;                 /*!< Nibbles */
	uint8_t     byte[2];        /*!< Byte */
	uint16_t	word;           /*!< Word (2 bytes) */
} test_bitfield_t;
#endif // W25N01GV_TEST_UNION_STRUCT

/**
  * @brief  HAL Status structures definition
  */
typedef enum
{
  W25N01GV_OK              = HAL_OK,      // 0x00U,
  W25N01GV_ERROR           = HAL_ERROR,   // 0x01U,
  W25N01GV_BUSY            = HAL_BUSY,    // 0x02U,
  W25N01GV_TIMEOUT         = HAL_TIMEOUT, // 0x03U,
  W25N01GV_ADDR_OVERRUN    = 0x04U,
  W25N01GV_WRITE_NON_EMPTY = 0x05U,
  W25N01GV_PROG_FAILURE    = 0x06U,
  W25N01GV_ECC_UNCORRECTABLE = 0x07U,
  W25N01GV_VERIFY_FAILED     = 0x08U
} w25n01gv_status_t;

typedef union {
	uint8_t reg;							/*!< direct register access */
	struct {
		uint8_t sreg_prot_1			: 1;	/*!< Status Register Protect-1, (Volatile Writable, OTP Lock) */
		uint8_t wp_enable			: 1;	/*!< /WP Enable Bit, (Volatile Writable, OTP Lock) */
		uint8_t top_bot_prot		: 1;	/*!< Top/Bottom Protect Bit, (Volatile Writable, OTP Lock) */
		uint8_t block_prot			: 4;	/*!< Block Protect Bits, (Volatile Writable, OTP Lock) */
		uint8_t sreg_prot_0			: 1;	/*!< Status Register Protect-0, (Volatile Writable, OTP Lock) */
	} fields;
	struct {
		uint8_t sreg_prot_1			: 1;	/*!< Status Register Protect-1, (Volatile Writable, OTP Lock) */
		uint8_t wp_enable			: 1;	/*!< /WP Enable Bit, (Volatile Writable, OTP Lock) */
		uint8_t top_bot_prot		: 1;	/*!< Top/Bottom Protect Bit, (Volatile Writable, OTP Lock) */
		uint8_t block_prot_0		: 1;	/*!< Block Protect Bit 0, (Volatile Writable, OTP Lock) */
		uint8_t block_prot_1		: 1;	/*!< Block Protect Bit 1, (Volatile Writable, OTP Lock) */
		uint8_t block_prot_2		: 1;	/*!< Block Protect Bit 2, (Volatile Writable, OTP Lock) */
		uint8_t block_prot_3		: 1;	/*!< Block Protect Bit 3, (Volatile Writable, OTP Lock) */
		uint8_t sreg_prot_0			: 1;	/*!< Status Register Protect-0, (Volatile Writable, OTP Lock) */
	} bits;
} w25n01gv_prot_reg_t;

typedef union {
	uint8_t reg;							/*!< direct register access */
	struct {
		uint8_t reserved			: 3;	/*!< Reserved */
		uint8_t buf_mode			: 1;	/*!< Buffer Mode, (Volatile Writable) */
		uint8_t ecc_enabe			: 1;	/*!< Enable ECC, (Volatile Writable) */
		uint8_t sreg_1_lock			: 1;	/*!< Status Register-1 Lock, (OTP Lock) */
		uint8_t otp_enter			: 1;	/*!< Enter OTP Mode, (Volatile Writable) */
		uint8_t otp_pages_lock		: 1;	/*!< OTP Data Pages Lock, (OTP Lock) */
	} fields;
	struct {
		uint8_t reserved_0			: 1;	/*!< Reserved */
		uint8_t reserved_1			: 1;	/*!< Reserved */
		uint8_t reserved_2			: 1;	/*!< Reserved */
		uint8_t buf_mode			: 1;	/*!< Buffer Mode, (Volatile Writable) */
		uint8_t ecc_enabe			: 1;	/*!< Enable ECC, (Volatile Writable) */
		uint8_t sreg_1_lock			: 1;	/*!< Status Register-1 Lock, (OTP Lock) */
		uint8_t otp_enter			: 1;	/*!< Enter OTP Mode, (Volatile Writable) */
		uint8_t otp_pages_lock		: 1;	/*!< OTP Data Pages Lock, (OTP Lock) */
	} bits;
} w25n01gv_conf_reg_t;

typedef union {
	uint8_t reg;							/*!< direct register access */
	struct {
		uint8_t busy				: 1;	/*!< Operation In Progress, (Status-Only) */
		uint8_t write_enable_latch	: 1;	/*!< Write Enable Latch, (Status-Only) */
		uint8_t erase_failure		: 1;	/*!< Erase Failure, (Status-Only) */
		uint8_t program_failure		: 1;	/*!< Program Failure, (Status-Only) */
		uint8_t ecc_status			: 2;	/*!< ECC Status Bit[1:0], (Status-Only) */
		uint8_t bbm_lut_full		: 1;	/*!< BBM LUT Full, (Status Only) */
		uint8_t reserved			: 1;	/*!< Reserved */
	} fields;
	struct {
		uint8_t busy				: 1;	/*!< Operation In Progress, (Status-Only) */
		uint8_t write_enable_latch	: 1;	/*!< Write Enable Latch, (Status-Only) */
		uint8_t erase_failure		: 1;	/*!< Erase Failure, (Status-Only) */
		uint8_t program_failure		: 1;	/*!< Program Failure, (Status-Only) */
		uint8_t ecc_status_0		: 1;	/*!< ECC Status Bit 0, (Status-Only) */
		uint8_t ecc_status_1		: 1;	/*!< ECC Status Bit 1, (Status-Only) */
		uint8_t bbm_lut_full		: 1;	/*!< BBM LUT Full, (Status Only) */
		uint8_t reserved			: 1;	/*!< Reserved */
	} bits;
} w25n01gv_status_reg_t;

typedef struct {
	w25n01gv_prot_reg_t reg_prot;
	w25n01gv_conf_reg_t reg_conf;
	w25n01gv_status_reg_t reg_status;
} w25n01gv_reg_t;

typedef union {
	uint32_t word;
	struct {
		uint16_t phys_block_addr;
		uint16_t logic_block_addr;
	} addr;
	struct {
		uint8_t phys_block_addr_low;
		uint8_t phys_block_addr_high;
		uint8_t logic_block_addr_low;
		uint8_t logic_block_addr_high;
	} addr_byte;
	uint8_t bytes[4];
} w25n01gv_badblock_t;

typedef struct {
	QSPI_HandleTypeDef *hqspi;
	w25n01gv_prot_reg_t reg_prot;
	w25n01gv_conf_reg_t reg_conf;
	w25n01gv_status_reg_t reg_status;
	w25n01gv_badblock_t badblock_lut[W25N01GV_BADBLOCK_LUT_SIZE];
	uint32_t timeout;
} w25n01gv_handle;

w25n01gv_status_t w25n01gv_init(w25n01gv_handle *hw25n01gv, QSPI_HandleTypeDef *hqspi, uint32_t timeout);

w25n01gv_status_t w25n01gv_read(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size);

w25n01gv_status_t w25n01gv_read_otp(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size);

w25n01gv_status_t w25n01gv_erase(w25n01gv_handle *hw25n01gv, uint32_t start_address, uint32_t end_address);

w25n01gv_status_t w25n01gv_write(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size);

w25n01gv_status_t w25n01gv_verify(w25n01gv_handle *hw25n01gv, uint8_t *data, uint32_t address, uint32_t size);

w25n01gv_status_t w25n01gv_reset(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_read_id(w25n01gv_handle *hw25n01gv, uint8_t *id);

w25n01gv_status_t w25n01gv_read_reg(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_read_reg_prot(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_read_reg_conf(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_read_reg_status(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_write_reg(w25n01gv_handle *hw25n01gvm, w25n01gv_reg_t *regs);

w25n01gv_status_t w25n01gv_write_reg_prot(w25n01gv_handle *hw25n01gv, w25n01gv_prot_reg_t *reg);

w25n01gv_status_t w25n01gv_write_reg_conf(w25n01gv_handle *hw25n01gv, w25n01gv_conf_reg_t *reg);

w25n01gv_status_t w25n01gv_write_reg_status(w25n01gv_handle *hw25n01gv, w25n01gv_status_reg_t *reg);

w25n01gv_status_t w25n01gv_set_prot(w25n01gv_handle *hw25n01gv, uint8_t block_prot, uint8_t upper_lower);

w25n01gv_status_t w25n01gv_wait_busy(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_buffer_mode_enable(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_buffer_mode_disable(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_write_enable(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_write_disable(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_badblock_management(w25n01gv_handle *hw25n01gv, w25n01gv_badblock_t badblock);

w25n01gv_status_t w25n01gv_read_badblock(w25n01gv_handle *hw25n01gv);

w25n01gv_status_t w25n01gv_last_ecc_failure(w25n01gv_handle *hw25n01gv, uint16_t *page_addr);

w25n01gv_status_t w25n01gv_block_erase(w25n01gv_handle *hw25n01gv, uint16_t page_addr);

w25n01gv_status_t w25n01gv_prog_load(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size);

w25n01gv_status_t w25n01gv_prog_load_random(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size);

w25n01gv_status_t w25n01gv_prog_exec(w25n01gv_handle *hw25n01gv, uint16_t page_addr);

w25n01gv_status_t w25n01gv_page_read(w25n01gv_handle *hw25n01gv, uint16_t page_addr);

w25n01gv_status_t w25n01gv_read_data(w25n01gv_handle *hw25n01gv, uint8_t *data, uint16_t col_addr, uint32_t size);

#endif /* INC_W25N01GV_H_ */
