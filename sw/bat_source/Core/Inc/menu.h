/*
 * menu.h
 *
 * Single source of truth for the menu tree: which modes appear in the home
 * carousel, in what order, their display names, accent colors and icons.
 * Replaces the old fragile "(index +/- N) % 6" arithmetic in statemachine.c
 * that silently left STATEMACHINE_MODE_SETTINGS unreachable from the menu.
 */

#ifndef INC_MENU_H_
#define INC_MENU_H_

#include "stdint.h"
#include "statemachine.h"
#include "ugui.h"

/* Total number of entries in menu_order[], including ones (like CHARGE)
 * that aren't shown in the home carousel but still need their
 * accent/icon looked up by mode elsewhere. */
#ifdef EMI_MODE_ENABLED
#define MENU_TOTAL_LENGTH 10
#else
#define MENU_TOTAL_LENGTH 9
#endif

/* Number of modes shown in the home carousel - the first MENU_ORDER_LENGTH
 * slots of menu_order[]. Entries beyond this (currently just CHARGE, which
 * is auto-entered rather than user-selected) are unreachable via
 * menu_entry_at()/encoder navigation but still found by
 * menu_entry_for_mode(). */
#ifdef EMI_MODE_ENABLED
#define MENU_ORDER_LENGTH 9
#else
#define MENU_ORDER_LENGTH 8
#endif

typedef void (*menu_icon_draw_fn)(int16_t cx, int16_t cy, int16_t r, UG_COLOR color);

typedef struct {
	statemachine_modes_t mode;
	UG_COLOR accent;
	menu_icon_draw_fn draw_icon;
} menu_entry_t;

/* Carousel order: 60V, 10A, Resistance 4W/2W, Isolation, Voltmeter, Ampmeter, Settings.
 * Charge is appended after (see MENU_TOTAL_LENGTH) - it's auto-entered, not user-selectable. */
extern const menu_entry_t menu_order[MENU_TOTAL_LENGTH];

/* Returns the carousel entry at [0, MENU_ORDER_LENGTH). Index is wrapped. */
const menu_entry_t *menu_entry_at(uint8_t index);

/* Returns the menu_order[] entry for a given mode (including non-carousel entries like
 * CHARGE), or NULL if the mode has none. */
const menu_entry_t *menu_entry_for_mode(statemachine_modes_t mode);

/* Display name for any statemachine_modes_t value (sized for all 11 enum values --
 * the old DISPLAY_MODE_STRINGS[7] array this replaces was out-of-bounds for SETTINGS). */
const char *menu_name_for_mode(statemachine_modes_t mode);

#endif /* INC_MENU_H_ */
