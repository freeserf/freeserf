/*
 * gfx.h - General graphics and data file functions
 *
 * Copyright (C) 2012-2015  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "src/debug.h"

class GFX_Exception : public Freeserf_Exception {
 public:
  explicit GFX_Exception(const std::string &description) throw();
  virtual ~GFX_Exception() throw();

  virtual std::string get_system() const { return "graphics"; }
};

class video_t;
class video_frame_t;
class video_image_t;
class sprite_t;
class data_source_t;

class image_t {
 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;
  video_t *video;
  video_image_t *video_image;

  typedef std::map<uint64_t, image_t*> image_cache_t;
  static image_cache_t image_cache;

 public:
  image_t(video_t *video, sprite_t *sprite);
  virtual ~image_t();

  unsigned int get_width() const { return width; }
  unsigned int get_height() const { return height; }
  int get_delta_x() const { return delta_x; }
  int get_delta_y() const { return delta_y; }
  int get_offset_x() const { return offset_x; }
  int get_offset_y() const { return offset_y; }

  void set_offset(int x, int y) { offset_x = x; offset_y = y; }
  void set_delta(int x, int y) { delta_x = x; delta_y = y; }

  static void cache_image(uint64_t id, image_t *image);
  static image_t *get_cached_image(uint64_t id);
  static void clear_cache();

  video_image_t *get_video_image() const { return video_image; }
};

/* Frame. Keeps track of a specific rectangular area of a surface.
   Multiple frames can refer to the same surface. */
class frame_t {
 protected:
  video_t *video;
  video_frame_t *video_frame;
  bool owner;
  data_source_t *data_source;

 public:
  frame_t(video_t *video, unsigned int width, unsigned int height);
  frame_t(video_t *video, video_frame_t *video_frame);
  virtual ~frame_t();

  /* Sprite functions */
  void draw_sprite(int x, int y, unsigned int sprite);
  void draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off);
  void draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                          float progress);
  void draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                          unsigned char color_offs);
  void draw_transp_sprite_relatively(int x, int y, unsigned int sprite,
                                     unsigned int offs_sprite);
  void draw_masked_sprite(int x, int y, unsigned int mask, unsigned int sprite);
  void draw_overlay_sprite(int x, int y, unsigned int sprite);
  void draw_overlay_sprite(int x, int y, unsigned int sprite, float progress);
  void draw_waves_sprite(int x, int y, unsigned int mask, unsigned int sprite);

  /* Drawing functions */
  void draw_rect(int x, int y, int width, int height, unsigned char color);
  void fill_rect(int x, int y, int width, int height, unsigned char color);

  /* Text functions */
  void draw_string(int x, int y, unsigned char color, int shadow,
                   const std::string &str);
  void draw_number(int x, int y, unsigned char color, int shadow, int n);

  /* Frame functions */
  void draw_frame(int dx, int dy, int sx, int sy, frame_t *src, int w, int h);

 protected:
  void draw_char_sprite(int x, int y, unsigned char c, unsigned char color,
                        unsigned char shadow);
  void draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                          unsigned char color_off, float progress);
};

class gfx_t {
 protected:
  static gfx_t *instance;
  video_t *video;

  gfx_t() throw(Freeserf_Exception);

 public:
  virtual ~gfx_t();

  static gfx_t *get_instance();

  /* Frame functions */
  frame_t *create_frame(unsigned int width, unsigned int height);

  /* Screen functions */
  frame_t *get_screen_frame();
  void set_resolution(unsigned int width, unsigned int height, bool fullscreen);
  void get_resolution(unsigned int *width, unsigned int *height);
  void set_fullscreen(bool enable);
  bool is_fullscreen();

  void swap_buffers();

  float get_zoom_factor();
  bool set_zoom_factor(float factor);
};

#endif  // SRC_GFX_H_
