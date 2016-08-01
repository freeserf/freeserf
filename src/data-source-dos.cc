/*
 * data-source-dos.cc - DOS game resources file functions
 *
 * Copyright (C) 2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/data-source-dos.h"

#include <cassert>
#include <algorithm>
#include <fstream>

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

DataSourceDOS::DataSourceDOS() {
  sprites = NULL;
  sprites_size = 0;
  entry_count = 0;
  animation_table = NULL;
}

DataSourceDOS::~DataSourceDOS() {
  if (sprites != NULL) {
    free(sprites);
    sprites = NULL;
  }

  if (animation_table != NULL) {
    delete[] animation_table;
    animation_table = NULL;
  }
}

bool
DataSourceDOS::check(const std::string &path, std::string *load_path) {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    NULL
  };

  for (const char **df = default_data_file; *df != NULL; df++) {
    std::string file_path = path + '/' + *df;
    Log::Info["data"] << "Looking for game data in '"
                      << file_path.c_str() << "'...";
    if (check_file(file_path)) {
      if (load_path != NULL) {
        *load_path = file_path;
      }
      return true;
    }
  }

  return false;
}

bool
DataSourceDOS::load(const std::string &path) {
  sprites = file_read(path, &sprites_size);
  if (sprites == NULL) {
    return false;
  }

  /* Check that data file is decompressed. */
  if (tpwm_is_compressed(sprites, sprites_size)) {
    Log::Verbose["data"] << "Data file is compressed";
    void *uncompressed = NULL;
    size_t uncmpsd_size = 0;
    const char *error = NULL;
    if (!tpwm_uncompress(sprites, sprites_size,
                         &uncompressed, &uncmpsd_size,
                         &error)) {
      Log::Error["tpwm"] << error;
      Log::Error["data"] << "Data file is broken!";
      return false;
    }
    free(sprites);
    sprites = uncompressed;
    sprites_size = uncmpsd_size;
  }

  /* Read the number of entries in the index table.
     Some entries are undefined (size and offset are zero). */
  entry_count = *(reinterpret_cast<uint32_t*>(sprites) + 1);
  entry_count = le32toh(entry_count) + 1;

  fixup();

  return load_animation_table();
}

/* Return a pointer to the data object at index.
 If size is non-NULL it will be set to the size of the data object.
 (There's no guarantee that size is correct!). */
void *
DataSourceDOS::get_object(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  if (index <= 0 && index >= entry_count) {
    return NULL;
  }

  SpaeEntry *entries = reinterpret_cast<SpaeEntry*>(sprites);
  uint8_t *bytes = reinterpret_cast<uint8_t*>(sprites);

  size_t offset = le32toh(entries[index].offset);
  if (offset == 0) {
    return NULL;
  }

  if (size != NULL) {
    *size = le32toh(entries[index].size);
  }

  return &bytes[offset];
}

/* Perform various fixups of the data file entries. */
void
DataSourceDOS::fixup() {
  SpaeEntry *entries = reinterpret_cast<SpaeEntry*>(sprites);

  /* Fill out some undefined spaces in the index from other
     places in the data file index. */

  for (int i = 0; i < 48; i++) {
    for (int j = 1; j < 6; j++) {
      entries[3450+6*i+j].size = entries[3450+6*i].size;
      entries[3450+6*i+j].offset = entries[3450+6*i].offset;
    }
  }

  for (int i = 0; i < 3; i++) {
    entries[3765+i].size = entries[3762+i].size;
    entries[3765+i].offset = entries[3762+i].offset;
  }

  for (int i = 0; i < 6; i++) {
    entries[1363+i].size = entries[1352].size;
    entries[1363+i].offset = entries[1352].offset;
  }

  for (int i = 0; i < 6; i++) {
    entries[1613+i].size = entries[1602].size;
    entries[1613+i].offset = entries[1602].offset;
  }
}

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

