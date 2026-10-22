/*
 * fault.c
 *
 *  Created on: Oct 22, 2026
 *      Author: ahorat
 */

#include "main.h"
#include "hrtim.h"
#include "fault.h"

// Bounded wait per character, in case USART2 is not clocked/initialised.
#define FAULT_UART_TIMEOUT 100000U

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

	if (USART2->CR1 & USART_CR1_UE) {
		uint32_t cfsr = SCB->CFSR;
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
