/*
 * display.c
 *
 * See display.h. Static "enter" layout vs. per-tick "update" data is kept
 * deliberately separate: LCD_HANDLE is a bit-banged SPI (no local
 * framebuffer, see lcd.h LCD_LOCAL_FB), so a full-screen redraw every 100ms
 * tick would be slow and would flicker. Static chrome (labels, units,
 * footer) is drawn once on entry; only the numeric fields are redrawn each
 * tick, using fixed-width sprintf formatting so a shorter new value fully
 * overwrites a longer old one.
 */

#include "display.h"
#include "menu.h"
#include "lcd.h"
#include "ugui_fonts.h"
#include "ctrl_main.h"
#include "ctrl_param.h"
#include "adc.h"
#include "bq76905.h"
#include "balancing.h"
#include "aux_io_ctrl.h"
#include "ui_ctrl.h"
#include "config_store.h"
#include "icon_store.h"
#include "version.h"
#include <stdio.h>
#include <string.h>

extern ctrl_main_t ctrl_main_handle;
extern ADC_MEAS_DATA adc_data;
extern BQ76905_handle bms;

static char text[48];

/* ---------------------------------------------------------------------- */
/* Layout                                                                  */
/* ---------------------------------------------------------------------- */

#define STATUS_Y0     0
#define STATUS_H      26
#define BIG_Y         40
#define SECOND_Y      130
#define FOOTER_H      24
#define FOOTER_Y      (LCD_HEIGHT - FOOTER_H)

#define FONT_HUGE     FONT_40X78
#define FONT_BIG      FONT_32X53
#define FONT_MED      FONT_16X26
#define FONT_SMALL    FONT_12X16
#define FONT_TINY     FONT_8X12

/* ---------------------------------------------------------------------- */
/* Shared chrome helpers                                                   */
/* ---------------------------------------------------------------------- */

static void draw_battery_icon(int16_t x, int16_t y, uint8_t pct) {
	int16_t w = 28, h = 14;
	UG_COLOR fill = (pct < 20) ? C_RED : (pct < 50 ? C_ORANGE : C_GREEN);
	int16_t fill_w = (int16_t) ((w - 4) * pct / 100);

	UG_FillFrame(x, y, x + w + 3, y + h, C_BLACK);
	UG_DrawFrame(x, y, x + w, y + h, C_WHITE);
	UG_FillFrame(x + w + 1, y + h / 2 - 2, x + w + 3, y + h / 2 + 2, C_WHITE);
	UG_FillFrame(x + 2, y + 2, x + 2 + fill_w, y + h - 2, fill);
}

static void draw_status_bar(const char *title, UG_COLOR accent) {
	UG_FillFrame(0, STATUS_Y0, LCD_WIDTH - 1, STATUS_H - 1, C_BLACK);
	LCD_PutStr(4, 4, (char*) title, FONT_SMALL, C_WHITE, C_BLACK);
	draw_battery_icon(LCD_WIDTH - 40, 6, bms.charge_percentage);
	UG_FillFrame(0, STATUS_H - 3, LCD_WIDTH - 1, STATUS_H - 1, accent);
}

static void update_status_bar(void) {
	draw_battery_icon(LCD_WIDTH - 40, 6, bms.charge_percentage);
}

static void draw_footer(const char *left, const char *right) {
	UG_FillFrame(0, FOOTER_Y, LCD_WIDTH - 1, LCD_HEIGHT - 1, C_BLACK);
	LCD_PutStr(4, FOOTER_Y + 4, (char*) left, FONT_TINY, C_WHITE_63, C_BLACK);
	if (right != 0) {
		int16_t w = (int16_t) (strlen(right) * UG_GetFontWidth(FONT_TINY));
		LCD_PutStr(LCD_WIDTH - 4 - w, FOOTER_Y + 4, (char*) right, FONT_TINY,
		C_WHITE_63, C_BLACK);
	}
}

static void draw_output_border(UG_COLOR accent, uint8_t active) {
	UG_COLOR c = active ? accent : C_DIM_GRAY;
	UG_DrawFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, c);
	UG_DrawFrame(1, STATUS_H + 1, LCD_WIDTH - 2, FOOTER_Y - 2, c);
}

/* ---------------------------------------------------------------------- */
/* Home / carousel                                                        */
/* ---------------------------------------------------------------------- */

static uint8_t wrap_index(int idx) {
	int m = idx % MENU_ORDER_LENGTH;
	if (m < 0)
		m += MENU_ORDER_LENGTH;
	return (uint8_t) m;
}

/* icon_seeds[]/the flash icon store are indexed in exactly the same order as
 * menu_order[] (see menu.c / icon_seed_data.c), so a carousel position's
 * index doubles as its icon_id -- no separate mapping table needed.
 *
 * Bitmap icons are drawn at a fixed size regardless of position; the
 * selected one is distinguished by an accent-colored ring instead of being
 * physically larger (bitmaps don't scale as cleanly as the procedural
 * fallback icons do). If the flash store isn't available/seeded, falls back
 * to the original procedurally-drawn vector icon, which *does* still scale
 * with `selected`. */
