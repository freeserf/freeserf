/*
 * gfx.h - General graphics and data file functions
 *
 * Copyright (C) 2012-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef _GFX_H
#define _GFX_H

#include "data.h"

typedef struct {
	int x;
	int y;
	int w;
	int h;
} rect_t;

/* Frame. Keeps track of a specific rectangular area of a surface.
   Multiple frames can refer to the same surface. */
typedef struct {
	void *surf;
	rect_t clip;
} frame_t;

int gfx_init(int width, int height, int fullscreen);
void gfx_deinit();

/* Sprite functions */
void gfx_draw_sprite(int x, int y, uint sprite, frame_t *dest);
void gfx_draw_transp_sprite(int x, int y, uint sprite, int use_off, int y_off, int color_off, frame_t *dest);
void gfx_draw_masked_sprite(int x, int y, uint mask, uint sprite, frame_t *dest);
void gfx_draw_overlay_sprite(int x, int y, uint sprite, int y_off, frame_t *dest);
void gfx_draw_waves_sprite(int x, int y, uint mask, uint sprite, frame_t *dest);
void gfx_get_sprite_size(int sprite, uint *width, uint *height);
void gfx_get_sprite_offset(int sprite, int *dx, int *dy);

/* Graphics functions */
void gfx_draw_rect(int x, int y, int width, int height, int color, frame_t *dest);
void gfx_fill_rect(int x, int y, int width, int height, int color, frame_t *dest);

/* Text functions */
void gfx_draw_string(int x, int y, int color, int shadow, frame_t *dest, const char *str);
void gfx_draw_number(int x, int y, int color, int shadow, frame_t *dest, int n);

/* Frame functions */
void gfx_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest);
void gfx_frame_deinit(frame_t *frame);
void gfx_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h);

/* Screen functions */
void gfx_get_resolution(int *width, int *height);
int gfx_set_fullscreen(int enable);
int gfx_is_fullscreen();
int gfx_is_fullscreen_possible();

/* Image caching functions */
void gfx_add_image_to_cache(int sprite, int mask, int offset, sprite_t *image);
sprite_t *gfx_get_image_from_cache(int sprite, int mask, int offset);
void gfx_clear_cache();

#endif /* ! _GFX_H */
