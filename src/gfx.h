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
#include <cstdint>

#include "src/data.h"
#include "src/debug.h"
#include "src/video.h"

class ExceptionGFX : public ExceptionFreeserf {
 public:
  explicit ExceptionGFX(const std::string &description) throw();
  virtual ~ExceptionGFX() throw();

  virtual std::string get_system() const { return "graphics"; }
};

class Sprite;
class DataSource;

class Color {
 protected:
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;

 public:
  Color() : r(0), g(0), b(0), a(0) {}
  Color(uint8_t _r, uint8_t _g, uint8_t _b, uint8_t _a = 0xff) :
    r(_r), g(_g), b(_b), a(_a) {}

  uint8_t get_red() const { return r; }
  uint8_t get_green() const { return g; }
  uint8_t get_blue() const { return b; }
  uint8_t get_alpha() const { return a; }

  static const Color black;
  static const Color green;
  static const Color transparent;

  bool operator==(const Color &c) const { return (r == c.r) &&
                                                 (g == c.g) &&
                                                 (b == c.b) &&
                                                 (a == c.a); }
  inline bool operator!=(const Color &c) const { return !((*this) == c); }
};

class Image {
 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;
  Video *video;
  Video::Image *video_image;

  typedef std::map<uint64_t, Image*> ImageCache;
  static ImageCache image_cache;

 public:
  Image(Video *video, Sprite *sprite);
  virtual ~Image();

  unsigned int get_width() const { return width; }
  unsigned int get_height() const { return height; }
  int get_delta_x() const { return delta_x; }
  int get_delta_y() const { return delta_y; }
  int get_offset_x() const { return offset_x; }
  int get_offset_y() const { return offset_y; }

  void set_offset(int x, int y) { offset_x = x; offset_y = y; }
  void set_delta(int x, int y) { delta_x = x; delta_y = y; }

  static void cache_image(uint64_t id, Image *image);
  static Image *get_cached_image(uint64_t id);
  static void clear_cache();

  Video::Image *get_video_image() const { return video_image; }
};

/* Frame. Keeps track of a specific rectangular area of a surface.
   Multiple frames can refer to the same surface. */
class Frame {
 protected:
  Video *video;
  Video::Frame *video_frame;
  bool owner;
  PDataSource data_source;

 public:
  Frame(Video *video, unsigned int width, unsigned int height);
  Frame(Video *video, Video::Frame *video_frame);
  virtual ~Frame();

  /* Sprite functions */
  void draw_sprite(int x, int y, Data::Resource res, unsigned int index);
  void draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off);
  void draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, float progress);
  void draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, const Color &color);
  void draw_sprite_relatively(int x, int y, Data::Resource res,
                              unsigned int index,
                              Data::Resource relative_to_res,
                              unsigned int relative_to_index);
  void draw_masked_sprite(int x, int y, Data::Resource mask_res,
                          unsigned int mask_index, Data::Resource res,
                          unsigned int index);
  void draw_waves_sprite(int x, int y, Data::Resource mask_res,
                         unsigned int mask_index, Data::Resource res,
                         unsigned int index);

  /* Drawing functions */
  void draw_rect(int x, int y, int width, int height, const Color &color);
  void fill_rect(int x, int y, int width, int height, const Color &color);
  void draw_line(int x, int y, int x1, int y1, const Color &color);

  /* Text functions */
  void draw_string(int x, int y, const std::string &str, const Color &color,
                   const Color &shadow = Color::transparent);
  void draw_number(int x, int y, int value, const Color &color,
                   const Color &shadow = Color::transparent);

  /* Frame functions */
  void draw_frame(int dx, int dy, int sx, int sy, Frame *src, int w, int h);

 protected:
  void draw_char_sprite(int x, int y, unsigned char c, const Color &color,
                        const Color &shadow);
  void draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, const Color &color, float progress);
};

class Graphics {
 protected:
  static Graphics *instance;
  Video *video;

  Graphics() throw(ExceptionFreeserf);

 public:
  virtual ~Graphics();

  static Graphics *get_instance();

  /* Frame functions */
  Frame *create_frame(unsigned int width, unsigned int height);

  /* Screen functions */
  Frame *get_screen_frame();
  void set_resolution(unsigned int width, unsigned int height, bool fullscreen);
  void get_resolution(unsigned int *width, unsigned int *height);
  void set_fullscreen(bool enable);
  bool is_fullscreen();

  void swap_buffers();

  float get_zoom_factor();
  bool set_zoom_factor(float factor);
};

#endif  // SRC_GFX_H_