static void draw_menu_icon(uint8_t icon_id, const menu_entry_t *entry,
		int16_t cx, int16_t cy, UG_COLOR color, uint8_t selected) {
	static uint16_t pixel_buf[ICON_STORE_ICON_WIDTH * ICON_STORE_ICON_HEIGHT];

	if (icon_store_available()) {
		UG_BMP bmp = icon_store_load(icon_id, pixel_buf);
		if (bmp.width != 0) {
			LCD_DrawImage(cx - bmp.width / 2, cy - bmp.height / 2, &bmp);
			if (selected)
				UG_DrawCircle(cx, cy, bmp.width / 2 + 4, color);
			return;
		}
	}
	entry->draw_icon(cx, cy, selected ? 28 : 16, color);
}

void display_show_idle(uint8_t menu_index) {
	uint8_t prev_idx = wrap_index((int) menu_index - 1);
	uint8_t next_idx = wrap_index((int) menu_index + 1);
	const menu_entry_t *prev = menu_entry_at(prev_idx);
	const menu_entry_t *cur = menu_entry_at(menu_index);
	const menu_entry_t *next = menu_entry_at(next_idx);
	int16_t cy = STATUS_H + (FOOTER_Y - STATUS_H) / 2 - 10;

	UG_FillFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, C_BLACK);
	draw_status_bar("BatSource", C_SILVER);
	draw_footer("ESC: Shutdown", "OK: Select");

	draw_menu_icon(prev_idx, prev, LCD_WIDTH / 2 - 110, cy, C_DIM_GRAY, 0);
	draw_menu_icon(next_idx, next, LCD_WIDTH / 2 + 110, cy, C_DIM_GRAY, 0);
	draw_menu_icon(menu_index, cur, LCD_WIDTH / 2, cy, cur->accent, 1);

	strncpy(text, menu_name_for_mode(cur->mode), sizeof(text) - 1);
	text[sizeof(text) - 1] = 0;
	{
		int16_t w = (int16_t) (strlen(text) * UG_GetFontWidth(FONT_SMALL));
		UG_FillFrame(0, cy + 70, LCD_WIDTH - 1,
				cy + 70 + UG_GetFontHeight(FONT_SMALL),
				C_BLACK);
		LCD_PutStr(LCD_WIDTH / 2 - w / 2, cy + 70, text, FONT_SMALL, C_WHITE,
		C_BLACK);
	}
}

/* ---------------------------------------------------------------------- */
/* Active-output template (60V / 10A): big measured value, secondary row,  */
/* setpoint row, hold-to-enable border.                                    */
/* ---------------------------------------------------------------------- */

static void enter_active_output(statemachine_modes_t mode, const char *big_unit,
		const char *secondary_label, const char *secondary_unit,
		const char *setpoint_unit) {
	const menu_entry_t *entry = menu_entry_for_mode(mode);
	if (entry == NULL)
		return;

	draw_status_bar(menu_name_for_mode(mode), entry->accent);
	draw_output_border(entry->accent, 0);
	draw_footer("ESC: Back", "Hold OUT to enable");

	LCD_PutStr(LCD_WIDTH / 2 + 70, BIG_Y + 10, (char*) big_unit, FONT_SMALL,
	C_WHITE, C_BLACK);
	LCD_PutStr(16, SECOND_Y, (char*) secondary_label, FONT_TINY, C_WHITE_63,
	C_BLACK);
	LCD_PutStr(16, SECOND_Y + 18, (char*) secondary_unit, FONT_TINY, C_WHITE_63,
	C_BLACK);
	LCD_PutStr(LCD_WIDTH - 100, SECOND_Y, "SET", FONT_TINY, C_WHITE_63,
	C_BLACK);
	LCD_PutStr(LCD_WIDTH - 100, SECOND_Y + 18, (char*) setpoint_unit,
	FONT_TINY, C_WHITE_63, C_BLACK);
}

static void update_active_output(statemachine_modes_t mode,
		uint8_t output_active, int32_t big_value_x100,
		int32_t secondary_value_x1000, uint32_t setpoint_value_x1000,  int32_t debug_value) {
	const menu_entry_t *entry = menu_entry_for_mode(mode);
	if (entry == NULL)
		return;

	draw_output_border(entry->accent, output_active);
	update_status_bar();

	snprintf(text, sizeof(text), "%3d.%02d", (int) (big_value_x100 / 100),
			(int) ((big_value_x100 < 0 ? -big_value_x100 : big_value_x100) % 100));
	LCD_PutStr(16, BIG_Y, text, FONT_BIG, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "%3d.%03d", (int) (secondary_value_x1000 / 1000),
			(int) ((secondary_value_x1000 < 0 ?
					-secondary_value_x1000 : secondary_value_x1000) % 1000));
	LCD_PutStr(16, SECOND_Y + 36, text, FONT_SMALL, C_WHITE, C_BLACK);


	snprintf(text, sizeof(text), "%3d.%03d", (int) (debug_value / 1000),
			(int) ((debug_value < 0 ?
					-debug_value : debug_value) % 1000));
	LCD_PutStr(16, SECOND_Y + 54, text, FONT_SMALL, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "%3u.%03u", (unsigned) (setpoint_value_x1000 / 1000),
			(unsigned) (setpoint_value_x1000 % 1000));
	LCD_PutStr(LCD_WIDTH - 100, SECOND_Y + 36, text, FONT_SMALL, C_WHITE,
	C_BLACK);
}

