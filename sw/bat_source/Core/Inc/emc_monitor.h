/*
 * emc_monitor.h
 *
 * Pass/fail monitor for EMC immunity tests in EMI mode (EMI_DEBUG). Checks
 * the channels of emc_param.h every statemachine tick against their limits
 * and latches excursions. The run starts and stops with output_on.
 *
 *  Created on: Nov 8, 2026
 *      Author: ahorat
 */

#ifndef INC_EMC_MONITOR_H_
#define INC_EMC_MONITOR_H_

#include <stdint.h>
#include <stddef.h>
#include "debug.h"

#ifdef EMI_DEBUG

#define EMC_ROW_LEN 34 // characters of emc_monitor_row_text()

typedef struct {
	uint8_t running;
	uint8_t started;       // a run happened since the last reset
	uint8_t failed;
	uint32_t first_fail_ms; // since start
	uint32_t elapsed_ms;    // run time, frozen after stop
} emc_status_t;

/** @brief Per-tick check; a rising/falling edge of run starts/stops the run. */
void emc_monitor_step(uint8_t run);

/** @brief Clears all latches and restarts time and blanking. */
void emc_monitor_reset(void);

void emc_monitor_get_status(emc_status_t *st);

uint8_t emc_monitor_channel_count(void);

/** @brief Fixed-width row "name value min max" (EMC_ROW_LEN chars). */
void emc_monitor_row_text(uint8_t ch, char *buf, size_t len);

/** @brief 1 if the channel went out of spec since the start. */
uint8_t emc_monitor_row_failed(uint8_t ch);

/** @brief Prints the per-channel summary on the CLI. */
void emc_monitor_report(void);

#endif /* EMI_DEBUG */

#endif /* INC_EMC_MONITOR_H_ */
