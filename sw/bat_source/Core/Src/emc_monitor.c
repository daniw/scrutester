/*
 * emc_monitor.c
 *
 * EMC immunity monitor, see emc_monitor.h and emc_param.h.
 *
 *  Created on: Nov 8, 2026
 *      Author: ahorat
 */

#include "emc_monitor.h"

#ifdef EMI_DEBUG

#include <stdio.h>
#include "stm32g4xx_hal.h"
#include "adc.h"
#include "bq76905.h"
#include "ads131m04.h"
#include "ctrl_main.h"
#include "i2c.h"
#include "protection_param.h"

extern ctrl_main_t ctrl_main_handle;
extern ADC_MEAS_DATA adc_data;
extern BQ76905_handle bms;
extern ADS131M04_handle ext_adc;

typedef enum { EMC_RANGE, EMC_NOMINAL, EMC_SETPOINT, EMC_BASELINE, EMC_COUNT } emc_kind_t;

typedef struct {
	const char *name;
	const char *unit;
	uint8_t dec;
	emc_kind_t kind;
	int32_t a;
	int32_t b;
} emc_crit_t;

typedef struct {
	int32_t cur;
	int32_t min;
	int32_t max;
	int32_t base;      // raw value at the end of blanking
	uint32_t last_print; // ms of the last CLI line
	uint32_t count;    // excursions
	uint32_t first_ms;
	uint8_t seen;      // min/max valid
	uint8_t bad;       // out of spec now
	uint8_t failed;    // latched
} emc_chan_t;

/** @brief Min (sel 0) or max (sel 1) of the pack cells in mV. */
static int32_t cell_mV(uint8_t sel) {
	int32_t r = bms.CellVoltageRegisters.CellVoltages[0];
	for (uint8_t i = 1; i < BQ76905_PACK_CELL_COUNT; i++) {
		int32_t v = bms.CellVoltageRegisters.CellVoltages[i];
		if (sel ? v > r : v < r)
			r = v;
	}
	return r;
}

/** @brief Hottest NTC in degC; 1000 if any NTC is open. */
static int32_t temp_max_C(void) {
	const int16_t t[4] = { adc_data.converted.temp_sec,
			adc_data.converted.temp_trafo, adc_data.converted.temp_current,
			adc_data.converted.temp_prim };
	int32_t r = t[0];
	for (uint8_t i = 0; i < 4; i++) {
		if (t[i] <= PROTECTION_TEMP_ERROR_OPEN_NTC)
			return 1000;
		if (t[i] > r)
			r = t[i];
	}
	return r;
}

#include "emc_param.h"

#define X_ID(id, ...) EMC_CH_##id,
enum { EMC_CHANNELS(X_ID) EMC_CH_COUNT };

#define X_CRIT(id, name, unit, dec, kind, a, b, read) { name, unit, dec, kind, a, b },
static const emc_crit_t crit[EMC_CH_COUNT] = { EMC_CHANNELS(X_CRIT) };

static emc_chan_t chan[EMC_CH_COUNT];
static uint8_t running;
static uint8_t started;
static uint8_t base_set;
static uint32_t t_start;
static uint32_t t_last; // last tick of the run
static uint32_t first_fail_ms;
static uint8_t any_failed;

/** @brief Reads all channels into raw[]. */
static void sample(int32_t *raw) {
#define X_READ(id, name, unit, dec, kind, a, b, read) raw[EMC_CH_##id] = (int32_t) (read);
	EMC_CHANNELS(X_READ)
}

/** @brief Limits of channel i at this moment, on the value as stored in cur. */
static void limits(uint8_t i, int32_t *lo, int32_t *hi) {
	int32_t base = chan[i].base;
	const emc_crit_t *c = &crit[i];
	switch (c->kind) {
	case EMC_NOMINAL:
		*lo = c->a - (int32_t) ((int64_t) c->a * c->b / 1000);
		*hi = c->a + (int32_t) ((int64_t) c->a * c->b / 1000);
		break;
	case EMC_SETPOINT: {
		int32_t ref = (int32_t) ctrl_main_handle.voltage_reference_mV;
		int32_t tol = (int32_t) ((int64_t) ref * c->a / 1000);
		if (tol < c->b)
			tol = c->b;
		*lo = ref - tol;
		*hi = ref + tol;
		break;
	}
	case EMC_BASELINE:
		*lo = base - (int32_t) ((int64_t) base * c->a / 1000);
		*hi = base + (int32_t) ((int64_t) base * c->a / 1000);
		break;
	case EMC_COUNT:
		*lo = 0;
		*hi = c->a;
		break;
	default:
		*lo = c->a;
		*hi = c->b;
		break;
	}
}

/** @brief Value with c->dec decimals, e.g. 3 -> "40.512". */
static void fmt_val(char *buf, size_t len, int32_t v, uint8_t dec) {
	static const uint32_t p10[] = { 1, 10, 100, 1000 };
	uint32_t a = (uint32_t) (v < 0 ? -v : v);

	if (dec == 0)
		snprintf(buf, len, "%ld", (long) v);
	else
		snprintf(buf, len, "%s%lu.%0*lu", v < 0 ? "-" : "",
				(unsigned long) (a / p10[dec]), dec, (unsigned long) (a % p10[dec]));
}

