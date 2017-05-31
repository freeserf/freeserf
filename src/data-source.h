/*
 * data-source.h - Game resources file functions
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

#ifndef SRC_DATA_SOURCE_H_
#define SRC_DATA_SOURCE_H_

#include <string>
#include <cstdint>

#include "src/data.h"

/* Sprite object. Contains BGRA data. */
class Sprite {
 public:
  typedef struct Color {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char alpha;
  } Color;

 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  unsigned int width;
  unsigned int height;
  uint8_t *data;

 public:
  Sprite();
  explicit Sprite(Sprite *base);
  Sprite(unsigned int w, unsigned int h);
  virtual ~Sprite();

  virtual uint8_t *get_data() const { return data; }
  virtual unsigned int get_width() const { return width; }
  virtual unsigned int get_height() const { return height; }
  virtual int get_delta_x() const { return delta_x; }
  virtual int get_delta_y() const { return delta_y; }
  virtual int get_offset_x() const { return offset_x; }
  virtual int get_offset_y() const { return offset_y; }

  virtual Sprite *get_masked(Sprite *mask);
  virtual Sprite *create_mask(Sprite *other);
  virtual Sprite *create_diff(Sprite *other);
  virtual void fill(Sprite::Color color);
  virtual void fill_masked(Sprite::Color color);
  virtual void add(Sprite *other);

  virtual void stick(Sprite *sticker, unsigned int x, unsigned int y);

  static uint64_t create_id(uint64_t resource, uint64_t index,
                            uint64_t mask_resource, uint64_t mask_index,
                            const Color &color);

 protected:
  void create(unsigned int w, unsigned int h);
};

class Animation {
 public:
  uint8_t time;
  int8_t x;
  int8_t y;
};

class DataSource {
 protected:
  std::string path;
  bool loaded;

 public:
  explicit DataSource(const std::string &path);
  virtual ~DataSource() {}

  virtual std::string get_name() const = 0;
  virtual std::string get_path() const { return path; }
  virtual bool is_loaded() const { return loaded; }
  virtual unsigned int get_scale() const = 0;
  virtual unsigned int get_bpp() const = 0;

  virtual bool check() = 0;
  virtual bool load() = 0;

  virtual Sprite *get_sprite(Data::Resource res, unsigned int index,
                             const Sprite::Color &color) = 0;

  virtual Animation *get_animation(unsigned int animation,
                                   unsigned int phase) = 0;

  virtual void *get_sound(unsigned int index, size_t *size) = 0;
  virtual void *get_music(unsigned int index, size_t *size) = 0;

  bool check_file(const std::string &path);
  void *file_read(const std::string &path, size_t *size);
  bool file_write(const std::string &path, void *data, size_t size);
};

typedef std::shared_ptr<DataSource> PDataSource;

#endif  // SRC_DATA_SOURCE_H_
