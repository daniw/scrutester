/*
 * balancing.h
 *
 * Cell balancing during the CV tail of a charge cycle, using the BQ76905's
 * built-in per-cell passive (dissipative) balancing FETs -- no external
 * balance hardware on this board. Commanded from main-loop context only
 * (see balancing.c), never from ctrl_main_ctrl()'s ISR context.
 */

#ifndef INC_BALANCING_H_
#define INC_BALANCING_H_

#include <stdint.h>

/** Call once per statemachine tick (statemachine_step(), main-loop context
 *  only -- never from ctrl_main_ctrl()). Unconditional: must be called every
 *  tick regardless of the current statemachine mode, including while NOT
 *  charging, so that a mode exit (ESC, protection interlock, normal charge
 *  completion) is guaranteed to command the balancing mask back to 0 on the
 *  very next tick. Internally a no-op I2C-wise unless the target mask has
 *  changed since the last write. */
void balancing_update(void);

/** Last mask actually written to BQ76905_EnableBalancing() (bit i = cell i
 *  currently instructed to balance). Pure getter over module-local state --
 *  reflects commanded intent, not a live chip readback -- safe for the
 *  display to poll every redraw. */
uint8_t balancing_get_active_mask(void);

#endif /* INC_BALANCING_H_ */
