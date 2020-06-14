/*
 * data.h - Definitions for data file access.
 *
 * Copyright (C) 2012-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_DATA_H_
#define SRC_DATA_H_

#include <string>
#include <list>
#include <memory>
#include <tuple>

class Buffer;
typedef std::shared_ptr<Buffer> PBuffer;

class Data {
 public:
  typedef enum Type {
    TypeUnknown,
    TypeSprite,
    TypeAnimation,
    TypeSound,
    TypeMusic,
  } Type;

  typedef enum Resource {
    AssetNone,
    AssetArtLandscape,
    AssetAnimation,
    AssetSerfShadow,
    AssetDottedLines,
    AssetArtFlag,
    AssetArtBox,
    AssetCreditsBg,
    AssetLogo,
    AssetSymbol,
    AssetMapMaskUp,
    AssetMapMaskDown,
    AssetPathMask,
    AssetMapGround,
    AssetPathGround,
    AssetGameObject,
    AssetFrameTop,
    AssetMapBorder,
    AssetMapWaves,
    AssetFramePopup,
    AssetIndicator,
    AssetFont,
    AssetFontShadow,
    AssetIcon,
    AssetMapObject,
    AssetMapShadow,
    AssetPanelButton,
    AssetFrameBottom,
    AssetSerfTorso,
    AssetSerfHead,
    AssetFrameSplit,
    AssetSound,
    AssetMusic,
    AssetCursor,
  } Asset;

  enum MusicFormat {
    MusicFormatNone,
    MusicFormatMidi,
    MusicFormatMod
  };

  class Animation {
   public:
    uint8_t sprite;
    int x;
    int y;
  };

  class Sprite;
  typedef std::shared_ptr<Sprite> PSprite;
  class Sprite : public std::enable_shared_from_this<Sprite> {
   public:
    typedef struct Color {
      unsigned char blue;
      unsigned char green;
      unsigned char red;
      unsigned char alpha;
    } Color;

   public:
    virtual uint8_t *get_data() const = 0;
    virtual size_t get_width() const = 0;
    virtual size_t get_height() const = 0;
    virtual int get_delta_x() const = 0;
    virtual int get_delta_y() const = 0;
    virtual int get_offset_x() const = 0;
    virtual int get_offset_y() const = 0;

    virtual PSprite get_masked(PSprite mask) = 0;
    virtual PSprite create_mask(PSprite other) = 0;
    virtual void fill(Sprite::Color color) = 0;
    virtual void fill_masked(Sprite::Color color) = 0;
    virtual void add(PSprite other) = 0;
    virtual void del(PSprite other) = 0;
    virtual void blend(PSprite other) = 0;
    virtual void make_alpha_mask() = 0;

    virtual void stick(PSprite sticker, unsigned int x, unsigned int y) = 0;

    static uint64_t create_id(uint64_t resource, uint64_t index,
                              uint64_t mask_resource, uint64_t mask_index,
                              const Color &color);
  };

  typedef std::tuple<PSprite, PSprite> MaskImage;

  class Source {
   public:
    virtual std::string get_name() const = 0;
    virtual std::string get_path() const = 0;
    virtual bool is_loaded() const = 0;
    virtual unsigned int get_scale() const = 0;
    virtual unsigned int get_bpp() const = 0;

    virtual bool check() = 0;
    virtual bool load() = 0;

    virtual PSprite get_sprite(Resource res, size_t index,
                               const Sprite::Color &color) = 0;

    virtual MaskImage get_sprite_parts(Resource res, size_t index) = 0;

    virtual size_t get_animation_phase_count(size_t animation) = 0;
    virtual Animation get_animation(size_t animation, size_t phase) = 0;

    virtual PBuffer get_sound(size_t index) = 0;
    virtual MusicFormat get_music_format() = 0;
    virtual PBuffer get_music(size_t index) = 0;

    virtual bool check_file(const std::string &path) = 0;
  };

  typedef std::shared_ptr<Source> PSource;

 protected:
  PSource data_source;

  Data();

 public:
  Data(const Data& that) = delete;
  virtual ~Data();

  Data& operator = (const Data& that) = delete;

  static Data &get_instance();

  bool load(const std::string &path);

  PSource get_data_source() const { return data_source; }

  static Type get_resource_type(Resource resource);
  static unsigned int get_resource_count(Resource resource);
  static const std::string get_resource_name(Resource resource);

 protected:
  std::list<std::string> get_standard_search_paths() const;
};

#endif  // SRC_DATA_H_
