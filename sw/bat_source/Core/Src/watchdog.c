/*
 * watchdog.c
 *
 * See watchdog.h.
 *
 *  Created on: Oct 12, 2026
 *      Author: ahorat
 */

#include "main.h"
#include "watchdog.h"
#include <stdio.h>

#define WATCHDOG_KEY_RELOAD  0xAAAAU  /* reload the counter                   */
#define WATCHDOG_KEY_ENABLE  0xCCCCU  /* start the watchdog (irreversible)    */
#define WATCHDOG_KEY_UNLOCK  0x5555U  /* allow writes to PR / RLR / WINR      */

/* PR = 0b011 -> LSI / 32 -> ~1 kHz counter clock. */
#define WATCHDOG_PRESCALER   (IWDG_PR_PR_0 | IWDG_PR_PR_1)

/* The PR/RLR update crosses into the LSI clock domain, so SR stays non-zero for
 * up to a few LSI cycles (~ms at worst). Bounded so a dead LSI can't hang boot
 * here before the watchdog even runs. */
#define WATCHDOG_UPDATE_TIMEOUT_MS  100U

#if (WATCHDOG_TIMEOUT_MS < 1U) || (WATCHDOG_TIMEOUT_MS > 4095U)
#error "WATCHDOG_TIMEOUT_MS must fit the 12-bit IWDG reload register"
#endif

void watchdog_report_reset_cause(void) {
	if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
		printf("Reset caused by watchdog\r\n");
	}
	// Clear unconditionally: the flags are sticky across resets, so leaving
	// them set would make every later boot report a stale cause.
	__HAL_RCC_CLEAR_RESET_FLAGS();
}

void watchdog_init(void) {
	// Freeze the counter while a debugger has the core halted, otherwise every
	// breakpoint in CubeIDE ends in a reset. Has no effect without a debugger.
	__HAL_DBGMCU_FREEZE_IWDG();

	IWDG->KR = WATCHDOG_KEY_ENABLE;   // also forces the LSI on
	IWDG->KR = WATCHDOG_KEY_UNLOCK;
	IWDG->PR = WATCHDOG_PRESCALER;
	IWDG->RLR = WATCHDOG_TIMEOUT_MS;

	// Wait for the prescaler/reload update to land. On timeout carry on: the
	// watchdog is already running (it can't be stopped), and the first kick
	// reloads it from whatever RLR ends up being.
	uint32_t start = HAL_GetTick();
	while (IWDG->SR != 0U) {
		if ((HAL_GetTick() - start) > WATCHDOG_UPDATE_TIMEOUT_MS) {
			break;
		}
	}

	IWDG->KR = WATCHDOG_KEY_RELOAD;
}

void watchdog_kick(void) {
	// Writing the reload key is harmless while the watchdog isn't running, so
	// the flash wait loop can call this during boot-time flash init too.
	IWDG->KR = WATCHDOG_KEY_RELOAD;
}
