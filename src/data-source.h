/*
 * data-source.h - Game resources file functions
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

#ifndef SRC_DATA_SOURCE_H_
#define SRC_DATA_SOURCE_H_

#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

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
 public:
  virtual ~data_source_t() {}

  virtual bool check(const std::string &path, std::string *load_path) = 0;
  virtual bool load(const std::string &path) = 0;

  virtual sprite_t *get_sprite(unsigned int index) = 0;
  virtual sprite_t *get_empty_sprite(unsigned int index) = 0;
  virtual sprite_t *get_transparent_sprite(unsigned int index,
                                           int color_off) = 0;
  virtual sprite_t *get_overlay_sprite(unsigned int index) = 0;
  virtual sprite_t *get_mask_sprite(unsigned int index) = 0;

  virtual color_t get_color(unsigned int index) = 0;

  virtual animation_t *get_animation(unsigned int animation,
                                     unsigned int phase) = 0;

  virtual void *get_sound(unsigned int index, size_t *size) = 0;
  virtual void *get_music(unsigned int index, size_t *size) = 0;

  bool check_file(const std::string &path);
  void *file_read(const std::string &path, size_t *size);
};

#endif  // SRC_DATA_SOURCE_H_
