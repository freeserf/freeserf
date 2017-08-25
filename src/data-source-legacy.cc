/*
 * data-source-legacy.cc - Legacy game resources file functions
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source-legacy.h"

#include <algorithm>
#include <vector>

#include "src/freeserf_endian.h"
#include "src/log.h"

DataSourceLegacy::DataSourceLegacy(const std::string &_path)
  : DataSource(_path) {
}

DataSourceLegacy::~DataSourceLegacy() {
}

bool
DataSourceLegacy::load_animation_table(PBuffer data) {
  // The serf animation table is stored in big endian order in the data file.
  data->set_endianess(Buffer::EndianessBig);

  // * Starts with 200 uint32s that are offsets from the start
  // of this table to an animation table (one for each animation).
  // * The animation tables are of varying lengths.
  // Each entry in the animation table is three bytes long.
  // First byte is used to determine the serf body sprite.
  // Second byte is a signed horizontal sprite offset.
  // Third byte is a signed vertical offset.

  if (data == nullptr) {
    return false;
  }
  uint32_t *animation_block = reinterpret_cast<uint32_t*>(data->get_data());

  // Endianess convert from big endian.
  for (size_t i = 0; i < 200; i++) {
    animation_block[i] = betoh(animation_block[i]);
  }

  size_t sizes[200];

  for (size_t i = 0; i < 200; i++) {
    size_t a = animation_block[i];
    size_t next = data->get_size();
    for (int j = 0; j < 200; j++) {
      size_t b = animation_block[j];
      if (b > a) {
        next = std::min(next, b);
      }
    }
    sizes[i] = (next - a)/3;
  }

  for (size_t i = 0; i < 200; i++) {
    int offset = animation_block[i];
    AnimationPhase *anims =
    reinterpret_cast<AnimationPhase*>(reinterpret_cast<char*>(animation_block) +
                                      offset);
    std::vector<Animation> animations;
    for (size_t j = 0; j < sizes[i]; j++) {
      Animation a;
      a.sprite = anims[j].sprite;
      a.x = anims[j].x;
      a.y = anims[j].y;
      animations.push_back(a);
    }
    animation_table.push_back(animations);
  }

  return true;
}
