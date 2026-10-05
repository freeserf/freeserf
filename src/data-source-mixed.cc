/*
 * data-source-mixed.cc - Game data combined from several sources
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

#include "src/data-source-mixed.h"

DataSourceMixed::DataSourceMixed() {
  for (Kind &kind : selected) {
    kind = KindCount;
  }
}

void
DataSourceMixed::add_source(Kind kind, Data::PSource source) {
  sources[kind] = source;
  for (Kind &sel : selected) {
    if (sel == KindCount) sel = kind;
  }
}

bool
DataSourceMixed::is_empty() const {
  for (const Data::PSource &source : sources) {
    if (source) return false;
  }
  return true;
}

bool
DataSourceMixed::select(Category category, Kind kind) {
  if (kind >= KindCount || !sources[kind]) return false;
  selected[category] = kind;
  return true;
}

const char *
DataSourceMixed::get_kind_name(Kind kind) {
  switch (kind) {
    case KindCustom: return "Custom";
    case KindDOS: return "DOS";
    case KindAmiga: return "Amiga";
    default: return "None";
  }
}

std::string
DataSourceMixed::get_name() const {
  return graphics() ? graphics()->get_name() : std::string();
}

std::string
DataSourceMixed::get_path() const {
  return graphics() ? graphics()->get_path() : std::string();
}

/* The SoundFont for MIDI is looked for next to the data of the music. */
std::string
DataSourceMixed::get_music_path() const {
  Data::PSource music = sources[selected[CategoryMusic]];
  return music ? music->get_path() : get_path();
}

unsigned int
DataSourceMixed::get_scale() const {
  return graphics() ? graphics()->get_scale() : 1;
}

unsigned int
DataSourceMixed::get_bpp() const {
  return graphics() ? graphics()->get_bpp() : 0;
}

Data::PSprite
DataSourceMixed::get_sprite(Data::Resource res, size_t index,
                            const Data::Sprite::Color &color) {
  return graphics()->get_sprite(res, index, color);
}

Data::MaskImage
DataSourceMixed::get_sprite_parts(Data::Resource res, size_t index) {
  return graphics()->get_sprite_parts(res, index);
}

Data::PSprite
DataSourceMixed::apply_mask(Data::Resource res, Data::PSprite sprite,
                            Data::Resource mask_res, size_t mask_index,
                            Data::PSprite mask) {
  return graphics()->apply_mask(res, sprite, mask_res, mask_index, mask);
}

size_t
DataSourceMixed::get_animation_phase_count(size_t animation) {
  return graphics()->get_animation_phase_count(animation);
}

Data::Animation
DataSourceMixed::get_animation(size_t animation, size_t phase) {
  return graphics()->get_animation(animation, phase);
}

/* A sound missing in the selected source (custom data often has none) is
   taken from the next source that has it. */
PBuffer
DataSourceMixed::get_sound(size_t index) {
  Kind first = selected[CategorySound];
  for (int i = 0; i < KindCount; i++) {
    Kind kind = static_cast<Kind>((first + i) % KindCount);
    if (!sources[kind]) continue;
    PBuffer sound = sources[kind]->get_sound(index);
    if (sound) return sound;
  }
  return nullptr;
}

/* The selected music source, or the next one that has music. */
Data::PSource
DataSourceMixed::music_source() {
  Kind first = selected[CategoryMusic];
  for (int i = 0; i < KindCount; i++) {
    Kind kind = static_cast<Kind>((first + i) % KindCount);
    if (sources[kind] &&
        sources[kind]->get_music_format() != Data::MusicFormatNone) {
      return sources[kind];
    }
  }
  return nullptr;
}

Data::MusicFormat
DataSourceMixed::get_music_format() {
  Data::PSource music = music_source();
  return music ? music->get_music_format() : Data::MusicFormatNone;
}

PBuffer
DataSourceMixed::get_music(size_t index) {
  Data::PSource music = music_source();
  return music ? music->get_music(index) : nullptr;
}

bool
DataSourceMixed::check_file(const std::string &path) {
  return graphics() ? graphics()->check_file(path) : false;
}
