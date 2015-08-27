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
#include "video.h"
#include "log.h"

#include <stdlib.h>

int
gfx_init(int width, int height, int fullscreen)
{
	LOGI("graphics", "Init...");

	int r = video_init();
	if (r < 0) return r;

	LOGI("graphics", "Setting resolution to %ix%i...", width, height);

	video_get_resolution(&width, &height);
	r = video_set_resolution(width, height, fullscreen);
	if (r < 0) return r;

	sprite_t *sprite = data_get_cursor();
	video_set_cursor(sprite);
	data_sprite_free(sprite);

	return 0;
}

void
gfx_deinit()
{
	gfx_clear_cache();
	video_deinit();
}

// Image tools
image_t *
gfx_image_from_sprite(sprite_t *sprite)
{
	image_t *image = (image_t*)malloc(sizeof(image_t));

	image->sprite = sprite;
	image->native_image = video_native_image_from_sprite(sprite);

	return image;
}

void
gfx_image_free(image_t *image)
{
	if (image->sprite != NULL) {
		data_sprite_free(image->sprite);
	}

	if (image->native_image != NULL) {
		video_native_image_free(image->native_image);
	}

	free(image);
}

void
gfx_draw_image_to_frame(image_t *image, frame_t *frame, int x, int y, int y_offset)
{
	video_draw_image_to_frame(image, frame, x, y, y_offset);
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
gfx_draw_sprite(int x, int y, uint sprite, frame_t *dest)
{
	image_t *image = gfx_get_image_from_cache(sprite, 0, 0);
	if (image == NULL) {
		sprite_t *spr = data_sprite_for_index(sprite);
		if (spr == NULL) {
			return;
		}
		image = gfx_image_from_sprite(spr);
		gfx_add_image_to_cache(sprite, 0, 0, image);
	}

	x += image->sprite->offset_x;
	y += image->sprite->offset_y;
	gfx_draw_image_to_frame(image, dest, x, y, 0);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
gfx_draw_transp_sprite(int x, int y, uint sprite, int use_off,
		       int y_off, int color_off, frame_t *dest)
{
	image_t *image = gfx_get_image_from_cache(sprite, 0, color_off);
	if (image == NULL) {
		sprite_t *spr = data_transparent_sprite_for_index(sprite, color_off);
		if (spr == NULL) {
			return;
		}
		image = gfx_image_from_sprite(spr);
		gfx_add_image_to_cache(sprite, 0, color_off, image);
	}

	if (use_off) {
		x += image->sprite->offset_x;
		y += image->sprite->offset_y;
	}

	gfx_draw_image_to_frame(image, dest, x, y, y_off);
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
gfx_draw_masked_sprite(int x, int y, uint mask, uint sprite, frame_t *dest)
{
	image_t *image = gfx_get_image_from_cache(sprite, mask, 0);
	if (image == NULL) {
		sprite_t *spr = data_sprite_for_index(sprite);
		if (NULL == spr) {
			return;
		}
		sprite_t *msk = data_mask_sprite_for_index(mask);
		if (msk == NULL) {
			data_sprite_free(spr);
			return;
		}

		sprite_t *masked = data_apply_mask(spr, msk);
		image = gfx_image_from_sprite(masked);
		gfx_add_image_to_cache(sprite, mask, 0, image);

		data_sprite_free(spr);
		data_sprite_free(msk);
	}

	x += image->sprite->offset_x;
	y += image->sprite->offset_y;
	gfx_draw_image_to_frame(image, dest, x, y, 0);
}

/* Draw the overlay sprite with data file index of
   sprite at x, y in dest frame. Rendering will be
   offset in the vertical axis from y_off in the
   sprite. */
void
gfx_draw_overlay_sprite(int x, int y, uint sprite, int y_off, frame_t *dest)
{
	image_t *image = gfx_get_image_from_cache(sprite, 0, 0);
	if (image == NULL) {
		sprite_t *spr = data_overlay_sprite_for_index(sprite);
		if (spr == NULL) {
			return;
		}
		image = gfx_image_from_sprite(spr);
		gfx_add_image_to_cache(sprite, 0, 0, image);
	}

	x += image->sprite->offset_x;
	y += image->sprite->offset_y;

	gfx_draw_image_to_frame(image, dest, x, y, y_off);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
void
gfx_draw_waves_sprite(int x, int y, uint mask, uint sprite,
		      frame_t *dest)
{
	if (mask == 0) {
		gfx_draw_transp_sprite(x, y, sprite, 0, 0, 0, dest);
		return;
	}

	image_t *image = gfx_get_image_from_cache(sprite, mask, 0);
	if (image == NULL) {
		sprite_t *spr = data_transparent_sprite_for_index(sprite, 0);
		if (spr == NULL) {
			return;
		}
		sprite_t *msk = data_mask_sprite_for_index(mask);
		if (msk == NULL) {
			data_sprite_free(spr);
			return;
		}

		sprite_t *masked = data_apply_mask(spr, msk);
		image = gfx_image_from_sprite(masked);
		gfx_add_image_to_cache(sprite, mask, 0, image);

		data_sprite_free(spr);
		data_sprite_free(msk);
	}

	x += image->sprite->offset_x;
	y += image->sprite->offset_y;
	gfx_draw_image_to_frame(image, dest, x, y, 0);
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
	color_t clr = data_get_color(color);

	video_fill_rect(x, y, width, height, &clr, dest);
}


/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
frame_t *
gfx_frame_create(int width, int height)
{
	frame_t *frame = (frame_t*)calloc(sizeof(frame_t), 1);
	video_frame_init(frame, width, height);
	return frame;
}

/* Deinitialize frame and backing surface. */
void
gfx_frame_destroy(frame_t *frame)
{
	if (frame == NULL) {
		return;
	}
	video_frame_deinit(frame);
	free(frame);
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
gfx_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h)
{
	video_draw_frame(dx, dy, dest, sx, sy, src, w, h);
}


/* Enable or disable fullscreen mode */
int
gfx_set_fullscreen(int enable)
{
	return video_set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
int
gfx_is_fullscreen()
{
	return video_is_fullscreen();
}

int
gfx_is_fullscreen_possible()
{
	return video_is_fullscreen_possible();
}

void
gfx_get_resolution(int *width, int *height)
{
	video_get_resolution(width, height);
}
