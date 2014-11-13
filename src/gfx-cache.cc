/*
 * gfx-cache.cc - Graphics caching functions
 *
 * Copyright (C) 2014  Wicked_Digger <wicked_digger@mail.ru>
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

#include <map>
#include <string>

#ifndef _MSC_VER
extern "C" {
#endif
  #include "gfx.h"
#ifndef _MSC_VER
}
#endif

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <cstdlib>
#include <cstdio>
#include <cstring>

static uint64_t
gfx_image_id(int sprite, int mask, int offset)
{
  uint64_t result = (uint64_t)sprite + (((uint64_t)mask) << 32) + (((uint64_t)offset) << 48);
  return result;
}

typedef std::map<uint64_t, image_t *> image_cache_t;
image_cache_t image_cache;

void
gfx_add_image_to_cache(int sprite, int mask, int offset, image_t *image)
{
  image_cache[gfx_image_id(sprite, mask, offset)] = image;
}

image_t *
gfx_get_image_from_cache(int sprite, int mask, int offset)
{
  image_cache_t::iterator result = image_cache.find(gfx_image_id(sprite, mask, offset));
  if(result == image_cache.end()) {
    return NULL;
  }
  return result->second;
}

void gfx_clear_cache()
{
  while(!image_cache.empty()) {
    gfx_image_free(image_cache.begin()->second);
    image_cache.erase(image_cache.begin());
  }
}