/* ---------------------------------------------------------------------- */
/* Passive-readout template (Voltmeter / Ampmeter): big value only.        */
/* Value + unit are centered as a group in the output window, since this   */
/* template has no secondary/setpoint fields competing for space.          */
/* ---------------------------------------------------------------------- */

#define PASSIVE_VALUE_CHARS  6  /* fixed width of "%3d.%02d" */
#define PASSIVE_GAP_PX       12

static int16_t passive_readout_x;
static int16_t passive_readout_y;

static void enter_passive_readout(statemachine_modes_t mode, const char *unit) {
	const menu_entry_t *entry = menu_entry_for_mode(mode);
	if (entry == NULL)
		return;
	int16_t value_w = PASSIVE_VALUE_CHARS * UG_GetFontWidth(FONT_HUGE);
	int16_t unit_w = (int16_t) (strlen(unit) * UG_GetFontWidth(FONT_BIG));
	int16_t total_w = value_w + PASSIVE_GAP_PX + unit_w;
	int16_t value_h = UG_GetFontHeight(FONT_HUGE);

	draw_status_bar(menu_name_for_mode(mode), entry->accent);
	draw_output_border(entry->accent, 1);
	draw_footer("ESC: Back", 0);

	passive_readout_x = (LCD_WIDTH - total_w) / 2;
	passive_readout_y = STATUS_H + ((FOOTER_Y - STATUS_H) - value_h) / 2;

	LCD_PutStr(passive_readout_x + value_w + PASSIVE_GAP_PX,
			passive_readout_y + (value_h - UG_GetFontHeight(FONT_BIG)) / 2,
			(char*) unit, FONT_BIG, C_WHITE, C_BLACK);
}

static void update_passive_readout(int32_t value_x100) {
	update_status_bar();
	snprintf(text, sizeof(text), "%3d.%02d", (int) (value_x100 / 100),
			(int) ((value_x100 < 0 ? -value_x100 : value_x100) % 100));
	LCD_PutStr(passive_readout_x, passive_readout_y, text, FONT_HUGE, C_WHITE,
	C_BLACK);
}

/* ---------------------------------------------------------------------- */
/* Resistance 1A / 1mA                                                     */
/* ---------------------------------------------------------------------- */

static void enter_resistance(statemachine_modes_t mode) {
	const menu_entry_t *entry = menu_entry_for_mode(mode);
	if (entry == NULL)
		return;
	const char *excitation =
			(mode == STATEMACHINE_MODE_RESISTANCE_1A) ?
					"I_set  = 1.000 A" : "I_set  = 1.000 mA";
	const char *unit =
			(mode == STATEMACHINE_MODE_RESISTANCE_1A) ? "mOhm" : "Ohm";

	draw_status_bar(menu_name_for_mode(mode), entry->accent);
	draw_output_border(entry->accent, 1);
	draw_footer("ESC: Back", 0);
	LCD_PutStr(16, SECOND_Y, (char*) excitation, FONT_TINY, C_WHITE_63,
	C_BLACK);
	LCD_PutStr(LCD_WIDTH / 2 + 100, BIG_Y + 10, (char*) unit, FONT_SMALL,
	C_WHITE, C_BLACK);
}

static void update_resistance(statemachine_modes_t mode) {
	update_status_bar();
	/* The two modes read different fields with different over-range
	 * semantics (see adc_convert_data() in adc.c):
	 *  - RESISTANCE_1A/Milliohmmeter: r_mOhmx10 is UINT32_MAX when the
	 *    measured current is zero, and is additionally clamped to
	 *    UINT32_MAX once it exceeds ADC_R_MOHMX10_MAX_VALUE. That clamp is
	 *    keyed on adc.c's adc_injected_mode, not on the statemachine mode
	 *    tested here, and the two can diverge (calibration.c retargets the
	 *    ADCs without changing the statemachine mode), so the threshold
	 *    test below is load-bearing rather than merely defensive - do not
	 *    drop it as redundant with the clamp.
	 *  - RESISTANCE_1mA/Ohmmeter: r_Ohmx10 is an unclamped ratio, only ever
	 *    UINT32_MAX via the zero-current sentinel - it must not be gated by
	 *    the milliohm threshold/value, which belongs to the other mode. */
	uint8_t over_range = (mode == STATEMACHINE_MODE_RESISTANCE_1A) ?
			(adc_data.r_mOhmx10 > ADC_R_MOHMX10_MAX_VALUE || adc_data.r_mOhmx10 == UINT32_MAX) :
			(adc_data.r_Ohmx10 == UINT32_MAX);
	if (over_range) {
		snprintf(text, sizeof(text), " OVER ");
	} else if (mode == STATEMACHINE_MODE_RESISTANCE_1A) {
		/* Milliohmmeter: r_mOhmx10 is already mOhm*10. */
		snprintf(text, sizeof(text), "%4u.%01u", (unsigned) (adc_data.r_mOhmx10 / 10),
				(unsigned) (adc_data.r_mOhmx10 % 10));
	} else {
		/* Ohmmeter: reformat the same field as Ohms (mOhm/1000), one decimal digit. */
		uint32_t ohm_x10 = adc_data.r_Ohmx10;
		snprintf(text, sizeof(text), "%4u.%01u", (unsigned) (ohm_x10 / 10),
				(unsigned) (ohm_x10 % 10));
	}
	// ToDo: Remove/mask Debug Values using a define DEBUG
	LCD_PutStr(16, BIG_Y, text, FONT_BIG, C_WHITE, C_BLACK);
	if (mode == STATEMACHINE_MODE_RESISTANCE_1A) {
		/* Milliohmmeter: Take V_sense */
		snprintf(text, sizeof(text), "V_sense = %d uV     ",
				(int) adc_data.converted.v_sens_ext_uv);
	} else {
		/* Ohmmeter: take V_term*/
		snprintf(text, sizeof(text), "V_term = %d mV     ",
				(int) adc_data.converted.v_term_ext_mv);
	}
	LCD_PutStr(16, SECOND_Y + 36, text, FONT_TINY, C_WHITE_63, C_BLACK);

	if (mode == STATEMACHINE_MODE_RESISTANCE_1A) {
		/* Milliohmmeter: Take V_sense */
		snprintf(text, sizeof(text), "I_meas = %d mA    ",
				(int) adc_data.converted.i_out_ext_mA);
	} else {
		/* Ohmmeter: take V_term*/
		snprintf(text, sizeof(text), "I_meas= %d mA     ",
				(int) adc_data.converted.i_out_ext_mA);
	}
	LCD_PutStr(16, SECOND_Y + 18, text, FONT_TINY, C_WHITE_63, C_BLACK);
}

