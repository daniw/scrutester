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

#define CTRL_PARAM_IDLE_VOLTAGE_1A_mV 5000
#define CTRL_PARAM_IDLE_VOLTAGE_1mA_mV 3000

#define CTRL_PARAM_CHARGE_CURRENT_mA 1000
#define CTRL_PARAM_CHARGE_END_VOLTAGE_mV (3550*4)
#define CTRL_PARAM_CHARGE_BO_VOLTAGE_mV (16000)

// Rated pack capacity, in mA-seconds to match the BQ76905 PASSQ
// accumulator's units (4.5Ah LiFePO4 = 4500mAh * 3600s/h). Stored
// adaptable in EEPROM (config_store.calibration.battery_capacity_mAs)
// rather than hardcoded, so a different pack doesn't need a recompile -
// this is only the power-on default.
#define CTRL_PARAM_BATTERY_CAPACITY_mAs (4500UL*3600UL)

#define CTRL_PARAM_CHARGE_START_VIN_LOW_mV 15000
#define CTRL_PARAM_CHARGE_START_VIN_HIGH_mV 24000
#define CTRL_PARAM_CHARGE_STOP_VIN_mV (CTRL_PARAM_CHARGE_END_VOLTAGE_mV+200)

// Margin below CTRL_PARAM_CHARGE_END_VOLTAGE_mV that StackVoltage must drop
// under before CHARGE is allowed to auto-restart from Idle - without this,
// a pack sitting right at the end-voltage threshold (e.g. rebounding once
// current stops) could flap in and out of CHARGE mode.
#define CTRL_PARAM_CHARGE_RESTART_MARGIN_mV 300


#endif /* INC_CTRL_PARAM_H_ */
