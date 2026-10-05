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

#include <functional>

#include <utility>
#include <vector>
#include <algorithm>

#include "src/log.h"
#include "src/data.h"
#include "src/font.h"
#include "src/sprite-file.h"
#include "src/video.h"

const Color Color::black = Color(0x00, 0x00, 0x00);
const Color Color::white = Color(0xff, 0xff, 0xff);
const Color Color::green = Color(0x73, 0xb3, 0x43);
const Color Color::transparent = Color(0x00, 0x00, 0x00, 0x00);

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

Graphics::Graphics()
  : map_zoom(1.f) {
  if (instance != nullptr) {
    throw ExceptionGFX("Unable to create second instance.");
  }

  try {
    video = &Video::get_instance();
  } catch (ExceptionVideo &e) {
    throw ExceptionGFX(e.what());
  }

  set_cursor_from_data();

  font.reset(new Font());
  if (!font->load_resource("settlers.ttf")) {
    Log::Warn["graphics"] << "Failed to load the font settlers.ttf, the "
                          << "texts are drawn with the font of the game data";
    font.reset();
  }

  Graphics::instance = this;
}

void
Graphics::set_cursor_from_data() {
  Data::PSource data_source = Data::get_instance().get_data_source();
  Data::PSprite sprite = data_source->get_sprite(Data::AssetCursor, 0,
                                                 {0, 0, 0, 0});
  if (sprite) {
    video->set_cursor(sprite->get_data(),
                      static_cast<unsigned int>(sprite->get_width()),
                      static_cast<unsigned int>(sprite->get_height()));
  }
}

