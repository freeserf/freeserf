/*
 * gfx.cc - General graphics and data file functions
 *
 * Copyright (C) 2013-2015  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/gfx.h"

#include <cstring>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
END_EXT_C
#include "src/data.h"
#include "src/video.h"
#include "src/data-source.h"

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

Freeserf_Exception::Freeserf_Exception(const std::string &description) throw() {
  this->description = description;
}

Freeserf_Exception::~Freeserf_Exception() throw() {
}

const char*
Freeserf_Exception::what() const throw() {
  return get_description();
}

const char*
Freeserf_Exception::get_description() const {
  return description.c_str();
}

GFX_Exception::GFX_Exception(const std::string &description) throw()
  : Freeserf_Exception(description) {
}

GFX_Exception::~GFX_Exception() throw() {
}

image_t::image_t(video_t *video, sprite_t *sprite) {
  this->video = video;
  width = sprite->get_width();
  height = sprite->get_height();
  offset_x = sprite->get_offset_x();
  offset_y = sprite->get_offset_y();
  delta_x = sprite->get_delta_x();
  delta_y = sprite->get_delta_y();
  video_image = video->create_image(sprite->get_data(), width, height);
}

image_t::~image_t() {
  if (video_image != NULL) {
    video->destroy_image(video_image);
    video_image = NULL;
  }
  video = NULL;
}

/* Sprite cache hash table */
image_t::image_cache_t image_t::image_cache;

void
image_t::cache_image(uint64_t id, image_t *image) {
  image_cache[id] = image;
}

/* Return a pointer to the sprite pointer associated with id. */
image_t *
image_t::get_cached_image(uint64_t id) {
  image_cache_t::iterator result = image_cache.find(id);
  if (result == image_cache.end()) {
    return NULL;
  }
  return result->second;
}

void
image_t::clear_cache() {
  while (!image_cache.empty()) {
    image_t *image = image_cache.begin()->second;
    image_cache.erase(image_cache.begin());
    delete image;
  }
}

gfx_t *gfx_t::instance = NULL;

gfx_t::gfx_t() throw(Freeserf_Exception) {
  if (instance != NULL) {
    throw GFX_Exception("Unable to create second instance.");
  }

  try {
    video = video_t::get_instance();
  } catch (Video_Exception &e) {
    throw GFX_Exception(e.what());
  }

  data_t *data = data_t::get_instance();
  data_source_t *data_source = data->get_data_source();
  sprite_t *sprite = data_source->get_transparent_sprite(DATA_CURSOR, 0);
  video->set_cursor(sprite->get_data(), sprite->get_width(),
                    sprite->get_height());
  delete sprite;

  gfx_t::instance = this;
}

gfx_t::~gfx_t() {
  image_t::clear_cache();

  if (video != NULL) {
    delete video;
    video = NULL;
  }

  gfx_t::instance = NULL;
}

gfx_t*
gfx_t::get_instance() {
  if (instance == NULL) {
    instance = new gfx_t();
  }
  return instance;
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
frame_t::draw_sprite(int x, int y, unsigned int sprite) {
  uint64_t id = sprite_t::create_sprite_id(sprite, 0, 0);
  image_t *image = image_t::get_cached_image(id);
  if (image == NULL) {
    sprite_t *s = data_source->get_sprite(sprite);
    if (s == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", sprite);
      return;
    }

    image = new image_t(video, s);
    image_t::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/

void
frame_t::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                            unsigned char color_off, float progress) {
  uint64_t id = sprite_t::create_sprite_id(sprite, 0, color_off);
  image_t *image = image_t::get_cached_image(id);
  if (image == NULL) {
    sprite_t *s = data_source->get_transparent_sprite(sprite, color_off);
    if (s == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", sprite);
      return;
    }

    image = new image_t(video, s);
    image_t::cache_image(id, image);

    delete s;
  }

  if (use_off) {
    x += image->get_offset_x();
    y += image->get_offset_y();
  }
  int y_off = image->get_height() - static_cast<int>(image->get_height() *
                                                     progress);
  video->draw_image(image->get_video_image(), x, y, y_off, video_frame);
}


void
frame_t::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off) {
  draw_transp_sprite(x, y, sprite, use_off, 0, 1.f);
}

void
frame_t::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                            float progress) {
  draw_transp_sprite(x, y, sprite, use_off, 0, progress);
}

void
frame_t::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                            unsigned char color_offs) {
  draw_transp_sprite(x, y, sprite, use_off, color_offs, 1.f);
}

