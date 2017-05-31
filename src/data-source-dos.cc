/*
 * data-source-dos.cc - DOS game resources file functions
 *
 * Copyright (C) 2014-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#define DATA_SERF_ANIMATION_TABLE  2
#define DATA_PALETTE_GAME          3
#define DATA_SERF_ARMS             1850  // 3, dos_sprite_type_transparent
/* Sound effects (index 0 is undefined). */
#define DATA_SFX_BASE              3900
/* XMI music (similar to MIDI). */
#define DATA_MUSIC_GAME            3990
#define DATA_MUSIC_ENDING          3992

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

DataSourceDOS::Resource dos_resources[] = {
  {   0, 0,    DataSourceDOS::SpriteTypeUnknown     },  // none
  {   1, 3997, DataSourceDOS::SpriteTypeSolid       },  // art_landscape
  {   2, 0,    DataSourceDOS::SpriteTypeUnknown     },  // animation
  {   4, 3,    DataSourceDOS::SpriteTypeOverlay     },  // serf_shadow
  {   5, 3,    DataSourceDOS::SpriteTypeSolid       },  // dotted_lines
  {  15, 3997, DataSourceDOS::SpriteTypeSolid       },  // art_flag
  {  25, 3,    DataSourceDOS::SpriteTypeSolid       },  // art_box
  {  40, 3998, DataSourceDOS::SpriteTypeSolid       },  // credits_bg
  {  41, 3998, DataSourceDOS::SpriteTypeSolid       },  // logo
  {  42, 3,    DataSourceDOS::SpriteTypeSolid       },  // symbol
  {  60, 3,    DataSourceDOS::SpriteTypeMask        },  // map_mask_up
  { 141, 3,    DataSourceDOS::SpriteTypeMask        },  // map_mask_down
  { 230, 3,    DataSourceDOS::SpriteTypeMask        },  // path_mask
  { 260, 3,    DataSourceDOS::SpriteTypeSolid       },  // map_ground
  { 300, 3,    DataSourceDOS::SpriteTypeSolid       },  // path_ground
  { 321, 3,    DataSourceDOS::SpriteTypeTransparent },  // game_object
  { 600, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_top
  { 610, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_border
  { 630, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_waves
  { 660, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_popup
  { 670, 3,    DataSourceDOS::SpriteTypeSolid       },  // indicator
  { 750, 3,    DataSourceDOS::SpriteTypeTransparent },  // font
  { 810, 3,    DataSourceDOS::SpriteTypeTransparent },  // font_shadow
  { 870, 3,    DataSourceDOS::SpriteTypeSolid       },  // icon
  {1250, 3,    DataSourceDOS::SpriteTypeTransparent },  // map_object
  {1500, 3,    DataSourceDOS::SpriteTypeOverlay     },  // map_shadow
  {1750, 3,    DataSourceDOS::SpriteTypeSolid       },  // panel_button
  {1780, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_bottom
  {2500, 3,    DataSourceDOS::SpriteTypeTransparent },  // serf_torso
  {3150, 3,    DataSourceDOS::SpriteTypeTransparent },  // serf_head
  {3880, 3,    DataSourceDOS::SpriteTypeSolid       },  // frame_split
  {3900, 0,    DataSourceDOS::SpriteTypeUnknown     },  // sound
  {3990, 0,    DataSourceDOS::SpriteTypeUnknown     },  // music
  {3999, 3,    DataSourceDOS::SpriteTypeTransparent },  // cursor
  {   3, 0,    DataSourceDOS::SpriteTypeUnknown     }   // palette
};

DataSourceDOS::DataSourceDOS(const std::string &_path)
  : DataSource(_path)
  , sprites(nullptr)
  , sprites_size(0)
  , entry_count(0)
  , animation_table(nullptr) {
}

DataSourceDOS::~DataSourceDOS() {
  if (sprites != nullptr) {
    free(sprites);
    sprites = nullptr;
  }

  if (animation_table != nullptr) {
    delete[] animation_table;
    animation_table = nullptr;
  }
}

bool
DataSourceDOS::check() {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    nullptr
  };

  if (check_file(path)) {
    return true;
  }

  for (const char **df = default_data_file; *df != NULL; df++) {
    std::string file_path = path + '/' + *df;
    Log::Info["data"] << "Looking for game data in '"
                      << file_path.c_str() << "'...";
    if (check_file(file_path)) {
      path = file_path;
      return true;
    }
  }

  return false;
}

bool
DataSourceDOS::load() {
  if (!check()) {
    return false;
  }

  sprites = file_read(path, &sprites_size);
  if (sprites == nullptr) {
    return false;
  }

  /* Check that data file is decompressed. */
  if (tpwm_is_compressed(sprites, sprites_size)) {
    Log::Verbose["data"] << "Data file is compressed";
    void *uncompressed = nullptr;
    size_t uncmpsd_size = 0;
    const char *error = nullptr;
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

  if (!load_animation_table()) {
    return false;
  }

  loaded = true;

  return true;
}

/* Return a pointer to the data object at index.
 If size is non-NULL it will be set to the size of the data object.
 (There's no guarantee that size is correct!). */
void *
DataSourceDOS::get_object(unsigned int index, size_t *size) {
  if (size != nullptr) {
    *size = 0;
  }

  // first entry is whole file itself
  if ((index <= 0) || (index >= entry_count)) {
    return nullptr;
  }

  SpaeEntry *entries = reinterpret_cast<SpaeEntry*>(sprites);
  uint8_t *bytes = reinterpret_cast<uint8_t*>(sprites);

  size_t offset = le32toh(entries[index].offset);
  if (offset == 0) {
    return nullptr;
  }

  if (size != nullptr) {
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
DataSourceDOS::get_sprite(Data::Resource res, unsigned int index,
                          const Sprite::Color &color) {
  if (index >= Data::get_resource_count(res)) {
    return nullptr;
  }

  Resource &dos_res = dos_resources[res];

  ColorDOS *palette = get_dos_palette(dos_res.dos_palette);
  if (palette == nullptr) {
    return nullptr;
  }

  if (res == Data::AssetSerfTorso) {
    size_t size = 0;
    void *data = get_object(dos_res.index + index, &size);
    if (data == nullptr) {
      return nullptr;
    }
    SpriteBaseDOS *torso = new SpriteDosTransparent(data, size, palette, 64);

    data = get_object(dos_res.index + index, &size);
    if (data == nullptr) {
      delete torso;
      return nullptr;
    }
    SpriteBaseDOS *torso2 = new SpriteDosTransparent(data, size, palette, 72);
    Sprite *filled = torso->create_mask(torso2);
    filled->fill_masked({0xe3, 0xe3, 0x00, 0xff});
    Sprite *diff = torso->create_diff(filled);
    filled->fill_masked(color);
    filled->add(diff);
    torso->stick(filled, 0, 0);
    delete torso2;
    delete filled;
    delete diff;
    data = get_object(DATA_SERF_ARMS + index, &size);
    SpriteBaseDOS *arms = new SpriteDosTransparent(data, size, palette);
    torso->stick(arms, 0, 0);
    delete arms;
    return torso;
  } else if (res == Data::AssetMapObject) {
    if ((index >= 128) && (index <= 143)) {
      int flag_frame = (index - 128) % 4;
      size_t size = 0;
      void *data = get_object(dos_res.index + 128 + flag_frame, &size);
      if (data == nullptr) {
        return nullptr;
      }
      Sprite *s1 = new SpriteDosTransparent(data, size, palette);
      data = get_object(dos_res.index + 128 + 4 + flag_frame, &size);
      if (data == nullptr) {
        delete s1;
        return nullptr;
      }
      Sprite *s2 = new SpriteDosTransparent(data, size, palette);
      Sprite *filled = s1->create_mask(s2);
      filled->fill_masked({0xe3, 0xe3, 0x00, 0xff});
      Sprite *diff = s1->create_diff(filled);
      filled->fill_masked(color);
      filled->add(diff);
      s1->stick(filled, 0, 0);
      delete s2;
      delete filled;
      delete diff;
      return s1;
    }
  } else if (res == Data::AssetFont || res == Data::AssetFontShadow) {
    size_t size = 0;
    void *data = get_object(dos_res.index + index, &size);
    if (data == nullptr) {
      return nullptr;
    }
    Sprite *res = new SpriteDosTransparent(data, size, palette);
    res->fill_masked(color);
    return res;
  }

  size_t size = 0;
  void *data = get_object(dos_res.index + index, &size);
  if (data == nullptr) {
    return nullptr;
  }

  Sprite *sprite = nullptr;
  switch (dos_res.sprite_type) {
    case SpriteTypeSolid: {
      sprite = new SpriteDosSolid(data, size, palette);
      break;
    }
    case SpriteTypeTransparent: {
      sprite = new SpriteDosTransparent(data, size, palette);
      break;
    }
    case SpriteTypeOverlay: {
      sprite = new SpriteDosOverlay(data, size, palette, 0x80);
      break;
    }
    case SpriteTypeMask: {
      sprite = new SpriteDosMask(data, size);
      break;
    }
    default:
      return nullptr;
  }

  return sprite;
}

DataSourceDOS::SpriteDosSolid::SpriteDosSolid(void *data, size_t size,
                                              ColorDOS *palette)
  : SpriteBaseDOS(data, size) {
  size -= sizeof(dos_sprite_header_t);
  if (size != width * height) {
    assert(0);
  }

  uint8_t *src = reinterpret_cast<uint8_t*>(data) + sizeof(dos_sprite_header_t);
  uint8_t *end = src + size;
  uint8_t *dest = this->data;

  while (src < end) {
    ColorDOS color = palette[*src++];
    *dest++ = color.b; /* Blue */
    *dest++ = color.g; /* Green */
    *dest++ = color.r; /* Red */
    *dest++ = 0xff;    /* Alpha */
  }
}

DataSourceDOS::SpriteDosTransparent::SpriteDosTransparent(void *data,
                                                          size_t size,
                                                          ColorDOS *palette,
                                                          uint8_t color)
  : SpriteBaseDOS(data, size) {
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
      unsigned int p_index = *src++ + color;  // color_off;
      ColorDOS color = palette[p_index];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = 0xff;    /* Alpha */
    }
  }
}

DataSourceDOS::SpriteDosOverlay::SpriteDosOverlay(void *data, size_t size,
                                                  ColorDOS *palette,
                                                  unsigned char value)
  : SpriteBaseDOS(data, size) {
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
      ColorDOS color = palette[value];
      *dest++ = color.b; /* Blue */
      *dest++ = color.g; /* Green */
      *dest++ = color.r; /* Red */
      *dest++ = value;   /* Alpha */
    }
  }
}

DataSourceDOS::SpriteDosMask::SpriteDosMask(void *data, size_t size)
  : SpriteBaseDOS(data, size) {
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


DataSourceDOS::SpriteBaseDOS::SpriteBaseDOS(void *data, size_t size) {
  if (size < sizeof(dos_sprite_header_t)) {
    assert(0);
  }

  dos_sprite_header_t *sprite = reinterpret_cast<dos_sprite_header_t*>(data);
  delta_x = sprite->b_x;
  delta_y = sprite->b_y;
  offset_x = sprite->x;
  offset_y = sprite->y;

  create(sprite->w, sprite->h);
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
    return nullptr;
  }
  Animation *animation_phase = animation_table[animation] + (phase >> 3);

  return animation_phase;
}

void *
DataSourceDOS::get_sound(unsigned int index, size_t *size) {
  if (size != nullptr) {
    *size = 0;
  }

  size_t sfx_size = 0;
  void *data = get_object(DATA_SFX_BASE + index, &sfx_size);
  if (data == nullptr) {
    Log::Error["data"] << "Could not extract SFX clip: #" << index;
    return nullptr;
  }

  void *wav = sfx2wav(data, sfx_size, size, -0x20);
  if (wav == nullptr) {
    Log::Error["data"] << "Could not convert SFX clip to WAV: #" << index;
    return nullptr;
  }

  return wav;
}

void *
DataSourceDOS::get_music(unsigned int index, size_t *size) {
  if (size != nullptr) {
    *size = 0;
  }

  size_t xmi_size = 0;
  void *data = get_object(DATA_MUSIC_GAME + index, &xmi_size);
  if (data == nullptr) {
    Log::Error["data"] << "Could not extract XMI clip: #" << index;
    return nullptr;
  }

  void *mid = xmi2mid(data, xmi_size, size);
  if (mid == nullptr) {
    Log::Error["data"] << "Could not convert XMI clip to MID: #" << index;
    return nullptr;
  }

  return mid;
}

DataSourceDOS::ColorDOS *
DataSourceDOS::get_dos_palette(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if ((data == nullptr) || (size != sizeof(ColorDOS)*256)) {
    return nullptr;
  }

  return reinterpret_cast<ColorDOS*>(data);
}
