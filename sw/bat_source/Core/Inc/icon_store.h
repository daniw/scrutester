/*
 * icon_store.h
 *
 * Stores the menu's color icon bitmaps on the external W25N01GV QSPI NAND
 * flash (previously unused) instead of compiling them into internal flash,
 * and loads them on demand for the carousel. See icon_seed_data.h for the
 * compiled-in seed artwork used to (re)provision the flash-resident store.
 */

#ifndef INC_ICON_STORE_H_
#define INC_ICON_STORE_H_

#include "stdint.h"
#include "ugui.h"
#include "statemachine.h" // for EMI_MODE_ENABLED, see below

#define ICON_SIZE_64 64
#define ICON_SIZE_96 96

/* Compile mask: pick which seeded icon resolution icon_seed_data.c builds
 * and the store/carousel use. Override with -DICON_STORE_ICON_SIZE=... on
 * the command line if desired; defaults to 64x64 otherwise. */
#ifndef ICON_STORE_ICON_SIZE
#define ICON_STORE_ICON_SIZE ICON_SIZE_96
#endif

/* One icon per menu_order[] entry, in the same order (display.c uses the
 * carousel index as icon_id), so this has to track MENU_TOTAL_LENGTH,
 * including the EMI icon only when that mode is built in. The store header's
 * icon_count check rejects a store seeded by the other variant -- the
 * carousel then falls back to the vector icons until `flashIcons` is rerun. */
#ifdef EMI_MODE_ENABLED
#define ICON_STORE_ICON_COUNT  10
#else
#define ICON_STORE_ICON_COUNT  9
#endif
#define ICON_STORE_ICON_WIDTH  ICON_STORE_ICON_SIZE
#define ICON_STORE_ICON_HEIGHT ICON_STORE_ICON_SIZE

/* Reads and validates the store's header from flash. Call once, after
 * w25n01gv_init(). Safe to call even if the flash isn't present/seeded yet --
 * icon_store_available() will just report false. */
void icon_store_init(void);

/* True once a valid, seeded icon store has been found on flash. */
uint8_t icon_store_available(void);

/* Loads icon `icon_id` (0..ICON_STORE_ICON_COUNT-1) into the caller-provided
 * `pixel_buf` (must be large enough for that icon's width*height pixels) and
 * returns a UG_BMP describing it, ready for LCD_DrawImage()/UG_DrawBMP(). On
 * any failure (store unavailable, bad id, read/CRC error) returns a UG_BMP
 * with width == 0 -- callers should check that before drawing.
 *
 * Blocking call (the NAND driver has no async/DMA path); this is fine since
 * icons only reload when the carousel selection changes, not every redraw
 * tick. */
UG_BMP icon_store_load(uint8_t icon_id, uint16_t *pixel_buf);

/* Erases and (re)writes the whole icon store on flash from the compiled-in
 * seed data in icon_seed_data.h. Used by the CLI's `flashIcons` command.
 * Returns 0 on success, -1 on any flash error. */
int8_t icon_store_flash_seed(void);

#endif /* INC_ICON_STORE_H_ */
