
#include <stdlib.h>
#include <string.h>
#include <stdint.h>


typedef struct {
	uint32_t size;
	uint32_t offset;
} spae_entry_t;


void
gfx_data_fixup()
{
	spae_entry_t *entries = sprites;

	/* Fill out some undefined spaces in the index from other
	   places in the data file index. */

	for (int i = 0; i < 48; i++) {
		for (int j = 1; j < 6; j++) {
			entries[3450+6*i+j].size = entries[3450+6*i].size;
			entries[3450+6*i+j].offset = entries[3450+6*i].offset;
		}
	}

	for (int i = 0; i < 3; i++) {
		entries[3765+i].size = entries[3762+i].size;
		entries[3765+i].offset = entries[3762+i].offset;
	}

	for (int i = 0; i < 6; i++) {
		entries[1363+i].size = entries[1352].size;
		entries[1363+i].offset = entries[1352].offset;
	}

	for (int i = 0; i < 6; i++) {
		entries[1613+i].size = entries[1602].size;
		entries[1613+i].offset = entries[1602].offset;
	}
}

void
gfx_unpack_transparent_sprite(void *dest, const void *src, size_t destlen, int offset)
{
	const uint8_t *bsrc = (uint8_t *)src;
	uint8_t *bdest = (uint8_t *)dest;

	int i = 0;
	int j = 0;
	while (j < destlen) {
		j += bsrc[i];
		int n = bsrc[i+1];

		if (n) {
			memcpy(&bdest[j], &bsrc[i+2], n);
			if (offset) {
				for (int k = 0; k < n; k++) {
					bdest[j+k] += offset;
				}
			}
		}
		i += n + 2;
		j += n;
	}
}

static void
gfx_unpack_bitmap_sprite(void *dest, const void *src, size_t destlen, int value)
{
	const uint8_t *bsrc = (uint8_t *)src;
	uint8_t *bdest = (uint8_t *)dest;

	int i = 0;
	int j = 0;
	while (j < destlen) {
		j += bsrc[i];
		int n = bsrc[i+1];

		for (int k = 0; k < n; k++) bdest[j+k] = value;

		i += 2;
		j += n;
	}
}

void
gfx_unpack_overlay_sprite(void *dest, const void *src, size_t destlen)
{
	gfx_unpack_bitmap_sprite(dest, src, destlen, 0x80);
}

void
gfx_unpack_mask_sprite(void *dest, const void *src, size_t destlen)
{
	gfx_unpack_bitmap_sprite(dest, src, destlen, 0xff);
}
