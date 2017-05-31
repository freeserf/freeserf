/*
 * data-source-amiga.h - Amiga data loading
 *
 * Copyright (C) 2016  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source.h"

class DataSourceAmiga : public DataSource {
 protected:
  class SpriteAmiga : public Sprite {
   public:
    SpriteAmiga(unsigned int width, unsigned int height);
    virtual ~SpriteAmiga();

    void set_delta(int x, int y) { delta_x = x; delta_y = y; }
    void set_offset(int x, int y) { offset_x = x; offset_y = y; }
    SpriteAmiga *get_amiga_masked(Sprite *mask);
    void clear() {}
    Sprite::Color *get_writable_data() {
      return reinterpret_cast<Sprite::Color*>(data);
    }

    void make_transparent(uint32_t rc = 0);
    SpriteAmiga *merge_horizontaly(SpriteAmiga *right);
    SpriteAmiga *split_horizontaly(bool return_right);
  };

 public:
  explicit DataSourceAmiga(const std::string &path);
  virtual ~DataSourceAmiga();

  virtual std::string get_name() const { return "Amiga"; }
  virtual unsigned int get_scale() const { return 1; }
  virtual unsigned int get_bpp() const { return 5; }

  virtual bool check();
  virtual bool load();

  virtual void get_sprite_parts(Data::Resource res, unsigned int index,
                                Sprite **mask, Sprite **image);

  virtual Animation *get_animation(unsigned int animation, unsigned int phase);

  virtual void *get_sound(unsigned int index, size_t *size);
  virtual void *get_music(unsigned int index, size_t *size);

 private:
  void *gfxfast;
  void *gfxchip;
  void *gfxpics;
  void *sound;
  void *music;
  size_t music_size;

  void *data_pointers[24];
  typedef std::map<size_t, void*> pics_t;
  pics_t pics;

  size_t *icon_catalog;

  void decode(void *data, size_t size);
  void *unpack(void *data, size_t size, size_t *unpacked_size);

  unsigned char *get_data_from_catalog(size_t catalog_index, size_t index,
                                       void *base);

  SpriteAmiga *get_menu_sprite(unsigned int index, void *block,
                               unsigned int width, unsigned int height,
                               unsigned char compression,
                               unsigned char filling);
  Sprite *get_icon_sprite(unsigned int index);
  SpriteAmiga *get_ground_sprite(unsigned int index);
  Sprite *get_ground_mask_sprite(unsigned int index);
  SpriteAmiga *get_mirrored_horizontaly_sprite(Sprite *sprite);
  Sprite *get_path_mask_sprite(unsigned int index);
  Sprite *get_game_object_sprite(unsigned int catalog, unsigned int index);
  Sprite *get_torso_sprite(unsigned int index, uint8_t *palette);
  Sprite *get_map_object_sprite(unsigned int index);
  Sprite *get_map_object_shadow(unsigned int index);
  SpriteAmiga *get_hud_sprite(unsigned int index);

  SpriteAmiga *decode_planned_sprite(void *data,
                                     unsigned int width, unsigned int height,
                                     uint8_t compression, uint8_t filling,
                                     uint8_t *palette, bool invert = true);
  SpriteAmiga *decode_interlased_sprite(void *data,
                                        unsigned int width, unsigned int height,
                                        uint8_t compression, uint8_t filling,
                                        uint8_t *palette,
                                        size_t skip_lines = 0);
  SpriteAmiga *decode_amiga_sprite(void *data,
                                   unsigned int width, unsigned int height,
                                   uint8_t *palette);
  SpriteAmiga *decode_mask_sprite(void *data,
                                  unsigned int width, unsigned int height);

  unsigned int bitplane_count_from_compression(unsigned char compression);

  void *get_sound_data(unsigned int index, size_t *size);
};

#endif  // SRC_DATA_SOURCE_AMIGA_H_
