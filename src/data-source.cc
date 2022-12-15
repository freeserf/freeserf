/*
 * data-source.cc - Game resources file functions
 *
 * Copyright (C) 2015-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>

#include "src/freeserf_endian.h"
#include "src/log.h"
#include "src/tpwm.h"
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

DataSourceBase::DataSourceBase(const std::string &_path)
  : path(_path)
  , loaded(false) {
}

bool
DataSourceBase::check_file(const std::string &fpath) {
  struct stat info;

  if (stat(fpath.c_str(), &info) != 0) {
    return false;
  }

  if (info.st_mode & S_IFDIR) {
    return false;
  }

  return true;
}

SpriteBase::SpriteBase()
  : delta_x(0)
  , delta_y(0)
  , offset_x(0)
  , offset_y(0)
  , width(0)
  , height(0)
  , data(nullptr) {
}

SpriteBase::SpriteBase(Data::PSprite base)
  : delta_x(base->get_delta_x())
  , delta_y(base->get_delta_y())
  , offset_x(base->get_offset_x())
  , offset_y(base->get_offset_y())
  , data(nullptr) {
  create(base->get_width(), base->get_height());
}

SpriteBase::SpriteBase(unsigned int w, unsigned int h)
  : delta_x(0)
  , delta_y(0)
  , offset_x(0)
  , offset_y(0)
  , data(nullptr) {
  create(w, h);
}

SpriteBase::~SpriteBase() {
  if (data != nullptr) {
    delete[] data;
    data = nullptr;
  }
}

void
SpriteBase::create(size_t w, size_t h) {
  if (data != nullptr) {
    delete[] data;
    data = nullptr;
  }

  width = w;
  height = h;
  data =  new uint8_t[width * height * 4];
}

// Apply mask to map tile sprite
// The resulting sprite will be extended to the height of the mask
// by repeating lines from the top of the sprite. The width of the
// mask and the sprite must be identical.
Data::PSprite
SpriteBase::get_masked(Data::PSprite mask) {
  if (mask->get_width() > width) {
    throw ExceptionFreeserf("Failed to apply mask to sprite");
  }

  Data::PSprite masked = std::make_shared<SpriteBase>(mask);

  uint32_t *pos = reinterpret_cast<uint32_t*>(masked->get_data());

  uint32_t *s_beg = reinterpret_cast<uint32_t*>(data);
  uint32_t *s_pos = s_beg;
  uint32_t *s_end = s_beg + (width * height);
  size_t s_delta = width - masked->get_width();

  uint32_t *m_pos = reinterpret_cast<uint32_t*>(mask->get_data());

  for (size_t y = 0; y < masked->get_height(); y++) {
    for (size_t x = 0; x < masked->get_width(); x++) {
      if (s_pos >= s_end) {
        s_pos = s_beg;
      }
      *pos++ = *s_pos++ & *m_pos++;
    }
    s_pos += s_delta;
  }

  return masked;
}

Data::PSprite
SpriteBase::create_mask(Data::PSprite other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return nullptr;
  }

  Data::PSprite result = std::make_shared<SpriteBase>(shared_from_this());

  uint32_t *src1 = reinterpret_cast<uint32_t*>(data);
  uint32_t *src2 = reinterpret_cast<uint32_t*>(other->get_data());
  uint32_t *res = reinterpret_cast<uint32_t*>(result->get_data());

  for (unsigned int i = 0; i < width * height; i++) {
    if (*src1++ == *src2++) {
      *res++ = 0x00000000;
    } else {
      *res++ = 0xFFFFFFFF;
    }
  }

  return result;
}

void
SpriteBase::fill(Data::Sprite::Color color) {
  Data::Sprite::Color *c = reinterpret_cast<Data::Sprite::Color*>(data);
  for (unsigned int i = 0; i < (width * height); i++) {
    *c++ = color;
  }
}

void
SpriteBase::fill_masked(Data::Sprite::Color color) {
  Data::Sprite::Color *res = reinterpret_cast<Data::Sprite::Color*>(data);

  for (unsigned int i = 0; i < width * height; i++) {
    if ((res->alpha & 0xFF) != 0x00) {
      *res = color;
    }
    res++;
  }
}

void
SpriteBase::add(Data::PSprite other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return;
  }

  uint32_t *src = reinterpret_cast<uint32_t*>(other->get_data());
  uint32_t *res = reinterpret_cast<uint32_t*>(data);

  for (unsigned int i = 0; i < width * height; i++) {
    *res++ += *src++;
  }

  return;
}

void
SpriteBase::del(Data::PSprite other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return;
  }

  uint32_t *src = reinterpret_cast<uint32_t*>(other->get_data());
  uint32_t *res = reinterpret_cast<uint32_t*>(data);

  for (unsigned int i = 0; i < width * height; i++) {
    if (*src++ == 0xFFFFFFFF) {
      *res = 0x00000000;
    }
    res++;
  }

  return;
}

void
SpriteBase::blend(Data::PSprite other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return;
  }

#define UNMULTIPLY(color, a) ((0xFF * (color)) / (a))
#define BLEND(back, front, a) (((front) * (a)) + ((back) * (0xFF - (a)))) / 0xFF

  Color *c = reinterpret_cast<Color*>(data);
  Color *o = reinterpret_cast<Color*>(other->get_data());
  for (unsigned int i = 0; i < width * height; i++) {
    const uint32_t alpha = o->alpha;

    if (alpha == 0x00) {
      c++;
      o++;
      continue;
    }

    if (alpha == 0xFF) {
      *c++ = *o++;
      continue;
    }

    const uint8_t backR = c->red;
    const uint8_t backG = c->green;
    const uint8_t backB = c->blue;

    const uint8_t frontR = UNMULTIPLY(o->red, alpha);
    const uint8_t frontG = UNMULTIPLY(o->green, alpha);
    const uint8_t frontB = UNMULTIPLY(o->blue, alpha);

    const uint32_t R = BLEND(backR, frontR, alpha);
    const uint32_t G = BLEND(backG, frontG, alpha);
    const uint32_t B = BLEND(backB, frontB, alpha);

    *c++ = {(uint8_t)B, (uint8_t)G, (uint8_t)R, 0xFF};
    o++;
  }

  delta_x = other->get_delta_x();
  delta_y = other->get_delta_y();
}

void
SpriteBase::make_alpha_mask() {
  Color *c = reinterpret_cast<Color*>(data);
  uint8_t min = 0xFF;
  for (unsigned int i = 0; i < width * height; i++) {
    if (c->alpha != 0x00) {
      c->alpha = 0xff - static_cast<uint8_t>((0.21 * c->red) +
                                             (0.72 * c->green) +
                                             (0.07 * c->blue));
      c->red = 0;
      c->green = 0;
      c->blue = 0;
      min = std::min(min, c->alpha);
    }
    c++;
  }

  c = reinterpret_cast<Color*>(data);
  for (unsigned int i = 0; i < width * height; i++) {
    if (c->alpha != 0x00) {
      c->alpha = c->alpha - min;
    }
    c++;
  }
}

void
SpriteBase::stick(Data::PSprite sticker, unsigned int dx, unsigned int dy) {
  Color *base = reinterpret_cast<Color*>(data);
  Color *stkr = reinterpret_cast<Color*>(sticker->get_data());
  size_t w = std::min(width, sticker->get_width());
  size_t h = std::min(height, sticker->get_height());

  base += dy * width;
  for (size_t y = 0; y < w; y++) {
    base += dx;
    for (size_t x = 0; x < h; x++) {
      Color pixel = *stkr++;
      if ((pixel.alpha & 0xFF) != 0x00) {
        *base = pixel;
      }
      base++;
    }
  }

  delta_x = sticker->get_delta_x();
  delta_y = sticker->get_delta_y();
}

// Calculate hash of sprite identifier.
uint64_t
Data::Sprite::create_id(uint64_t resource, uint64_t index,
                        uint64_t mask_resource, uint64_t mask_index,
                        const Sprite::Color &color) {
  // 0xFF00000000000000
  uint64_t result = static_cast<uint64_t>(resource & 0xFF) << 56;
  // 0x00FFF00000000000
  result |= static_cast<uint64_t>(index & 0xFFF) << 44;
  // 0x00000FF000000000
  result |= static_cast<uint64_t>(mask_resource & 0xFF) << 36;
  // 0x0000000FFF000000
  result |= static_cast<uint64_t>(mask_index & 0xFFF) << 24;
  // 0x0000000000FF0000
  result |= static_cast<uint64_t>(color.red & 0xFF) << 16;
  // 0x000000000000FF00
  result |= static_cast<uint64_t>(color.green & 0xFF) << 8;
  // 0x00000000000000FF
  result |= static_cast<uint64_t>(color.blue & 0xFF) << 0;
  return result;
}

// NOTE that 'index' is the relative index within the Data::Resource type
//  and the base resource type starting index is added in get_sprite_parts
//  when get_object is called to get the "from zero" data object id
Data::PSprite
//DataSourceBase::get_sprite(Data::Resource res, size_t index, const Data::Sprite::Color &color) {
DataSourceBase::get_sprite(Data::Resource res, size_t index, const Data::Sprite::Color &color, bool darken) {
  Log::Debug["data-source.cc"] << "inside DataSourceBase::get_sprite, res type " << res << ", sprite index " << index << ", darken bool is " << darken;
  
  //Data::MaskImage ms = get_sprite_parts(res, index);
  Data::MaskImage ms = get_sprite_parts(res, index, darken);
  Data::PSprite mask = std::get<0>(ms);
  Data::PSprite image = std::get<1>(ms);

  if (mask) {
    mask->fill_masked(color);
    if (image) {
      mask->blend(image);
      return mask;
    }
    return mask;
  }
  return image;
}

Data::MaskImage
DataSourceBase::separate_sprites(Data::PSprite s1, Data::PSprite s2) {
  if (!s1 || !s2) {
    return std::make_tuple(nullptr, nullptr);
  }

  Data::PSprite filled = s1->create_mask(s2);
  Data::PSprite masked = s1->get_masked(filled);
  masked->make_alpha_mask();
  s1->del(filled);
  s1->stick(masked, 0, 0);

  return std::make_tuple(filled, s1);
}

size_t
DataSourceBase::get_animation_phase_count(size_t animation) {
  if (animation >= animation_table.size()) {
    Log::Error["data"] << "Failed to get phase count for animation #"
    << animation
    << " (got only " << animation_table.size()
    << " animations)";
    return 0;
  }

  return animation_table[animation].size();
}

Data::Animation
DataSourceBase::get_animation(size_t animation, size_t phase) {
  phase >>= 3;
  if ((animation >= animation_table.size()) ||
      (phase >= animation_table[animation].size())) {
    Log::Error["data"] << "Failed to get animation #" << animation
    << " phase #" << phase
    << " (got only " << animation_table[animation].size()
    << " phases)";
    return {0, 0, 0};
  }

  return animation_table[animation][phase];
}
