/*
 * icon_store.c
 *
 * See icon_store.h. On-flash layout: a header page at ICON_STORE_BASE_ADDR
 * (magic + version + per-icon {width, height, size, crc32} index, mirroring
 * config_store.c's versioned+CRC pattern), followed by one erase-block per
 * icon's pixel data (block granularity is required since w25n01gv_write()
 * refuses to write over non-erased pages, and erase is block-only).
 */

#include "icon_store.h"
#include "icon_seed_data.h"
#include "w25n01gv.h"
#include <string.h>
#include <stddef.h>
#include "stdio.h"

extern w25n01gv_handle flash;

#define ICON_STORE_BASE_ADDR 0
#define ICON_STORE_MAGIC     0x314E4349u /* "ICN1" */
#define ICON_STORE_VERSION   1
#define ICON_SLOT_SIZE       (W25N01GV_PAGES_PER_BLOCK * W25N01GV_PAGE_SIZE) /* 1 erase block per icon */

typedef struct {
	uint16_t width;
	uint16_t height;
	uint32_t size;
	uint32_t crc32;
} icon_index_entry_t;

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t icon_count;
	icon_index_entry_t icons[ICON_STORE_ICON_COUNT];
	uint32_t header_crc32;
} icon_store_header_t;

static icon_store_header_t header;
static uint8_t store_available = 0;

/* Same polynomial/algorithm as config_store.c's config_store_crc32b (kept as
 * a separate static copy here rather than exporting that one, to keep the
 * two stores independent). */
static uint32_t icon_store_crc32(const uint8_t *data, uint32_t length) {
	uint32_t crc = 0xFFFFFFFF;

	for (uint32_t i = 0; i < length; i++) {
		crc ^= data[i];
		for (int8_t j = 7; j >= 0; j--) {
			uint32_t mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
	}
	return ~crc;
}

static uint32_t icon_slot_addr(uint8_t icon_id) {
	/* Block 0 is the header; icon i's pixel data lives in block (i+1). */
	return ICON_STORE_BASE_ADDR + (uint32_t) (icon_id + 1) * ICON_SLOT_SIZE;
}

uint8_t icon_store_available(void) {
	return store_available;
}

void icon_store_init(void) {
	store_available = 0;

	if (w25n01gv_read(&flash, (uint8_t*) &header, ICON_STORE_BASE_ADDR,
			sizeof(header)) != W25N01GV_OK)
		return;
	if (header.magic != ICON_STORE_MAGIC || header.version != ICON_STORE_VERSION)
		return;
	if (header.icon_count != ICON_STORE_ICON_COUNT)
		return;
	if (icon_store_crc32((uint8_t*) &header, offsetof(icon_store_header_t, header_crc32))
			!= header.header_crc32)
		return;

	store_available = 1;
}

UG_BMP icon_store_load(uint8_t icon_id, uint16_t *pixel_buf) {
	UG_BMP invalid = { .p = 0, .width = 0, .height = 0, .bpp = 0, .colors = 0 };

	if (!store_available || icon_id >= header.icon_count)
		return invalid;

	const icon_index_entry_t *entry = &header.icons[icon_id];
	uint32_t addr = icon_slot_addr(icon_id);

	if (w25n01gv_read(&flash, (uint8_t*) pixel_buf, addr, entry->size) != W25N01GV_OK)
		return invalid;
	if (icon_store_crc32((uint8_t*) pixel_buf, entry->size) != entry->crc32)
		return invalid;

	UG_BMP bmp = { .p = pixel_buf, .width = entry->width, .height = entry->height,
			.bpp = BMP_BPP_16, .colors = BMP_RGB565 };
	return bmp;
}

int8_t icon_store_flash_seed(void) {
#ifdef ICON_INCLUDE_SEED
	memset(&header, 0, sizeof(header));
	header.magic = ICON_STORE_MAGIC;
	header.version = ICON_STORE_VERSION;
	header.icon_count = ICON_STORE_ICON_COUNT;

	for (uint8_t i = 0; i < ICON_STORE_ICON_COUNT; i++) {
		const icon_seed_t *seed = &icon_seeds[i];
		uint32_t size = (uint32_t) seed->width * seed->height * 2;
		uint32_t addr = icon_slot_addr(i);

		if (size > ICON_SLOT_SIZE)
			return -1;
		if (w25n01gv_erase(&flash, addr, addr + ICON_SLOT_SIZE - 1) != W25N01GV_OK)
			return -1;
		if (w25n01gv_write(&flash, (uint8_t*) seed->pixels, addr, size) != W25N01GV_OK)
			return -1;

		header.icons[i].width = seed->width;
		header.icons[i].height = seed->height;
		header.icons[i].size = size;
		header.icons[i].crc32 = icon_store_crc32((const uint8_t*) seed->pixels, size);
	}

	header.header_crc32 = icon_store_crc32((uint8_t*) &header,
			offsetof(icon_store_header_t, header_crc32));

	if (w25n01gv_erase(&flash, ICON_STORE_BASE_ADDR,
			ICON_STORE_BASE_ADDR + ICON_SLOT_SIZE - 1) != W25N01GV_OK)
		return -1;
	if (w25n01gv_write(&flash, (uint8_t*) &header, ICON_STORE_BASE_ADDR, sizeof(header))
			!= W25N01GV_OK)
		return -1;

	store_available = 1;
	return 0;
#else
	printf("flashIcon Failed: No Icon seed present in software\r\n");
	return -1;
#endif
}
