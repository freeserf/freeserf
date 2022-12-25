/*
 * data-source-amiga.h - Amiga data loading
 *
 * Copyright (C) 2016-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_DATA_SOURCE_AMIGA_H_
#define SRC_DATA_SOURCE_AMIGA_H_

#include <string>
#include <map>
#include <array>
#include <vector>
#include <memory>

#include "src/data-source-legacy.h"
#include "src/buffer.h"

class DataSourceAmiga : public DataSourceLegacy {
 protected:
  class SpriteAmiga;
  typedef std::shared_ptr<SpriteAmiga> PSpriteAmiga;
  class SpriteAmiga : public SpriteBase {
   public:
    SpriteAmiga(size_t width, size_t height);
    virtual ~SpriteAmiga();

    void set_delta(int x, int y) { delta_x = x; delta_y = y; }
    void set_offset(int x, int y) { offset_x = x; offset_y = y; }
    PSpriteAmiga get_amiga_masked(Data::PSprite mask);
    void clear() {}
    Sprite::Color *get_writable_data() {
      return reinterpret_cast<Sprite::Color*>(data);
    }

    void make_transparent(uint32_t rc = 0);
    PSpriteAmiga merge_horizontaly(PSpriteAmiga right);
    PSpriteAmiga split_horizontaly(bool return_right);
  };

 public:
  explicit DataSourceAmiga(const std::string &path);
  virtual ~DataSourceAmiga();

  virtual std::string get_name() const { return "Amiga"; }
  virtual unsigned int get_scale() const { return 1; }
  virtual unsigned int get_bpp() const { return 5; }

  virtual bool check();
  virtual bool load();

  //virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index);
  virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index, int mutate = 0);

  virtual PBuffer get_sound(size_t index);
  virtual Data::MusicFormat get_music_format() { return Data::MusicFormatMod; }
  virtual PBuffer get_music(size_t index);

 private:
  PBuffer gfxfast;
  PBuffer gfxchip;
  PBuffer sound;
  PBuffer music;

  std::array<PBuffer, 24> data_pointers;
  std::array<PBuffer, 14> pics;

  std::vector<size_t> icon_catalog;

  PBuffer decode(PBuffer data);
  PBuffer unpack(PBuffer data);

  PBuffer get_data_from_catalog(size_t catalog, size_t index, PBuffer base);

  PSpriteAmiga get_menu_sprite(size_t index, PBuffer block,
                               size_t width, size_t height,
                               unsigned char compression,
                               unsigned char filling);
  Data::PSprite get_icon_sprite(size_t index);
  PSpriteAmiga get_ground_sprite(size_t index);
  Data::PSprite get_ground_mask_sprite(size_t index);
  PSpriteAmiga get_mirrored_horizontaly_sprite(Data::PSprite sprite);
  Data::PSprite get_path_mask_sprite(size_t index);
  Data::PSprite get_game_object_sprite(size_t catalog, size_t index);
  Data::PSprite get_torso_sprite(size_t index, uint8_t *palette);
  Data::PSprite get_map_object_sprite(size_t index);
  Data::PSprite get_map_object_shadow(size_t index);
  PSpriteAmiga get_hud_sprite(size_t index);

  PSpriteAmiga decode_planned_sprite(PBuffer data, size_t width, size_t height,
                                     uint8_t compression, uint8_t filling,
                                     uint8_t *palette, bool invert = true);
  PSpriteAmiga decode_interlased_sprite(PBuffer data,
                                        size_t width, size_t height,
                                        uint8_t compression, uint8_t filling,
                                        uint8_t *palette,
                                        size_t skip_lines = 0);
  PSpriteAmiga decode_amiga_sprite(PBuffer data, size_t width, size_t height,
                                   uint8_t *palette);
  PSpriteAmiga decode_mask_sprite(PBuffer data, size_t width, size_t height);

  unsigned int bitplane_count_from_compression(unsigned char compression);

  PSpriteAmiga make_shadow_from_symbol(PSpriteAmiga symbol);

  PBuffer get_sound_data(size_t index);
};

#endif  // SRC_DATA_SOURCE_AMIGA_H_
