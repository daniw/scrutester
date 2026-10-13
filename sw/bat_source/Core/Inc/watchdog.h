/*
 * watchdog.h
 *
 * Independent watchdog (IWDG), driven at register level -- the HAL IWDG
 * driver isn't part of this project, and nothing here needs it.
 *
 * Why it exists: the 25 kHz control loop runs from the ADC ISR, which keeps
 * driving the converter even if the main loop (state machine, UI, CLI, BMS)
 * is stuck. A reset puts the HRTIM outputs back to their safe reset state.
 *
 *  Created on: Oct 12, 2026
 *      Author: ahorat
 */

#ifndef INC_WATCHDOG_H_
#define INC_WATCHDOG_H_

#include <stdint.h>

/* Nominal timeout. The IWDG runs from the LSI (32 kHz nominal) with a /32
 * prescaler, so the counter ticks at ~1 kHz and the reload value is simply
 * this many milliseconds (max 4095). The LSI has a wide tolerance (tens of
 * percent, temperature dependent), so the real timeout can be well below this
 * -- never size it close to the longest legitimate main-loop iteration. */
#define WATCHDOG_TIMEOUT_MS  2000U

/* Prints a message if the last reset was caused by the watchdog and clears the
 * reset flags. Call once at boot, after the CLI is up. */
void watchdog_report_reset_cause(void);

/* Starts the watchdog. It cannot be stopped again until the next reset, so call
 * this only once initialisation is done (init has long blocking waits that
 * would otherwise all need kicks). */
void watchdog_init(void);

/* Reloads the counter. Call once per main-loop iteration, and from any wait
 * loop that can legitimately block for a sizeable fraction of the timeout
 * (flash busy-wait, ...). Safe to call before watchdog_init(). */
void watchdog_kick(void);

#endif /* INC_WATCHDOG_H_ */
