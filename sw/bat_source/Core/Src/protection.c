/*
 * protection.c
 *
 * Self-contained hardware protection module: polls the OVP_N/OCP_N
 * fault-latch pins and the 4 external NTC temperatures once per
 * statemachine tick and classifies each into OK/WARNING/ERROR
 * (temperatures only -- OVP/OCP are ERROR-only, no warning tier).
 * Per-mode voltage/current limits are ERROR-only and latched.
 */

#include "protection.h"
#include "protection_param.h"
#include "adc.h"
#include "bq76905.h"
#include "main.h"
#include <stdlib.h>

extern ADC_MEAS_DATA adc_data;
extern BQ76905_handle bms;

typedef enum {
	PROTECTION_TEMP_IDX_SEC = 0,
	PROTECTION_TEMP_IDX_TRAFO,
	PROTECTION_TEMP_IDX_CURRENT,
	PROTECTION_TEMP_IDX_PRIM,
	PROTECTION_TEMP_IDX_COUNT
} protection_temp_idx_t;

static const protection_source_t protection_temp_src[PROTECTION_TEMP_IDX_COUNT] = {
	[PROTECTION_TEMP_IDX_SEC]     = PROTECTION_SRC_TEMP_SEC,
	[PROTECTION_TEMP_IDX_TRAFO]   = PROTECTION_SRC_TEMP_TRAFO,
	[PROTECTION_TEMP_IDX_CURRENT] = PROTECTION_SRC_TEMP_CURRENT,
	[PROTECTION_TEMP_IDX_PRIM]    = PROTECTION_SRC_TEMP_PRIM,
};

static protection_level_t protection_temp_level[PROTECTION_TEMP_IDX_COUNT];
static uint16_t protection_warning_mask;
static uint16_t protection_error_mask;

typedef struct {
	int32_t v_max_mV;   // |V_TERM|
	int32_t i_max_mA;   // |I_OUT|
	int32_t iso_max_uA; // I_ISO
} protection_limit_t;

static const protection_limit_t protection_limits[STATEMACHINE_MODE_RESERVED] = {
	[STATEMACHINE_MODE_60V_OUT]         = { PROTECTION_60V_MAX_mV, PROTECTION_60V_MAX_mA, 0 },
	[STATEMACHINE_MODE_10A_OUT]         = { PROTECTION_10A_MAX_mV, PROTECTION_10A_MAX_mA, 0 },
	[STATEMACHINE_MODE_RESISTANCE_1A]   = { PROTECTION_RES_1A_MAX_mV, PROTECTION_RES_1A_MAX_mA, 0 },
	[STATEMACHINE_MODE_RESISTANCE_1mA]  = { PROTECTION_RES_1mA_MAX_mV, PROTECTION_RES_1mA_MAX_mA, 0 },
	[STATEMACHINE_MODE_ISOMETER]        = { PROTECTION_ISO_MAX_mV, 0, PROTECTION_ISO_MAX_uA },
	[STATEMACHINE_MODE_VOLTMETER]       = { PROTECTION_VOLTMETER_MAX_mV, 0, 0 },
	[STATEMACHINE_MODE_AMPMETER]        = { 0, PROTECTION_AMPMETER_MAX_mA, 0 },
	[STATEMACHINE_MODE_CHARGE]          = { PROTECTION_CHARGE_MAX_mV, PROTECTION_CHARGE_MAX_mA, 0 },
	[STATEMACHINE_MODE_EMI]             = { PROTECTION_EMI_MAX_mV, PROTECTION_EMI_MAX_mA, 0 },
};

static uint16_t protection_limit_latch;
static uint8_t protection_limit_ticks_v;
static uint8_t protection_limit_ticks_i;
static statemachine_modes_t protection_limit_mode;

static protection_level_t protection_classify_temp(int16_t temp, protection_level_t prev) {
	if (temp >= PROTECTION_TEMP_ERROR_C)
		return PROTECTION_LEVEL_ERROR;
	else if (prev == PROTECTION_LEVEL_ERROR && temp >= PROTECTION_TEMP_ERROR_C - PROTECTION_TEMP_HYSTERESIS_C)
		return PROTECTION_LEVEL_ERROR;        // stays latched until it drops far enough
	else if (temp >= PROTECTION_TEMP_WARNING_C)
		return PROTECTION_LEVEL_WARNING;
	else if (prev == PROTECTION_LEVEL_WARNING && temp >= PROTECTION_TEMP_WARNING_C - PROTECTION_TEMP_HYSTERESIS_C)
		return PROTECTION_LEVEL_WARNING;      // stays latched until it drops far enough
	else if (temp <= PROTECTION_TEMP_ERROR_OPEN_NTC) // open NTC reads the -50 °C table end
		return PROTECTION_LEVEL_ERROR;
	else
		return PROTECTION_LEVEL_OK;
}

void protection_init(void) {
	for (int i = 0; i < PROTECTION_TEMP_IDX_COUNT; i++) {
		protection_temp_level[i] = PROTECTION_LEVEL_OK;
	}
	protection_warning_mask = 0;
	protection_error_mask = 0;
	protection_clear_limits();
}

/**
 * @brief Releases the latched per-mode limit errors. A limit that is still
 *        exceeded trips again on the next update.
 */
void protection_clear_limits(void) {
	protection_limit_latch = 0;
}

static uint8_t protection_debounce(uint8_t *ticks, uint8_t over) {
	if (!over)
		*ticks = 0;
	else if (*ticks < PROTECTION_LIMIT_DEBOUNCE_TICKS)
		(*ticks)++;
	return *ticks >= PROTECTION_LIMIT_DEBOUNCE_TICKS;
}