/* ---------------------------------------------------------------------- */
/* Isolation test                                                          */
/* ---------------------------------------------------------------------- */

static void enter_isometer(void) {
	const menu_entry_t *entry = menu_entry_for_mode(STATEMACHINE_MODE_ISOMETER);
	if (entry == NULL)
		return;

	draw_status_bar("Isolation Test", entry->accent);
	draw_output_border(entry->accent, 0);
	draw_footer("ESC: Back", "Encoder: test V");
	LCD_PutStr(LCD_WIDTH / 2 + 100, BIG_Y + 10, "Mohm", FONT_SMALL, C_WHITE,
	C_BLACK);
}

static void update_isometer(uint8_t output_active) {
	const menu_entry_t *entry = menu_entry_for_mode(STATEMACHINE_MODE_ISOMETER);
	draw_output_border(entry->accent, output_active);
	update_status_bar();

	snprintf(text, sizeof(text), "Test Voltage: %4u V   ",
			ctrl_main_handle.voltage_iso_reference_V);
	LCD_PutStr(16, SECOND_Y, text, FONT_TINY, C_WHITE_63, C_BLACK);

	if (adc_data.converted.i_iso_ext_uA > 0) {
		/* R[Mohm] = V / I[uA] (since Mohm = V/uA algebraically); keep one
		 * decimal digit of precision via a x10 fixed-point intermediate. */
//		int32_t r_megaohm_x10 =
//				(int32_t) adc_data.converted.v_term_ext_mv / 100
//						/ adc_data.converted.i_iso_ext_uA;
		int32_t r_megaohm_x10 =
				(int32_t) adc_data.converted.v_term_ext_mv / 1
						/ adc_data.converted.i_iso_ext_uA;
		snprintf(text, sizeof(text), "%3d.%03d", (int) (r_megaohm_x10 / 1000),
				(int) (r_megaohm_x10 % 1000));
	} else {
		snprintf(text, sizeof(text), " OVER  ");
	}
	LCD_PutStr(16, BIG_Y, text, FONT_BIG, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "Ileak = %d uA     ", (int) adc_data.converted.i_iso_ext_uA);
	LCD_PutStr(16, SECOND_Y + 18, text, FONT_TINY, C_WHITE_63, C_BLACK);

	snprintf(text, sizeof(text), "Vterm = %d V     ", (int) adc_data.converted.v_term_ext_mv_filt/1000);
	LCD_PutStr(16, SECOND_Y + 36, text, FONT_TINY, C_WHITE_63, C_BLACK);

	snprintf(text, sizeof(text), "Duty = %3d.%01d  ", (int) ( ctrl_main_handle.duty/ 10),
			(int) ((ctrl_main_handle.duty < 0 ?
					-ctrl_main_handle.duty : ctrl_main_handle.duty) % 10));
	LCD_PutStr(16, SECOND_Y + 54, text, FONT_TINY, C_WHITE_63, C_BLACK);
}

/* ---------------------------------------------------------------------- */
/* Charge                                                                  */
/* ---------------------------------------------------------------------- */

static void enter_charge(void) {
	const menu_entry_t *entry = menu_entry_for_mode(STATEMACHINE_MODE_CHARGE);
	if (entry == NULL)
		return;

	draw_status_bar("Charge", entry->accent);
	draw_output_border(entry->accent, 0);
	draw_footer("ESC: Back", "OK: Start/Stop");
	LCD_PutStr(LCD_WIDTH / 2 + 100, BIG_Y + 10, "V", FONT_SMALL, C_WHITE,
	C_BLACK);
	LCD_PutStr(16, SECOND_Y + 36, "Cells", FONT_TINY, C_WHITE_63, C_BLACK);
}