static void fmt_time(char *buf, size_t len, uint32_t ms) {
	snprintf(buf, len, "%02lu:%02lu.%03lu", (unsigned long) (ms / 60000),
			(unsigned long) (ms / 1000 % 60), (unsigned long) (ms % 1000));
}

void emc_monitor_reset(void) {
	for (uint8_t i = 0; i < EMC_CH_COUNT; i++) {
		chan[i].min = 0;
		chan[i].max = 0;
		chan[i].count = 0;
		chan[i].first_ms = 0;
		chan[i].seen = 0;
		chan[i].bad = 0;
		chan[i].failed = 0;
	}
	any_failed = 0;
	first_fail_ms = 0;
	base_set = 0;
	t_start = HAL_GetTick();
	t_last = t_start;
	started = running;
}

/** @brief Latches a new excursion of channel i and prints it. */
static void excursion(uint8_t i, uint32_t ms) {
	emc_chan_t *ch = &chan[i];
	char v[16], t[16];

	if (!ch->failed)
		ch->first_ms = ms;
	if (!any_failed || ms < first_fail_ms)
		first_fail_ms = ms;
	ch->failed = 1;
	any_failed = 1;
	ch->count++;
	fmt_val(v, sizeof(v), ch->cur, crit[i].dec);
	fmt_time(t, sizeof(t), ms);
	if (ch->count > 1 && ms - ch->last_print < EMC_PRINT_MIN_ms)
		return;
	ch->last_print = ms;
	printf("EMC FAIL %s %s %s at %s\r\n", crit[i].name, v, crit[i].unit, t);
}

void emc_monitor_step(uint8_t run) {
	int32_t raw[EMC_CH_COUNT];
	uint32_t now = HAL_GetTick();

	sample(raw);
	if (run && !running) {
		running = 1;
		emc_monitor_reset();
	} else if (!run && running) {
		running = 0;
	}
	if (running)
		t_last = now;
	uint32_t ms = now - t_start;
	uint8_t checking = running && ms >= EMC_BLANKING_ms;

	if (checking && !base_set) {
		for (uint8_t i = 0; i < EMC_CH_COUNT; i++)
			chan[i].base = raw[i];
		base_set = 1;
	}
	for (uint8_t i = 0; i < EMC_CH_COUNT; i++) {
		emc_chan_t *ch = &chan[i];
		int32_t v = raw[i];
		int32_t lo, hi;

		if (crit[i].kind == EMC_COUNT)
			v = base_set ? v - ch->base : 0;
		ch->cur = v;
		if (!checking)
			continue;
		if (!ch->seen || v < ch->min)
			ch->min = v;
		if (!ch->seen || v > ch->max)
			ch->max = v;
		ch->seen = 1;
		limits(i, &lo, &hi);
		uint8_t bad = (v < lo || v > hi);
		if (bad && !ch->bad)
			excursion(i, ms);
		ch->bad = bad;
	}
}

void emc_monitor_get_status(emc_status_t *st) {
	st->running = running;
	st->started = started;
	st->failed = any_failed;
	st->first_fail_ms = first_fail_ms;
	st->elapsed_ms = (running ? HAL_GetTick() : t_last) - t_start;
}

uint8_t emc_monitor_channel_count(void) {
	return EMC_CH_COUNT;
}

void emc_monitor_row_text(uint8_t ch, char *buf, size_t len) {
	char cur[16], min[16] = "--", max[16] = "--";
	const emc_chan_t *c = &chan[ch];

	fmt_val(cur, sizeof(cur), c->cur, crit[ch].dec);
	if (c->seen) {
		fmt_val(min, sizeof(min), c->min, crit[ch].dec);
		fmt_val(max, sizeof(max), c->max, crit[ch].dec);
	}
	snprintf(buf, len, "%-7s%-3s%8s%8s%8s", crit[ch].name, crit[ch].unit, cur,
			min, max);
}

uint8_t emc_monitor_row_failed(uint8_t ch) {
	return chan[ch].failed;
}

void emc_monitor_report(void) {
	emc_status_t st;
	char t[16];

	emc_monitor_get_status(&st);
	fmt_time(t, sizeof(t), st.elapsed_ms);
	printf("EMC %s, run %s, %s\r\n", st.running ? "running" : "stopped", t,
			st.failed ? "FAIL" : (st.started ? "PASS" : "no run"));
	for (uint8_t i = 0; i < EMC_CH_COUNT; i++) {
		char row[EMC_ROW_LEN + 1], first[16] = "--";
		const emc_chan_t *c = &chan[i];

		emc_monitor_row_text(i, row, sizeof(row));
		if (c->failed)
			fmt_time(first, sizeof(first), c->first_ms);
		printf("  %s  %s  exc %lu  first %s\r\n", row,
				c->failed ? "FAIL" : "OK  ", (unsigned long) c->count, first);
	}
}

#endif /* EMI_DEBUG */
