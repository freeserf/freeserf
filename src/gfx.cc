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

#include "src/log.h"
#include "src/data.h"
#include "src/video.h"
#include "src/data-source.h"

ExceptionGFX::ExceptionGFX(const std::string &description) throw()
  : ExceptionFreeserf(description) {
}

ExceptionGFX::~ExceptionGFX() throw() {
}

Image::Image(Video *video, Sprite *sprite) {
  this->video = video;
  width = sprite->get_width();
  height = sprite->get_height();
  offset_x = sprite->get_offset_x();
  offset_y = sprite->get_offset_y();
  delta_x = sprite->get_delta_x();
  delta_y = sprite->get_delta_y();
  video_image = video->create_image(sprite->get_data(), width, height);
}

Image::~Image() {
  if (video_image != NULL) {
    video->destroy_image(video_image);
    video_image = NULL;
  }
  video = NULL;
}

/* Sprite cache hash table */
Image::ImageCache Image::image_cache;

void
Image::cache_image(uint64_t id, Image *image) {
  image_cache[id] = image;
}

/* Return a pointer to the sprite pointer associated with id. */
Image *
Image::get_cached_image(uint64_t id) {
  ImageCache::iterator result = image_cache.find(id);
  if (result == image_cache.end()) {
    return NULL;
  }
  return result->second;
}

void
Image::clear_cache() {
  while (!image_cache.empty()) {
    Image *image = image_cache.begin()->second;
    image_cache.erase(image_cache.begin());
    delete image;
  }
}

Graphics *Graphics::instance = NULL;

Graphics::Graphics() throw(ExceptionFreeserf) {
  if (instance != NULL) {
    throw ExceptionGFX("Unable to create second instance.");
  }

  try {
    video = Video::get_instance();
  } catch (ExceptionVideo &e) {
    throw ExceptionGFX(e.what());
  }

  Data *data = Data::get_instance();
  DataSource *data_source = data->get_data_source();
  Sprite *sprite = data_source->get_transparent_sprite(DATA_CURSOR, 0);
  video->set_cursor(sprite->get_data(), sprite->get_width(),
                    sprite->get_height());
  delete sprite;

  Graphics::instance = this;
}

Graphics::~Graphics() {
  Image::clear_cache();

  if (video != NULL) {
    delete video;
    video = NULL;
  }

  Graphics::instance = NULL;
}

