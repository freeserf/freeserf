/*
 * sprite-file.h - Sprite loaded from file declaration
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

#ifndef SRC_SPRITE_FILE_H_
#define SRC_SPRITE_FILE_H_

#include <string>
#include <memory>

#include "src/data-source.h"

class SpriteFile : public Sprite {
 public:
  SpriteFile();

  bool load(const std::string &path);

  void set_delta(int x, int y) { delta_x = x; delta_y = y; }
  void set_offset(int x, int y) { offset_x = x; offset_y = y; }
};

typedef std::shared_ptr<SpriteFile> PSpriteFile;

#endif  // SRC_SPRITE_FILE_H_
