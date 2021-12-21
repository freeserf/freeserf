/*
 * gfx.cc - General graphics and data file functions
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <utility>
#include <algorithm>

#include "src/log.h"
#include "src/data.h"
#include "src/video.h"

const Color Color::black = Color(0x00, 0x00, 0x00);
const Color Color::white = Color(0xff, 0xff, 0xff);
const Color Color::green = Color(0x73, 0xb3, 0x43);
const Color Color::transparent = Color(0x00, 0x00, 0x00, 0x00);
// added by me
const Color Color::red     = Color(207,  99,  99);
const Color Color::yellow  = Color(239, 239, 143);
const Color Color::gold    = Color(255, 255,  55);
const Color Color::dk_gray = Color( 35,  35,  35);
const Color Color::blue    = Color( 44,  44, 255);
const Color Color::lt_gray = Color(188, 188, 188);
const Color Color::cyan    = Color(  0, 227, 227);
const Color Color::magenta = Color(223, 127, 239);

/* original player colors:
  {0x00, 0xe3, 0xe3},  // cyan   player0
  {0xcf, 0x63, 0x63},  // red    player1
  {0xdf, 0x7f, 0xef},  // pink   player2
  {0xef, 0xef, 0x8f},  // yellow player3
*/

// I am not sure why these get_cyan/magenta/etc functions exist
//  when they could just be hardcoded(?)

double
Color::get_cyan() const {
  double k = get_key();
  return (1. - (static_cast<double>(r)/255.) -k ) / (1. - k);
}

double
Color::get_magenta() const {
  double k = get_key();
  return (1. - (static_cast<double>(g)/255.) -k ) / (1. - k);
}

double
Color::get_yellow() const {
  double k = get_key();
  return (1. - (static_cast<double>(b)/255.) -k ) / (1. - k);
}

double
Color::get_key() const {
  return 1. - std::max(static_cast<double>(r)/255.,
                       std::max(static_cast<double>(g)/255.,
                       static_cast<double>(b)/255.));
}

ExceptionGFX::ExceptionGFX(const std::string &description)
  : ExceptionFreeserf(description) {
}

ExceptionGFX::~ExceptionGFX() {
}

Image::Image(Video *_video, Data::PSprite sprite) {
  video = _video;
  width = static_cast<unsigned int>(sprite->get_width());
  height = static_cast<unsigned int>(sprite->get_height());
  offset_x = sprite->get_offset_x();
  offset_y = sprite->get_offset_y();
  delta_x = sprite->get_delta_x();
  delta_y = sprite->get_delta_y();
  video_image = video->create_image(sprite->get_data(), width, height);
}

Image::~Image() {
  if (video_image != nullptr) {
    video->destroy_image(video_image);
    video_image = nullptr;
  }
  video = nullptr;
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
    return nullptr;
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

Graphics *Graphics::instance = nullptr;

Graphics::Graphics() {
  if (instance != nullptr) {
    throw ExceptionGFX("Unable to create second instance.");
  }

  try {
    video = &Video::get_instance();
  } catch (ExceptionVideo &e) {
    throw ExceptionGFX(e.what());
  }

  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();
  Data::PSprite sprite = data_source->get_sprite(Data::AssetCursor, 0,
                                                 {0, 0, 0, 0});
  video->set_cursor(sprite->get_data(),
                    static_cast<unsigned int>(sprite->get_width()),
                    static_cast<unsigned int>(sprite->get_height()));

  Graphics::instance = this;
}

Graphics::~Graphics() {
  Image::clear_cache();
}

Graphics &
Graphics::get_instance() {
  static Graphics graphics;
  return graphics;
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index) {
  draw_sprite(x, y, res, index, false, Color::transparent, 1.f);
}

// this function only seems to affect text as far as I can tell
void
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, const Color &color, float progress) {
  Data::Sprite::Color pc = {color.get_blue(),
                            color.get_green(),
                            color.get_red(),
                            color.get_alpha()};

  uint64_t id = Data::Sprite::create_id(res, index, 0, 0, pc);
  Image *image = Image::get_cached_image(id);
  if (image == nullptr) {
    Data::PSprite s = data_source->get_sprite(res, index, pc);
    if (!s) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }

    image = new Image(video, s);
    Image::cache_image(id, image);
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
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off) {
  draw_sprite(x, y, res, index, use_off, Color::transparent, 1.f);
}

void
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, float progress) {
  draw_sprite(x, y, res, index, use_off, Color::transparent, progress);
}

// this only seems to affect text
void
Frame::draw_sprite(int x, int y, Data::Resource res, unsigned int index,
                   bool use_off, const Color &color) {
  draw_sprite(x, y, res, index, use_off, color, 1.f);
}

