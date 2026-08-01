/*
 * ctrl_param.h
 *
 *  Created on: Jul 30, 2025
 *      Author: ahorat
 */

#ifndef INC_CTRL_PARAM_H_
#define INC_CTRL_PARAM_H_

#define CTRL_FREQ 25000.0F

#define CTRL_PARAM_SW_FREQ_HIGH 1000000
#define CTRL_PARAM_SW_FREQ_LOW  50000
#define CTRL_PARAM_CONST_DUTY_HIGH 0.985
#define CTRL_PARAM_CONST_DUTY_LOW 0.015
#define CTRL_PARAM_SW_FREQ_HV 350000

#define CTRL_PARAM_VOLTAGE_BOOST_P 0.01F
#define CTRL_PARAM_VOLTAGE_BOOST_I (1.0F/CTRL_FREQ)

#define CTRL_PARAM_VOLTAGE_BUCK_P 0.01F
#define CTRL_PARAM_VOLTAGE_BUCK_I (1.0F/CTRL_FREQ)

#define CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_HIGH 0.8F
#define CTRL_PARAM_VOLTAGE_BUCK_DUTY_SAT_LOW 0.0F

#define CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_HIGH 0.8F
#define CTRL_PARAM_VOLTAGE_BOOST_DUTY_SAT_LOW 0.015F

#define CTRL_PARAM_BOOST_IOUT_LIMIT_mA 1000
#define CTRL_PARAM_BOOST_IOUT_LIMIT_P (0.01F)
#define CTRL_PARAM_BOOST_IOUT_LIMIT_I (1.0F/CTRL_FREQ)

#define CTRL_PARAM_CURRENT_P 0.001F
#define CTRL_PARAM_CURRENT_I (0.1F/CTRL_FREQ)
#define CTRL_PARAM_CURRENT_DUTY_SAT_HIGH 0.5F
#define CTRL_PARAM_CURRENT_DUTY_SAT_LOW 0.0F

#define CTRL_PARAM_CHARGE_CURRENT_P 0.001F
#define CTRL_PARAM_CHARGE_CURRENT_I (0.1F/CTRL_FREQ)
#define CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH 0.985F
#define CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_LOW 0.0F

// CV tail of the charge cycle, once CTRL_PARAM_CHARGE_END_VOLTAGE_mV has
// been reached. Sat limits are placeholders reusing the charge-current
// loop's, pending real bench tuning.
#define CTRL_PARAM_CHARGE_VOLTAGE_P 0.001F
#define CTRL_PARAM_CHARGE_VOLTAGE_I (0.1F/CTRL_FREQ)
#define CTRL_PARAM_CHARGE_VOLTAGE_DUTY_SAT_HIGH CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_HIGH
#define CTRL_PARAM_CHARGE_VOLTAGE_DUTY_SAT_LOW CTRL_PARAM_CHARGE_CURRENT_DUTY_SAT_LOW


#define CTRL_PARAM_HV_VOLTAGE_P 0.001F
#define CTRL_PARAM_HV_VOLTAGE_I (0.1F/CTRL_FREQ)
#define CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_HIGH 0.2F
#define CTRL_PARAM_HV_VOLTAGE_DUTY_SAT_LOW 0.0F

#define CTRL_PARAM_HV_IOUT_LIMIT_mA 1
#define CTRL_PARAM_HV_CURRENT_P 0.001F
#define CTRL_PARAM_HV_CURRENT_I (0.1F/CTRL_FREQ)

#define CTRL_PARAM_IDLE_VOLTAGE_1A_mV 2000
#define CTRL_PARAM_IDLE_VOLTAGE_1mA_mV 3000

// 12 bit DAC code for the "high" half of the RESISTANCE_1A test-current
// reference square wave. Placeholder mid-scale value only - needs real
// bench tuning against the actual 1A current source hardware.
#define CTRL_PARAM_1A_REF_DAC_VALUE 2048

#define CTRL_PARAM_CHARGE_CURRENT_mA 1000
#define CTRL_PARAM_CHARGE_END_VOLTAGE_mV (3500*4)

// Rated pack capacity, in mA-seconds to match the BQ76905 PASSQ
// accumulator's units (4.5Ah LiFePO4 = 4500mAh * 3600s/h). Stored
// adaptable in EEPROM (config_store.calibration.battery_capacity_mAs)
// rather than hardcoded, so a different pack doesn't need a recompile -
// this is only the power-on default.
#define CTRL_PARAM_BATTERY_CAPACITY_mAs (4500UL*3600UL)

// Charge is only considered finished once the current has tapered below
// this (in addition to reaching CTRL_PARAM_CHARGE_END_VOLTAGE_mV) - roughly
// C/20 for the 4.5Ah pack. Placeholder needing bench tuning, like the other
// charge params.
#define CTRL_PARAM_CHARGE_TAPER_CURRENT_mA 100

// Plausibility window for bms.VoltageRegisters.StackVoltage before it is
// trusted to gate the CC->CV transition. The BMS is read asynchronously over
// I2C at ~1Hz, so the field reads 0 before the first successful poll and
// holds its last value if the bus fails - and a 0 would sit below the end
// voltage forever, leaving the charger stuck in constant current with the CV
// phase that is supposed to end it never engaging. A 4-cell LiFePO4 stack
// below 8V is either deeply discharged (in which case the BMS should have cut
// the FETs long before) or not a real reading; above 20V is not a 4-cell
// LiFePO4 stack at all.
#define CTRL_PARAM_STACK_VOLTAGE_MIN_VALID_mV 8000
#define CTRL_PARAM_STACK_VOLTAGE_MAX_VALID_mV 20000

#define CTRL_PARAM_CHARGE_START_VIN_LOW_mV 15000
#define CTRL_PARAM_CHARGE_START_VIN_HIGH_mV 24000
#define CTRL_PARAM_CHARGE_STOP_VIN_mV (CTRL_PARAM_CHARGE_END_VOLTAGE_mV+200)

// Margin below CTRL_PARAM_CHARGE_END_VOLTAGE_mV that StackVoltage must drop
// under before CHARGE is allowed to auto-restart from Idle - without this,
// a pack sitting right at the end-voltage threshold (e.g. rebounding once
// current stops) could flap in and out of CHARGE mode.
#define CTRL_PARAM_CHARGE_RESTART_MARGIN_mV 300


#endif /* INC_CTRL_PARAM_H_ */
