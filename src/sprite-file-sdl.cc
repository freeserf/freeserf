/*
 * sprite-file-sdl.cc - Sprite loaded from file implementation
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

#include "src/sprite-file.h"

#include <SDL_image.h>

SpriteFile::SpriteFile() {
  IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF);
}

bool
SpriteFile::load(const std::string &path) {
  SDL_Surface *image = IMG_Load(path.c_str());
  if (image == nullptr) {
    return false;
  }
  SDL_Surface *surf = SDL_ConvertSurfaceFormat(image,
                                               SDL_PIXELFORMAT_ARGB8888,
                                               0);
  SDL_FreeSurface(image);
  SDL_LockSurface(surf);
  width = surf->w;
  height = surf->h;
  size_t size = width * height * 4;
  data =  reinterpret_cast<uint8_t*>(malloc(size));
  memcpy(data, surf->pixels, size);
  SDL_UnlockSurface(surf);
  SDL_FreeSurface(surf);
  return true;
}