void
Frame::draw_sprite_relatively(int x, int y, Data::Resource res,
                              unsigned int index,
                              Data::Resource relative_to_res,
                              unsigned int relative_to_index) {
  Data::PSprite s = data_source->get_sprite(relative_to_res, relative_to_index,
                                            {0, 0, 0, 0});
  if (s == nullptr) {
    Log::Warn["graphics"] << "Failed to decode sprite #"
                          << Data::get_resource_name(res) << ":" << index;
    return;
  }

  x += s->get_delta_x();
  y += s->get_delta_y();

  draw_sprite(x, y, res, index, true, Color::transparent, 1.f);
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
Frame::draw_masked_sprite(int x, int y, Data::Resource mask_res,
                          unsigned int mask_index, Data::Resource res,
                          unsigned int index) {
  uint64_t id = Data::Sprite::create_id(res, index, mask_res, mask_index,
                                        {0, 0, 0, 0});
  Image *image = Image::get_cached_image(id);
  if (image == nullptr) {
    Data::PSprite s = data_source->get_sprite(res, index, {0, 0, 0, 0});
    
    if (!s) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }

    Data::PSprite m = data_source->get_sprite(mask_res, mask_index,
                                              {0, 0, 0, 0});
    if (!m) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(mask_res)
                            << ":" << mask_index;
      return;
    }

    Data::PSprite masked = s->get_masked(m);
    if (!masked) {
      Log::Warn["graphics"] << "Failed to apply mask #"
                            << Data::get_resource_name(mask_res)
                            << ":" << mask_index
                            << " to sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }

    s = std::move(masked);

    image = new Image(video, s);
    Image::cache_image(id, image);
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
void
Frame::draw_waves_sprite(int x, int y, Data::Resource mask_res,
                         unsigned int mask_index, Data::Resource res,
                         unsigned int index) {
  uint64_t id = Data::Sprite::create_id(res, index, mask_res, mask_index,
                                        {0, 0, 0, 0});
  Image *image = Image::get_cached_image(id);
  if (image == nullptr) {
    Data::PSprite s = data_source->get_sprite(res, index, {0, 0, 0, 0});
    if (!s) {
      Log::Warn["graphics"] << "Failed to decode sprite #"
                            << Data::get_resource_name(res) << ":" << index;
      return;
    }

    if (mask_res > 0) {
      Data::PSprite m = data_source->get_sprite(mask_res, mask_index,
                                                {0, 0, 0, 0});
      if (!m) {
        Log::Warn["graphics"] << "Failed to decode sprite #"
                              << Data::get_resource_name(mask_res)
                              << ":" << mask_index;
        return;
      }

      Data::PSprite masked = s->get_masked(m);
      if (!masked) {
        Log::Warn["graphics"] << "Failed to apply mask #"
                              << Data::get_resource_name(mask_res)
                              << ":" << mask_index
                              << " to sprite #"
                              << Data::get_resource_name(res) << ":" << index;
        return;
      }

      s = std::move(masked);
    }

    image = new Image(video, s);
    Image::cache_image(id, image);
  }

  x += image->get_offset_x();
  y += image->get_offset_y();
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

/* Draw a character at x, y in the dest frame. */
void
Frame::draw_char_sprite(int x, int y, unsigned char c, const Color &color,
                        const Color &shadow) {
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

  if (shadow != Color::transparent) {
    draw_sprite(x, y, Data::AssetFontShadow, s, false, shadow);
  }
  draw_sprite(x, y, Data::AssetFont, s, false, color);
}

/* Draw the string str at x, y in the dest frame. */
void
Frame::draw_string(int x, int y, const std::string &str, const Color &color,
                   const Color &shadow) {
  int cx = x;

  for (char c : str) {
    if (c == '\t') {
      cx += 8 * 2;
    } else if (c == '\n') {
      y += 8;
      cx = x;
    } else {
      draw_char_sprite(cx, y, c, color, shadow);
      cx += 8;
    }
  }
}

/* Draw the number n at x, y in the dest frame. */
void
Frame::draw_number(int x, int y, int value, const Color &color,
                   const Color &shadow) {
  if (value < 0) {
    draw_char_sprite(x, y, '-', color, shadow);
    x += 8;
    value *= -1;
  }

  if (value == 0) {
    draw_char_sprite(x, y, '0', color, shadow);
    return;
  }

  int digits = 0;
  for (int i = value; i > 0; i /= 10) digits += 1;

  for (int i = digits-1; i >= 0; i--) {
    draw_char_sprite(x+8*i, y, '0'+(value % 10), color, shadow);
    value /= 10;
  }
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
Frame::draw_rect(int x, int y, int width, int height, const Color &color) {
  Video::Color c = { color.get_red(),
                     color.get_green(),
                     color.get_blue(),
                     color.get_alpha() };
  video->draw_rect(x, y, width, height, c, video_frame);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
Frame::fill_rect(int x, int y, int width, int height, const Color &color) {
  Video::Color c = { color.get_red(),
                     color.get_green(),
                     color.get_blue(),
                     color.get_alpha() };
  video->fill_rect(x, y, width, height, c, video_frame);
}

/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
Frame::Frame(Video *video_, unsigned int width, unsigned int height) {
  video = video_;
  video_frame = video->create_frame(width, height);
  owner = true;
  data_source = Data::get_instance().get_data_source();
}

Frame::Frame(Video *video_, Video::Frame *video_frame_) {
  video = video_;
  video_frame = video_frame_;
  owner = false;
  data_source = Data::get_instance().get_data_source();
}

/* Deinitialize frame and backing surface. */
Frame::~Frame() {
  if (owner) {
    video->destroy_frame(video_frame);
  }
  video_frame = nullptr;
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
Frame::draw_frame(int dx, int dy, int sx, int sy, Frame *src, int w, int h) {
  video->draw_frame(dx, dy, video_frame, sx, sy, src->video_frame, w, h);
}

void
Frame::draw_line(int x, int y, int x1, int y1, const Color &color) {
  Video::Color c = {color.get_red(),
                    color.get_green(),
                    color.get_blue(),
                    color.get_alpha()};
  video->draw_line(x, y, x1, y1, c, video_frame);
}

void
Frame::draw_thick_line(int x, int y, int x1, int y1, const Color &color) {
  Video::Color c = {color.get_red(),
                    color.get_green(),
                    color.get_blue(),
                    color.get_alpha()};
  video->draw_thick_line(x, y, x1, y1, c, video_frame);
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

void
Graphics::get_screen_factor(float *fx, float *fy) {
  video->get_screen_factor(fx, fy);
}
