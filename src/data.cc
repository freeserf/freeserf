/*
 * data.cc - Game resources file functions
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

#include "src/data.h"

#include <cstdlib>
#include <algorithm>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
  #include "src/freeserf_endian.h"
END_EXT_C
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

static uint32_t *serf_animation_table = NULL;

static void
load_serf_animation_table() {
  /* The serf animation table is stored in big endian
   order in the data file.

   * The first uint32 is the byte length of the rest
   of the table.
   * Next is 199 uint32s that are offsets from the start
   of this table to an animation table (one for each
   animation).
   * The animation tables are of varying lengths.
   Each entry in the animation table is three bytes
   long. First byte is used to determine the serf body
   sprite. Second byte is a signed horizontal sprite
   offset. Third byte is a signed vertical offset.
   */

  size_t size = 0;
  serf_animation_table =
  reinterpret_cast<uint32_t*>(data_get_object(DATA_SERF_ANIMATION_TABLE,
                                              &size));
  if (NULL == serf_animation_table) {
    return;
  }

  if (serf_animation_table[0] != 0xFFFFFFFF) {
    if (size != be32toh(serf_animation_table[0])) {
      // TODO(digger): report and assert
    }
    serf_animation_table[0] = 0xFFFFFFFF;
    serf_animation_table++;

    /* Endianess convert from big endian. */
    for (int i = 0; i < 200; i++) {
      serf_animation_table[i] = be32toh(serf_animation_table[i]);
    }
  }
}

#define MAX_DATA_PATH      1024

/* Load data file from path is non-NULL, otherwise search in
 various standard paths. */
bool
load_data_file(const std::string &path) {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    NULL
  };

  /* Use specified path. If something was specified
   but not found, this function should fail without
   looking anywhere else. */
  if (!path.empty()) {
    LOGI("main", "Looking for game data in `%s'...", path.c_str());
    int r = data_load(path);
    if (r < 0) return false;
    load_serf_animation_table();
    return true;
  }

  /* If a path is not specified (path is NULL) then
   the configuration file is searched for in the directories
   specified by the XDG Base Directory Specification
   <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.

   On windows platforms the %localappdata% is used in place of XDG_CONFIG_HOME.
   */

  char cp[MAX_DATA_PATH];
  char *env;

  /* Look in home */
  if ((env = getenv("XDG_DATA_HOME")) != NULL &&
      env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) {
        load_serf_animation_table();
        return true;
      }
    }
  } else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) {
        load_serf_animation_table();
        return true;
      }
    }
  }

#ifdef _WIN32
  if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) {
        load_serf_animation_table();
        return true;
      }
    }
  }
#endif

  if ((env = getenv("XDG_DATA_DIRS")) != NULL && env[0] != '\0') {
    char *begin = env;
    while (1) {
      char *end = strchr(begin, ':');
      if (end == NULL) end = strchr(begin, '\0');

      int len = static_cast<int>(end - begin);
      if (len > 0) {
        for (const char **df = default_data_file; *df != NULL; df++) {
          snprintf(cp, sizeof(cp), "%.*s/freeserf/%s", len, begin, *df);
          LOGI("main", "Looking for game data in `%s'...", cp);
          int r = data_load(cp);
          if (r >= 0) {
            load_serf_animation_table();
            return true;
          }
        }
      }

      if (end[0] == '\0') break;
      begin = end + 1;
    }
  } else {
    /* Look in /usr/local/share and /usr/share per XDG spec. */
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "/usr/local/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) {
        load_serf_animation_table();
        return true;
      }
    }

    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "/usr/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      int r = data_load(cp);
      if (r >= 0) {
        load_serf_animation_table();
        return true;
      }
    }
  }

  /* Look in current directory */
  for (const char **df = default_data_file; *df != NULL; df++) {
    LOGI("main", "Looking for game data in `%s'...", *df);
    int r = data_load(*df);
    if (r >= 0) {
      load_serf_animation_table();
      return true;
    }
  }

  return false;
}

/* There are different types of sprites:
 - Non-packed, rectangular sprites: These are simple called sprites here.
 - Transparent sprites, "transp": These are e.g. buldings/serfs.
 The transparent regions are RLE encoded.
 - Bitmap sprites: Conceptually these contain either 0 or 1 at each pixel.
 This is used to either modify the alpha level of another sprite (shadows)
 or mask parts of other sprites completely (mask sprites).
 */

/* Create empty sprite object */
static sprite_t *
data_create_empty_sprite(const dos_sprite_t *sprite) {
  sprite_t *s = new sprite_t(sprite->w, sprite->h);
  s->set_delta(sprite->b_x, sprite->b_y);
  s->set_offset(sprite->x, sprite->y);
  return s;
}

