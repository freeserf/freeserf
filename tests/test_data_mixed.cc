/*
 * test_data_mixed.cc - Game data combined from several sources
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

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "src/data-source-mixed.h"
#include "src/buffer.h"

/* A source that tells which one it is: its scale identifies it, sounds
   and music are buffers holding its name. */
class FakeSource : public Data::Source {
 protected:
  std::string name;
  unsigned int scale;
  bool sounds;
  Data::MusicFormat music;

 public:
  FakeSource(const std::string &_name, unsigned int _scale, bool _sounds,
             Data::MusicFormat _music)
    : name(_name), scale(_scale), sounds(_sounds), music(_music) {}

  virtual std::string get_name() const { return name; }
  virtual std::string get_path() const { return "/" + name; }
  virtual bool is_loaded() const { return true; }
  virtual unsigned int get_scale() const { return scale; }
  virtual unsigned int get_bpp() const { return 8; }
  virtual bool check() { return true; }
  virtual bool load() { return true; }
  virtual Data::PSprite get_sprite(Data::Resource, size_t,
                                   const Data::Sprite::Color &) {
    return nullptr;
  }
  virtual Data::MaskImage get_sprite_parts(Data::Resource, size_t) {
    return Data::MaskImage(nullptr, nullptr);
  }
  virtual Data::PSprite apply_mask(Data::Resource, Data::PSprite sprite,
                                   Data::Resource, size_t, Data::PSprite) {
    return sprite;
  }
  virtual size_t get_animation_phase_count(size_t) { return scale; }
  virtual Data::Animation get_animation(size_t, size_t) {
    return Data::Animation();
  }
  virtual PBuffer get_sound(size_t) {
    if (!sounds) return nullptr;
    return text_buffer();
  }
  virtual Data::MusicFormat get_music_format() { return music; }
  virtual PBuffer get_music(size_t) { return text_buffer(); }
  virtual bool check_file(const std::string &) { return true; }

 protected:
  PBuffer text_buffer() const {
    PMutableBuffer buffer =
      std::make_shared<MutableBuffer>(Buffer::EndianessLittle);
    buffer->push(name);
    return buffer;
  }
};

static std::string
buffer_text(PBuffer buffer) {
  if (!buffer) return std::string();
  return std::string(reinterpret_cast<const char *>(buffer->get_data()),
                     buffer->get_size());
}

TEST(DataMixed, SingleSourceAsItself) {
  DataSourceMixed mixed;
  EXPECT_TRUE(mixed.is_empty());
  mixed.add_source(DataSourceMixed::KindAmiga,
                   std::make_shared<FakeSource>("amiga", 2, true,
                                                Data::MusicFormatMod));
  EXPECT_FALSE(mixed.is_empty());
  EXPECT_EQ("amiga", mixed.get_name());
  EXPECT_EQ(2u, mixed.get_scale());
  EXPECT_EQ("amiga", buffer_text(mixed.get_sound(1)));
  EXPECT_EQ(Data::MusicFormatMod, mixed.get_music_format());
  EXPECT_FALSE(mixed.select(DataSourceMixed::CategorySound,
                            DataSourceMixed::KindDOS));
}

TEST(DataMixed, CategoriesFromDifferentSources) {
  DataSourceMixed mixed;
  mixed.add_source(DataSourceMixed::KindDOS,
                   std::make_shared<FakeSource>("dos", 1, true,
                                                Data::MusicFormatMidi));
  mixed.add_source(DataSourceMixed::KindAmiga,
                   std::make_shared<FakeSource>("amiga", 2, true,
                                                Data::MusicFormatMod));

  // The first source added is the default of everything.
  EXPECT_EQ(DataSourceMixed::KindDOS,
            mixed.get_selected(DataSourceMixed::CategoryGraphics));
  EXPECT_EQ("dos", buffer_text(mixed.get_sound(1)));

  // DOS graphics with Amiga sounds and music.
  EXPECT_TRUE(mixed.select(DataSourceMixed::CategorySound,
                           DataSourceMixed::KindAmiga));
  EXPECT_TRUE(mixed.select(DataSourceMixed::CategoryMusic,
                           DataSourceMixed::KindAmiga));
  EXPECT_EQ(1u, mixed.get_scale());
  EXPECT_EQ(1u, mixed.get_animation_phase_count(0));
  EXPECT_EQ("/dos", mixed.get_path());
  EXPECT_EQ("amiga", buffer_text(mixed.get_sound(1)));
  EXPECT_EQ(Data::MusicFormatMod, mixed.get_music_format());
  EXPECT_EQ("amiga", buffer_text(mixed.get_music(0)));
  EXPECT_EQ("/amiga", mixed.get_music_path());
}

TEST(DataMixed, MissingSoundsAndMusicFromNextSource) {
  DataSourceMixed mixed;
  // Custom graphics without sounds or music.
  mixed.add_source(DataSourceMixed::KindCustom,
                   std::make_shared<FakeSource>("custom", 3, false,
                                                Data::MusicFormatNone));
  mixed.add_source(DataSourceMixed::KindDOS,
                   std::make_shared<FakeSource>("dos", 1, true,
                                                Data::MusicFormatMidi));
  EXPECT_EQ(3u, mixed.get_scale());
  EXPECT_EQ("dos", buffer_text(mixed.get_sound(1)));
  EXPECT_EQ(Data::MusicFormatMidi, mixed.get_music_format());
  EXPECT_EQ("dos", buffer_text(mixed.get_music(0)));
}
