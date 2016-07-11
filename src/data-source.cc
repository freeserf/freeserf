/*
 * data-source.cc - Game resources file functions
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data-source.h"

#include <algorithm>
#include <fstream>

#include "src/freeserf_endian.h"
#include "src/log.h"
#include "src/tpwm.h"
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

bool
data_source_t::check_file(const std::string &path) {
  std::ifstream ifile(path.c_str());
  if (ifile.good()) {
    ifile.close();
    return true;
  }

  return false;
}

char*
data_source_t::file_read(const std::string &path, size_t *size) {
  char *data = NULL;
  *size = 0;

  do {
    std::ifstream file(path.c_str(), std::ifstream::binary |
                                     std::ifstream::ate);
    if (!file.good()) {
      break;
    }

    *size = (size_t)file.tellg();
    if (*size == 0) {
      break;
    }

    file.seekg(0, file.beg);

    data = reinterpret_cast<char*>(malloc(*size));
    if (data == NULL) {
      *size = 0;
      break;
    }

    file.read(data, *size);
    file.close();
  } while (false);

  return data;
}

/* Calculate hash of sprite identifier. */
uint64_t
sprite_t::create_sprite_id(uint64_t sprite, uint64_t mask, uint64_t offset) {
  uint64_t result = sprite + (mask << 32) + (offset << 48);
  return result;
}