static void update_charge(uint8_t output_active) {
	const menu_entry_t *entry = menu_entry_for_mode(STATEMACHINE_MODE_CHARGE);
	if (entry == NULL)
		return;
	uint32_t elapsed_s = bms.Accumulator.passedTime / 4;

	draw_output_border(entry->accent, output_active);
	update_status_bar();

	snprintf(text, sizeof(text), "%2u.%03u", bms.VoltageRegisters.StackVoltage / 1000,
			bms.VoltageRegisters.StackVoltage % 1000);
	LCD_PutStr(16, BIG_Y, text, FONT_BIG, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "I %4d mA   %3u%%   %02u:%02u:%02u    ",
			bms.CurrentRegisters.CC1Current, bms.charge_percentage,
			(unsigned) (elapsed_s / 3600), (unsigned) ((elapsed_s / 60) % 60),
			(unsigned) (elapsed_s % 60));
	LCD_PutStr(16, SECOND_Y + 18, text, FONT_TINY, C_WHITE_63, C_BLACK);

	/* One fixed-width segment per cell so each can be colored independently
	 * (balancing_get_active_mask() -- attention color while that cell's
	 * bleed FET is commanded on, dim white otherwise) without leaving
	 * stale pixels from a differently-colored previous draw. Fixed x
	 * offsets, evenly spaced after the static "Cells" label drawn once in
	 * enter_charge(). */
	static const int16_t CELL_SEGMENT_X[4] = { 64, 124, 184, 244 };
	uint8_t balancing_mask = balancing_get_active_mask();
	for (int i = 0; i < 4; i++) {
		UG_COLOR color = (balancing_mask & (1u << i)) ? C_ORANGE : C_WHITE_63;
		snprintf(text, sizeof(text), "%u.%03uV",
				bms.CellVoltageRegisters.CellVoltages[i] / 1000,
				bms.CellVoltageRegisters.CellVoltages[i] % 1000);
		LCD_PutStr(CELL_SEGMENT_X[i], SECOND_Y + 36, text, FONT_TINY, color, C_BLACK);
	}

	snprintf(text, sizeof(text), "Phase: %s",
			ctrl_main_handle.charge_cv_phase ? "CV" : "CC");
	LCD_PutStr(16, SECOND_Y + 54, text, FONT_TINY, C_WHITE, C_BLACK);

}

/* ---------------------------------------------------------------------- */
/* Public API: mode dispatch                                              */
/* ---------------------------------------------------------------------- */

void display_init(void) {
	UG_FillScreen(C_BLACK);
}

void display_enter_mode(statemachine_modes_t mode) {
	UG_FillFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, C_BLACK);

	switch (mode) {
	case STATEMACHINE_MODE_60V_OUT:
		enter_active_output(mode, "V", "IOUT", "A", "V");
		break;
	case STATEMACHINE_MODE_10A_OUT:
		enter_active_output(mode, "A", "VOUT", "V", "A");
		break;
	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
		enter_resistance(mode);
		break;
	case STATEMACHINE_MODE_ISOMETER:
		enter_isometer();
		break;
	case STATEMACHINE_MODE_VOLTMETER:
		enter_passive_readout(mode, "V");
		break;
	case STATEMACHINE_MODE_AMPMETER:
		enter_passive_readout(mode, "A");
		break;
	case STATEMACHINE_MODE_CHARGE:
		enter_charge();
		break;
	default:
		break;
	}
}

void display_update_mode(statemachine_modes_t mode, uint8_t output_active) {
	switch (mode) {
	case STATEMACHINE_MODE_60V_OUT:
		update_active_output(mode, output_active,
				adc_data.converted.v_term_ext_mv_filt / 10,
				adc_data.converted.i_out_ext_mA,
				ctrl_main_handle.voltage_reference_mV, ctrl_main_handle.duty);
		break;
	case STATEMACHINE_MODE_10A_OUT:
		update_active_output(mode, output_active,
				adc_data.converted.i_out_ext_mA / 10,
				adc_data.converted.v_term_ext_mv_filt,
				ctrl_main_handle.current_reference_mA, ctrl_main_handle.duty);
		break;
	case STATEMACHINE_MODE_RESISTANCE_1A:
	case STATEMACHINE_MODE_RESISTANCE_1mA:
		update_resistance(mode);
		break;
	case STATEMACHINE_MODE_ISOMETER:
		update_isometer(output_active);
		break;
	case STATEMACHINE_MODE_VOLTMETER:
		update_passive_readout(adc_data.converted.v_term_ext_mv_filt / 10);
		break;
	case STATEMACHINE_MODE_AMPMETER:
		update_passive_readout(adc_data.converted.i_out_ext_mA / 10);
		break;
	case STATEMACHINE_MODE_CHARGE:
		update_charge(output_active);
		break;
	default:
		break;
	}
}

/* ---------------------------------------------------------------------- */
/* Settings                                                                */
/* ---------------------------------------------------------------------- */

static const char *const SETTINGS_ITEMS[STATEMACHINE_SETTINGS_MODE_LENGTH] = {
		[STATEMACHINE_SETTINGS_MODE_MENU] = "(list)",
		[STATEMACHINE_SETTINGS_MODE_BMS] = "BMS Diagnostics",
		[STATEMACHINE_SETTINGS_MODE_DISPLAY] = "Display Brightness",
		[STATEMACHINE_SETTINGS_MODE_CALIBRATION] = "Calibration",
		[STATEMACHINE_SETTINGS_MODE_ADC1] = "ADC Readings 1",
		[STATEMACHINE_SETTINGS_MODE_ADC2] = "ADC Readings 2",
		[STATEMACHINE_SETTINGS_MODE_ABOUT] = "About", };

