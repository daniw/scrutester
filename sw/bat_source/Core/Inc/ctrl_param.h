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


/* The HV/ISOMETER loop does NOT execute at CTRL_FREQ, unlike every other
 * loop here. It is paced by HRTIM TRG2 off Timer C at CTRL_PARAM_SW_FREQ_HV
 * (350 kHz), post-scaled by 15 in MX_HRTIM1_Init() and halved again in
 * HAL_ADC_ConvCpltCallback() -- about 11.7 kHz, not 25 kHz.
 *
 * The two I gains below are still written as x/CTRL_FREQ, so their real
 * integral action is roughly 2.1x weaker than the constant reads, and the
 * startup ramp is roughly 2.1x slower. That is deliberate for now: the rate
 * was previously nondeterministic (it inherited whichever trigger the last
 * mode set), so making it fixed had to come before tuning against it. When
 * the HV loop is tuned on the bench, close the gap either by retuning these
 * against the real rate or by changing TRG2's post-scaler from 15 to 7,
 * which lands the loop on 25 kHz exactly. Note the post-scaler lives in
 * CubeMX-generated code and would be reverted by regenerating the .ioc. */
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
#define CTRL_PARAM_CHARGE_END_VOLTAGE_mV (3550*4)

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
#define CTRL_PARAM_CHARGE_TAPER_CURRENT_mA 225

#define CTRL_PARAM_CHARGE_START_VIN_LOW_mV 15000
#define CTRL_PARAM_CHARGE_START_VIN_HIGH_mV 24000
#define CTRL_PARAM_CHARGE_STOP_VIN_mV (CTRL_PARAM_CHARGE_END_VOLTAGE_mV+200)

// Margin below CTRL_PARAM_CHARGE_END_VOLTAGE_mV that StackVoltage must drop
// under before CHARGE is allowed to auto-restart from Idle - without this,
// a pack sitting right at the end-voltage threshold (e.g. rebounding once
// current stops) could flap in and out of CHARGE mode.
#define CTRL_PARAM_CHARGE_RESTART_MARGIN_mV 300


#endif /* INC_CTRL_PARAM_H_ */
