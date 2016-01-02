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

#ifndef SRC_GFX_H_
#define SRC_GFX_H_

#include <map>

class video_t;
class video_frame_t;

/* Frame. Keeps track of a specific rectangular area of a surface.
   Multiple frames can refer to the same surface. */
class frame_t {
 public:
  video_t *video;
  video_frame_t *video_frame;
  bool owner;

 public:
  frame_t(video_t *video, unsigned int width, unsigned int height);
  frame_t(video_t *video, video_frame_t *video_frame);
  virtual ~frame_t();

  /* Sprite functions */
  void draw_sprite(int x, int y, unsigned int sprite);
  void draw_transp_sprite(int x, int y, unsigned int sprite, int use_off,
                          int y_off, int color_off);
  void draw_masked_sprite(int x, int y, unsigned int mask,
                          unsigned int sprite);
  void draw_overlay_sprite(int x, int y, unsigned int sprite, int y_off);
  void draw_waves_sprite(int x, int y, unsigned int mask, unsigned int sprite);

  /* Drawing functions */
  void draw_rect(int x, int y, int width, int height, unsigned int color);
  void fill_rect(int x, int y, int width, int height, unsigned int color);

  /* Text functions */
  void draw_string(int x, int y, unsigned int color, int shadow,
                   const char *str);
  void draw_number(int x, int y, unsigned int color, int shadow, int n);

  /* Frame functions */
  void draw_frame(int dx, int dy, int sx, int sy, frame_t *src, int w, int h);

 protected:
  void draw_char_sprite(int x, int y, unsigned int c, int color, int shadow);
};

/* Sprite object. Contains RGBA data. */
class sprite_t {
 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;
  uint8_t *data;

  typedef std::map<uint64_t, sprite_t*> sprite_cache_t;
  static sprite_cache_t sprite_cache;

 public:
  sprite_t(unsigned int width, unsigned int height);
  virtual ~sprite_t();

  uint8_t *get_data() const { return data; }
  unsigned int get_width() const { return width; }
  unsigned int get_height() const { return height; }
  int get_delta_x() const { return delta_x; }
  int get_delta_y() const { return delta_y; }
  int get_offset_x() const { return offset_x; }
  int get_offset_y() const { return offset_y; }

  void set_offset(int x, int y) { offset_x = x; offset_y = y; }
  void set_delta(int x, int y) { delta_x = x; delta_y = y; }

  static uint64_t create_sprite_id(uint64_t sprite, uint64_t mask,
                                   uint64_t offset);
  static void cache_sprite(uint64_t id, sprite_t *sprite);
  static sprite_t *get_cached_sprite(uint64_t id);

  sprite_t *get_masked(sprite_t *mask);
};

class gfx_t {
 protected:
  static gfx_t *instance;
  video_t *video;

 public:
  gfx_t(unsigned int width, unsigned int height, bool fullscreen);
  virtual ~gfx_t();

  static gfx_t *get_instance();

  /* Frame functions */
  frame_t *create_frame(unsigned int width, unsigned int height);

  /* Screen functions */
  frame_t *get_screen_frame();
  void set_resolution(unsigned int width, unsigned int height, bool fullscreen);
  void get_resolution(unsigned int *width, unsigned int *height);
  bool set_fullscreen(bool enable);
  bool is_fullscreen();

  void swap_buffers();
};

#endif  // SRC_GFX_H_
