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

char *
gfx_image_id(int sprite, int mask, int offset)
{
  char str[255];
  snprintf(str, 255, "%05d_%05d_%05d", sprite, mask, offset);
  return strdup(str);
}

typedef std::map<std::string, sprite_t *> image_cache_t;
image_cache_t image_cache;

void
gfx_add_image_to_cache(int sprite, int mask, int offset, sprite_t *image)
{
  char *str = gfx_image_id(sprite, mask, offset);
  std::string key = str;
  free(str);
  image_cache[key] = image;
}

sprite_t *
gfx_get_image_from_cache(int sprite, int mask, int offset)
{
  char *str = gfx_image_id(sprite, mask, offset);
  std::string key = str;
  free(str);
  image_cache_t::iterator result = image_cache.find(key);
  if(result == image_cache.end()) {
    return NULL;
  }
  return result->second;
}

void gfx_clear_cache()
{
  while(!image_cache.empty()) {
    data_sprite_free(image_cache.begin()->second);
    image_cache.erase(image_cache.begin());
  }
}