/**
 * @brief Checks the limits of the given mode and latches a trip.
 * @param mode Current statemachine mode.
 * @return Latched PROTECTION_SRC_LIMIT_* bits.
 */
static uint16_t protection_check_limits(statemachine_modes_t mode) {
	if (mode != protection_limit_mode) {
		protection_limit_mode = mode;
		protection_limit_latch = 0;
		protection_limit_ticks_v = 0;
		protection_limit_ticks_i = 0;
	}
	if (mode >= STATEMACHINE_MODE_RESERVED)
		return 0;

	const protection_limit_t *l = &protection_limits[mode];
	int32_t v = labs(adc_data.converted.v_term_ext_mv);
	int32_t i = labs(adc_data.converted.i_out_ext_mA);
	uint8_t over_v = l->v_max_mV && v > l->v_max_mV;
	uint8_t over_i = (l->i_max_mA && i > l->i_max_mA)
			|| (l->iso_max_uA && adc_data.converted.i_iso_ext_uA > l->iso_max_uA);

	if (protection_debounce(&protection_limit_ticks_v, over_v))
		protection_limit_latch |= PROTECTION_SRC_LIMIT_V;
	if (protection_debounce(&protection_limit_ticks_i, over_i))
		protection_limit_latch |= PROTECTION_SRC_LIMIT_I;
	return protection_limit_latch;
}

void protection_update(statemachine_modes_t mode) {
	const int16_t temps[PROTECTION_TEMP_IDX_COUNT] = {
		[PROTECTION_TEMP_IDX_SEC]     = adc_data.converted.temp_sec,
		[PROTECTION_TEMP_IDX_TRAFO]   = adc_data.converted.temp_trafo,
		[PROTECTION_TEMP_IDX_CURRENT] = adc_data.converted.temp_current,
		[PROTECTION_TEMP_IDX_PRIM]    = adc_data.converted.temp_prim,
	};

	uint16_t warning_mask = 0;
	uint16_t error_mask = 0;

	for (int i = 0; i < PROTECTION_TEMP_IDX_COUNT; i++) {
		protection_level_t level = protection_classify_temp(temps[i], protection_temp_level[i]);
		protection_temp_level[i] = level;
		if (level == PROTECTION_LEVEL_ERROR) {
			error_mask |= (uint16_t) protection_temp_src[i];
		} else if (level == PROTECTION_LEVEL_WARNING) {
			warning_mask |= (uint16_t) protection_temp_src[i];
		}
	}

	if (HAL_GPIO_ReadPin(OVP_N_GPIO_Port, OVP_N_Pin) == GPIO_PIN_RESET) {
		error_mask |= (uint16_t) PROTECTION_SRC_OVP;
	}
	// OCP_N is only meaningful while ISOMETER is driving the HV converter --
	// it trips spuriously in other modes, so only evaluate it there.
	if (mode == STATEMACHINE_MODE_ISOMETER
			&& HAL_GPIO_ReadPin(OCP_N_GPIO_Port, OCP_N_Pin) == GPIO_PIN_RESET) {
		error_mask |= (uint16_t) PROTECTION_SRC_OCP;
	}

	error_mask |= protection_check_limits(mode);

	// CHARGE acts on the BMS data; elsewhere the BMS hardware protects alone.
	if (BQ76905_data_stale(&bms)) {
		if (mode == STATEMACHINE_MODE_CHARGE)
			error_mask |= (uint16_t) PROTECTION_SRC_BMS_STALE;
		else
			warning_mask |= (uint16_t) PROTECTION_SRC_BMS_STALE;
	}

	protection_warning_mask = warning_mask;
	protection_error_mask = error_mask;
}

protection_level_t protection_get_worst_level(void) {
	if (protection_error_mask)
		return PROTECTION_LEVEL_ERROR;
	if (protection_warning_mask)
		return PROTECTION_LEVEL_WARNING;
	return PROTECTION_LEVEL_OK;
}

uint16_t protection_get_warning_mask(void) {
	return protection_warning_mask;
}

uint16_t protection_get_error_mask(void) {
	return protection_error_mask;
}

const char *protection_source_name(protection_source_t src) {
	switch (src) {
	case PROTECTION_SRC_TEMP_SEC:     return "TEMP_SEC";
	case PROTECTION_SRC_TEMP_TRAFO:   return "TEMP_TRAFO";
	case PROTECTION_SRC_TEMP_CURRENT: return "TEMP_CURRENT";
	case PROTECTION_SRC_TEMP_PRIM:    return "TEMP_PRIM";
	case PROTECTION_SRC_OVP:          return "OVP";
	case PROTECTION_SRC_OCP:          return "OCP";
	case PROTECTION_SRC_LIMIT_V:      return "LIMIT_V";
	case PROTECTION_SRC_LIMIT_I:      return "LIMIT_I";
	case PROTECTION_SRC_BMS_STALE:    return "BMS_STALE";
	default:                          return "?";
	}
}

/**
 * @brief Banner text for the most relevant active error.
 * @return Text, or NULL if no error is active.
 */
const char *protection_error_text(void) {
	uint16_t e = protection_error_mask;
	if (e & PROTECTION_SRC_OVP)
		return "ERROR: Overvoltage (OVP latch)";
	if (e & PROTECTION_SRC_OCP)
		return "ERROR: HV overcurrent";
	if (e & PROTECTION_SRC_LIMIT_V)
		return "ERROR: Voltage limit exceeded";
	if (e & PROTECTION_SRC_LIMIT_I)
		return "ERROR: Current limit exceeded";
	if (e & PROTECTION_SRC_BMS_STALE)
		return "ERROR: BMS communication";
	if (e)
		return "ERROR: Temperature / NTC";
	return NULL;
}
