/*
 * balancing.c
 *
 * Cell balancing during the CV tail of a charge cycle. Passive/dissipative
 * balancing only (internal BQ76905 bleed FETs, no external balance
 * hardware on this board), commanded via BQ76905_EnableBalancing() -- a
 * full-replace write of the CB_ACTIVE_CELLS mask, never additive.
 *
 * Enabled exactly while ctrl_main_handle.mode == CTRL_MODE_CHARGE &&
 * ctrl_main_handle.charge_cv_phase; that condition already reverts to
 * false the instant charging stops for any reason (ESC exit, protection
 * interlock, normal completion all route through ctrl_main_stop_control()),
 * so no separate mode-exit/cleanup hook is needed here.
 */

#include "balancing.h"
#include "bq76905.h"
#include "ctrl_main.h"

extern BQ76905_handle bms;
extern ctrl_main_t ctrl_main_handle;

/* Only cells 0-3 are physically wired on this 4S pack; CellVoltages[4] is
 * unused. Same hardcoded-4 precedent as display.c's update_settings_bms()
 * and cli.c's BMS dump. */
#define BALANCING_ACTIVE_CELL_COUNT 4

/* Per-cell hysteresis against each cell's own current balancing state, to
 * avoid chatter near the ADC noise floor: a cell starts balancing once its
 * delta above the pack minimum reaches the start threshold, and keeps
 * balancing until the delta drops below the (lower) stop threshold. */
#define BALANCING_START_THRESHOLD_mV 30
#define BALANCING_STOP_THRESHOLD_mV  20

static uint8_t balancing_active_mask;
static uint8_t balancing_manual_override;

void balancing_update(void) {
	uint8_t target_mask = 0;

	if ((ctrl_main_handle.mode == CTRL_MODE_CHARGE
			&& ctrl_main_handle.charge_cv_phase) || balancing_manual_override) {
		uint16_t min_mV = bms.CellVoltageRegisters.CellVoltages[0];
		for (int i = 1; i < BALANCING_ACTIVE_CELL_COUNT; i++) {
			if (bms.CellVoltageRegisters.CellVoltages[i] < min_mV)
				min_mV = bms.CellVoltageRegisters.CellVoltages[i];
		}

		for (int i = 0; i < BALANCING_ACTIVE_CELL_COUNT; i++) {
			uint16_t delta_mV = bms.CellVoltageRegisters.CellVoltages[i] - min_mV;
			uint8_t was_balancing = (balancing_active_mask >> i) & 0x01;

			if (was_balancing) {
				if (delta_mV >= BALANCING_STOP_THRESHOLD_mV)
					target_mask |= (1u << i);
			} else {
				if (delta_mV >= BALANCING_START_THRESHOLD_mV)
					target_mask |= (1u << i);
			}
		}
	}

	if (target_mask != balancing_active_mask) {
		BQ76905_EnableBalancing(&bms, target_mask);
		balancing_active_mask = target_mask;
	}
}

uint8_t balancing_get_active_mask(void) {
	return balancing_active_mask;
}

void balancing_toggle_manual_override(void) {
	balancing_manual_override = !balancing_manual_override;
}

void balancing_clear_manual_override(void) {
	balancing_manual_override = 0;
}

uint8_t balancing_is_manual_override_active(void) {
	return balancing_manual_override;
}
