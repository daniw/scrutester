/*
 * tools/tetris_sim/sim_lcd.c
 *
 * The drawing primitives tetris.c calls, implemented against a plain 320x240
 * shadow framebuffer instead of an SPI panel. Two things make this more than
 * a no-op stub:
 *
 *  - every fill is clipped and counted, so sim_main.c can report how many
 *    primitive calls a frame costs (that is the number that has to stay small
 *    on real hardware) and can flag any draw that lands outside the panel;
 *  - the framebuffer is sampled back per board cell, so the terminal view is
 *    generated from the pixels tetris.c actually wrote, which tests its
 *    layout arithmetic rather than trusting it.
 */

#include "lcd.h"
#include <stdio.h>
#include <string.h>

UG_FONT FONT_8X12[] = { 8, 12 };
UG_FONT FONT_12X16[] = { 12, 16 };

static uint16_t fb[LCD_WIDTH * LCD_HEIGHT];
static uint32_t fill_count;
static uint32_t oob_count;

static struct {
	uint16_t x, y;
	char str[40];
	uint8_t used;
} texts[SIM_TEXT_MAX];

static uint32_t text_count;

static void fill_raw(int16_t x1, int16_t y1, int16_t x2, int16_t y2,
		uint16_t c) {
	if (x1 > x2 || y1 > y2 || x2 < 0 || y2 < 0 || x1 >= LCD_WIDTH
			|| y1 >= LCD_HEIGHT) {
		oob_count++;
		return;
	}
	if (x1 < 0 || y1 < 0 || x2 >= LCD_WIDTH || y2 >= LCD_HEIGHT)
		oob_count++; /* partially off-panel: still a layout bug */

	for (int16_t y = (y1 < 0 ? 0 : y1); y <= y2 && y < LCD_HEIGHT; y++)
		for (int16_t x = (x1 < 0 ? 0 : x1); x <= x2 && x < LCD_WIDTH; x++)
			fb[y * LCD_WIDTH + x] = c;
}

void UG_FillFrame(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c) {
	fill_count++;
	fill_raw(x1, y1, x2, y2, c);
}

void UG_FillScreen(UG_COLOR c) {
	fill_count++;
	fill_raw(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1, c);
}

void UG_DrawFrame(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c) {
	fill_count += 4;
	fill_raw(x1, y1, x2, y1, c);
	fill_raw(x1, y2, x2, y2, c);
	fill_raw(x1, y1, x1, y2, c);
	fill_raw(x2, y1, x2, y2, c);
}

/* Records the string for the terminal view, keyed by position so a repainted
 * field replaces the old value the way it does on the panel. */
void LCD_PutStr(uint16_t x, uint16_t y, char *str, UG_FONT *font,
		uint16_t color, uint16_t bgcolor) {
	(void) font;
	(void) color;
	(void) bgcolor;

	for (uint32_t i = 0; i < text_count; i++) {
		if (texts[i].x == x && texts[i].y == y) {
			snprintf(texts[i].str, sizeof(texts[i].str), "%s", str);
			return;
		}
	}
	if (text_count >= SIM_TEXT_MAX)
		return;
	texts[text_count].x = x;
	texts[text_count].y = y;
	snprintf(texts[text_count].str, sizeof(texts[text_count].str), "%s", str);
	text_count++;
}

uint16_t sim_lcd_pixel(int16_t x, int16_t y) {
	if (x < 0 || y < 0 || x >= LCD_WIDTH || y >= LCD_HEIGHT)
		return 0;
	return fb[y * LCD_WIDTH + x];
}

uint32_t sim_lcd_fills(void) {
	return fill_count;
}

uint32_t sim_lcd_out_of_bounds(void) {
	return oob_count;
}

void sim_lcd_fills_reset(void) {
	fill_count = 0;
}

uint32_t sim_lcd_text_count(void) {
	return text_count;
}

const char *sim_lcd_text_str(uint32_t i) {
	return (i < text_count) ? texts[i].str : "";
}

uint16_t sim_lcd_text_x(uint32_t i) {
	return (i < text_count) ? texts[i].x : 0;
}

uint16_t sim_lcd_text_y(uint32_t i) {
	return (i < text_count) ? texts[i].y : 0;
}
