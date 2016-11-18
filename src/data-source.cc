/*
 * data-source.cc - Game resources file functions
 *
 * Copyright (C) 2015-2016  Wicked_Digger <wicked_digger@mail.ru>
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

#include <algorithm>
#include <fstream>
#include <cassert>

#include "src/freeserf_endian.h"
#include "src/log.h"
#include "src/tpwm.h"
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

bool
DataSource::check_file(const std::string &path) {
  std::ifstream ifile(path.c_str());
  if (ifile.good()) {
    ifile.close();
    return true;
  }

  return false;
}

void*
DataSource::file_read(const std::string &path, size_t *size) {
  char *data = NULL;
  *size = 0;

  do {
    std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
    if (!file.good()) {
      break;
    }

    *size = (size_t)file.tellg();
    if (*size == 0) {
      break;
    }

    file.seekg(0, file.beg);

    data = reinterpret_cast<char*>(malloc(*size));
    if (data == NULL) {
      *size = 0;
      break;
    }

    file.read(data, *size);
    file.close();
  } while (false);

  return data;
}

bool
DataSource::file_write(const std::string &path, void *data, size_t size) {
  std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }

  file.write(reinterpret_cast<char*>(data), size);
  file.close();

  return true;
}

Sprite::Sprite() {
  data = nullptr;
}

Sprite::Sprite(Sprite *base) {
  data = nullptr;
  delta_x = 0;
  delta_y = 0;
  offset_x = base->get_offset_x();
  offset_y = base->get_offset_y();
  create(base->get_width(), base->get_height());
}

Sprite::Sprite(unsigned int w, unsigned int h) {
  data = nullptr;
  create(w, h);
}

Sprite::~Sprite() {
  if (data != nullptr) {
    delete[] data;
    data = nullptr;
  }
}

void
Sprite::create(unsigned int w, unsigned int h) {
  if (data != nullptr) {
    delete[] data;
    data = nullptr;
  }

  width = w;
  height = h;
  data =  new uint8_t[width * height * 4];
}

/* Apply mask to map tile sprite
 The resulting sprite will be extended to the height of the mask
 by repeating lines from the top of the sprite. The width of the
 mask and the sprite must be identical. */
Sprite *
Sprite::get_masked(Sprite *mask) {
  if (mask->get_width() > width) {
    assert(0);
  }

  Sprite *masked = new Sprite(mask);

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

Sprite *
Sprite::create_mask(Sprite *other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return nullptr;
  }

  Sprite *result = new Sprite(this);

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

Sprite *
Sprite::create_diff(Sprite *other) {
  if ((width != other->get_width()) || (height != other->get_height())) {
    return nullptr;
  }

  Sprite *result = new Sprite(this);

  uint32_t *src1 = reinterpret_cast<uint32_t*>(data);
  uint32_t *src2 = reinterpret_cast<uint32_t*>(other->get_data());
  uint32_t *res = reinterpret_cast<uint32_t*>(result->get_data());

  for (unsigned int i = 0; i < width * height; i++) {
    *res++ = *src1++ - *src2++;
  }

  return result;
}

void
Sprite::fill(Sprite::Color color) {
  Color *c = reinterpret_cast<Color*>(data);
  for (unsigned int i = 0; i < (width * height); i++) {
    *c++ = color;
  }
}

void
Sprite::fill_masked(Sprite::Color color) {
  Color *res = reinterpret_cast<Color*>(data);

  for (unsigned int i = 0; i < width * height; i++) {
    if ((res->alpha & 0xFF) != 0x00) {
      *res = color;
    }
    res++;
  }
}

void
Sprite::add(Sprite *other) {
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
Sprite::stick(Sprite *sticker, unsigned int dx, unsigned int dy) {
  Color *base = reinterpret_cast<Color*>(data);
  Color *stkr = reinterpret_cast<Color*>(sticker->get_data());
  unsigned int w = std::min(width, sticker->get_width());
  unsigned int h = std::min(height, sticker->get_height());

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

/* Calculate hash of sprite identifier. */
uint64_t
Sprite::create_id(uint64_t resource, uint64_t index,
                  uint64_t mask_resource, uint64_t mask_index,
                  const Sprite::Color &color) {
  uint64_t result = (resource & 0xFF) << 56;  // 0xFF00000000000000
  result |= (index & 0xFFF) << 44;            // 0x00FFF00000000000
  result |= (mask_resource & 0xFF) << 36;     // 0x00000FF000000000
  result |= (mask_index & 0xFFF) << 24;       // 0x0000000FFF000000
  result |= (color.red & 0xFF) << 16;         // 0x0000000000FF0000
  result |= (color.green & 0xFF) << 8;        // 0x000000000000FF00
  result |= (color.blue & 0xFF) << 0;         // 0x00000000000000FF
  return result;
}
