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

/* There are different types of sprites:
   - Non-packed, rectangular sprites: These are simple called sprites here.
   - Transparent sprites, "transp": These are e.g. buldings/serfs.
   The transparent regions are RLE encoded.
   - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
   This is used to either modify the alpha level of another sprite (shadows)
   or mask parts of other sprites completely (mask sprites).
*/

int
gfx_init(int width, int height, int fullscreen)
{
	int r = sdl_init();
	if (r < 0) return -1;

	LOGI("graphics", "Setting resolution to %ix%i...", width, height);

	r = sdl_set_resolution(width, height, fullscreen);
	if (r < 0) return -1;

	gfx_set_palette(DATA_PALETTE_GAME);

	sprite_t *cursor = (sprite_t *)data_get_object(DATA_CURSOR, NULL);
	sdl_set_cursor(cursor);

	return 0;
}

void
gfx_deinit()
{
	sdl_deinit();
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
		sdl_draw_transp_sprite((sprite_t*)data_get_object(DATA_FONT_SHADOW_BASE + s, NULL),
				       x, y, 0, 0, shadow, dest);
	}
	sdl_draw_transp_sprite((sprite_t*)data_get_object(DATA_FONT_BASE + s, NULL),
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
	sprite_t *spr = (sprite_t*)data_get_object(sprite, NULL);
	if (spr != NULL) sdl_draw_sprite(spr, x, y, dest);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
gfx_draw_transp_sprite(int x, int y, int sprite, frame_t *dest)
{
	sprite_t *spr = (sprite_t*)data_get_object(sprite, NULL);
	if (spr != NULL) sdl_draw_transp_sprite(spr, x, y, 0, 0, 0, dest);
}

/* Fill a rectangle with color at x, y in the dest frame. */
void
gfx_fill_rect(int x, int y, int width, int height, int color, frame_t *dest)
{
	sdl_fill_rect(x, y, width, height, color, dest);
}

/* Select the color palette that is location at the given data file index. */
void
gfx_set_palette(int palette)
{
	uint8_t *pal = (uint8_t*)data_get_object(palette, NULL);
	sdl_set_palette(pal);
}

