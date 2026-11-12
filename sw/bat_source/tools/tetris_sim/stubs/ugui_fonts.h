/*
 * tools/tetris_sim/stubs/ugui_fonts.h
 *
 * Host stand-in for Library/UGUI/ugui_fonts.h: the two fonts tetris.c uses,
 * reduced to their {width, height} header bytes (which is all µGUI's
 * UG_GetFontWidth/Height macros read). Glyph data is irrelevant here -- the
 * sim records strings instead of rasterising them.
 */

#ifndef SIM_UGUI_FONTS_H
#define SIM_UGUI_FONTS_H

#include "lcd.h"

extern UG_FONT FONT_8X12[];
extern UG_FONT FONT_12X16[];

#define UG_GetFontWidth(f)  (*((f) + 0))
#define UG_GetFontHeight(f) (*((f) + 1))

#endif /* SIM_UGUI_FONTS_H */
