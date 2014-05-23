/*
 * gfx-sdl.c - General graphics and data file functions
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>

#include "sdl-video.h"
#include "gfx.h"
#include "data.h"
#include "log.h"
#include "SDL.h"
#include "gui.h"

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
	LOGI("main", "SDL init...");

	int r = sdl_init();
	if (r < 0) return r;

	LOGI("main", "SDL resolution %ix%i...", width, height);

	r = sdl_set_resolution(width, height, fullscreen);
	if (r < 0) return r;

	/*gfx_set_palette(DATA_PALETTE_INTRO);*/
	gfx_set_palette(DATA_PALETTE_GAME);

	sdl_set_cursor(data_get_object(DATA_CURSOR, NULL));

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
gfx_draw_sprite(int x, int y, int sprite, frame_t *dest)
{
	sprite_t *spr = data_get_object(sprite, NULL);
	if (spr != NULL) sdl_draw_sprite(spr, x, y, dest);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
gfx_draw_transp_sprite(int x, int y, int sprite, int use_off, int y_off, int color_off, frame_t *dest)
{
	sprite_t *spr = data_get_object(sprite, NULL);
	if (spr != NULL) sdl_draw_transp_sprite(spr, x, y, use_off, y_off, color_off, dest);
}

void
gfx_draw_masked_sprite(int x, int y, int mask, int sprite, frame_t *dest)
{
	sprite_t *spr = data_get_object(sprite, NULL);
	sprite_t *msk = data_get_object(mask, NULL);
	if (spr != NULL && msk != NULL) sdl_draw_masked_sprite(spr, x, y, msk, NULL, dest);
}

void
gfx_draw_overlay_sprite(int x, int y, int sprite, int y_off, frame_t *dest)
{
	sprite_t *spr = data_get_object(sprite, NULL);
	if (spr != NULL) sdl_draw_overlay_sprite(spr, x, y, y_off, dest);
}

void
gfx_draw_waves_sprite(int x, int y, int mask, int sprite, int mask_off, frame_t *dest)
{
	sprite_t *spr = data_get_object(sprite, NULL);
	sprite_t *msk = (mask == 0) ? NULL : data_get_object(mask, NULL);
	if (spr != NULL || msk != NULL) sdl_draw_waves_sprite(spr, msk, x, y, mask_off, dest);
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
	uint8_t *pal = data_get_object(palette, NULL);
	sdl_set_palette(pal);
}

void
gfx_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest)
{
	sdl_frame_init(frame, x, y, width, height, dest);
}

void
gfx_frame_deinit(frame_t *frame)
{
	sdl_frame_deinit(frame);
}

void
gfx_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	sdl_draw_frame(dx, dy, dest, sx, sy, src, w, h);
}

int
gfx_set_fullscreen(int enable)
{
	return sdl_set_fullscreen(enable);
}

int
gfx_is_fullscreen()
{
	return sdl_is_fullscreen();
}

int
gfx_is_fullscreen_possible()
{
	return 1;
}
