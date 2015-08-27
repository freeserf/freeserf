/*
 * gfx.c - General graphics and data file functions
 *
 * Copyright (C) 2013-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "gfx.h"
#include "sdl-video.h"
#include "data.h"
#include "log.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


/* Unique identifier for a sprite. */
typedef struct {
	uint sprite;
	uint mask;
	uint offset;
} sprite_id_t;

typedef struct sprite_ht_entry sprite_ht_entry_t;

/* Entry in the hashtable of sprites. */
struct sprite_ht_entry {
	sprite_ht_entry_t *next;
	sprite_id_t id;
	sprite_t *value;
};

/* Hashtable of sprites, used as sprite cache. */
typedef struct {
	size_t size;
	uint entry_count;
	sprite_ht_entry_t **entries;
} sprite_ht_t;


/* Sprite cache hash table */
static sprite_ht_t sprite_cache;


/* Calculate hash of sprite identifier. */
static uint32_t
sprite_id_hash(const sprite_id_t *id)
{
	const uint8_t *s = (uint8_t *)id;

	/* FNV-1 */
	uint32_t hash = 2166136261;
	for (int i = 0; i < sizeof(sprite_id_t); i++) {
		hash *= 16777619;
		hash ^= s[i];
	}

	return hash;
}

/* Intialize sprite hashtable. */
static int
sprite_ht_init(sprite_ht_t *ht, size_t size)
{
	ht->size = size;
	ht->entries = (sprite_ht_entry_t**)calloc(size, sizeof(sprite_ht_entry_t *));
	if (ht->entries == NULL) return -1;

	ht->entry_count = 0;

	return 0;
}

/* Return a pointer to the surface pointer associated with
   id. If it does not exist in the table it a new entry is
   created. */
static sprite_ht_entry_t *
sprite_ht_store(sprite_ht_t *ht, const sprite_id_t *id)
{
	uint32_t hash = sprite_id_hash(id);
	sprite_ht_entry_t *entry =
		(sprite_ht_entry_t *)&ht->entries[hash % ht->size];

	/* The first entry pointed to is not really an
	   entry but a sentinel. */
	while (entry->next != NULL) {
		entry = entry->next;
		if (memcmp(id, &entry->id, sizeof(sprite_id_t)) == 0) {
			return entry;
		}
	}

	sprite_ht_entry_t *new_entry = (sprite_ht_entry_t*)calloc(1, sizeof(sprite_ht_entry_t));
	if (new_entry == NULL) return NULL;

	ht->entry_count += 1;

	entry->next = new_entry;
	memcpy(&new_entry->id, id, sizeof(sprite_id_t));

	return new_entry;
}

int
gfx_init(int width, int height, int fullscreen)
{
	int r = sdl_init();
	if (r < 0) return -1;

	LOGI("graphics", "Setting resolution to %ix%i...", width, height);

	r = sdl_set_resolution(width, height, fullscreen);
	if (r < 0) return -1;

	sprite_t *sprite = data_get_cursor();
	sdl_set_cursor(sprite);
	data_sprite_free(sprite);

	/* Init sprite cache */
	sprite_ht_init(&sprite_cache, 10240);

	return 0;
}

void
gfx_deinit()
{
	sdl_deinit();
}


