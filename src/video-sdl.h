/*
 * video-sdl.h - SDL graphics rendering
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

#ifndef SRC_VIDEO_SDL_H_
#define SRC_VIDEO_SDL_H_

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "src/gfx.h"

class video_sdl_t {
 protected:
  static int bpp;
  static Uint32 Rmask;
  static Uint32 Gmask;
  static Uint32 Bmask;
  static Uint32 Amask;
  static Uint32 pixel_format;

  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *screen_texture;
  frame_t *screen;
  bool fullscreen;
  SDL_Color pal_colors[256];
  SDL_Cursor *cursor;

 public:
  video_sdl_t();
  virtual ~video_sdl_t();

  bool set_resolution(int width, int height, int fullscreen);
  void get_resolution(int *width, int *height);
  bool set_fullscreen(int enable);
  bool is_fullscreen();

  frame_t *get_screen_frame();
  void frame_init(frame_t *frame, int width, int height);
  void frame_deinit(frame_t *frame);
  void warp_mouse(int x, int y);

  void draw_sprite(const sprite_t *sprite, int x, int y, int y_offset,
                   frame_t *dest);
  void draw_frame(int dx, int dy, frame_t *dest, int sx, int sy, frame_t *src,
                  int w, int h);
  void draw_rect(int x, int y, int width, int height, const color_t *color,
                 frame_t *dest);
  void fill_rect(int x, int y, int width, int height, const color_t *color,
                 frame_t *dest);
  void swap_buffers();

  void set_cursor(const sprite_t *sprite);

 protected:
  SDL_Surface *create_surface(int width, int height);
  SDL_Surface *create_surface_from_data(void *data, int width, int height);
  SDL_Surface *create_surface_from_sprite(const sprite_t *sprite);
  void set_palette(const uint8_t *palette);
};

#endif  // SRC_VIDEO_SDL_H_
