/*
 * protection.c
 *
 * Self-contained hardware protection module: polls the OVP_N/OCP_N
 * fault-latch pins and the 4 external NTC temperatures once per
 * statemachine tick and classifies each into OK/WARNING/ERROR
 * (temperatures only -- OVP/OCP are ERROR-only, no warning tier).
 */

#include "protection.h"
#include "protection_param.h"
#include "adc.h"
#include "main.h"

extern ADC_MEAS_DATA adc_data;

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

static protection_level_t protection_classify_temp(int16_t temp, protection_level_t prev) {
	if (temp >= PROTECTION_TEMP_ERROR_C)
		return PROTECTION_LEVEL_ERROR;
	else if (prev == PROTECTION_LEVEL_ERROR && temp >= PROTECTION_TEMP_ERROR_C - PROTECTION_TEMP_HYSTERESIS_C)
		return PROTECTION_LEVEL_ERROR;        // stays latched until it drops far enough
	else if (temp >= PROTECTION_TEMP_WARNING_C)
		return PROTECTION_LEVEL_WARNING;
	else if (prev == PROTECTION_LEVEL_WARNING && temp >= PROTECTION_TEMP_WARNING_C - PROTECTION_TEMP_HYSTERESIS_C)
		return PROTECTION_LEVEL_WARNING;      // stays latched until it drops far enough
	else
		return PROTECTION_LEVEL_OK;
}

void protection_init(void) {
	for (int i = 0; i < PROTECTION_TEMP_IDX_COUNT; i++) {
		protection_temp_level[i] = PROTECTION_LEVEL_OK;
	}
	protection_warning_mask = 0;
	protection_error_mask = 0;
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
	default:                          return "?";
	}
}