/* Create sprite object */
Sprite *
DataSourceDOS::get_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  Color *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new SpriteDosSolid(data, size, palette);
}

DataSourceDOS::SpriteDosSolid::SpriteDosSolid(void *data, size_t size,
                                              Color *palette)
  : SpriteDosBase(data, size) {
  size -= sizeof(dos_sprite_header_t);
  if (size != width * height) {
    assert(0);
  }

  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    Color color = palette[*src++];
    *dest++ = color.b; /* Blue */
    *dest++ = color.g; /* Green */
    *dest++ = color.r; /* Red */
    *dest++ = 0xff;    /* Alpha */
  }
}

Sprite *
DataSourceDOS::get_empty_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  return new SpriteDosEmpty(data, size);
}

/* Create transparent sprite object */
Sprite *
DataSourceDOS::get_transparent_sprite(unsigned int index, int color_off) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  Color *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new SpriteDosTransparent(data, size, palette, color_off);
}

DataSourceDOS::SpriteDosTransparent::SpriteDosTransparent(void *data,
                                                          size_t size,
                                                          Color *palette,
                                                          int color_off)
  : SpriteDosBase(data, size) {
  size -= sizeof(dos_sprite_header_t);
  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    size_t drop = *src++;
    for (size_t i = 0; i < drop; i++) {
      *dest++ = 0x00; /* Blue */
      *dest++ = 0x00; /* Green */
      *dest++ = 0x00; /* Red */
      *dest++ = 0x00; /* Alpha */
    }

    size_t fill = *src++;
    for (size_t i = 0; i < fill; i++) {
      unsigned int p_index = *src++ + color_off;
      Color color = palette[p_index];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = 0xff;    /* Alpha */
    }
  }
}

Sprite *
DataSourceDOS::get_overlay_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  Color *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new SpriteDosOverlay(data, size, palette, 0x80);
}

DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay(void *data, size_t size,
                                                  Color *palette,
                                                  unsigned char value)
  : SpriteDosBase(data, size) {
  size -= sizeof(dos_sprite_header_t);
  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    size_t drop = *src++;
    for (size_t i = 0; i < drop; i++) {
      *dest++ = 0x00; /* Blue */
      *dest++ = 0x00; /* Green */
      *dest++ = 0x00; /* Red */
      *dest++ = 0x00; /* Alpha */
    }

    size_t fill = *src++;
    for (size_t i = 0; i < fill; i++) {
      Color color = palette[value];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = value;   /* Alpha */
    }
  }
}

Sprite *
DataSourceDOS::get_mask_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  return new SpriteDosMask(data, size);
}

DataSourceDOS::SpriteDosMask::SpriteDosMask(void *data, size_t size)
  : SpriteDosBase(data, size) {
  size -= sizeof(dos_sprite_header_t);
  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    size_t drop = *src++;
    for (size_t i = 0; i < drop; i++) {
      *dest++ = 0x00; /* Blue */
      *dest++ = 0x00; /* Green */
      *dest++ = 0x00; /* Red */
      *dest++ = 0x00; /* Alpha */
    }

    size_t fill = *src++;
    for (size_t i = 0; i < fill; i++) {
      *dest++ = 0xFF; /* Blue */
      *dest++ = 0xFF; /* Green */
      *dest++ = 0xFF; /* Red */
      *dest++ = 0xFF; /* Alpha */
    }
  }
}


DataSourceDOS::SpriteDosEmpty::SpriteDosEmpty(void *data, size_t size) {
  if (size < sizeof(dos_sprite_header_t)) {
    assert(0);
  }

  dos_sprite_header_t *sprite = reinterpret_cast<dos_sprite_header_t*>(data);
  delta_x = sprite->b_x;
  delta_y = sprite->b_y;
  offset_x = sprite->x;
  offset_y = sprite->y;
  width = sprite->w;
  height = sprite->h;
}

/* Apply mask to map tile sprite
 The resulting sprite will be extended to the height of the mask
 by repeating lines from the top of the sprite. The width of the
 mask and the sprite must be identical. */
