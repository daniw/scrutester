/*
 * menu.c
 *
 * Menu tree definition: carousel order, names, accent colors and procedurally
 * drawn icons (no bitmap art pipeline exists, so icons are simple vector
 * glyphs built from UGUI primitives instead of pre-rendered images).
 */

#include "menu.h"
#include "lcd.h"
#include "ugui_fonts.h"
#include <math.h>

/* ---------------------------------------------------------------------- */
/* Icon glyphs                                                            */
/* ---------------------------------------------------------------------- */

static void icon_draw_letter(int16_t cx, int16_t cy, int16_t r, UG_COLOR color,
		const char *letter) {
	UG_FONT *font = (r >= 24) ? FONT_16X26 : FONT_12X16;
	int16_t fw = UG_GetFontWidth(font);
	int16_t fh = UG_GetFontHeight(font);

	UG_DrawCircle(cx, cy, r, color);
	LCD_PutStr(cx - fw / 2, cy - fh / 2, (char*) letter, font, color, C_BLACK);
}

static void icon_60v(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	icon_draw_letter(cx, cy, r, color, "V");
}

static void icon_10a(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	icon_draw_letter(cx, cy, r, color, "A");
}

static void icon_voltmeter(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	icon_draw_letter(cx, cy, r, color, "V");
}

static void icon_ampmeter(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	icon_draw_letter(cx, cy, r, color, "A");
}

static void icon_resistance_zigzag(int16_t cx, int16_t cy, int16_t r,
		UG_COLOR color) {
	static const int8_t pattern[7] = { 0, -1, 1, -1, 1, -1, 0 };
	int16_t amp = r * 2 / 5;
	int16_t span = r * 3 / 2;
	int16_t x0 = cx - span / 2;
	int16_t dx = span / 6;
	int16_t px = x0, py = cy;

	for (int i = 1; i <= 6; i++) {
		int16_t nx = x0 + dx * i;
		int16_t ny = cy + pattern[i] * amp;
		UG_DrawLine(px, py, nx, ny, color);
		px = nx;
		py = ny;
	}
}

static void icon_resistance(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	UG_DrawCircle(cx, cy, r, color);
	icon_resistance_zigzag(cx, cy, r, color);
}

static void icon_isometer(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	int16_t h = r * 3 / 2;
	int16_t top_y = cy - h / 2, bot_y = cy + h / 2;

	UG_DrawCircle(cx, cy, r, color);
	UG_DrawLine(cx + r / 4, top_y, cx - r / 4, cy, color);
	UG_DrawLine(cx - r / 4, cy, cx + r / 6, cy, color);
	UG_DrawLine(cx + r / 6, cy, cx - r / 4, bot_y, color);
}

static void icon_charge(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	int16_t bw = r * 3 / 2, bh = r;
	int16_t x0 = cx - bw / 2, x1 = cx + bw / 2;
	int16_t y0 = cy - bh / 2, y1 = cy + bh / 2;

	UG_DrawCircle(cx, cy, r, color);
	UG_DrawFrame(x0, y0, x1, y1, color);
	UG_FillFrame(x1, cy - bh / 6, x1 + r / 6, cy + bh / 6, color);
	UG_DrawLine(cx, y0 + 3, cx, y1 - 3, color);
	UG_DrawLine(x0 + 3, cy, x1 - 3, cy, color);
}

#ifdef EMI_MODE_ENABLED
static void icon_emi(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	icon_draw_letter(cx, cy, r, color, "E");
}
#endif

static void icon_settings(int16_t cx, int16_t cy, int16_t r, UG_COLOR color) {
	int16_t r_in = r * 3 / 5;

	UG_DrawCircle(cx, cy, r, color);
	UG_DrawCircle(cx, cy, r_in, color);
	UG_FillCircle(cx, cy, r / 4, color);
	for (int i = 0; i < 8; i++) {
		float ang = (float) i * (3.14159265f / 4.0f);
		int16_t x0 = cx + (int16_t) (r_in * cosf(ang));
		int16_t y0 = cy + (int16_t) (r_in * sinf(ang));
		int16_t x1 = cx + (int16_t) (r * cosf(ang));
		int16_t y1 = cy + (int16_t) (r * sinf(ang));
		UG_DrawLine(x0, y0, x1, y1, color);
	}
}

/* ---------------------------------------------------------------------- */
/* Menu tree                                                               */
/* ---------------------------------------------------------------------- */

const menu_entry_t menu_order[MENU_TOTAL_LENGTH] = {
	{ STATEMACHINE_MODE_60V_OUT,        C_DODGER_BLUE,     icon_60v },
	{ STATEMACHINE_MODE_10A_OUT,        C_ORANGE,          icon_10a },
	{ STATEMACHINE_MODE_RESISTANCE_1A,  C_TEAL,            icon_resistance },
	{ STATEMACHINE_MODE_RESISTANCE_1mA, C_DARK_CYAN,       icon_resistance },
	{ STATEMACHINE_MODE_ISOMETER,       C_RED,             icon_isometer },
	{ STATEMACHINE_MODE_VOLTMETER,      C_SKY_BLUE,        icon_voltmeter },
	{ STATEMACHINE_MODE_AMPMETER,       C_GOLD,            icon_ampmeter },
#ifdef EMI_MODE_ENABLED
	{ STATEMACHINE_MODE_EMI,            C_VIOLET,          icon_emi },
#endif
	{ STATEMACHINE_MODE_SETTINGS,       C_SILVER,          icon_settings },
	{ STATEMACHINE_MODE_CHARGE,         C_GREEN,           icon_charge },
};

static const char *const MODE_NAMES[STATEMACHINE_MODE_RESERVED + 1] = {
	[STATEMACHINE_IDLE] = "BatSource",
	[STATEMACHINE_MODE_60V_OUT] = "Voltage Source",
	[STATEMACHINE_MODE_10A_OUT] = "Current Source",
	[STATEMACHINE_MODE_RESISTANCE_1A] = "Milliohmmeter",
	[STATEMACHINE_MODE_RESISTANCE_1mA] = "Ohmmeter",
	[STATEMACHINE_MODE_ISOMETER] = "Isolation Test",
	[STATEMACHINE_MODE_VOLTMETER] = "Voltmeter",
	[STATEMACHINE_MODE_SETTINGS] = "Settings",
	[STATEMACHINE_MODE_SHUTDOWN] = "Shutdown",
	[STATEMACHINE_MODE_CHARGE] = "Charge",
	[STATEMACHINE_MODE_AMPMETER] = "Ampmeter",
	[STATEMACHINE_MODE_EMI] = "EMI Test",
	[STATEMACHINE_MODE_RESERVED] = "Reserved",
};

const menu_entry_t *menu_entry_at(uint8_t index) {
	return &menu_order[index % MENU_ORDER_LENGTH];
}

const menu_entry_t *menu_entry_for_mode(statemachine_modes_t mode) {
	for (int i = 0; i < MENU_TOTAL_LENGTH; i++) {
		if (menu_order[i].mode == mode)
			return &menu_order[i];
	}
	return 0;
}

const char *menu_name_for_mode(statemachine_modes_t mode) {
	if (mode > STATEMACHINE_MODE_RESERVED)
		return "?";
	return MODE_NAMES[mode];
}