Graphics*
Graphics::get_instance() {
  if (instance == NULL) {
    instance = new Graphics();
  }
  return instance;
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
Frame::draw_sprite(int x, int y, unsigned int sprite) {
  uint64_t id = Sprite::create_sprite_id(sprite, 0, 0);
  Image *image = Image::get_cached_image(id);
  if (image == NULL) {
    Sprite *s = data_source->get_sprite(sprite);
    if (s == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      return;
    }

    image = new Image(video, s);
    Image::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/

void
Frame::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                            unsigned char color_off, float progress) {
  uint64_t id = Sprite::create_sprite_id(sprite, 0, color_off);
  Image *image = Image::get_cached_image(id);
  if (image == NULL) {
    Sprite *s = data_source->get_transparent_sprite(sprite, color_off);
    if (s == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      return;
    }

    image = new Image(video, s);
    Image::cache_image(id, image);

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
Frame::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off) {
  draw_transp_sprite(x, y, sprite, use_off, 0, 1.f);
}

void
Frame::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                          float progress) {
  draw_transp_sprite(x, y, sprite, use_off, 0, progress);
}

void
Frame::draw_transp_sprite(int x, int y, unsigned int sprite, bool use_off,
                          unsigned char color_offs) {
  draw_transp_sprite(x, y, sprite, use_off, color_offs, 1.f);
}

void
Frame::draw_transp_sprite_relatively(int x, int y, unsigned int sprite,
                                     unsigned int offs_sprite) {
  Sprite *s = data_source->get_empty_sprite(offs_sprite);
  x += s->get_delta_x();
  y += s->get_delta_y();

  draw_transp_sprite(x, y, sprite, true, 0, 1.f);
  delete s;
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
Frame::draw_masked_sprite(int x, int y, unsigned int mask,
                          unsigned int sprite) {
  uint64_t id = Sprite::create_sprite_id(sprite, mask, 0);
  Image *image = Image::get_cached_image(id);
  if (image == NULL) {
    Sprite *s = data_source->get_sprite(sprite);
    if (s == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      return;
    }

    Sprite *m = data_source->get_mask_sprite(mask);
    if (m == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      delete s;
      return;
    }

    Sprite *masked = s->get_masked(m);
    if (masked == NULL) {
      Log::Warn["graphics"] << "Failed to apply mask #" << mask
                            << " to sprite #" << sprite;
      return;
    }

    delete s;
    delete m;

    s = masked;

    image = new Image(video, s);
    Image::cache_image(id, image);

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
Frame::draw_overlay_sprite(int x, int y, unsigned int sprite) {
  draw_overlay_sprite(x, y, sprite, 1.f);
}

void
Frame::draw_overlay_sprite(int x, int y, unsigned int sprite,
                             float progress) {
  uint64_t id = Sprite::create_sprite_id(sprite, 0, 0);
  Image *image = Image::get_cached_image(id);
  if (image == NULL) {
    Sprite *s = data_source->get_overlay_sprite(sprite);
    if (s == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      return;
    }

    image = new Image(video, s);
    Image::cache_image(id, image);

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
Frame::draw_waves_sprite(int x, int y, unsigned int mask, unsigned int sprite) {
  uint64_t id = Sprite::create_sprite_id(sprite, mask, 0);
  Image *image = Image::get_cached_image(id);
  if (image == NULL) {
    Sprite *s = data_source->get_transparent_sprite(sprite, 0);
    if (s == NULL) {
      Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
      return;
    }

    if (mask > 0) {
      Sprite *m = data_source->get_mask_sprite(mask);
      if (m == NULL) {
        Log::Warn["graphics"] << "Failed to decode sprite #" << sprite;
        return;
      }

      Sprite *masked = s->get_masked(m);
      if (masked == NULL) {
        Log::Warn["graphics"] << "Failed to apply mask #" << mask
                              << " to sprite #" << sprite;
        return;
      }

      delete s;
      delete m;

      s = masked;
    }

    image = new Image(video, s);
    Image::cache_image(id, image);

    delete s;
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw a character at x, y in the dest frame. */
void
Frame::draw_char_sprite(int x, int y, unsigned char c, unsigned char color,
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
Frame::draw_string(int x, int y, unsigned char color, int shadow,
                   const std::string &str) {
  for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
/*
    if (string_bg) {
      fill_rect(x, y, 8, 8, 0);
    }
*/

    draw_char_sprite(x, y, *it, color, shadow);

    x += 8;
  }
}

/* Draw the number n at x, y in the dest frame. */
void
Frame::draw_number(int x, int y, unsigned char color, int shadow, int n) {
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
Frame::draw_rect(int x, int y, int width, int height, unsigned char color) {
  Color col = data_source->get_color(color);
  Video::Color c = { col.red, col.green, col.blue, col.alpha };
  video->draw_rect(x, y, width, height, c, video_frame);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
Frame::fill_rect(int x, int y, int width, int height, unsigned char color) {
  Color col = data_source->get_color(color);
  Video::Color c = { col.red, col.green, col.blue, col.alpha };
  video->fill_rect(x, y, width, height, c, video_frame);
}

/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
Frame::Frame(Video *video, unsigned int width, unsigned int height) {
  this->video = video;
  video_frame = video->create_frame(width, height);
  owner = true;
  Data *data = Data::get_instance();
  data_source = data->get_data_source();
}

Frame::Frame(Video *video, Video::Frame *video_frame) {
  this->video = video;
  this->video_frame = video_frame;
  owner = false;
  Data *data = Data::get_instance();
  data_source = data->get_data_source();
}

/* Deinitialize frame and backing surface. */
Frame::~Frame() {
  if (owner) {
    video->destroy_frame(video_frame);
  }
  video_frame = NULL;
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
Frame::draw_frame(int dx, int dy, int sx, int sy, Frame *src,
                  int w, int h) {
  video->draw_frame(dx, dy, video_frame, sx, sy, src->video_frame, w, h);
}

Frame *
Graphics::create_frame(unsigned int width, unsigned int height) {
  return new Frame(video, width, height);
}

/* Enable or disable fullscreen mode */
void
Graphics::set_fullscreen(bool enable) {
  video->set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
bool
Graphics::is_fullscreen() {
  return video->is_fullscreen();
}

Frame *
Graphics::get_screen_frame() {
  return new Frame(video, video->get_screen_frame());
}

void
Graphics::set_resolution(unsigned int width, unsigned int height,
                      bool fullscreen) {
  video->set_resolution(width, height, fullscreen);
}

void
Graphics::get_resolution(unsigned int *width, unsigned int *height) {
  video->get_resolution(width, height);
}

void
Graphics::swap_buffers() {
  video->swap_buffers();
}

float
Graphics::get_zoom_factor() {
  return video->get_zoom_factor();
}

bool
Graphics::set_zoom_factor(float factor) {
  return video->set_zoom_factor(factor);
}
