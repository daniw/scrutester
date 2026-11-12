/*
 * tools/tetris_sim/stubs/lcd.h
 *
 * Host stand-in for Library/LCD/lcd.h, so the real Core/Src/tetris.c can be
 * compiled and played on a PC. Only the handful of primitives tetris.c
 * actually uses are declared; sim_lcd.c implements them against a 320x240
 * shadow framebuffer. Shadows the firmware header via -Istubs coming first on
 * the include path -- tetris.c itself is compiled unmodified.
 */

#ifndef SIM_LCD_H
#define SIM_LCD_H

#include <stdint.h>
#include "ugui_colors.h" /* the real one, from Library/UGUI */

/* Same geometry the firmware ends up with: ST7789 240x320 at LCD_ROTATION 3. */
#define LCD_WIDTH  320
#define LCD_HEIGHT 240

typedef int16_t UG_S16;
typedef uint16_t UG_COLOR;
typedef unsigned char UG_FONT;

void UG_FillFrame(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c);
void UG_FillScreen(UG_COLOR c);
void UG_DrawFrame(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c);
void LCD_PutStr(uint16_t x, uint16_t y, char *str, UG_FONT *font,
		uint16_t color, uint16_t bgcolor);

uint32_t HAL_GetTick(void);

/* ---- sim-only introspection, used by sim_main.c ---------------------- */

uint16_t sim_lcd_pixel(int16_t x, int16_t y);

/* Fill/text accounting. `fills` counts primitive calls (each one is an
 * address-window setup plus a DMA burst on real hardware, so it is the number
 * that decides whether a frame fits in a 20 ms tick). `out_of_bounds` counts
 * calls that tried to draw outside the panel -- any non-zero value is a
 * coordinate bug in tetris.c. */
uint32_t sim_lcd_fills(void);
uint32_t sim_lcd_out_of_bounds(void);
void sim_lcd_fills_reset(void);

/* Text is not rasterised; the strings are just recorded per position so the
 * terminal view can show the side panel. */
#define SIM_TEXT_MAX 24
uint32_t sim_lcd_text_count(void);
const char *sim_lcd_text_str(uint32_t i);
uint16_t sim_lcd_text_x(uint32_t i);
uint16_t sim_lcd_text_y(uint32_t i);

/* Provided by sim_main.c: advances the fake millisecond clock. */
void sim_time_advance(uint32_t ms);

#endif /* SIM_LCD_H */
