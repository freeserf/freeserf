/*
 * characters.h - Characters definitions
 *
 * Copyright (C) 2019  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_CHARACTERS_H_
#define SRC_CHARACTERS_H_

#include <string>
#include <memory>

typedef struct CharacterPreset {
  size_t face;
  const std::string name;
  const std::string characterization;
} CharacterPreset;

class Character {
 public:
  explicit Character(const CharacterPreset &character) {
    face = character.face;
    name = character.name;
    characterization = character.characterization;
  }

  size_t get_face() const { return face; }
  std::string get_name() const { return name; }
  std::string get_characterization() const { return characterization; }

 protected:
  size_t face;
  std::string name;
  std::string characterization;
};

class Characters {
 public:
  size_t get_count() const;
  Character &get_character(size_t index) const;

 public:
  static Characters characters;
};

#endif  // SRC_CHARACTERS_H_
