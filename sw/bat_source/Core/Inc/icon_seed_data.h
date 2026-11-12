/*
 * icon_seed_data.h
 *
 * Compiled-in RGB565 icon artwork (generated offline from simple pictograms,
 * same authoring format as Library/LCD/images.c's "fry" sample) used only to
 * (re)provision the QSPI-flash-resident icon store -- see icon_store.h.
 */

#ifndef INC_ICON_SEED_DATA_H_
#define INC_ICON_SEED_DATA_H_

#include "stdint.h"
#include "icon_store.h"
//#define ICON_INCLUDE_SEED

typedef struct {
	uint16_t width;
	uint16_t height;
	const uint16_t *pixels; /* RGB565, row-major */
} icon_seed_t;

extern const icon_seed_t icon_seeds[ICON_STORE_ICON_COUNT];

#endif /* INC_ICON_SEED_DATA_H_ */