/* Create sprite object */
sprite_t *
data_create_sprite(const dos_sprite_t *sprite) {
  sprite_t *s = data_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  uint8_t *palette =
  reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
  sizeof(dos_sprite_t);
  uint8_t *dest = s->get_data();
  size_t size = sprite->w * sprite->h;

  for (size_t i = 0; i < size; i++) {
    dest[4*i+0] = palette[3*src[i]+0]; /* Red */
    dest[4*i+1] = palette[3*src[i]+1]; /* Green */
    dest[4*i+2] = palette[3*src[i]+2]; /* Blue */
    dest[4*i+3] = 0xff; /* Alpha */
  }

  return s;
}

/* Create transparent sprite object */
sprite_t *
data_create_transparent_sprite(const dos_sprite_t *sprite, int color_off) {
  sprite_t *s = data_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  uint8_t *palette =
  reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
  sizeof(dos_sprite_t);
  uint8_t *dest = s->get_data();
  size_t size = sprite->w * sprite->h;

  size_t i = 0;
  size_t j = 0;
  while (j < size) {
    j += src[i];
    int n = src[i+1];

    for (int k = 0; k < n; k++) {
      uint p_index = src[i+2+k] + color_off;
      dest[4*(j+k)+0] = palette[3*p_index+0]; /* Red */
      dest[4*(j+k)+1] = palette[3*p_index+1]; /* Green */
      dest[4*(j+k)+2] = palette[3*p_index+2]; /* Blue */
      dest[4*(j+k)+3] = 0xff; /* Alpha */
    }
    i += n + 2;
    j += n;
  }

  return s;
}

/* Create overlay sprite object */
sprite_t *
data_create_bitmap_sprite(const dos_sprite_t *sprite, unsigned int value) {
  sprite_t *s = data_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
  sizeof(dos_sprite_t);
  uint8_t *dest = s->get_data();
  size_t size = sprite->w * sprite->h;

  size_t i = 0;
  size_t j = 0;
  while (j < size) {
    j += src[i];
    int n = src[i+1];
    for (int k = 0; k < n && j + k < size; k++) {
      dest[4*(j+k)+3] = value; /* Alpha */
    }
    i += 2;
    j += n;
  }

  return s;
}

sprite_t::sprite_t(unsigned int width, unsigned int height) {
  this->width = width;
  this->height = height;
  size_t size = width * height * 4;
  data = new uint8_t[size];
  memset(data, 0, size);
}

sprite_t::~sprite_t() {
  if (data != NULL) {
    delete[] data;
    data = NULL;
  }
}

/* Apply mask to map tile sprite
 The resulting sprite will be extended to the height of the mask
 by repeating lines from the top of the sprite. The width of the
 mask and the sprite must be identical. */
sprite_t *
sprite_t::get_masked(sprite_t *mask) {
  sprite_t *s = new sprite_t(mask->get_width(), mask->get_height());
  s->set_delta(mask->get_delta_x(), mask->get_delta_y());
  s->set_offset(mask->get_offset_x(), mask->get_offset_y());

  uint8_t *dest_data = s->get_data();
  size_t to_copy = mask->get_width() * mask->get_height() * 4;

  /* Copy extended data to new sprite */
  while (to_copy > 0) {
    size_t s = std::min(to_copy, size_t(width * height * 4));
    memcpy(dest_data, data, s);
    to_copy -= s;
    dest_data += s;
  }

  /* Apply mask from mask sprite */
  uint8_t *s_data = s->get_data();
  const uint8_t *m_data = mask->get_data();
  for (size_t y = 0; y < mask->get_height(); y++) {
    for (size_t x = 0; x < mask->get_width(); x++) {
      size_t alpha_index = 4*(y * mask->get_width() + x) + 3;
      s_data[alpha_index] &= m_data[alpha_index];
    }
  }

  return s;
}

void *
data_get_wav(unsigned int index, size_t *size) {
  size_t data_size = 0;
  void *data = data_get_object(DATA_SFX_BASE + index, &data_size);
  if (data == NULL) {
    return NULL;
  }

  return sfx2wav(data, data_size, size, -0x20);
}

void *
data_get_midi(unsigned int index, size_t *size) {
  size_t data_size = 0;
  void *data = data_get_object(DATA_MUSIC_GAME + index, &data_size);
  if (NULL == data) {
    return NULL;
  }

  return xmi2mid(data, data_size, size);
}

animation_t*
data_get_animation(unsigned int animation, unsigned int phase) {
  uint8_t *tbl_ptr = reinterpret_cast<uint8_t*>(serf_animation_table) +
                     serf_animation_table[animation] +
                     (3 * phase);
  return reinterpret_cast<animation_t*>(tbl_ptr);
}
