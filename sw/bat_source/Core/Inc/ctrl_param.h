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
#define CTRL_PARAM_1A_REF_DAC_VALUE 3820

#define CTRL_PARAM_CHARGE_CURRENT_mA 1000
#define CTRL_PARAM_CHARGE_END_VOLTAGE_mV (3500*4)

// Reduced current target for a deep-discharge/CUV recovery charge (see
// statemachine_enter_charge_low_current() in statemachine.c) -- charging
// into a pack the BMS has flagged undervoltage on, through the DSG FET's
// body diode with no closed-loop DSG path. Independently bench-tunable, not
// derived from CTRL_PARAM_CHARGE_CURRENT_mA, in case the safe recovery
// current doesn't simply scale with the normal charge current.
#define CTRL_PARAM_CHARGE_DEEP_DISCHARGE_CURRENT_mA 100

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

// ---- Multi-phase CHARGE start (see charge_seq.h) --------------------------
// All placeholders pending bench tuning: OUT_LV (the converter output node
// behind the output relay) has no ADC channel, so PRECHARGE is open-loop and
// its accuracy is only as good as V_IN and the duty model.

// PRECHARGE target for the converter output node, as a fraction of the
// charger voltage. Kept below 1.0 so an open-loop error cannot overshoot the
// charger.
#define CTRL_PARAM_CHARGE_PRECHARGE_RATIO 0.99F
// PRIM duty ramp (PRIM_START_FRACTION -> full, like the 60V boost start),
// then the SEK duty ramp towards the target, then a hold before the relay
// is closed.
#define CTRL_PARAM_CHARGE_PRECHARGE_PRIM_RAMP_s 0.2F
#define CTRL_PARAM_CHARGE_PRECHARGE_SEK_RAMP_s 0.2F
#define CTRL_PARAM_CHARGE_PRECHARGE_HOLD_s 0.2F

// Wait after commanding the output relay closed before current control
// starts. The relay's release time is not documented in the repo (datasheet
// missing) - verify on the bench.
#define CTRL_PARAM_CHARGE_RELAY_SETTLE_ms 100

// Charge current reference ramps 0 -> CTRL_PARAM_CHARGE_CURRENT_mA over this.
#define CTRL_PARAM_CHARGE_RAMP_s 5.0F

// Window after the relay-close command over which the peak |I_OUT| is
// recorded and printed (bench aid for tuning PRECHARGE_RATIO/RELAY_SETTLE_ms).
#define CTRL_PARAM_CHARGE_PEAK_WINDOW_s 0.4F

// Abort when the current reference is above ..._REF_mA but less than
// ..._MEAS_mA actually flows for ..._ms: the charge path is open (relay
// stuck open, charger gone) and the PI integrator would only wind up.
#define CTRL_PARAM_CHARGE_STUCK_OPEN_REF_mA 300
#define CTRL_PARAM_CHARGE_STUCK_OPEN_MEAS_mA 20
#define CTRL_PARAM_CHARGE_STUCK_OPEN_ms 2000


#endif /* INC_CTRL_PARAM_H_ */
