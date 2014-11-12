/*
 * video.h - Graphics rendering functions
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

#ifndef _VIDEO_H
#define _VIDEO_H

#include "gfx.h"

int video_init();
void video_deinit();
int video_set_resolution(int width, int height, int fullscreen);
void video_get_resolution(int *width, int *height);
int video_set_fullscreen(int enable);
int video_is_fullscreen();
int video_is_fullscreen_possible();

frame_t *video_get_screen_frame();
void video_frame_init(frame_t *frame, int x, int y, int width, int height, frame_t *dest);
void video_frame_deinit(frame_t *frame);
int video_frame_get_width(const frame_t *frame);
int video_frame_get_height(const frame_t *frame);
void video_warp_mouse(int x, int y);

void video_draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src, int w, int h);
void video_fill_rect(int x, int y, int width, int height, const color_t *color, frame_t *dest);
void video_swap_buffers();

void video_set_cursor(const sprite_t *sprite);

void *video_native_image_from_sprite(const sprite_t *sprite);
void video_native_image_free(void *native_image);
void video_draw_image_to_frame(image_t *image, frame_t *frame, int x, int y, int y_offset);

#endif /* ! _VIDEO_H */
