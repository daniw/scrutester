/*
 * fault.c
 *
 *  Created on: Oct 22, 2026
 *      Author: ahorat
 */

#include "main.h"
#include "hrtim.h"
#include "fault.h"
#include <stdio.h>

// Bounded wait per character, in case USART2 is not clocked/initialised.
#define FAULT_UART_TIMEOUT 100000U

#define FAULT_MAGIC 0xFA17C0DEU
#define FAULT_FLASH_BASE 0x08000000U
#define FAULT_FLASH_END  0x08080000U

// Survives the reset: .noinit is neither zeroed nor loaded at startup.
typedef struct {
	uint32_t magic;
	const char *name; // points into flash, valid for the same firmware
	uint32_t addr;
	uint32_t cfsr;
	uint32_t hfsr;
} fault_record_t;

static fault_record_t fault_record __attribute__((section(".noinit")));

static char fault_text[48]; // set by fault_report_reset()

static const struct {
	uint8_t bit;
	const char *name;
} fault_cfsr_bits[] = {
	{ 0, "IACCVIOL" }, { 1, "DACCVIOL" }, { 3, "MUNSTKERR" },
	{ 4, "MSTKERR" }, { 5, "MLSPERR" }, { 8, "IBUSERR" },
	{ 9, "PRECISERR" }, { 10, "IMPRECISERR" }, { 11, "UNSTKERR" },
	{ 12, "STKERR" }, { 13, "LSPERR" }, { 16, "UNDEFINSTR" },
	{ 17, "INVSTATE" }, { 18, "INVPC" }, { 19, "NOCP" },
	{ 24, "UNALIGNED" }, { 25, "DIVBYZERO" },
};

static void fault_putc(char c) {
	for (uint32_t t = 0; !(USART2->ISR & USART_ISR_TXE_TXFNF); t++)
		if (t > FAULT_UART_TIMEOUT)
			return;
	USART2->TDR = (uint8_t) c;
}

static void fault_puts(const char *s) {
	while (*s)
		fault_putc(*s++);
}

static void fault_puthex(const char *label, uint32_t value) {
	fault_puts(label);
	fault_puts("=0x");
	for (int i = 28; i >= 0; i -= 4)
		fault_putc("0123456789ABCDEF"[(value >> i) & 0xF]);
	fault_puts(" ");
}

/**
 * @brief Turns the power stage off, prints the fault on USART2 (polled, no
 *        HAL) and halts until the IWDG resets the MCU.
 * @param name Fault name to print.
 * @param addr Extra address to print (e.g. caller), 0 = none.
 */
void fault_halt(const char *name, uint32_t addr) {
	__disable_irq();
	hrtim_emergency_off();

	uint32_t cfsr = SCB->CFSR;
	fault_record.name = name;
	fault_record.addr = addr;
	fault_record.cfsr = cfsr;
	fault_record.hfsr = SCB->HFSR;
	fault_record.magic = FAULT_MAGIC;

	if (USART2->CR1 & USART_CR1_UE) {
		fault_puts("\r\n*** FAULT: ");
		fault_puts(name);
		fault_puts(" ***\r\n");
		if (addr)
			fault_puthex("addr", addr);
		fault_puthex("CFSR", cfsr);
		fault_puthex("HFSR", SCB->HFSR);
		if (cfsr & SCB_CFSR_MMARVALID_Msk)
			fault_puthex("MMFAR", SCB->MMFAR);
		if (cfsr & SCB_CFSR_BFARVALID_Msk)
			fault_puthex("BFAR", SCB->BFAR);
		for (unsigned i = 0; i < sizeof(fault_cfsr_bits) / sizeof(fault_cfsr_bits[0]); i++)
			if (cfsr & (1UL << fault_cfsr_bits[i].bit)) {
				fault_puts(fault_cfsr_bits[i].name);
				fault_puts(" ");
			}
		fault_puts("\r\n");
	}

	while (1) {
	}
}

/**
 * @brief Prints the reset cause (RCC flags) and a fault left by fault_halt().
 *        Clears both. Call once at boot, after the CLI is up.
 */
void fault_report_reset(void) {
	static const struct {
		uint32_t flag;
		const char *name;
	} causes[] = {
		{ RCC_FLAG_BORRST, "BOR" }, { RCC_FLAG_PINRST, "PIN" },
		{ RCC_FLAG_SFTRST, "SW" }, { RCC_FLAG_IWDGRST, "IWDG" },
		{ RCC_FLAG_WWDGRST, "WWDG" }, { RCC_FLAG_LPWRRST, "LPWR" },
		{ RCC_FLAG_OBLRST, "OBL" },
	};
	int iwdg = __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST);

	// PIN is set by most resets too, so print every flag that is set.
	printf("Reset cause:");
	for (unsigned i = 0; i < sizeof(causes) / sizeof(causes[0]); i++)
		if (__HAL_RCC_GET_FLAG(causes[i].flag))
			printf(" %s", causes[i].name);
	printf("\r\n");
	// Flags are sticky across resets: clear, or the next boot reports stale causes.
	__HAL_RCC_CLEAR_RESET_FLAGS();

	// Name pointer is only trusted if it lies in flash (e.g. after a reflash).
	uintptr_t np = (uintptr_t) fault_record.name;
	if (fault_record.magic == FAULT_MAGIC && np >= FAULT_FLASH_BASE && np < FAULT_FLASH_END) {
		printf("Last reset: FAULT %.24s addr 0x%08lX CFSR 0x%08lX HFSR 0x%08lX\r\n",
				fault_record.name, (unsigned long) fault_record.addr,
				(unsigned long) fault_record.cfsr, (unsigned long) fault_record.hfsr);
		snprintf(fault_text, sizeof(fault_text), "Restarted after fault (%.24s)", fault_record.name);
	} else if (iwdg) {
		snprintf(fault_text, sizeof(fault_text), "Restarted by watchdog");
	}
	fault_record.magic = 0;
}

/**
 * @brief Short text for the screen about the last reset.
 * @return Text, or NULL after a normal reset. Valid after fault_report_reset().
 */
const char *fault_last_reset_text(void) {
	return fault_text[0] ? fault_text : NULL;
}
