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
#include "tpwm.h"

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
static int mapped = 0;
static size_t sprites_size;
static uint entry_count;

static void data_fixup();

#define MAX_DATA_PATH      1024

int
data_check(const char *path, char **load_path)
{
	const char *default_data_file[] = {
		"SPAE.PA", /* English */
		"SPAF.PA", /* French */
		"SPAD.PA", /* German */
		"SPAU.PA", /* Engish (US) */
		NULL
	};

	char cp[MAX_DATA_PATH];

	for (const char **df = default_data_file; *df != NULL; df++) {
		snprintf(cp, MAX_DATA_PATH, "%s/%s", path, *df);
		LOGI("data", "Looking for game data in '%s'...", cp);
		int r = data_check_file(cp);
		if (r >= 0) {
			*load_path = (char*)malloc(strlen(cp)+1);
			strcpy(*load_path, cp);
			return 0;
		}
	}

	return -1;
}

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

	mapped = 1;
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
	if (tpwm_is_compressed(sprites, sprites_size) >= 0) {
		LOGV("data", "Data file is compressed");
		void *uncompressed = NULL;
		unsigned int uncmpsd_size = 0;
		char *error = NULL;
		if (tpwm_uncompress(sprites, sprites_size,
		                    &uncompressed, &uncmpsd_size,
		                    &error) < 0) {
			LOGE("tpwm", error);
			LOGE("data", "Data file is broken!");
			return -1;
		}
#ifdef HAVE_MMAP
		if (mapped) {
			munmap(sprites, sprites_size);
			mapped = 0;
		} else {
			free(sprites);
		}
#else /* ! HAVE_MMAP */
		free(sprites);
#endif
		sprites = uncompressed;
		sprites_size = uncmpsd_size;
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
data_deinit()
{
#ifdef HAVE_MMAP
	if (mapped) {
		munmap(sprites, sprites_size);
	} else {
		free(sprites);
	}
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

const dos_sprite_t *
data_get_dos_sprite(uint index)
{
	return (const dos_sprite_t *)data_get_object(index, NULL);
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
