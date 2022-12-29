/*
 * data-source-dos.h - DOS game resources file functions
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

#ifndef SRC_DATA_SOURCE_DOS_H_
#define SRC_DATA_SOURCE_DOS_H_

#include <string>
#include <vector>
#include <memory>

#include "src/data.h"
#include "src/data-source-legacy.h"

class Buffer;

class DataSourceDOS : public DataSourceLegacy {
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

  class SpriteBaseDOS : public SpriteBase {
   public:
    SpriteBaseDOS() {}
    explicit SpriteBaseDOS(PBuffer data);
    virtual ~SpriteBaseDOS() {}
  };

  class SpriteDosSolid : public SpriteBaseDOS {
   public:
    //SpriteDosSolid(PBuffer data, ColorDOS *palette);
    //SpriteDosSolid(PBuffer data, ColorDOS *palette, Data::Resource res);
    //SpriteDosSolid(PBuffer data, ColorDOS *palette, Data::Resource res, size_t index);
    SpriteDosSolid(PBuffer data, ColorDOS *palette, Data::Resource res, size_t index, int mutate = 0);
    virtual ~SpriteDosSolid() {}
  };
  typedef std::shared_ptr<SpriteDosSolid> PSpriteDosSolid;  // this is never used

  class SpriteDosTransparent : public SpriteBaseDOS {
   public:
    //SpriteDosTransparent(PBuffer data, ColorDOS *palette, uint8_t color = 0);
    //SpriteDosTransparent(PBuffer data, ColorDOS *palette, Data::Resource res, uint8_t color = 0);  // added Data::Resource type
    //SpriteDosTransparent(PBuffer data, ColorDOS *palette, Data::Resource res, size_t index, uint8_t color = 0);  // sprite data index (within resource type)
    SpriteDosTransparent(PBuffer data, ColorDOS *palette, Data::Resource res, size_t index, uint8_t color = 0, int mutate = 0);  // sprite data index (within resource type)
    virtual ~SpriteDosTransparent() {}
  };
  typedef std::shared_ptr<SpriteDosTransparent> PSpriteDosTransparent;  // this is never used

  class SpriteDosOverlay : public SpriteBaseDOS {
   public:
    SpriteDosOverlay(PBuffer data, ColorDOS *palette, unsigned char value);
    virtual ~SpriteDosOverlay() {}
  };
  typedef std::shared_ptr<SpriteDosOverlay> PSpriteDosOverlay;  // this is never used

  class SpriteDosMask : public SpriteBaseDOS {
   public:
    explicit SpriteDosMask(PBuffer data);
    virtual ~SpriteDosMask() {}
  };
  typedef std::shared_ptr<SpriteDosMask> PSpriteDosMask;  // this is never used

 protected:
  // These entries follow the 8 byte header of the data file.
  typedef struct DataEntry {
    size_t offset;
    size_t size;
  } DataEntry;

  PBuffer spae;
  std::vector<DataEntry> entries;

 public:
  explicit DataSourceDOS(const std::string &path);
  virtual ~DataSourceDOS();

  virtual std::string get_name() const { return "DOS"; }
  virtual unsigned int get_scale() const { return 1; }
  virtual unsigned int get_bpp() const { return 8; }

  virtual bool check();
  virtual bool load();

  //virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index);
  virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index, int mutate = 0);

  virtual PBuffer get_sound(size_t index);
  virtual Data::MusicFormat get_music_format() { return Data::MusicFormatMidi; }
  virtual PBuffer get_music(size_t index);

 protected:
  PBuffer get_object(size_t index);
  void fixup();
  DataSourceDOS::ColorDOS *get_dos_palette(size_t index);
};

#endif  // SRC_DATA_SOURCE_DOS_H_
