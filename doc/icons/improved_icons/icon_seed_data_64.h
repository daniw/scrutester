/*
 * icon_seed_data_64.h
 *
 * See icon_seed_data_64.c. Standalone reference header (does not depend on
 * the firmware's icon_store.h) so this can be reviewed/tested outside the
 * sw/bat_source build; adapt when/if wiring into the firmware.
 */

#ifndef ICON_SEED_DATA_64_H_
#define ICON_SEED_DATA_64_H_

#include "stdint.h"

#define ICON_STORE_ICON_COUNT_64  9
#define ICON_STORE_ICON_WIDTH_64  64
#define ICON_STORE_ICON_HEIGHT_64 64

typedef struct {
	uint16_t width;
	uint16_t height;
	const uint16_t *pixels; /* RGB565, row-major */
} icon_seed_t;

/* Order: 60v_source, 10a_source, milliohmmeter, ohmmeter, isolation, voltmeter, ampmeter, charge, settings */
extern const icon_seed_t icon_seeds_64[ICON_STORE_ICON_COUNT_64];

#endif /* ICON_SEED_DATA_64_H_ */
