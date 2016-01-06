/*
 * data-source-dos.h - DOS game resources file functions
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_DATA_SOURCE_DOS_H_
#define SRC_DATA_SOURCE_DOS_H_

#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Sprite object. Contains RGBA data. */
class sprite_t {
 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;
  uint8_t *data;

 public:
  sprite_t(unsigned int width, unsigned int height);
  virtual ~sprite_t();

  uint8_t *get_data() const { return data; }
  unsigned int get_width() const { return width; }
  unsigned int get_height() const { return height; }
  int get_delta_x() const { return delta_x; }
  int get_delta_y() const { return delta_y; }
  int get_offset_x() const { return offset_x; }
  int get_offset_y() const { return offset_y; }

  void set_offset(int x, int y) { offset_x = x; offset_y = y; }
  void set_delta(int x, int y) { delta_x = x; delta_y = y; }

  sprite_t *get_masked(sprite_t *mask);

  static uint64_t create_sprite_id(uint64_t sprite, uint64_t mask,
                                   uint64_t offset);
};

class animation_t {
 public:
  uint8_t time;
  int8_t x;
  int8_t y;
};

typedef struct {
  unsigned char red;
  unsigned char green;
  unsigned char blue;
  unsigned char alpha;
} color_t;

class data_source_t {
 protected:
  /* These entries follow the 8 byte header of the data file. */
  typedef struct {
    uint32_t size;
    uint32_t offset;
  } spae_entry_t;

  /* Sprite header. In the data file this is
   * immediately followed by sprite data. */
  typedef struct {
    int8_t b_x;
    int8_t b_y;
    uint16_t w;
    uint16_t h;
    int16_t x;
    int16_t y;
  } sprite_dos_t;

  typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
  } color_dos_t;

  void *sprites;
#ifdef HAVE_MMAP
  int mapped;
#endif
  size_t sprites_size;
  size_t entry_count;
  uint32_t *serf_animation_table;

 public:
  data_source_t();
  virtual ~data_source_t();

  bool load(const std::string &path);

  void *get_object(unsigned int index, size_t *size);

  sprite_t *get_sprite(unsigned int index);
  sprite_t *get_empty_sprite(unsigned int index);
  sprite_t *get_transparent_sprite(unsigned int index, int color_off);
  sprite_t *get_overlay_sprite(unsigned int index);
  sprite_t *get_mask_sprite(unsigned int index);

  color_t get_color(unsigned int index);

  animation_t *get_animation(unsigned int animation, unsigned int phase);

  void *get_sound(unsigned int index, size_t *size);
  void *get_music(unsigned int index, size_t *size);

 protected:
  sprite_t *create_empty_sprite(unsigned int index, uint8_t **source);
  void fixup();
  bool load_animation_table();
  color_dos_t *get_palette(unsigned int index);
  sprite_t *get_bitmap_sprite(unsigned int index, unsigned int value);
};

#endif  // SRC_DATA_SOURCE_DOS_H_