void
frame_t::draw_transp_sprite_relatively(int x, int y, unsigned int sprite,
                                       unsigned int offs_sprite) {
  sprite_t *s = data_source->get_empty_sprite(offs_sprite);
  x += s->get_delta_x();
  y += s->get_delta_y();

  draw_transp_sprite(x, y, sprite, true, 0, 1.f);
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
frame_t::draw_masked_sprite(int x, int y, unsigned int mask,
                            unsigned int sprite) {
  uint64_t id = sprite_t::create_sprite_id(sprite, mask, 0);
  image_t *image = image_t::get_cached_image(id);
  if (image == NULL) {
    sprite_t *s = data_source->get_sprite(sprite);
    if (s == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", sprite);
      return;
    }

    sprite_t *m = data_source->get_mask_sprite(mask);
    if (m == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", mask);
      delete s;
      return;
    }

    sprite_t *masked = s->get_masked(m);
    if (masked == NULL) {
      LOGW("graphics", "Failed to apply mask #%i to sprite #%i", mask, sprite);
      return;
    }

    delete s;
    delete m;

    s = masked;

    image = new image_t(video, s);
    image_t::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw the overlay sprite with data file index of
   sprite at x, y in dest frame. Rendering will be
   offset in the vertical axis from y_off in the
   sprite. */
void
frame_t::draw_overlay_sprite(int x, int y, unsigned int sprite) {
  draw_overlay_sprite(x, y, sprite, 1.f);
}

void
frame_t::draw_overlay_sprite(int x, int y, unsigned int sprite,
                             float progress) {
  uint64_t id = sprite_t::create_sprite_id(sprite, 0, 0);
  image_t *image = image_t::get_cached_image(id);
  if (image == NULL) {
    sprite_t *s = data_source->get_overlay_sprite(sprite);
    if (s == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", sprite);
      return;
    }

    image = new image_t(video, s);
    image_t::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();

  int y_off = image->get_height() - static_cast<int>(image->get_height() *
                                                     progress);

  video->draw_image(image->get_video_image(), x, y, y_off, video_frame);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
void
frame_t::draw_waves_sprite(int x, int y, unsigned int mask,
                           unsigned int sprite) {
  uint64_t id = sprite_t::create_sprite_id(sprite, mask, 0);
  image_t *image = image_t::get_cached_image(id);
  if (image == NULL) {
    sprite_t *s = data_source->get_transparent_sprite(sprite, 0);
    if (s == NULL) {
      LOGW("graphics", "Failed to decode sprite #%i", sprite);
      return;
    }

    if (mask > 0) {
      sprite_t *m = data_source->get_mask_sprite(mask);
      if (m == NULL) {
        LOGW("graphics", "Failed to decode sprite #%i", sprite);
        return;
      }

      sprite_t *masked = s->get_masked(m);
      if (masked == NULL) {
        LOGW("graphics", "Failed to apply mask #%i to sprite #%i",
             mask, sprite);
        return;
      }

      delete s;
      delete m;

      s = masked;
    }

    image = new image_t(video, s);
    image_t::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw a character at x, y in the dest frame. */
void
frame_t::draw_char_sprite(int x, int y, unsigned char c, unsigned char color,
                          unsigned char shadow) {
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
    draw_transp_sprite(x, y, DATA_FONT_SHADOW_BASE + s, false, shadow);
  }
  draw_transp_sprite(x, y, DATA_FONT_BASE + s, false, color);
}

/* Draw the string str at x, y in the dest frame. */
void
frame_t::draw_string(int x, int y, unsigned char color, int shadow,
                     const std::string &str) {
  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
    if (/*string_bg*/ 0) {
      fill_rect(x, y, 8, 8, 0);
    }

    draw_char_sprite(x, y, *it, color, shadow);

    x += 8;
  }
}

/* Draw the number n at x, y in the dest frame. */
void
frame_t::draw_number(int x, int y, unsigned char color, int shadow, int n) {
  if (n < 0) {
    draw_char_sprite(x, y, '-', color, shadow);
    x += 8;
    n *= -1;
  }

  if (n == 0) {
    draw_char_sprite(x, y, '0', color, shadow);
    return;
  }

  int digits = 0;
  for (int i = n; i > 0; i /= 10) digits += 1;

  for (int i = digits-1; i >= 0; i--) {
    draw_char_sprite(x+8*i, y, '0'+(n % 10), color, shadow);
    n /= 10;
  }
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
frame_t::draw_rect(int x, int y, int width, int height, unsigned char color) {
  color_t col = data_source->get_color(color);
  video_color_t c = { col.red, col.green, col.blue, col.alpha };
  video->draw_rect(x, y, width, height, c, video_frame);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
frame_t::fill_rect(int x, int y, int width, int height, unsigned char color) {
  color_t col = data_source->get_color(color);
  video_color_t c = { col.red, col.green, col.blue, col.alpha };
  video->fill_rect(x, y, width, height, c, video_frame);
}

/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
frame_t::frame_t(video_t *video, unsigned int width, unsigned int height) {
  this->video = video;
  video_frame = video->create_frame(width, height);
  owner = true;
  data_t *data = data_t::get_instance();
  data_source = data->get_data_source();
}

frame_t::frame_t(video_t *video, video_frame_t *video_frame) {
  this->video = video;
  this->video_frame = video_frame;
  owner = false;
  data_t *data = data_t::get_instance();
  data_source = data->get_data_source();
}

/* Deinitialize frame and backing surface. */
frame_t::~frame_t() {
  if (owner) {
    video->destroy_frame(video_frame);
  }
  video_frame = NULL;
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
frame_t::draw_frame(int dx, int dy, int sx, int sy, frame_t *src,
                    int w, int h) {
  video->draw_frame(dx, dy, video_frame, sx, sy, src->video_frame, w, h);
}

frame_t *
gfx_t::create_frame(unsigned int width, unsigned int height) {
  return new frame_t(video, width, height);
}

/* Enable or disable fullscreen mode */
void
gfx_t::set_fullscreen(bool enable) {
  video->set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
bool
gfx_t::is_fullscreen() {
  return video->is_fullscreen();
}

frame_t *
gfx_t::get_screen_frame() {
  return new frame_t(video, video->get_screen_frame());
}

void
gfx_t::set_resolution(unsigned int width, unsigned int height,
                      bool fullscreen) {
  video->set_resolution(width, height, fullscreen);
}

void
gfx_t::get_resolution(unsigned int *width, unsigned int *height) {
  video->get_resolution(width, height);
}

void
gfx_t::swap_buffers() {
  video->swap_buffers();
}

float
gfx_t::get_zoom_factor() {
  return video->get_zoom_factor();
}

bool
gfx_t::set_zoom_factor(float factor) {
  return video->set_zoom_factor(factor);
}