void display_show_settings_list(uint8_t submenu_index) {
	UG_FillFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, C_BLACK);
	draw_status_bar("Settings", C_SILVER);
	draw_footer("ESC: Back", "OK: Open");

	for (int i = STATEMACHINE_SETTINGS_MODE_BMS;
			i < STATEMACHINE_SETTINGS_MODE_LENGTH; i++) {
		int16_t y = STATUS_H + 16 + (i - 1) * 24;
		uint8_t selected = (i == submenu_index);

		LCD_PutStr(selected ? 26 : 16, y, (char*) SETTINGS_ITEMS[i], FONT_SMALL,
		C_WHITE, C_BLACK);
		if (selected)
			LCD_PutStr(6, y, ">", FONT_SMALL, C_WHITE, C_BLACK);
	}
}

void display_enter_settings_detail(uint8_t submenu_index) {
	UG_FillFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, C_BLACK);
	draw_status_bar(SETTINGS_ITEMS[submenu_index], C_SILVER);
	draw_footer("ESC: Back",
			submenu_index == STATEMACHINE_SETTINGS_MODE_BMS ? "OK: Balance" : 0);

	if (submenu_index == STATEMACHINE_SETTINGS_MODE_ABOUT) {
		snprintf(text, sizeof(text), "BatSource Firmware");
		LCD_PutStr(16, STATUS_H + 12, text, FONT_SMALL, C_WHITE, C_BLACK);
		snprintf(text, sizeof(text), "Serial number: %04d", config_store.hardware_data.serial_number);
		LCD_PutStr(16, STATUS_H + 34, text, FONT_TINY, C_WHITE_63, C_BLACK);
		snprintf(text, sizeof(text), "Built %s %s", __DATE__, __TIME__);
		LCD_PutStr(16, STATUS_H + 52, text, FONT_TINY, C_WHITE_63, C_BLACK);
		snprintf(text, sizeof(text), "HW revision: %u", aux_io_ctrl_readHW_Revision());
		LCD_PutStr(16, STATUS_H + 70, text, FONT_TINY, C_WHITE_63, C_BLACK);
		snprintf(text, sizeof(text), "MCU: STM32G474VET6");
		LCD_PutStr(16, STATUS_H + 88, text, FONT_TINY, C_WHITE_63, C_BLACK);
		snprintf(text, sizeof(text), "FW version: %d.%d", FW_VERSION_MAJOR, FW_VERSION_MINOR);
		LCD_PutStr(16, STATUS_H + 106, text, FONT_TINY, C_WHITE_63, C_BLACK);
		snprintf(text, sizeof(text), "Designed by daniw & ahorat");
		LCD_PutStr(16, STATUS_H + 124, text, FONT_TINY, C_WHITE_63, C_BLACK);
	}
}

static void update_settings_bms(void) {
	snprintf(text, sizeof(text), "Safety Alert  AB = 0x%04X",
			(bms.SafetyRegisters.safetyAlertA << 8)
					| bms.SafetyRegisters.safetyAlertB);
	LCD_PutStr(16, STATUS_H + 8, text, FONT_TINY, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "Safety Status AB = 0x%04X",
			(bms.SafetyRegisters.safetyStatusA << 8)
					| bms.SafetyRegisters.safetyStatusB);
	LCD_PutStr(16, STATUS_H + 24, text, FONT_TINY, C_WHITE, C_BLACK);

	LCD_PutStr(16, STATUS_H + 40, "Cell = ", FONT_TINY, C_WHITE, C_BLACK);
	{
		/* Fixed-width segment per cell, colored orange while that cell's
		 * bleed FET is commanded on (balancing_get_active_mask()) -- same
		 * convention as the charge screen's cell-voltage line, needed
		 * because this LCD has no local framebuffer (see file header). */
		uint8_t balancing_mask = balancing_get_active_mask();
		for (int i = 0; i < 4; i++) {
			UG_COLOR color = (balancing_mask & (1u << i)) ? C_ORANGE : C_WHITE;
			snprintf(text, sizeof(text), "%4u", bms.CellVoltageRegisters.CellVoltages[i]);
			LCD_PutStr(72 + i * 40, STATUS_H + 40, text, FONT_TINY, color, C_BLACK);
		}
	}
	LCD_PutStr(232, STATUS_H + 40, " mV", FONT_TINY, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "Current = %d mA    ", bms.CurrentRegisters.CC2Current);
	LCD_PutStr(16, STATUS_H + 56, text, FONT_TINY, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "Passed Q = %d mAs    ",
			(int) (bms.Accumulator.accumulatedCharge & 0xFFFFFFFF) / 4);
	LCD_PutStr(16, STATUS_H + 72, text, FONT_TINY, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text), "Passed T = %u s    ",
			(unsigned) (bms.Accumulator.passedTime / 4));
	LCD_PutStr(16, STATUS_H + 88, text, FONT_TINY, C_WHITE, C_BLACK);

	if (balancing_is_manual_override_active())
		snprintf(text, sizeof(text), "Balancing: ON  (mask=0x%X)   ",
				balancing_get_active_mask());
	else
		snprintf(text, sizeof(text), "Balancing: OFF               ");
	LCD_PutStr(16, STATUS_H + 104, text, FONT_TINY, C_WHITE, C_BLACK);
}

