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

#include "misc.h"

#include "SDL.h"

#define MAP_TILE_WIDTH   32
#define MAP_TILE_HEIGHT  20


/* Frame. Keeps track of a specific rectangular area of a surface.
   Multiple frames can refer to the same surface. */
typedef struct {
	SDL_Surface *surf;
	SDL_Rect clip;
} frame_t;

/* Sprite object. Immediately followed by RGBA data. */
typedef struct {
       int delta_x;
       int delta_y;
       int offset_x;
       int offset_y;
       uint width;
       uint height;
} sprite_t;

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
} color_t;


int gfx_init(int width, int height, int fullscreen);
void gfx_deinit();

/* Sprite functions */
void gfx_draw_sprite(int x, int y, uint sprite, frame_t *dest);
void gfx_draw_transp_sprite(int x, int y, uint sprite, int use_off, int y_off, int color_off, frame_t *dest);
void gfx_draw_masked_sprite(int x, int y, uint mask, uint sprite, frame_t *dest);
void gfx_draw_overlay_sprite(int x, int y, uint sprite, int y_off, frame_t *dest);
void gfx_draw_waves_sprite(int x, int y, uint mask, uint sprite, int mask_off, frame_t *dest);

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
int gfx_set_fullscreen(int enable);
int gfx_is_fullscreen();

#endif /* ! _GFX_H */
