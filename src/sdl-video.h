/*
 * sdl-video.h - SDL graphics rendering
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef _SDL_VIDEO_H
#define _SDL_VIDEO_H

#include <stdint.h>

#include "SDL.h"

#include "gfx.h"


typedef struct surface surface_t;

/* Sprite header. In the data file this is immediately followed by sprite data. */
typedef struct {
	int8_t b_x;
	int8_t b_y;
	uint16_t w;
	uint16_t h;
	int16_t x;
	int16_t y;
} sprite_t;

int sdl_init();
void sdl_deinit();
int sdl_set_resolution(int width, int height, int fullscreen);
void sdl_get_resolution(int *width, int *height);
int sdl_set_fullscreen(int enable);
int sdl_is_fullscreen();

frame_t *sdl_get_screen_frame();
void sdl_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest);
void sdl_frame_deinit(frame_t *frame);
int sdl_frame_get_width(const frame_t *frame);
int sdl_frame_get_height(const frame_t *frame);
void sdl_warp_mouse(int x, int y);

void sdl_draw_transp_sprite(const sprite_t *sprite, int x, int y, int use_off, int y_off, int color_off, frame_t *dest);
void sdl_draw_waves_sprite(const sprite_t *sprite, const sprite_t *mask, int x, int y, int mask_off, frame_t *dest);
void sdl_draw_sprite(const sprite_t *sprite, int x, int y, frame_t *dest);
void sdl_draw_overlay_sprite(const sprite_t *sprite, int x, int y, int y_off, frame_t *dest);
surface_t *sdl_draw_masked_sprite(const sprite_t *sprite, int x, int y, const sprite_t *mask, surface_t *surface, frame_t *dest);
void sdl_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h);
void sdl_draw_rect(int x, int y, int width, int height, int color, frame_t *dest);
void sdl_fill_rect(int x, int y, int width, int height, int color, frame_t *dest);
void sdl_set_palette(const uint8_t *palette);
void sdl_swap_buffers();

void sdl_set_cursor(const sprite_t *sprite);

#endif /* ! _SDL_VIDEO_H */
