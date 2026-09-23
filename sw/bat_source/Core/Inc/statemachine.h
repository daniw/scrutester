/*
 * statemachine.h
 *
 *  Created on: Aug 9, 2025
 *      Author: ahorat
 */

#ifndef INC_STATEMACHINE_H_
#define INC_STATEMACHINE_H_

#include "stdint.h"
#include "charge_seq.h" // for CHARGE_DEBUG, see below

#define STATEMACHINE_STEP_PERIOD_mS 20 //ms

typedef enum
{
	  STATEMACHINE_IDLE = 0,
	  STATEMACHINE_MODE_60V_OUT,
	  STATEMACHINE_MODE_10A_OUT,
	  STATEMACHINE_MODE_RESISTANCE_1A,
	  STATEMACHINE_MODE_RESISTANCE_1mA,
	  STATEMACHINE_MODE_ISOMETER,
	  STATEMACHINE_MODE_VOLTMETER,
	  STATEMACHINE_MODE_AMPMETER,
	  STATEMACHINE_MODE_SETTINGS,
	  STATEMACHINE_MODE_SHUTDOWN,
	  STATEMACHINE_MODE_CHARGE,
	  STATEMACHINE_MODE_RESERVED
} statemachine_modes_t;

typedef enum
{
	  STATEMACHINE_SETTINGS_MODE_MENU = 0,
	  STATEMACHINE_SETTINGS_MODE_BMS,
	  STATEMACHINE_SETTINGS_MODE_DISPLAY,
	  STATEMACHINE_SETTINGS_MODE_CALIBRATION,
	  STATEMACHINE_SETTINGS_MODE_ADC1,
	  STATEMACHINE_SETTINGS_MODE_ADC2,
	  STATEMACHINE_SETTINGS_MODE_ABOUT,
	  STATEMACHINE_SETTINGS_MODE_LENGTH
} statemachine_settings_modes_t;

typedef struct{
	statemachine_modes_t current_mode;
	uint8_t output_on;
	uint8_t current_menu_index;
	statemachine_settings_modes_t settings_mode;
	uint8_t protection_forced_output_off;

}statemachine_t;


void statemachine_init(void);
void statemachine_step(void);

void statemachine_switchfromIdle(statemachine_modes_t mode);

#ifdef CHARGE_DEBUG
/* CHARGE single-step bench test (charge_seq.h): with test mode on, CHARGE
 * holds after the open-loop precharge (relay still open) until
 * statemachine_charge_test_advance() closes the relay and starts the
 * current ramp -- the CLI's `chargeStep`/`chargeNext` commands. */
void statemachine_charge_test_set_manual(uint8_t enable);
uint8_t statemachine_charge_test_advance(void);
#endif

#endif /* INC_STATEMACHINE_H_ */
