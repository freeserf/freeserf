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

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/freeserf_endian.h"
  #include "src/log.h"
END_EXT_C
#include "src/tpwm.h"
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

data_source_dos_t::data_source_dos_t() {
  sprites = NULL;
  sprites_size = 0;
  entry_count = 0;
  animation_table = NULL;
}

data_source_dos_t::~data_source_dos_t() {
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
data_source_dos_t::check(const std::string &path, std::string *load_path) {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    NULL
  };

  for (const char **df = default_data_file; *df != NULL; df++) {
    std::string file_path = path + '/' + *df;
    LOGI("data", "Looking for game data in '%s'...", file_path.c_str());
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
data_source_dos_t::load(const std::string &path) {
  sprites = file_read(path, &sprites_size);
  if (sprites == NULL) {
    return false;
  }

  /* Check that data file is decompressed. */
  if (tpwm_is_compressed(sprites, sprites_size)) {
    LOGV("data", "Data file is compressed");
    void *uncompressed = NULL;
    size_t uncmpsd_size = 0;
    const char *error = NULL;
    if (!tpwm_uncompress(sprites, sprites_size,
                         &uncompressed, &uncmpsd_size,
                         &error)) {
      LOGE("tpwm", error);
      LOGE("data", "Data file is broken!");
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
data_source_dos_t::get_object(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  if (index <= 0 && index >= entry_count) {
    return NULL;
  }

  spae_entry_t *entries = reinterpret_cast<spae_entry_t*>(sprites);
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
data_source_dos_t::fixup() {
  spae_entry_t *entries = reinterpret_cast<spae_entry_t*>(sprites);

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
sprite_t *
data_source_dos_t::get_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new sprite_dos_solid_t(data, size, palette);
}

sprite_dos_solid_t::sprite_dos_solid_t(void *data, size_t size,
                                       color_dos_t *palette)
  : sprite_dos_base_t(data, size) {
  size -= sizeof(dos_sprite_header_t);
  if (size != width * height) {
    assert(0);
  }

  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    color_dos_t color = palette[*src++];
    *dest++ = color.b; /* Blue */
    *dest++ = color.g; /* Green */
    *dest++ = color.r; /* Red */
    *dest++ = 0xff;    /* Alpha */
  }
}

sprite_t *
data_source_dos_t::get_empty_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  return new sprite_dos_empty_t(data, size);
}

/* Create transparent sprite object */
sprite_t *
data_source_dos_t::get_transparent_sprite(unsigned int index, int color_off) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new sprite_dos_transparent_t(data, size, palette, color_off);
}

sprite_dos_transparent_t::sprite_dos_transparent_t(void *data, size_t size,
                                                   color_dos_t *palette,
                                                   int color_off)
  : sprite_dos_base_t(data, size) {
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
      uint p_index = *src++ + color_off;
      color_dos_t color = palette[p_index];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = 0xff;    /* Alpha */
    }
  }
}

sprite_t *
data_source_dos_t::get_overlay_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);
  if (palette == NULL) {
    return NULL;
  }

  return new sprite_dos_overlay_t(data, size, palette, 0x80);
}

sprite_dos_overlay_t::sprite_dos_overlay_t(void *data, size_t size,
                                           color_dos_t *palette,
                                           unsigned char value)
  : sprite_dos_base_t(data, size) {
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
      color_dos_t color = palette[value];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = value;   /* Alpha */
    }
  }
}

sprite_t *
data_source_dos_t::get_mask_sprite(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if (data == NULL) {
    return NULL;
  }

  return new sprite_dos_mask_t(data, size);
}

sprite_dos_mask_t::sprite_dos_mask_t(void *data, size_t size)
  : sprite_dos_base_t(data, size) {
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


sprite_dos_empty_t::sprite_dos_empty_t(void *data, size_t size) {
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
sprite_t *
sprite_dos_base_t::get_masked(sprite_t *mask) {
  if (mask->get_width() > width) {
    assert(0);
  }

  sprite_t *masked = new sprite_dos_base_t(mask);

  uint8_t *pos = masked->get_data();

  uint8_t *s_pos = data;
  uint8_t *s_end = s_pos + (width * height * 4);
  size_t s_delta = (width - masked->get_width())*4;

  uint8_t *m_pos = mask->get_data();

  for (size_t y = 0; y < masked->get_height(); y++) {
    for (size_t x = 0; x < masked->get_width(); x++) {
      if (s_pos > s_end) {
        s_pos = data;
      }
      *pos++ = *s_pos++ & *m_pos++;
      *pos++ = *s_pos++ & *m_pos++;
      *pos++ = *s_pos++ & *m_pos++;
      *pos++ = *s_pos++ & *m_pos++;
    }
    s_pos += s_delta;
  }

  return masked;
}

sprite_dos_base_t::sprite_dos_base_t(void *data, size_t size)
  : sprite_dos_empty_t(data, size) {
  this->data = new uint8_t[width * height * 4];
}

sprite_dos_base_t::sprite_dos_base_t(sprite_t *base) {
  width = base->get_width();
  height = base->get_height();
  delta_x = 0;
  delta_y = 0;
  offset_x = base->get_offset_x();
  offset_y = base->get_offset_y();
  data = new uint8_t[width * height * 4];
}

sprite_dos_base_t::~sprite_dos_base_t() {
  if (data != NULL) {
    delete[] data;
    data = NULL;
  }
}

color_t
data_source_dos_t::get_color(unsigned int index) {
  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);
  color_t color = { palette[index].r,
                    palette[index].g,
                    palette[index].b,
                    0xff };
  return color;
}

bool
data_source_dos_t::load_animation_table() {
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
    LOGE("data", "Could not extract animation table.");
    return false;
  }
  animation_block++;

  animation_table = new animation_t*[200];
  /* Endianess convert from big endian. */
  for (int i = 0; i < 200; i++) {
    int offset = be32toh(animation_block[i]);
    animation_table[i] =
      reinterpret_cast<animation_t*>(
        reinterpret_cast<char*>(animation_block) + offset);
  }

  return true;
}

animation_t*
data_source_dos_t::get_animation(unsigned int animation, unsigned int phase) {
  if (animation > 199) {
    return NULL;
  }
  animation_t *animation_phase = animation_table[animation] + (phase >> 3);

  return animation_phase;
}

void *
data_source_dos_t::get_sound(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  size_t sfx_size = 0;
  void *data = get_object(DATA_SFX_BASE + index, &sfx_size);
  if (data == NULL) {
    LOGE("data", "Could not extract SFX clip: #%d.", index);
    return NULL;
  }

  void *wav = sfx2wav(data, sfx_size, size, -0x20);
  if (wav == NULL) {
    LOGE("data", "Could not convert SFX clip to WAV: #%d.", index);
    return NULL;
  }

  return wav;
}

void *
data_source_dos_t::get_music(unsigned int index, size_t *size) {
  if (size != NULL) {
    *size = 0;
  }

  size_t xmi_size = 0;
  void *data = get_object(DATA_MUSIC_GAME + index, &xmi_size);
  if (data == NULL) {
    LOGE("data", "Could not extract XMI clip: #%d.", index);
    return NULL;
  }

  void *mid = xmi2mid(data, xmi_size, size);
  if (mid == NULL) {
    LOGE("data", "Could not convert XMI clip to MID: #%d.", index);
    return NULL;
  }

  return mid;
}

color_dos_t *
data_source_dos_t::get_palette(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if ((data == NULL) || (size != sizeof(color_dos_t)*256)) {
    return NULL;
  }

  return reinterpret_cast<color_dos_t*>(data);
}