static void update_settings_display(void) {
	snprintf(text, sizeof(text), "Ambient: %6u clux  ",
			(unsigned) ui_ctrl_readBrightness());
	LCD_PutStr(16, STATUS_H + 12, text, FONT_SMALL, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "Display brightness: %u%%    ",
			(unsigned) ui_ctrl_readBacklightPercent());
	LCD_PutStr(16, STATUS_H + 40, text, FONT_SMALL, C_WHITE, C_BLACK);
}

static void update_settings_adc1(void) {
	static uint8_t downsample = 0;
	if(downsample++<10)
		return;
	downsample = 0;
	adc_convert_data();
	snprintf(text, sizeof(text),"v_in       : %8u : %7ld mV", adc_data.raw.v_in   , adc_data.converted.v_in  );
	LCD_PutStr(16, STATUS_H + 8, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"v_out      : %8u : %7ld mV", adc_data.raw.v_out  , adc_data.converted.v_out );
	LCD_PutStr(16, STATUS_H + 23, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"v_term     : %8u : %7ld mV", adc_data.raw.v_term , adc_data.converted.v_term);
	LCD_PutStr(16, STATUS_H + 38, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"v_hv       : %8u : %7ld mV", adc_data.raw.v_hv   , adc_data.converted.v_hv  );
	LCD_PutStr(16, STATUS_H + 53, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"i_bat      : %8u : %7d mA",  adc_data.raw.i_bat  , adc_data.converted.i_bat );
	LCD_PutStr(16, STATUS_H + 68, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"i_out      : %8u : %7d mA",  adc_data.raw.i_out  , adc_data.converted.i_out );
	LCD_PutStr(16, STATUS_H + 83, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"i_iso      : %8u : %7d mA",  adc_data.raw.i_iso  , adc_data.converted.i_iso );
	LCD_PutStr(16, STATUS_H + 98, text, FONT_TINY, C_WHITE, C_BLACK);

	snprintf(text, sizeof(text),"Ext V_Term : %8li : %7li mV", adc_data.ext_adc_data[0], adc_data.converted.v_term_ext_mv);
	LCD_PutStr(16, STATUS_H + 128, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"Ext I_Out  : %8li : %7li mA", adc_data.ext_adc_data[1], adc_data.converted.i_out_ext_mA );
	LCD_PutStr(16, STATUS_H + 143, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"Ext V_Sns  : %8li : %7li uV", adc_data.ext_adc_data[2], adc_data.converted.v_sens_ext_uv);
	LCD_PutStr(16, STATUS_H + 158, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text),"Ext I_Iso  : %8li : %7li uA", adc_data.ext_adc_data[3], adc_data.converted.i_iso_ext_uA );
	LCD_PutStr(16, STATUS_H + 173, text, FONT_TINY, C_WHITE, C_BLACK);
}

