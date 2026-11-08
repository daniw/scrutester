/*
 * emc_param.h
 *
 * Pass/fail criteria of the EMC immunity monitor (EMI_DEBUG, emc_monitor.c).
 * One row per channel. Include from emc_monitor.c only: the read expressions
 * need adc_data, bms and ext_adc in scope.
 *
 * X(id, name, unit, dec, kind, a, b, read)
 *   name/unit  screen/CLI text, name max 7 chars
 *   dec        decimals of the read value (3: mV shown as V)
 *   kind, a, b EMC_RANGE    a..b absolute
 *              EMC_NOMINAL  a +- b permille
 *              EMC_SETPOINT V setpoint +- a permille, at least +- b
 *              EMC_BASELINE value at the end of blanking +- a permille
 *              EMC_COUNT    increase since the end of blanking <= a
 *   read       C expression, evaluated every tick
 *
 *  Created on: Nov 8, 2026
 *      Author: ahorat
 */

#ifndef INC_EMC_PARAM_H_
#define INC_EMC_PARAM_H_

// Ignored after a start (setpoint ramp).
#define EMC_BLANKING_ms 2000
// CLI: first excursion per channel always, then at most one line per period.
#define EMC_PRINT_MIN_ms 1000

#define EMC_CELL_MIN_mV 3000
#define EMC_CELL_MAX_mV 4250
#define EMC_TEMP_MAX_C  70

// Rail nominals in mV.
#define EMC_V_3V3_mV    3300
#define EMC_V_3V3A_mV   3300
#define EMC_V_5V_mV     5000
#define EMC_V_15V_mV    15000
#define EMC_RAIL_TOL_PERMILLE 50

// V_TERM / I_OUT unfiltered (ext ADC); I_OUT: 500 Ohm load.
#define EMC_CHANNELS(X) \
	X(VTERM, "V_TERM", "V", 3, EMC_SETPOINT, 20, 500, \
		adc_data.converted.v_term_ext_mv) \
	X(IOUT, "I_OUT", "mA", 0, EMC_RANGE, 50, 150, \
		adc_data.converted.i_out_ext_mA) \
	X(VHV, "V_HV", "V", 3, EMC_RANGE, 0, 1100000, /* ToDo: set from bench */ \
		adc_data.converted.v_hv) \
	X(V3V3, "3V3", "V", 3, EMC_NOMINAL, EMC_V_3V3_mV, EMC_RAIL_TOL_PERMILLE, \
		adc_data.converted.v_3v3) \
	X(V3V3A, "3V3A", "V", 3, EMC_NOMINAL, EMC_V_3V3A_mV, EMC_RAIL_TOL_PERMILLE, \
		adc_data.converted.v_3v3a) \
	X(V5V, "5V", "V", 3, EMC_NOMINAL, EMC_V_5V_mV, EMC_RAIL_TOL_PERMILLE, \
		adc_data.converted.v_5v) \
	X(V15V, "15V", "V", 3, EMC_NOMINAL, EMC_V_15V_mV, EMC_RAIL_TOL_PERMILLE, \
		adc_data.converted.v_15v) \
	X(VCC, "VCC", "V", 3, EMC_BASELINE, EMC_RAIL_TOL_PERMILLE, 0, /* ~battery */ \
		adc_data.converted.v_vcc) \
	X(CELLMIN, "CELLmin", "V", 3, EMC_RANGE, EMC_CELL_MIN_mV, EMC_CELL_MAX_mV, \
		cell_mV(0)) \
	X(CELLMAX, "CELLmax", "V", 3, EMC_RANGE, EMC_CELL_MIN_mV, EMC_CELL_MAX_mV, \
		cell_mV(1)) \
	X(TEMP, "T_max", "C", 0, EMC_RANGE, PROTECTION_TEMP_ERROR_OPEN_NTC + 1, \
		EMC_TEMP_MAX_C, temp_max_C()) \
	X(EXTCRC, "ExtCRC", "", 0, EMC_COUNT, 0, 0, ext_adc.crc_errors) \
	X(BMSSTL, "BMSstl", "", 0, EMC_COUNT, 0, 0, BQ76905_data_stale(&bms)) \
	X(I2CERR, "I2Cerr", "", 0, EMC_COUNT, 0, 0, i2c_error_count())

#endif /* INC_EMC_PARAM_H_ */
