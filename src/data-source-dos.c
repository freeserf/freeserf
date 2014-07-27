/*
 * data-source-dos.c - DOS game resources file functions
 *
 * Copyright (C) 2014  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This file is part of freeserf.
 *
 * freeserf is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * freeserf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with freeserf.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "data.h"
#include "freeserf_endian.h"
#include "log.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdlib.h>

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

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
static uint entry_count;

static void data_fixup();

/* Load data file at path and let the global variable sprites refer to the memory
 with the data file content. */
int
data_load(const char *path)
{
	int r;

#ifdef HAVE_MMAP
	int fd = open(path, O_RDONLY);
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

	/* Check that data file is decompressed. */
	if (!memcmp(sprites, "TPWM", 4)) {
		LOGE("gfx", "Data file is compressed! Please run the installer"
		     " from the original game to decompress the data file.");
		return -1;
	}

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
				LOGD("gfx", "empty: %i-%i", run_start, i-1);
			}
			else LOGD("gfx", "empty: %i", i-1);
			run_start = 0;
		} else if (entries[i].offset == 0) {
			run_start = i;
		}
	}
#endif

	data_fixup();

	return 0;
}

/* Free the loaded data file. */
void
data_unload()
{
#ifdef HAVE_MMAP
	munmap(sprites, sprites_size);
#else /* ! HAVE_MMAP */
	free(sprites);
#endif
	sprites = NULL;
}

/* Return a pointer to the data object at index.
 If size is non-NULL it will be set to the size of the data object.
 (There's no guarantee that size is correct!). */
void *
data_get_object(uint index, size_t *size)
{
	assert(index > 0 && index < entry_count);

	spae_entry_t *entries = (spae_entry_t*)sprites;
	uint8_t *bytes = (uint8_t*)sprites;

	size_t offset = le32toh(entries[index].offset);
	assert(offset != 0);

	if (size) *size = le32toh(entries[index].size);

	return &bytes[offset];
}

/* Perform various fixups of the data file entries. */
static void
data_fixup()
{
	spae_entry_t *entries = (spae_entry_t*)sprites;

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

/* Unpack the uncompressed data of a transparent sprite. */
void
data_unpack_transparent_sprite(void *dest, const void *src, size_t destlen, int offset)
{
	const uint8_t *bsrc = (uint8_t *)src;
	uint8_t *bdest = (uint8_t *)dest;

	int i = 0;
	size_t j = 0;
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
data_unpack_bitmap_sprite(void *dest, const void *src, size_t destlen, int value)
{
	const uint8_t *bsrc = (uint8_t *)src;
	uint8_t *bdest = (uint8_t *)dest;

	int i = 0;
	size_t j = 0;
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
data_unpack_overlay_sprite(void *dest, const void *src, size_t destlen)
{
	data_unpack_bitmap_sprite(dest, src, destlen, 0x80);
}

/* Unpack the uncompressed data of a mask sprite. */
void
data_unpack_mask_sprite(void *dest, const void *src, size_t destlen)
{
	data_unpack_bitmap_sprite(dest, src, destlen, 0xff);
}
