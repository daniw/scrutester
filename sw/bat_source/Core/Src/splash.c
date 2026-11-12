/*
 * splash.c
 *
 * See splash.h. The logo is drawn from a handful of rectangles and four
 * parallelograms instead of a stored bitmap: the source artwork
 * (doc/img/LogoWHE.svg) is made of horizontal/vertical bars plus the four
 * diagonal strokes of the W, all with the same 5 mm width, so its geometry
 * is 32 bytes of coordinates rather than ~23 KB of RGB565 pixels, and it
 * scales to any size by changing LOGO_H_PX alone.
 *
 * Coordinates below are in half-millimetres of the SVG's 60 x 100 mm
 * artwork (x 0..120, y 0..200), so the 12.5 mm / 37.5 mm etc. positions of
 * the source stay integral.
 */

#include "splash.h"
#include "lcd.h"
#include "ugui_fonts.h"
#include <string.h>

#define SPLASH_LOGO_COLOR   C_DODGER_BLUE
#define SPLASH_TITLE_COLOR  C_WHITE
#define SPLASH_CREDIT_COLOR C_WHITE_63

#define SPLASH_TITLE   "ScruTester"
#define SPLASH_CREDIT  "by daniw & ahorat"
#define SPLASH_TITLE_FONT   FONT_16X26
#define SPLASH_CREDIT_FONT  FONT_12X16
#define SPLASH_GAP_PX  10   /* title -> logo and logo -> credit */

#define LOGO_H_U   200      /* logo box height in half-mm */
#define LOGO_H_PX  140      /* rendered logo height; width follows (0.6x) */
#define STROKE_W_U 10       /* W stroke width, measured horizontally */
#define STROKE_H_U 100      /* W stroke height (top of the W to the H/E bars) */

/* half-mm -> pixels, rounded to nearest */
#define U2PX(v) (((int32_t) (v) * LOGO_H_PX + LOGO_H_U / 2) / LOGO_H_U)

/* x0, y0, x1, y1 (end exclusive): H legs and crossbar, E bars. The H's right
 * leg doubles as the E's stem. */
static const uint8_t logo_rects[][4] = {
	{ 20, 100,  30, 200 },  /* H left leg */
	{ 60, 100,  70, 200 },  /* H right leg / E stem */
	{ 30, 145,  60, 155 },  /* H crossbar */
	{ 88,   0, 120,  10 },  /* E top bar */
	{ 70,  95, 120, 105 },  /* E middle bar */
	{ 70, 190, 120, 200 },  /* E bottom bar */
};

/* Left edge x at the top and at the bottom of each W stroke (they all run
 * from y = 0 to STROKE_H_U). */
static const uint8_t logo_strokes[][2] = {
	{  0, 20 },
	{ 40, 20 },
	{ 40, 60 },
	{ 80, 60 },
};

/* Fills one diagonal stroke row by row. The edge only moves one pixel every
 * few rows (slope 0.2), so runs of identical rows are merged into a single
 * fill -- about 14 fills per stroke instead of one per row, which matters
 * because every fill costs an address-window setup on the SPI bus. */
static void draw_stroke(int16_t ox, int16_t oy, uint8_t x_top, uint8_t x_bottom) {
	const int32_t rows = U2PX(STROKE_H_U);
	const int32_t den = 2 * rows * LOGO_H_U;
	int32_t run_start = 0, run_l = 0, run_r = 0;

	for (int32_t y = 0; y <= rows; y++) {
		int32_t l = 0, r = 0;
		if (y < rows) {
			/* Left edge at the middle of pixel row y, in half-mm times 2*rows. */
			int32_t left = (int32_t) x_top * 2 * rows
					+ ((int32_t) x_bottom - x_top) * (2 * y + 1);
			l = (left * LOGO_H_PX + den / 2) / den;
			r = ((left + STROKE_W_U * 2 * rows) * LOGO_H_PX + den / 2) / den - 1;
		}
		if (y > 0 && (y == rows || l != run_l || r != run_r)) {
			UG_FillFrame(ox + run_l, oy + run_start, ox + run_r, oy + y - 1,
					SPLASH_LOGO_COLOR);
			run_start = y;
		}
		run_l = l;
		run_r = r;
	}
}

static void draw_logo(int16_t ox, int16_t oy) {
	for (uint8_t i = 0; i < sizeof(logo_rects) / sizeof(logo_rects[0]); i++) {
		const uint8_t *r = logo_rects[i];
		UG_FillFrame(ox + U2PX(r[0]), oy + U2PX(r[1]),
				ox + U2PX(r[2]) - 1, oy + U2PX(r[3]) - 1, SPLASH_LOGO_COLOR);
	}
	for (uint8_t i = 0; i < sizeof(logo_strokes) / sizeof(logo_strokes[0]); i++)
		draw_stroke(ox, oy, logo_strokes[i][0], logo_strokes[i][1]);
}

static void put_centered(int16_t y, const char *str, UG_FONT *font, UG_COLOR color) {
	int16_t w = (int16_t) (strlen(str) * UG_GetFontWidth(font));
	LCD_PutStr((LCD_WIDTH - w) / 2, y, (char*) str, font, color, C_BLACK);
}

void splash_show(void) {
	const int16_t title_h = UG_GetFontHeight(SPLASH_TITLE_FONT);
	const int16_t credit_h = UG_GetFontHeight(SPLASH_CREDIT_FONT);
	const int16_t logo_w = U2PX(120);
	const int16_t total_h = title_h + SPLASH_GAP_PX + LOGO_H_PX
			+ SPLASH_GAP_PX + credit_h;
	int16_t y = (LCD_HEIGHT - total_h) / 2;

	// Also removes what LCD_init() leaves in the top-left corner.
	UG_FillScreen(C_BLACK);

	put_centered(y, SPLASH_TITLE, SPLASH_TITLE_FONT, SPLASH_TITLE_COLOR);
	y += title_h + SPLASH_GAP_PX;
	draw_logo((LCD_WIDTH - logo_w) / 2, y);
	y += LOGO_H_PX + SPLASH_GAP_PX;
	put_centered(y, SPLASH_CREDIT, SPLASH_CREDIT_FONT, SPLASH_CREDIT_COLOR);
}
