/*
 * data-source-legacy.h - Legacy game resources file functions
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

#ifndef SRC_DATA_SOURCE_LEGACY_H_
#define SRC_DATA_SOURCE_LEGACY_H_

#include <string>

#include "src/data-source.h"

class DataSourceLegacy : public DataSource {
 protected:
  typedef struct AnimationPhase {
    uint8_t sprite;
    int8_t x;
    int8_t y;
  } AnimationPhase;

 public:
  explicit DataSourceLegacy(const std::string &path);
  virtual ~DataSourceLegacy();

 protected:
  bool load_animation_table(PBuffer data);
};

#endif  // SRC_DATA_SOURCE_LEGACY_H_
