/*
 * ctrl_main.h
 *
 *  Created on: Jul 30, 2025
 *      Author: ahorat
 */

#ifndef INC_CTRL_MAIN_H_
#define INC_CTRL_MAIN_H_

#include "ctrl_PID_control.h"
#include "adc.h"
#include "statemachine.h"

typedef enum {
	CTRL_MODE_OFF,
	CTRL_MODE_60V,
	CTRL_MODE_10A,
	CTRL_MODE_RESISTANCE_1A,
	CTRL_MODE_RESISTANCE_1mA,
	CTRL_MODE_ISOMETER,
	CTRL_MODE_CHARGE
} ctrl_mode_t;


typedef struct{
	uint16_t poti_reference;
	uint16_t voltage_iso_reference_V;
	uint32_t voltage_reference_mV;
	uint32_t current_reference_mA;
	float ramp;
	ctrl_mode_t mode;
	int32_t duty;
	// One-way latch: set once CHARGE's CC phase reaches end-voltage and
	// switches to CV, cleared only when CHARGE is (re-)started.
	uint8_t charge_cv_phase;

} ctrl_main_t;

/**
 * One entry per tunable PID control loop: the live controller instance, the
 * EEPROM-backed calibration fields it's saved to/loaded from, its default
 * gains and (fixed, non-tunable) duty saturation limits, and a CLI-facing
 * name. ctrl_main_init() and the CLI's setPI/saveEEPROM/readEEPROM commands
 * all iterate ctrl_pid_table instead of hand-listing each controller, so
 * adding a new tunable loop only means adding one row here.
 */
typedef struct {
	PID_controller_t *ctrl;
	float *cal_p;
	float *cal_i;
	float default_p;
	float default_i;
	float sat_high;
	float sat_low;
	const char *name;
} ctrl_pid_entry_t;

#define CTRL_PID_TABLE_LEN 8
extern const ctrl_pid_entry_t ctrl_pid_table[CTRL_PID_TABLE_LEN];

void ctrl_main_init(void);
void ctrl_main_start_ctrl(ctrl_mode_t mode);
void ctrl_main_ctrl(ADC_MEAS_DATA *adc_data);
void ctrl_main_stop_control(void);
void ctrl_main_apply_reference(ctrl_mode_t mode, uint16_t reference_poti_count);
ctrl_mode_t statemachine_mode_to_ctrl_mode(statemachine_modes_t mode);


#endif /* INC_CTRL_MAIN_H_ */
