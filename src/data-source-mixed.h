/*
 * data-source-mixed.h - Game data combined from several sources
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_DATA_SOURCE_MIXED_H_
#define SRC_DATA_SOURCE_MIXED_H_

#include <string>

#include "src/data.h"

/* All game data found (custom, DOS, Amiga) behind one source: the
   graphics, the sounds and the music can each come from a different one
   (e.g. DOS graphics with Amiga sounds). With a single source found it
   behaves as that source. */
class DataSourceMixed : public Data::Source {
 public:
  typedef enum Kind {
    KindCustom = 0,
    KindDOS,
    KindAmiga,
    KindCount
  } Kind;

  typedef enum Category {
    CategoryGraphics = 0,  // sprites and the serf animations
    CategorySound,
    CategoryMusic,
    CategoryCount
  } Category;

 protected:
  Data::PSource sources[KindCount];
  Kind selected[CategoryCount];

 public:
  DataSourceMixed();
  virtual ~DataSourceMixed() {}

  /* Add a loaded source; the first one added becomes the default of every
     category (the order of preference is custom, DOS, Amiga). */
  void add_source(Kind kind, Data::PSource source);
  bool has_source(Kind kind) const { return sources[kind] != nullptr; }
  Data::PSource get_source(Kind kind) const { return sources[kind]; }
  bool is_empty() const;

  Kind get_selected(Category category) const { return selected[category]; }
  /* False if that source was not found. */
  bool select(Category category, Kind kind);

  static const char *get_kind_name(Kind kind);

  /* Data::Source */
  virtual std::string get_name() const;
  virtual std::string get_path() const;
  virtual std::string get_music_path() const;
  virtual bool is_loaded() const { return !is_empty(); }
  virtual unsigned int get_scale() const;
  virtual unsigned int get_bpp() const;
  virtual bool check() { return !is_empty(); }
  virtual bool load() { return !is_empty(); }
  virtual Data::PSprite get_sprite(Data::Resource res, size_t index,
                                   const Data::Sprite::Color &color);
  virtual Data::MaskImage get_sprite_parts(Data::Resource res, size_t index);
  virtual Data::PSprite apply_mask(Data::Resource res, Data::PSprite sprite,
                                   Data::Resource mask_res, size_t mask_index,
                                   Data::PSprite mask);
  virtual size_t get_animation_phase_count(size_t animation);
  virtual Data::Animation get_animation(size_t animation, size_t phase);
  virtual PBuffer get_sound(size_t index);
  virtual Data::MusicFormat get_music_format();
  virtual PBuffer get_music(size_t index);
  virtual bool check_file(const std::string &path);

 protected:
  Data::PSource graphics() const { return sources[selected[CategoryGraphics]]; }
  Data::PSource music_source();
};

#endif  // SRC_DATA_SOURCE_MIXED_H_
