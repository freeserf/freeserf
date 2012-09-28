/* gfx.c */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#include "freeserf_endian.h"
#include "sdl-video.h"
#include "gfx.h"
#include "data.h"
#include "log.h"

/* There are different types of sprites:
   - Non-packed, rectangular sprites: These are simple called sprites here.
   - Transparent sprites, "transp": These are e.g. buldings/serfs.
   The transparent regions are RLE encoded.
   - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
   This is used to either modify the alpha level of another sprite (shadows)
   or mask parts of other sprites completely (mask sprites).
*/

/* These entries follow the 8 byte header of the data file. */
typedef struct {
	uint32_t size;
	uint32_t offset;
} spae_entry_t;


static void *sprites;
static size_t sprites_size;
static unsigned int entry_count;


/* Load data file at path and let the global variable sprites refer to the memory
   with the data file content. */
int
gfx_load_file(const char *path)
{
	int r;

#ifdef HAVE_MMAP
	int fd = open(path, O_RDWR);
	if (fd < 0) return -1;

	struct stat sb;
	r = fstat(fd, &sb);
	if (r < 0) return -1;

	sprites_size = sb.st_size;
	sprites = mmap(NULL, sprites_size, PROT_READ | PROT_WRITE,
		       MAP_PRIVATE, fd, 0);
	if (sprites == MAP_FAILED) {
		int errsv = errno;
		close(fd);
		errno = errsv;
		return -1;
	}

	close(fd);
#else /* ! HAVE_MMAP */
	FILE *f = fopen(path, "rb");
	if (f == NULL) return -1;

	r = fseek(f, 0, SEEK_END);
	if (r < 0) return -1;

	sprites_size = ftell(f);
	if (sprites_size == (size_t)-1) return -1;

	r = fseek(f, 0, SEEK_SET);
	if (r < 0) return -1;

	sprites = malloc(sprites_size);
	if (sprites == NULL) {
		int errsv = errno;
		fclose(f);
		errno = errsv;
		return -1;
	}

	size_t rd = fread(sprites, sprites_size, 1, f);
	if (rd < 1) return -1;

	fclose(f);
#endif

	/* Read the number of entries in the index table.
	   Some entries are undefined (size and offset are zero). */
	entry_count = le32toh(*((uint32_t *)sprites + 1)) + 1;

#if 0
	/* Print list of undefined entries. */
	int run_start = 0;
	spae_entry_t *entries = sprites;
	for (int i = 1; i < entry_count; i++) {
		if (run_start > 0) {
			if (entries[i].offset == 0) continue;
			int length = i - run_start;
			if (length > 1) {
				LOGD("empty: %i-%i", run_start, i-1);
			}
			else LOGD("empty: %i", i-1);
			run_start = 0;
		} else if (entries[i].offset == 0) {
			run_start = i;
		}
	}
#endif

	return 0;
}

/* Free the loaded data file. */
void
gfx_unload()
{
#ifdef HAVE_MMAP
	munmap(sprites, sprites_size);
#else /* ! HAVE_MMAP */
	free(sprites);
#endif
}

/* Return a pointer to the data object at index.
   If size is non-NULL it will be set to the size of the data object.
   (There's no guarantee that size is correct!). */
void *
gfx_get_data_object(int index, size_t *size)
{
	assert(index > 0 && index < entry_count);

	spae_entry_t *entries = sprites;
	uint8_t *bytes = sprites;

	size_t offset = le32toh(entries[index].offset);
	assert(offset != 0);

	if (size) *size = le32toh(entries[index].size);

	return &bytes[offset];
}

/* Draw a character at x, y in the dest frame. */
static void
gfx_draw_char_sprite(int x, int y, unsigned int c, int color, int shadow, frame_t *dest)
{
	static const int sprite_offset_from_ascii[] = {

		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, 43, -1, -1,
		-1, -1, -1, -1, -1, 40, 39, -1,
		29, 30, 31, 32, 33, 34, 35, 36,
		37, 38, 41, -1, -1, -1, -1, 42,
		-1,  0,  1,  2,  3,  4,  5,  6,
		 7,  8,  9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, -1, -1, -1, -1, -1,
		-1,  0,  1,  2,  3,  4,  5,  6,
		 7,  8,  9, 10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22,
		23, 24, 25, -1, -1, -1, -1, -1,

		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,
	};

	int s = sprite_offset_from_ascii[c];
	if (s < 0) return;

	if (shadow) {
		sdl_draw_transp_sprite(gfx_get_data_object(DATA_FONT_SHADOW_BASE + s, NULL),
				       x, y, 0, 0, shadow, dest);
	}
	sdl_draw_transp_sprite(gfx_get_data_object(DATA_FONT_BASE + s, NULL),
			       x, y, 0, 0, color, dest);
}

/* Draw the string str at x, y in the dest frame. */
void
gfx_draw_string(int x, int y, int color, int shadow, frame_t *dest, const char *str)
{
	for (; *str != 0; x += 8) {
		if (/*string_bg*/ 0) {
			gfx_fill_rect(x, y, 8, 8, 0, dest);
		}

		unsigned char c = *str++;
		gfx_draw_char_sprite(x, y, c, color, shadow, dest);
	}
}

/* Draw the number n at x, y in the dest frame. */
void
gfx_draw_number(int x, int y, int color, int shadow, frame_t *dest, int n)
{
	if (n < 0) {
		gfx_draw_char_sprite(x, y, '-', color, shadow, dest);
		x += 8;
		n *= -1;
	}

	if (n == 0) {
		gfx_draw_char_sprite(x, y, '0', color, shadow, dest);
		return;
	}

	int digits = 0;
	for (int i = n; i > 0; i /= 10) digits += 1;

	for (int i = digits-1; i >= 0; i--) {
		gfx_draw_char_sprite(x+8*i, y, '0'+(n % 10), color, shadow, dest);
		n /= 10;
	}
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
gfx_draw_sprite(int x, int y, int sprite, frame_t *dest)
{
	sprite_t *spr = gfx_get_data_object(sprite, NULL);
	if (spr != NULL) sdl_draw_sprite(spr, x, y, dest);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
gfx_draw_transp_sprite(int x, int y, int sprite, frame_t *dest)
{
	sprite_t *spr = gfx_get_data_object(sprite, NULL);
	if (spr != NULL) sdl_draw_transp_sprite(spr, x, y, 0, 0, 0, dest);
}

/* Fill a rectangle with color at x, y in the dest frame. */
void
gfx_fill_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	sdl_fill_rect(x, y, width, height, color, dest);
}

/* Perform various fixups of the data file entries. */
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

/* Select the color palette that is location at the given data file index. */
void
gfx_set_palette(int palette)
{
	uint8_t *pal = gfx_get_data_object(palette, NULL);
	sdl_set_palette(pal);
}

/* Unpack the uncompressed data of a transparent sprite. */
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

/* Unpack the uncompressed data of a bitmap sprite. */
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

/* Unpack the uncompressed data of an overlay sprite. */
void
gfx_unpack_overlay_sprite(void *dest, const void *src, size_t destlen)
{
	gfx_unpack_bitmap_sprite(dest, src, destlen, 0x80);
}

/* Unpack the uncompressed data of a mask sprite. */
void
gfx_unpack_mask_sprite(void *dest, const void *src, size_t destlen)
{
	gfx_unpack_bitmap_sprite(dest, src, destlen, 0xff);
}
