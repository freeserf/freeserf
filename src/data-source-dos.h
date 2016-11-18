/*
 * data-source-dos.h - DOS game resources file functions
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

#ifndef SRC_DATA_SOURCE_DOS_H_
#define SRC_DATA_SOURCE_DOS_H_

#include <string>

#include "src/data.h"
#include "src/data-source.h"

class DataSourceDOS : public DataSource {
 public:
  typedef enum SpriteType {
    SpriteTypeUnknown = 0,
    SpriteTypeSolid,
    SpriteTypeTransparent,
    SpriteTypeOverlay,
    SpriteTypeMask,
  } SpriteType;

  typedef struct Resource {
    unsigned int index;
    unsigned int dos_palette;
    SpriteType sprite_type;
  } Resource;

 protected:
  typedef struct ColorDOS {
    unsigned char r;
    unsigned char g;
    unsigned char b;
  } ColorDOS;

  class SpriteBaseDOS : public Sprite {
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

   public:
    SpriteBaseDOS() {}
    SpriteBaseDOS(void *data, size_t size);
    virtual ~SpriteBaseDOS() {}
  };

  class SpriteDosSolid : public SpriteBaseDOS {
   public:
    SpriteDosSolid(void *data, size_t size, ColorDOS *palette);
    virtual ~SpriteDosSolid() {}
  };

  class SpriteDosTransparent : public SpriteBaseDOS {
   public:
    SpriteDosTransparent(void *data, size_t size, ColorDOS *palette,
                         uint8_t color = 0);
    virtual ~SpriteDosTransparent() {}
  };

  class SpriteDosOverlay : public SpriteBaseDOS {
   public:
    SpriteDosOverlay(void *data, size_t size, ColorDOS *palette,
      unsigned char value);
    virtual ~SpriteDosOverlay() {}
  };

  class SpriteDosMask : public SpriteBaseDOS {
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

  virtual Sprite *get_sprite(Data::Resource res, unsigned int index,
                             const Sprite::Color &color);

  virtual Animation *get_animation(unsigned int animation,
                                   unsigned int phase);

  virtual void *get_sound(unsigned int index, size_t *size);
  virtual void *get_music(unsigned int index, size_t *size);

 protected:
  void *get_object(unsigned int index, size_t *size);
  void fixup();
  bool load_animation_table();
  DataSourceDOS::ColorDOS *get_dos_palette(unsigned int index);
};

#endif  // SRC_DATA_SOURCE_DOS_H_
