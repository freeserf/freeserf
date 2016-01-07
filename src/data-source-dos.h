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

/* Sprite object. Contains BGRA data. */
class sprite_t {
 public:
  virtual ~sprite_t() {}

  virtual uint8_t *get_data() const = 0;
  virtual unsigned int get_width() const = 0;
  virtual unsigned int get_height() const = 0;
  virtual int get_delta_x() const = 0;
  virtual int get_delta_y() const = 0;
  virtual int get_offset_x() const = 0;
  virtual int get_offset_y() const = 0;

  virtual sprite_t *get_masked(sprite_t *mask) = 0;

  static uint64_t create_sprite_id(uint64_t sprite, uint64_t mask,
                                   uint64_t offset);
};

class sprite_dos_empty_t : public sprite_t {
 protected:
  /* Sprite header. In the data file this is
   * immediately followed by sprite data. */
  typedef struct {
    int8_t b_x;
    int8_t b_y;
    uint16_t w;
    uint16_t h;
    int16_t x;
    int16_t y;
  } dos_sprite_header_t;

  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;

 public:
  sprite_dos_empty_t() {}
  sprite_dos_empty_t(void *data, size_t size);
  virtual ~sprite_dos_empty_t() {}

  virtual uint8_t *get_data() const { return NULL; }
  virtual unsigned int get_width() const { return width; }
  virtual unsigned int get_height() const { return height; }
  virtual int get_delta_x() const { return delta_x; }
  virtual int get_delta_y() const { return delta_y; }
  virtual int get_offset_x() const { return offset_x; }
  virtual int get_offset_y() const { return offset_y; }

  virtual sprite_t *get_masked(sprite_t *mask) { return NULL; }

  static uint64_t create_sprite_id(uint64_t sprite, uint64_t mask,
                                   uint64_t offset);
};

typedef struct {
  unsigned char r;
  unsigned char g;
  unsigned char b;
} color_dos_t;

class sprite_dos_base_t : public sprite_dos_empty_t {
 protected:
  uint8_t *data;

 public:
  sprite_dos_base_t(void *data, size_t size);
  explicit sprite_dos_base_t(sprite_t *base);
  virtual ~sprite_dos_base_t();

  virtual uint8_t *get_data() const { return data; }
  virtual sprite_t *get_masked(sprite_t *mask);
};

class sprite_dos_solid_t : public sprite_dos_base_t {
 public:
  sprite_dos_solid_t(void *data, size_t size, color_dos_t *palette);
  virtual ~sprite_dos_solid_t() {}
};

class sprite_dos_transparent_t : public sprite_dos_base_t {
 public:
  sprite_dos_transparent_t(void *data, size_t size, color_dos_t *palette,
                           int color_off);
  virtual ~sprite_dos_transparent_t() {}
};

class sprite_dos_overlay_t : public sprite_dos_base_t {
 public:
  sprite_dos_overlay_t(void *data, size_t size, color_dos_t *palette,
                       unsigned char value);
  virtual ~sprite_dos_overlay_t() {}
};

class sprite_dos_mask_t : public sprite_dos_base_t {
 public:
  sprite_dos_mask_t(void *data, size_t size);
  virtual ~sprite_dos_mask_t() {}
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

  void *sprites;
  size_t sprites_size;
  size_t entry_count;
  animation_t **animation_table;

 public:
  data_source_t();
  virtual ~data_source_t();

  bool check(const std::string &path, std::string *load_path);
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

  static bool check_file(const std::string &path);
  void *file_read(const std::string &path, size_t *size);

 protected:
  void fixup();
  bool load_animation_table();
  color_dos_t *get_palette(unsigned int index);
};

#endif  // SRC_DATA_SOURCE_DOS_H_