void
Graphics::data_changed() {
  Image::clear_cache();
  set_cursor_from_data();
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

    Data::PSprite masked = data_source->apply_mask(res, s, mask_res,
                                                   mask_index, m);
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

/* A glyph of the font in one color, or its shadow: the glyph moved by a
   pixel left, right, up and down, as the shadow sprites of the font. */
class SpriteGlyph : public SpriteBase {
 public:
  SpriteGlyph(const Font::Glyph &glyph, Data::Sprite::Color color,
              bool shadow) {
    unsigned int border = shadow ? 1 : 0;
    create(glyph.width + 2 * border, glyph.height + 2 * border);
    fill({0, 0, 0, 0});
    offset_x = glyph.left - static_cast<int>(border);
    offset_y = glyph.top - static_cast<int>(border);
    Data::Sprite::Color *pixels = reinterpret_cast<Data::Sprite::Color*>(data);
    for (unsigned int y = 0; y < glyph.height; y++) {
      for (unsigned int x = 0; x < glyph.width; x++) {
        if (!glyph.get(x, y)) continue;
        if (shadow) {
          pixels[(y + 1) * width + x] = color;
          pixels[(y + 1) * width + x + 2] = color;
          pixels[y * width + x + 1] = color;
          pixels[(y + 2) * width + x + 1] = color;
        } else {
          pixels[y * width + x] = color;
        }
      }
    }
  }
};

/* The glyph of a character, a question mark for one the font has not. */
static const Font::Glyph *
get_glyph(Font *font, uint32_t ch) {
  const Font::Glyph *glyph = font->get_glyph(ch);
  return (glyph != nullptr) ? glyph : font->get_glyph('?');
}

int
Frame::draw_glyph(int x, int y, uint32_t ch, const Color &color,
                  const Color &shadow) {
  const Font::Glyph *glyph = get_glyph(Graphics::get_instance().get_font(),
                                       ch);
  if (glyph == nullptr) return 0;
  if (glyph->width == 0) return glyph->advance;  /* A space. */

  for (int pass = (shadow != Color::transparent) ? 0 : 1; pass < 2; pass++) {
    const Color &c = (pass == 0) ? shadow : color;
    Data::Sprite::Color pc = {c.get_blue(), c.get_green(), c.get_red(),
                              c.get_alpha()};
    /* Cached apart from the sprites of the game data (bit 62), the
       shadows apart from the glyphs (bit 61). */
    uint64_t id = (static_cast<uint64_t>(1) << 62) |
                  (static_cast<uint64_t>(pass == 0) << 61) |
                  (static_cast<uint64_t>(ch & 0x1fffff) << 24) |
                  (static_cast<uint64_t>(pc.red) << 16) |
                  (static_cast<uint64_t>(pc.green) << 8) | pc.blue;
    Image *image = Image::get_cached_image(id);
    if (image == nullptr) {
      image = new Image(video, std::make_shared<SpriteGlyph>(*glyph, pc,
                                                             pass == 0));
      Image::cache_image(id, image);
    }
    video->draw_image(image->get_video_image(), x + image->get_offset_x(),
                      y + image->get_offset_y(), 0, video_frame);
  }
  return glyph->advance;
}

/* The Unicode characters of a line of UTF-8; a byte that is not UTF-8 is
   the character of its value (Latin-1). A tab is two spaces. */
static std::vector<std::vector<uint32_t>>
split_lines(const std::string &str) {
  std::vector<std::vector<uint32_t>> lines(1);
  for (size_t i = 0; i < str.size();) {
    uint8_t c = static_cast<uint8_t>(str[i]);
    int length = (c >= 0xf0) ? 4 : (c >= 0xe0) ? 3 : (c >= 0xc0) ? 2 : 1;
    uint32_t ch = c;
    if (length > 1 && i + length <= str.size()) {
      ch = c & (0x3f >> (length - 1));
      for (int k = 1; k < length; k++) {
        uint8_t next = static_cast<uint8_t>(str[i + k]);
        if ((next & 0xc0) != 0x80) {
          length = 1;
          ch = c;
          break;
        }
        ch = (ch << 6) | (next & 0x3f);
      }
    } else {
      length = 1;
    }
    i += length;

    if (ch == '\n') {
      lines.emplace_back();
    } else if (ch == '\t') {
      lines.back().insert(lines.back().end(), 2, ' ');
    } else {
      lines.back().push_back(ch);
    }
  }
  return lines;
}

/* Draw the string str at x, y in the dest frame. */
void
Frame::draw_string(int x, int y, const std::string &str, const Color &color,
                   const Color &shadow) {
  Font *font = Graphics::get_instance().get_font();
  for (const std::vector<uint32_t> &line : split_lines(str)) {
    if (font == nullptr) {
      /* The font sprites of the game data, 8 pixels wide. */
      for (size_t i = 0; i < line.size(); i++) {
        if (line[i] < 0x100) {
          draw_char_sprite(x + 8 * static_cast<int>(i), y,
                           static_cast<unsigned char>(line[i]), color, shadow);
        }
      }
      y += 8;
      continue;
    }

    /* Parts of the line: words with one space between them. */
    size_t start = 0;
    while (start < line.size()) {
      if (line[start] == ' ') {
        start++;
        continue;
      }
      size_t end = start;
      while (end < line.size() &&
             (line[end] != ' ' ||
              (end + 1 < line.size() && line[end + 1] != ' '))) {
        end++;
      }

      int width = 0;
      for (size_t i = start; i < end; i++) {
        const Font::Glyph *glyph = get_glyph(font, line[i]);
        width += (glyph != nullptr) ? glyph->advance : 0;
      }
      /* The first part at x, the others centred where they were. */
      int cx = x;
      if (start > 0) {
        cx += 4 * static_cast<int>(start + end) - width / 2;
      }
      for (size_t i = start; i < end; i++) {
        cx += draw_glyph(cx, y, line[i], color, shadow);
      }
      start = end;
    }
    y += 8;
  }
}

int
Frame::get_string_width(const std::string &str) {
  Font *font = Graphics::get_instance().get_font();
  int result = 0;
  for (const std::vector<uint32_t> &line : split_lines(str)) {
    int width = 0;
    for (uint32_t ch : line) {
      if (font == nullptr) {
        width += 8;
      } else {
        const Font::Glyph *glyph = get_glyph(font, ch);
        width += (glyph != nullptr) ? glyph->advance : 0;
      }
    }
    result = std::max(result, width);
  }
  return result;
}

/* Draw the number n at x, y in the dest frame. */
void
Frame::draw_number(int x, int y, int value, const Color &color,
                   const Color &shadow) {
  draw_string(x, y, std::to_string(value), color, shadow);
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
Frame::draw_resource_image(int x, int y, const std::string &name) {
  /* Cached apart from the sprites of the game data (bit 63). */
  uint64_t id = (static_cast<uint64_t>(1) << 63) |
                (std::hash<std::string>()(name) >> 1);
  Image *image = Image::get_cached_image(id);
  if (image == nullptr) {
    PSpriteFile sprite = std::make_shared<SpriteFile>();
    if (!sprite->load_resource(name)) {
      Log::Warn["graphics"] << "Failed to load resource image " << name;
      return;
    }
    image = new Image(video, sprite);
    Image::cache_image(id, image);
  }
  video->draw_image(image->get_video_image(), x, y, 0, video_frame);
}

void
Frame::draw_frame_scaled(int dx, int dy, int dw, int dh, Frame *src,
                         int w, int h) {
  video->draw_frame_scaled(dx, dy, dw, dh, video_frame, src->video_frame,
                           w, h);
}

void
Frame::draw_line(int x, int y, int x1, int y1, const Color &color) {
  Video::Color c = {color.get_red(),
                    color.get_green(),
                    color.get_blue(),
                    color.get_alpha()};
  video->draw_line(x, y, x1, y1, c, video_frame);
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
