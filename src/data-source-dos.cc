/*
 * data-source-dos.cc - DOS game resources file functions
 *
 * Copyright (C) 2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/data-source-dos.h"

#include <cerrno>
#include <cassert>
#include <algorithm>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#ifdef HAVE_SYS_MMAN_H
# include <sys/mman.h>
#endif

#ifdef HAVE_MMAP
# include <fcntl.h>
# include <sys/stat.h>
#endif

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/freeserf_endian.h"
  #include "src/log.h"
END_EXT_C
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

data_source_t::data_source_t() {
  sprites = NULL;
#ifdef HAVE_MMAP
  mapped = 0;
#endif
  sprites_size = 0;
  entry_count = 0;
  serf_animation_table = NULL;
}

/* Load data file at path and let the global variable sprites refer to the memory
 with the data file content. */
bool
data_source_t::load(const std::string &path) {
  int r;

#ifdef HAVE_MMAP
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return false;

  struct stat sb;
  r = fstat(fd, &sb);
  if (r < 0) return false;

  sprites_size = sb.st_size;
  sprites = mmap(NULL, sprites_size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE, fd, 0);
  if (sprites == MAP_FAILED) {
    int errsv = errno;
    close(fd);
    errno = errsv;
    return false;
  }

  close(fd);

  mapped = 1;
#else /* ! HAVE_MMAP */
  FILE *f = fopen(path.c_str(), "rb");
  if (f == NULL) return false;

  r = fseek(f, 0, SEEK_END);
  if (r < 0) return false;

  sprites_size = ftell(f);
  if (sprites_size == (size_t)-1) return false;

  r = fseek(f, 0, SEEK_SET);
  if (r < 0) return false;

  sprites = malloc(sprites_size);
  if (sprites == NULL) {
    int errsv = errno;
    fclose(f);
    errno = errsv;
    return false;
  }

  size_t rd = fread(sprites, sprites_size, 1, f);
  if (rd < 1) return false;

  fclose(f);
#endif

  /* Check that data file is decompressed. */
  if (tpwm_is_compressed(sprites, sprites_size)) {
    LOGV("data", "Data file is compressed");
    void *uncompressed = NULL;
    size_t uncmpsd_size = 0;
    const char *error = NULL;
    if (tpwm_uncompress(sprites, sprites_size, &uncompressed, &uncmpsd_size,
                        &error)) {
      LOGE("tpwm", error);
      LOGE("data", "Data file is broken!");
      return false;
    }
#ifdef HAVE_MMAP
    if (mapped) {
      munmap(sprites, sprites_size);
      mapped = 0;
    } else {
      free(sprites);
    }
#else /* ! HAVE_MMAP */
    free(sprites);
#endif
    sprites = uncompressed;
    sprites_size = uncmpsd_size;
  }

  /* Read the number of entries in the index table.
     Some entries are undefined (size and offset are zero). */
  entry_count = le32toh(*(reinterpret_cast<uint32_t*>(sprites) + 1)) + 1;

#if 0
  /* Print list of undefined entries. */
  int run_start = 0;
  spae_entry_t *entries = sprites;
  for (int i = 1; i < entry_count; i++) {
    if (run_start > 0) {
      if (entries[i].offset == 0) continue;
      int length = i - run_start;
      if (length > 1) {
        LOGD("gfx", "empty: %i-%i", run_start, i-1);
      } else {
        LOGD("gfx", "empty: %i", i-1);
      }
      run_start = 0;
    } else if (entries[i].offset == 0) {
      run_start = i;
    }
  }
#endif

  fixup();

  return load_animation_table();
}

/* Free the loaded data file. */
data_source_t::~data_source_t() {
#ifdef HAVE_MMAP
  if (mapped) {
    munmap(sprites, sprites_size);
  } else {
    free(sprites);
  }
#else /* ! HAVE_MMAP */
  free(sprites);
#endif
  sprites = NULL;
}

/* Return a pointer to the data object at index.
 If size is non-NULL it will be set to the size of the data object.
 (There's no guarantee that size is correct!). */
void *
data_source_t::get_object(unsigned int index, size_t *size) {
  assert(index > 0 && index < entry_count);

  spae_entry_t *entries = reinterpret_cast<spae_entry_t*>(sprites);
  uint8_t *bytes = reinterpret_cast<uint8_t*>(sprites);

  size_t offset = le32toh(entries[index].offset);
  assert(offset != 0);

  if (size) *size = le32toh(entries[index].size);

  return &bytes[offset];
}

/* Perform various fixups of the data file entries. */
void
data_source_t::fixup() {
  spae_entry_t *entries = reinterpret_cast<spae_entry_t*>(sprites);

  /* Fill out some undefined spaces in the index from other
     places in the data file index. */

  for (int i = 0; i < 48; i++) {
    for (int j = 1; j < 6; j++) {
      entries[3450+6*i+j].size = entries[3450+6*i].size;
      entries[3450+6*i+j].offset = entries[3450+6*i].offset;
    }
  }

  for (int i = 0; i < 3; i++) {
    entries[3765+i].size = entries[3762+i].size;
    entries[3765+i].offset = entries[3762+i].offset;
  }

  for (int i = 0; i < 6; i++) {
    entries[1363+i].size = entries[1352].size;
    entries[1363+i].offset = entries[1352].offset;
  }

  for (int i = 0; i < 6; i++) {
    entries[1613+i].size = entries[1602].size;
    entries[1613+i].offset = entries[1602].offset;
  }
}

