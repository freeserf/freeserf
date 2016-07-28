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

#include "src/data-source.h"

class DataSourceDOS : public DataSource {
 protected:
  typedef struct Color {
    unsigned char r;
    unsigned char g;
    unsigned char b;
  } Color;

  class SpriteDosEmpty : public Sprite {
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
    SpriteDosEmpty() {}
    SpriteDosEmpty(void *data, size_t size);
    virtual ~SpriteDosEmpty() {}

    virtual uint8_t *get_data() const { return NULL; }
    virtual unsigned int get_width() const { return width; }
    virtual unsigned int get_height() const { return height; }
    virtual int get_delta_x() const { return delta_x; }
    virtual int get_delta_y() const { return delta_y; }
    virtual int get_offset_x() const { return offset_x; }
    virtual int get_offset_y() const { return offset_y; }

    virtual Sprite *get_masked(Sprite *mask) { return NULL; }
  };

  class SpriteDosBase : public SpriteDosEmpty {
   protected:
    uint8_t *data;

   public:
    SpriteDosBase(void *data, size_t size);
    explicit SpriteDosBase(Sprite *base);
    virtual ~SpriteDosBase();

    virtual uint8_t *get_data() const { return data; }
    virtual Sprite *get_masked(Sprite *mask);
  };

  class SpriteDosSolid : public SpriteDosBase {
   public:
    SpriteDosSolid(void *data, size_t size, Color *palette);
    virtual ~SpriteDosSolid() {}
  };

  class SpriteDosTransparent : public SpriteDosBase {
   public:
    SpriteDosTransparent(void *data, size_t size, Color *palette,
      int color_off);
    virtual ~SpriteDosTransparent() {}
  };

  class SpriteDosOverlay : public SpriteDosBase {
   public:
    SpriteDosOverlay(void *data, size_t size, Color *palette,
      unsigned char value);
    virtual ~SpriteDosOverlay() {}
  };

  class SpriteDosMask : public SpriteDosBase {
   public:
    SpriteDosMask(void *data, size_t size);
    virtual ~SpriteDosMask() {}
  };

 protected:
  /* These entries follow the 8 byte header of the data file. */
  typedef struct SpaeEntry {
    uint32_t size;
    uint32_t offset;
  } SpaeEntry;

  void *sprites;
  size_t sprites_size;
  size_t entry_count;
  Animation **animation_table;

 public:
  DataSourceDOS();
  virtual ~DataSourceDOS();

  virtual bool check(const std::string &path, std::string *load_path);
  virtual bool load(const std::string &path);

  virtual Sprite *get_sprite(unsigned int index);
  virtual Sprite *get_empty_sprite(unsigned int index);
  virtual Sprite *get_transparent_sprite(unsigned int index, int color_off);
  virtual Sprite *get_overlay_sprite(unsigned int index);
  virtual Sprite *get_mask_sprite(unsigned int index);

  virtual ::Color get_color(unsigned int index);

  virtual Animation *get_animation(unsigned int animation,
                                   unsigned int phase);

  virtual void *get_sound(unsigned int index, size_t *size);
  virtual void *get_music(unsigned int index, size_t *size);

 protected:
  void *get_object(unsigned int index, size_t *size);
  void fixup();
  bool load_animation_table();
  Color *get_palette(unsigned int index);
};

#endif  // SRC_DATA_SOURCE_DOS_H_