Sprite *
DataSourceDOS::SpriteDosBase::get_masked(Sprite *mask) {
  if (mask->get_width() > width) {
    assert(0);
  }

  Sprite *masked = new SpriteDosBase(mask);

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

DataSourceDOS::SpriteDosBase::SpriteDosBase(void *data, size_t size)
  : SpriteDosEmpty(data, size) {
  this->data = new uint8_t[width * height * 4];
}

DataSourceDOS::SpriteDosBase::SpriteDosBase(Sprite *base) {
  width = base->get_width();
  height = base->get_height();
  delta_x = 0;
  delta_y = 0;
  offset_x = base->get_offset_x();
  offset_y = base->get_offset_y();
  data = new uint8_t[width * height * 4];
}

DataSourceDOS::SpriteDosBase::~SpriteDosBase() {
  if (data != NULL) {
    delete[] data;
    data = NULL;
  }
}

::Color
DataSourceDOS::get_color(unsigned int index) {
  DataSourceDOS::Color *palette = get_palette(DATA_PALETTE_GAME);
  ::Color color = { palette[index].r,
                    palette[index].g,
                    palette[index].b,
                    0xff };
  return color;
}

bool
DataSourceDOS::load_animation_table() {
  /* The serf animation table is stored in big endian
   order in the data file.

   * The first uint32 is the byte length of the rest
   of the table.
   * Next is 199 uint32s that are offsets from the start
   of this table to an animation table (one for each
   animation).
   * The animation tables are of varying lengths.
   Each entry in the animation table is three bytes
   long. First byte is used to determine the serf body
   sprite. Second byte is a signed horizontal sprite
   offset. Third byte is a signed vertical offset.
   */

  size_t size = 0;
  uint32_t *animation_block =
    reinterpret_cast<uint32_t*>(get_object(DATA_SERF_ANIMATION_TABLE, &size));
  if (animation_block == NULL) {
    return false;
  }

  if (size != be32toh(animation_block[0])) {
    Log::Error["data"] << "Could not extract animation table.";
    return false;
  }
  animation_block++;

  animation_table = new Animation*[200];
  /* Endianess convert from big endian. */
  for (int i = 0; i < 200; i++) {
    int offset = be32toh(animation_block[i]);
    animation_table[i] =
      reinterpret_cast<Animation*>(
        reinterpret_cast<char*>(animation_block) + offset);
  }

  return true;
}

Animation*
DataSourceDOS::get_animation(unsigned int animation, unsigned int phase) {
  if (animation > 199) {
    return NULL;
  }
  Animation *animation_phase = animation_table[animation] + (phase >> 3);

  return animation_phase;
}

void *
DataSourceDOS::get_sound(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  size_t sfx_size = 0;
  void *data = get_object(DATA_SFX_BASE + index, &sfx_size);
  if (data == NULL) {
    Log::Error["data"] << "Could not extract SFX clip: #" << index;
    return NULL;
  }

  void *wav = sfx2wav(data, sfx_size, size, -0x20);
  if (wav == NULL) {
    Log::Error["data"] << "Could not convert SFX clip to WAV: #" << index;
    return NULL;
  }

  return wav;
}

void *
DataSourceDOS::get_music(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  size_t xmi_size = 0;
  void *data = get_object(DATA_MUSIC_GAME + index, &xmi_size);
  if (data == NULL) {
    Log::Error["data"] << "Could not extract XMI clip: #" << index;
    return NULL;
  }

  void *mid = xmi2mid(data, xmi_size, size);
  if (mid == NULL) {
    Log::Error["data"] << "Could not convert XMI clip to MID: #" << index;
    return NULL;
  }

  return mid;
}

DataSourceDOS::Color *
DataSourceDOS::get_palette(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if ((data == NULL) || (size != sizeof(Color)*256)) {
    return NULL;
  }

  return reinterpret_cast<Color*>(data);
}