bool
data_source_t::load_animation_table() {
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
  reinterpret_cast<uint32_t*>(get_object(DATA_SERF_ANIMATION_TABLE, &size));
  if (NULL == serf_animation_table) {
    return false;
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

  return true;
}

/* Calculate hash of sprite identifier. */
uint64_t
sprite_t::create_sprite_id(uint64_t sprite, uint64_t mask, uint64_t offset) {
  uint64_t result = sprite + (mask << 32) + (offset << 48);
  return result;
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
sprite_t *
data_source_t::create_empty_sprite(unsigned int index, uint8_t **source) {
  size_t size;
  sprite_dos_t *sprite = reinterpret_cast<sprite_dos_t*>(get_object(index,
                                                                    &size));
  if (sprite == NULL) {
    return NULL;
  }

  sprite_t *s = new sprite_t(sprite->w, sprite->h);
  s->set_delta(sprite->b_x, sprite->b_y);
  s->set_offset(sprite->x, sprite->y);

  *source = reinterpret_cast<uint8_t*>(sprite) + sizeof(sprite_dos_t);

  return s;
}

/* Create sprite object */
sprite_t *
data_source_t::get_sprite(unsigned int index) {
  uint8_t *src = NULL;
  sprite_t *s = create_empty_sprite(index, &src);
  if (s == NULL) return NULL;

  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);

  uint8_t *dest = s->get_data();
  size_t size = s->get_width() * s->get_height();

  for (size_t i = 0; i < size; i++) {
    color_dos_t color = palette[src[i]];
    dest[4*i+0] = color.r; /* Red */
    dest[4*i+1] = color.g; /* Green */
    dest[4*i+2] = color.b; /* Blue */
    dest[4*i+3] = 0xff; /* Alpha */
  }

  return s;
}

sprite_t *
data_source_t::get_empty_sprite(unsigned int index) {
  uint8_t *src = NULL;
  sprite_t *s = create_empty_sprite(index, &src);
  return s;
}

/* Create transparent sprite object */
sprite_t *
data_source_t::get_transparent_sprite(unsigned int index, int color_off) {
  uint8_t *src = NULL;
  sprite_t *s = create_empty_sprite(index, &src);
  if (s == NULL) return NULL;

  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);

  uint8_t *dest = s->get_data();
  size_t size = s->get_width() * s->get_height();

  size_t i = 0;
  size_t j = 0;
  while (j < size) {
    j += src[i];
    int n = src[i+1];

    for (int k = 0; k < n; k++) {
      uint p_index = src[i+2+k] + color_off;
      color_dos_t color = palette[p_index];
      dest[4*(j+k)+0] = color.r; /* Red */
      dest[4*(j+k)+1] = color.g; /* Green */
      dest[4*(j+k)+2] = color.b; /* Blue */
      dest[4*(j+k)+3] = 0xff; /* Alpha */
    }
    i += n + 2;
    j += n;
  }

  return s;
}

/* Create bitmap sprite object */
sprite_t *
data_source_t::get_bitmap_sprite(unsigned int index, unsigned int value) {
  uint8_t *src = NULL;
  sprite_t *s = create_empty_sprite(index, &src);
  if (s == NULL) return NULL;

  uint8_t *dest = s->get_data();
  size_t size = s->get_width() * s->get_height();

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

sprite_t *
data_source_t::get_overlay_sprite(unsigned int index) {
  return get_bitmap_sprite(index, 0x80);
}

sprite_t *
data_source_t::get_mask_sprite(unsigned int index) {
  return get_bitmap_sprite(index, 0xFF);
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
data_source_t::get_sound(unsigned int index, size_t *size) {
  size_t data_size = 0;
  void *data = get_object(DATA_SFX_BASE + index, &data_size);
  if (data == NULL) {
    return NULL;
  }

  return sfx2wav(data, data_size, size, -0x20);
}

void *
data_source_t::get_music(unsigned int index, size_t *size) {
  size_t data_size = 0;
  void *data = get_object(DATA_MUSIC_GAME + index, &data_size);
  if (NULL == data) {
    return NULL;
  }

  return xmi2mid(data, data_size, size);
}

animation_t*
data_source_t::get_animation(unsigned int animation, unsigned int phase) {
  uint8_t *tbl_ptr = reinterpret_cast<uint8_t*>(serf_animation_table) +
  serf_animation_table[animation] +
  (3 * phase);
  return reinterpret_cast<animation_t*>(tbl_ptr);
}

color_t
data_source_t::get_color(unsigned int index) {
  color_dos_t *palette = get_palette(DATA_PALETTE_GAME);
  color_t color = { palette[index].r,
                    palette[index].g,
                    palette[index].b,
                    0xff };
  return color;
}

data_source_t::color_dos_t *
data_source_t::get_palette(unsigned int index) {
  size_t size = 0;
  void *data = get_object(index, &size);
  if ((data == NULL) || (size != sizeof(color_dos_t)*256)) {
    return NULL;
  }

  return reinterpret_cast<color_dos_t*>(data);
}
