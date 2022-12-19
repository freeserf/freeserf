/*
 * data-source.h - Game resources file functions
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

#ifndef SRC_DATA_SOURCE_H_
#define SRC_DATA_SOURCE_H_

#include <string>
#include <memory>
#include <tuple>
#include <vector>

#include "src/data.h"
#include "src/debug.h"

class Buffer;
typedef std::shared_ptr<Buffer> PBuffer;

// Sprite object.
// Contains BGRA data.
class SpriteBase : public Data::Sprite {
 protected:
  int delta_x;
  int delta_y;
  int offset_x;
  int offset_y;
  size_t width;
  size_t height;
  uint8_t *data;

 public:
  SpriteBase();
  explicit SpriteBase(Data::PSprite base);
  SpriteBase(unsigned int w, unsigned int h);
  virtual ~SpriteBase();

  virtual uint8_t *get_data() const { return data; }
  virtual size_t get_width() const { return width; }
  virtual size_t get_height() const { return height; }
  virtual int get_delta_x() const { return delta_x; }
  virtual int get_delta_y() const { return delta_y; }
  virtual int get_offset_x() const { return offset_x; }
  virtual int get_offset_y() const { return offset_y; }

  virtual Data::PSprite get_masked(Data::PSprite mask);
  virtual Data::PSprite create_mask(Data::PSprite other);
  virtual void fill(Sprite::Color color);
  virtual void fill_masked(Sprite::Color color);
  virtual void add(Data::PSprite other);
  virtual void del(Data::PSprite other);
  virtual void blend(Data::PSprite other);
  virtual void make_alpha_mask();

  virtual void stick(Data::PSprite sticker, unsigned int x, unsigned int y);

 protected:
  void create(size_t w, size_t h);
};

class DataSourceBase : public Data::Source {
 public:
  typedef std::tuple<Data::Sprite, Data::Sprite> MaskImage;

 protected:
  std::string path;
  bool loaded;
  std::vector<std::vector<Data::Animation>> animation_table;

 public:
  explicit DataSourceBase(const std::string &path);
  virtual ~DataSourceBase() {}

  virtual std::string get_name() const = 0;
  virtual std::string get_path() const { return path; }
  virtual bool is_loaded() const { return loaded; }
  virtual unsigned int get_scale() const = 0;
  virtual unsigned int get_bpp() const = 0;

  virtual bool check() = 0;
  virtual bool load() = 0;

  //virtual Data::PSprite get_sprite(Data::Resource res, size_t index, const Data::Sprite::Color &color);
  virtual Data::PSprite get_sprite(Data::Resource res, size_t index, const Data::Sprite::Color &color, bool darken = false);  // for FogOfWar

  //virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index) = 0;
  virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index, bool darken = false) = 0;

  virtual size_t get_animation_phase_count(size_t animation);
  virtual Data::Animation get_animation(size_t animation, size_t phase);

  virtual PBuffer get_sound(size_t index) = 0;
  virtual Data::MusicFormat get_music_format() { return Data::MusicFormatNone; }
  virtual PBuffer get_music(size_t index) = 0;

  bool check_file(const std::string &path);

 protected:
  Data::MaskImage separate_sprites(Data::PSprite s1, Data::PSprite s2);
};

#endif  // SRC_DATA_SOURCE_H_