/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
gfx_draw_sprite(int x, int y, uint sprite, frame_t *dest)
{
	sprite_id_t id;
	id.sprite = sprite;
	id.mask = 0;
	id.offset = 0;
	sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
	if (entry->value == NULL) {
		sprite_t *s = data_sprite_for_index(sprite);
		assert(s != NULL);
		entry->value = s;
	}

	x += entry->value->offset_x;
	y += entry->value->offset_y;
	sdl_draw_sprite(entry->value, x, y, 0, dest);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
gfx_draw_transp_sprite(int x, int y, uint sprite, int use_off,
		       int y_off, int color_off, frame_t *dest)
{
	sprite_id_t id;
	id.sprite = sprite;
	id.mask = 0;
	id.offset = color_off;
	sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
	if (entry->value == NULL) {
		sprite_t *s = data_transparent_sprite_for_index(sprite, color_off);
		assert(s != NULL);
		entry->value = s;
	}

	if (use_off) {
		x += entry->value->offset_x;
		y += entry->value->offset_y;
	}
	sdl_draw_sprite(entry->value, x, y, y_off, dest);
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
gfx_draw_masked_sprite(int x, int y, uint mask, uint sprite, frame_t *dest)
{
	sprite_id_t id;
	id.sprite = sprite;
	id.mask = mask;
	id.offset = 0;
	sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
	if (entry->value == NULL) {
		sprite_t *s = data_sprite_for_index(sprite);
		assert(s != NULL);

		sprite_t *m = data_mask_sprite_for_index(mask);
		assert(m != NULL);

		sprite_t *masked = data_apply_mask(s, m);
		assert(masked != NULL);

		entry->value = masked;

		data_sprite_free(s);
		data_sprite_free(m);
	}

	x += entry->value->offset_x;
	y += entry->value->offset_y;
	sdl_draw_sprite(entry->value, x, y, 0, dest);
}

/* Draw the overlay sprite with data file index of
   sprite at x, y in dest frame. Rendering will be
   offset in the vertical axis from y_off in the
   sprite. */
void
gfx_draw_overlay_sprite(int x, int y, uint sprite, int y_off, frame_t *dest)
{
	sprite_id_t id;
	id.sprite = sprite;
	id.mask = 0;
	id.offset = 0;
	sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
	if (entry->value == NULL) {
		sprite_t *s = data_overlay_sprite_for_index(sprite);
		assert(s != NULL);
		entry->value = s;
	}

	x += entry->value->offset_x;
	y += entry->value->offset_y;
	sdl_draw_sprite(entry->value, x, y, y_off, dest);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
void
gfx_draw_waves_sprite(int x, int y, uint mask, uint sprite,
		      frame_t *dest)
{
	sprite_id_t id;
	id.sprite = sprite;
	id.mask = mask;
	id.offset = 0;
	sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
	if (entry->value == NULL) {
		sprite_t *s = data_transparent_sprite_for_index(sprite, 0);
		assert(s != NULL);

		sprite_t *m = NULL;
		if (mask > 0) {
			m = data_mask_sprite_for_index(mask);
			assert(m != NULL);

			sprite_t *masked = data_apply_mask(s, m);
			assert(masked != NULL);

			data_sprite_free(s);
			data_sprite_free(m);

			entry->value = masked;
		} else {
			entry->value = s;
		}
	}

	x += entry->value->offset_x;
	y += entry->value->offset_y;
	sdl_draw_sprite(entry->value, x, y, 0, dest);
}


/* Draw a character at x, y in the dest frame. */
static void
gfx_draw_char_sprite(int x, int y, uint c, int color, int shadow, frame_t *dest)
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
		gfx_draw_transp_sprite(x, y, DATA_FONT_SHADOW_BASE + s,
				       0, 0, shadow, dest);
	}
	gfx_draw_transp_sprite(x, y, DATA_FONT_BASE + s, 0, 0,
			       color, dest);
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

void
gfx_get_sprite_size(int sprite, uint *width, uint *height)
{
	data_get_sprite_size(sprite, width, height);
}

void
gfx_get_sprite_offset(int sprite, int *dx, int *dy)
{
	data_get_sprite_offset(sprite, dx, dy);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
gfx_draw_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	gfx_fill_rect(x, y, width, 1, color, dest);
	gfx_fill_rect(x, y+height-1, width, 1, color, dest);
	gfx_fill_rect(x, y, 1, height, color, dest);
	gfx_fill_rect(x+width-1, y, 1, height, color, dest);
}

/* Fill a rectangle with color at x, y in the dest frame. */
void
gfx_fill_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	uint8_t *palette = (uint8_t*)data_get_object(DATA_PALETTE_GAME, NULL);
	color_t c = { palette[3*color+0], palette[3*color+1],
		      palette[3*color+2], 0xff };

	sdl_fill_rect(x, y, width, height, &c, dest);
}


/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
void
gfx_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest)
{
	sdl_frame_init(frame, x, y, width, height, dest);
}

/* Deinitialize frame and backing surface. */
void
gfx_frame_deinit(frame_t *frame)
{
	sdl_frame_deinit(frame);
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
gfx_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	sdl_draw_frame(dx, dy, dest, sx, sy, src, w, h);
}


/* Enable or disable fullscreen mode */
int
gfx_set_fullscreen(int enable)
{
	return sdl_set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
int
gfx_is_fullscreen()
{
	return sdl_is_fullscreen();
}

int
gfx_is_fullscreen_possible()
{
	return sdl_is_fullscreen_possible();
}

void
gfx_get_resolution(int *width, int *height)
{
	sdl_get_resolution(width, height);
}