static void update_settings_adc2(void) {
	static uint8_t downsample = 0;
	if(downsample++<10)
		return;
	downsample = 0;
	adc_convert_data();
	snprintf(text, sizeof(text), "v_3v3         : %8u : %7u mV",			adc_data.raw.v_3v3, adc_data.converted.v_3v3);
	LCD_PutStr(16, STATUS_H + 8, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "temp_sec      : %8u : %7i \260C",			adc_data.raw.temp_sec, adc_data.converted.temp_sec);
	LCD_PutStr(16, STATUS_H + 23, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_3v3a        : %8u : %7u mV",			adc_data.raw.v_3v3a, adc_data.converted.v_3v3a);
	LCD_PutStr(16, STATUS_H + 38, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "temp_inductor : %8u : %7i \260C",			adc_data.raw.temp_trafo, adc_data.converted.temp_trafo);
	LCD_PutStr(16, STATUS_H + 53, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "temp_current  : %8u : %7i \260C",			adc_data.raw.temp_current, adc_data.converted.temp_current);
	LCD_PutStr(16, STATUS_H + 68, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "temp_prim     : %8u : %7i \260C",			adc_data.raw.temp_prim, adc_data.converted.temp_prim);
	LCD_PutStr(16, STATUS_H + 83, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_15v         : %8u : %7u mV",			adc_data.raw.v_15v, adc_data.converted.v_15v);
	LCD_PutStr(16, STATUS_H + 98, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_vcc         : %8u : %7u mV",			adc_data.raw.v_vcc, adc_data.converted.v_vcc);
	LCD_PutStr(16, STATUS_H + 113, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_5v          : %8u : %7u mV",			adc_data.raw.v_5v, adc_data.converted.v_5v);
	LCD_PutStr(16, STATUS_H + 128, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "int_temp      : %8u : %7i \260C",			adc_data.raw.int_temp, adc_data.converted.int_temp);
	LCD_PutStr(16, STATUS_H + 143, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_bat         : %8u : %7u mV",			adc_data.raw.v_bat, adc_data.converted.v_bat);
	LCD_PutStr(16, STATUS_H + 158, text, FONT_TINY, C_WHITE, C_BLACK);
	snprintf(text, sizeof(text), "v_ref_int     : %8u : %7u mV",			adc_data.raw.v_ref_int, adc_data.converted.v_ref_int);
	LCD_PutStr(16, STATUS_H + 173, text, FONT_TINY, C_WHITE, C_BLACK);

}

void display_update_settings_detail(uint8_t submenu_index) {
	switch (submenu_index) {
	case STATEMACHINE_SETTINGS_MODE_BMS:
		update_settings_bms();
		break;
	case STATEMACHINE_SETTINGS_MODE_DISPLAY:
		update_settings_display();
		break;
	case STATEMACHINE_SETTINGS_MODE_ADC1:
		update_settings_adc1();
		break;
	case STATEMACHINE_SETTINGS_MODE_ADC2:
		update_settings_adc2();
		break;
	default:
		break;
	}
}

/* ---------------------------------------------------------------------- */
/* Calibration (Settings > Calibration)                                   */
/* ---------------------------------------------------------------------- */

/* Non-blocking: called every statemachine tick (see display_calibration_update()),
 * so this must never sample/reconfigure the ADC itself -- it just reads
 * whatever calibration_peek_*() finds in adc_data right now. The ADC is
 * pointed at the right mode for ch once, when ch is armed (see
 * display_calibration_enter()/statemachine_step_calibration()), not here. */
static void draw_calibration_live(calibration_channel_t ch, int16_t y) {
	snprintf(text, sizeof(text), "Raw: %-8ld  %ld %s          ", (long) calibration_peek_raw(ch),
			(long) calibration_peek_converted(ch), calibration_channel_unit(ch));
	LCD_PutStr(16, y, text, FONT_SMALL, C_WHITE, C_BLACK);

	if (calibration_has_ext(ch)) {
		snprintf(text, sizeof(text), "Ext: %ld %s          ",
				(long) calibration_peek_ext_converted(ch), calibration_channel_unit(ch));
	} else {
		snprintf(text, sizeof(text), "                        ");
	}
	LCD_PutStr(16, y + 16, text, FONT_SMALL, C_WHITE, C_BLACK);
}

void display_calibration_enter(calibration_channel_t ch, uint8_t ui_state) {
	UG_FillFrame(0, STATUS_H, LCD_WIDTH - 1, FOOTER_Y - 1, C_BLACK);

	switch (ui_state) {
	case 0:
		draw_status_bar("Calibration", C_SILVER);
		draw_footer("ESC: Back", "OK: Select");
		break;
	case 1:
		snprintf(text, sizeof(text), "Calibration: %s", calibration_channel_name(ch));
		draw_status_bar(text, C_SILVER);
		LCD_PutStr(16, STATUS_H + 8, "Set input to 0", FONT_SMALL, C_WHITE,
		C_BLACK);
		LCD_PutStr(16, STATUS_H + 28, "(short/disconnect as needed)",
		FONT_TINY, C_WHITE_63, C_BLACK);
		draw_footer("ESC: Cancel", "OK: Zero");
		break;
	case 2:
		snprintf(text, sizeof(text), "Calibration: %s", calibration_channel_name(ch));
		draw_status_bar(text, C_SILVER);
		LCD_PutStr(16, STATUS_H + 8, "Apply known reference,", FONT_SMALL,
		C_WHITE, C_BLACK);
		LCD_PutStr(16, STATUS_H + 28, "dial in its value:", FONT_SMALL,
		C_WHITE, C_BLACK);
		draw_footer("ESC: Skip", "OK: Set gain");
		break;
	case 3:
		snprintf(text, sizeof(text), "Calibration: %s", calibration_channel_name(ch));
		draw_status_bar(text, C_SILVER);
		LCD_PutStr(16, STATUS_H + 8, "Gain calibration failed!", FONT_SMALL,
		C_RED, C_BLACK);
		LCD_PutStr(16, STATUS_H + 28, "Check reference value & retry",
		FONT_TINY, C_WHITE_63, C_BLACK);
		draw_footer("ESC: Skip", "OK: Retry");
		break;
	default:
		break;
	}
}

void display_calibration_update(calibration_channel_t ch, uint8_t ui_state,
		float reference_value) {
	switch (ui_state) {
	case 0:
		for (uint8_t i = 0; i < CAL_CH_COUNT; i++) {
			int16_t y = STATUS_H + 8 + i * 20;
			uint8_t selected = (i == ch);

			snprintf(text, sizeof(text), " %-6s (%s)                ",
					calibration_channel_name((calibration_channel_t) i),
					calibration_channel_unit((calibration_channel_t) i));
			LCD_PutStr(selected ? 14 : 6, y, text, FONT_TINY, C_WHITE,
			C_BLACK);
			if (selected)
				LCD_PutStr(6, y, "> ", FONT_TINY, C_WHITE, C_BLACK);
		}
		break;
	case 1:
		draw_calibration_live(ch, STATUS_H + 60);
		break;
	case 2:
		snprintf(text, sizeof(text), "Reference: %ld %s        ", (long) reference_value,
				calibration_channel_unit(ch));
		LCD_PutStr(16, STATUS_H + 52, text, FONT_SMALL, C_WHITE, C_BLACK);
		draw_calibration_live(ch, STATUS_H + 76);
		break;
	default:
		break;
	}
}
