/*
 * gfx.cc - General graphics and data file functions
 *
 * Copyright (C) 2013-2015  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/gfx.h"

#include <cassert>
#include <cstring>
#include <algorithm>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/data.h"
  #include "src/log.h"
END_EXT_C
#include "src/video-sdl.h"

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

/* Unique identifier for a sprite. */
typedef struct {
  const dos_sprite_t *sprite;
  const dos_sprite_t *mask;
  uint offset;
} sprite_id_t;

typedef struct sprite_ht_entry sprite_ht_entry_t;

/* Entry in the hashtable of sprites. */
struct sprite_ht_entry {
  sprite_ht_entry_t *next;
  sprite_id_t id;
  sprite_t *value;
};

/* Hashtable of sprites, used as sprite cache. */
typedef struct {
  size_t size;
  uint entry_count;
  sprite_ht_entry_t **entries;
} sprite_ht_t;


/* Sprite cache hash table */
static sprite_ht_t sprite_cache;


/* Calculate hash of sprite identifier. */
static uint32_t
sprite_id_hash(const sprite_id_t *id) {
  const uint8_t *s = reinterpret_cast<const uint8_t*>(id);

  /* FNV-1 */
  uint32_t hash = 2166136261;
  for (int i = 0; i < sizeof(sprite_id_t); i++) {
    hash *= 16777619;
    hash ^= s[i];
  }

  return hash;
}

/* Intialize sprite hashtable. */
static int
sprite_ht_init(sprite_ht_t *ht, size_t size) {
  ht->size = size;
  ht->entries = reinterpret_cast<sprite_ht_entry_t**>(calloc(size,
                                                  sizeof(sprite_ht_entry_t *)));
  if (ht->entries == NULL) return -1;

  ht->entry_count = 0;

  return 0;
}

/* Return a pointer to the surface pointer associated with
   id. If it does not exist in the table it a new entry is
   created. */
static sprite_ht_entry_t *
sprite_ht_store(sprite_ht_t *ht, const sprite_id_t *id) {
  uint32_t hash = sprite_id_hash(id);
  sprite_ht_entry_t *entry =
            reinterpret_cast<sprite_ht_entry_t*>(&ht->entries[hash % ht->size]);

  /* The first entry pointed to is not really an
     entry but a sentinel. */
  while (entry->next != NULL) {
    entry = entry->next;
    if (memcmp(id, &entry->id, sizeof(sprite_id_t)) == 0) {
      return entry;
    }
  }

  sprite_ht_entry_t *new_entry = reinterpret_cast<sprite_ht_entry_t*>(calloc(1,
                                                    sizeof(sprite_ht_entry_t)));
  if (new_entry == NULL) return NULL;

  ht->entry_count += 1;

  entry->next = new_entry;
  memcpy(&new_entry->id, id, sizeof(sprite_id_t));

  return new_entry;
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
gfx_create_empty_sprite(const dos_sprite_t *sprite) {
  uint size = sprite->w * sprite->h * 4;
  sprite_t *s = reinterpret_cast<sprite_t*>(calloc(sizeof(sprite_t) + size, 1));
  if (s == NULL) return NULL;

  s->delta_x = sprite->b_x;
  s->delta_y = sprite->b_y;
  s->offset_x = sprite->x;
  s->offset_y = sprite->y;
  s->width = sprite->w;
  s->height = sprite->h;

  return s;
}

/* Create sprite object */
static sprite_t *
gfx_create_sprite(const dos_sprite_t *sprite) {
  sprite_t *s = gfx_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  uint8_t *palette =
           reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
                       sizeof(dos_sprite_t);
  uint8_t *dest = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  uint size = sprite->w * sprite->h;

  for (uint i = 0; i < size; i++) {
    dest[4*i+0] = palette[3*src[i]+0]; /* Red */
    dest[4*i+1] = palette[3*src[i]+1]; /* Green */
    dest[4*i+2] = palette[3*src[i]+2]; /* Blue */
    dest[4*i+3] = 0xff; /* Alpha */
  }

  return s;
}

/* Create transparent sprite object */
static sprite_t *
gfx_create_transparent_sprite(const dos_sprite_t *sprite, int color_off) {
  sprite_t *s = gfx_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  uint8_t *palette =
           reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
                       sizeof(dos_sprite_t);
  uint8_t *dest = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  uint size = sprite->w * sprite->h;

  uint i = 0;
  uint j = 0;
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
static sprite_t *
gfx_create_bitmap_sprite(const dos_sprite_t *sprite, uint value) {
  sprite_t *s = gfx_create_empty_sprite(sprite);
  if (s == NULL) return NULL;

  const uint8_t *src = reinterpret_cast<const uint8_t*>(sprite) +
                       sizeof(dos_sprite_t);
  uint8_t *dest = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  uint size = sprite->w * sprite->h;

  uint i = 0;
  uint j = 0;
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

/* Free sprite object */
static void
gfx_free_sprite(sprite_t *sprite) {
  free(sprite);
}

/* Apply mask to map tile sprite
   The resulting sprite will be extended to the height of the mask
   by repeating lines from the top of the sprite. The width of the
   mask and the sprite must be identical. */
static sprite_t *
gfx_mask_map_tile_sprite(const sprite_t *sprite, const sprite_t *mask) {
  uint size = mask->width * mask->height * 4;
  sprite_t *s = reinterpret_cast<sprite_t*>(calloc(sizeof(sprite_t) + size, 1));
  if (s == NULL) return NULL;

  s->delta_x = mask->delta_x;
  s->delta_y = mask->delta_y;
  s->offset_x = mask->offset_x;
  s->offset_y = mask->offset_y;
  s->width = mask->width;
  s->height = mask->height;

  const uint8_t *src_data = reinterpret_cast<const uint8_t*>(sprite) +
                            sizeof(sprite_t);
  uint8_t *dest_data = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  uint to_copy = size;

  /* Copy extended data to new sprite */
  while (to_copy > 0) {
    uint s = std::min(to_copy, sprite->width * sprite->height * 4);
    memcpy(dest_data, src_data, s);
    to_copy -= s;
    dest_data += s;
  }

  /* Apply mask from mask sprite */
  uint8_t *s_data = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  const uint8_t *m_data = reinterpret_cast<const uint8_t*>(mask) +
                          sizeof(sprite_t);
  for (uint y = 0; y < mask->height; y++) {
    for (uint x = 0; x < mask->width; x++) {
      int alpha_index = 4*(y * mask->width + x) + 3;
      s_data[alpha_index] &= m_data[alpha_index];
    }
  }

  return s;
}

/* Apply mask to waves sprite
   The mask will be applied at a horizontal offset on the
   sprite, and the resulting sprite will be limited by the
   size of the mask, and the height of the sprite. The sprite
   must be at least as wide as the mask plus the mask offset. */
static sprite_t *
gfx_mask_waves_sprite(const sprite_t *sprite, const sprite_t *mask, int x_off) {
  uint height = std::min(mask->height, sprite->height);
  uint size = mask->width * height * 4;
  sprite_t *s = reinterpret_cast<sprite_t*>(calloc(sizeof(sprite_t) + size, 1));
  if (s == NULL) return NULL;

  s->delta_x = sprite->delta_x;
  s->delta_y = sprite->delta_y;
  s->offset_x = sprite->offset_x;
  s->offset_y = sprite->offset_y;
  s->width = mask->width;
  s->height = height;

  const uint8_t *src_data = reinterpret_cast<const uint8_t*>(sprite) +
                            sizeof(sprite_t) + 4*x_off;
  uint8_t *dest_data = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);

  /* Copy data to new sprite */
  for (uint i = 0; i < height; i++) {
    memcpy(dest_data, src_data, mask->width * 4);
    src_data += 4*sprite->width;
    dest_data += 4*mask->width;
  }

  /* Apply mask from mask sprite */
  uint8_t *s_data = reinterpret_cast<uint8_t*>(s) + sizeof(sprite_t);
  const uint8_t *m_data = reinterpret_cast<const uint8_t*>(mask) +
                          sizeof(sprite_t);
  for (uint y = 0; y < s->height; y++) {
    for (uint x = 0; x < s->width; x++) {
      int alpha_index = 4*(y * s->width + x) + 3;
      s_data[alpha_index] &= m_data[alpha_index];
    }
  }

  return s;
}

gfx_t *gfx_t::instance = NULL;

gfx_t::gfx_t(unsigned int width, unsigned int height, bool fullscreen) {
  video = new video_sdl_t();

  LOGI("graphics", "Setting resolution to %ix%i...", width, height);

  bool r = video->set_resolution(width, height, fullscreen);
  assert(r);

  const dos_sprite_t *cursor = data_get_dos_sprite(DATA_CURSOR);
  sprite_t *sprite = gfx_create_transparent_sprite(cursor, 0);
  video->set_cursor(sprite);
  gfx_free_sprite(sprite);

  /* Init sprite cache */
  sprite_ht_init(&sprite_cache, 10240);

  gfx_t::instance = this;
}

gfx_t::~gfx_t() {
  if (video != NULL) {
    delete video;
    video = NULL;
  }
}

gfx_t*
gfx_t::get_instance() {
  return instance;
}

/* Draw the opaque sprite with data file index of
   sprite at x, y in dest frame. */
void
frame_t::draw_sprite(int x, int y, uint sprite) {
  const dos_sprite_t *spr = data_get_dos_sprite(sprite);
  assert(spr != NULL);

  sprite_id_t id;
  id.sprite = spr;
  id.mask = NULL;
  id.offset = 0;
  sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
  if (entry->value == NULL) {
    sprite_t *s = gfx_create_sprite(spr);
    assert(s != NULL);
    entry->value = s;
  }

  x += entry->value->offset_x;
  y += entry->value->offset_y;
  video->draw_sprite(entry->value, x, y, 0, this);
}

/* Draw the transparent sprite with data file index of
   sprite at x, y in dest frame.*/
void
frame_t::draw_transp_sprite(int x, int y, uint sprite, int use_off,
                            int y_off, int color_off) {
  const dos_sprite_t *spr = data_get_dos_sprite(sprite);
  assert(spr != NULL);

  sprite_id_t id;
  id.sprite = spr;
  id.mask = NULL;
  id.offset = color_off;
  sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
  if (entry->value == NULL) {
    sprite_t *s = gfx_create_transparent_sprite(spr, color_off);
    assert(s != NULL);
    entry->value = s;
  }

  if (use_off) {
    x += entry->value->offset_x;
    y += entry->value->offset_y;
  }
  video->draw_sprite(entry->value, x, y, y_off, this);
}

/* Draw the masked sprite with given mask and sprite
   indices at x, y in dest frame. */
void
frame_t::draw_masked_sprite(int x, int y, uint mask, uint sprite) {
  const dos_sprite_t *spr = data_get_dos_sprite(sprite);
  assert(spr != NULL);

  const dos_sprite_t *msk = data_get_dos_sprite(mask);
  assert(msk != NULL);

  sprite_id_t id;
  id.sprite = spr;
  id.mask = msk;
  id.offset = 0;
  sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
  if (entry->value == NULL) {
    sprite_t *s = gfx_create_sprite(spr);
    assert(s != NULL);

    sprite_t *m = gfx_create_bitmap_sprite(msk, 0xff);
    assert(m != NULL);

    sprite_t *masked = gfx_mask_map_tile_sprite(s, m);
    assert(masked != NULL);

    entry->value = masked;

    gfx_free_sprite(s);
    gfx_free_sprite(m);
  }

  x += entry->value->offset_x;
  y += entry->value->offset_y;
  video->draw_sprite(entry->value, x, y, 0, this);
}

/* Draw the overlay sprite with data file index of
   sprite at x, y in dest frame. Rendering will be
   offset in the vertical axis from y_off in the
   sprite. */
void
frame_t::draw_overlay_sprite(int x, int y, uint sprite, int y_off) {
  const dos_sprite_t *spr = data_get_dos_sprite(sprite);
  assert(spr != NULL);

  sprite_id_t id;
  id.sprite = spr;
  id.mask = NULL;
  id.offset = 0;
  sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
  if (entry->value == NULL) {
    sprite_t *s = gfx_create_bitmap_sprite(spr, 0x80);
    assert(s != NULL);
    entry->value = s;
  }

  x += entry->value->offset_x;
  y += entry->value->offset_y;
  video->draw_sprite(entry->value, x, y, y_off, this);
}

/* Draw the waves sprite with given mask and sprite
   indices at x, y in dest frame. */
void
frame_t::draw_waves_sprite(int x, int y, uint mask, uint sprite, int mask_off) {
  const dos_sprite_t *spr = data_get_dos_sprite(sprite);
  assert(spr != NULL);

  const dos_sprite_t *msk = NULL;
  if (mask > 0) {
    msk = data_get_dos_sprite(mask);
    assert(msk != NULL);
  }

  sprite_id_t id;
  id.sprite = spr;
  id.mask = msk;
  id.offset = mask_off;
  sprite_ht_entry_t *entry = sprite_ht_store(&sprite_cache, &id);
  if (entry->value == NULL) {
    sprite_t *s = gfx_create_transparent_sprite(spr, 0);
    assert(s != NULL);

    sprite_t *m = NULL;
    if (msk != NULL) {
      m = gfx_create_bitmap_sprite(msk, 0xff);
      assert(m != NULL);

      sprite_t *masked = gfx_mask_waves_sprite(s, m, mask_off);
      assert(masked != NULL);

      gfx_free_sprite(s);
      gfx_free_sprite(m);

      entry->value = masked;
    } else {
      entry->value = s;
    }
  }

  x += entry->value->offset_x;
  y += entry->value->offset_y;
  video->draw_sprite(entry->value, x, y, 0, this);
}


/* Draw a character at x, y in the dest frame. */
void
frame_t::draw_char_sprite(int x, int y, uint c, int color, int shadow) {
  static const int sprite_offset_from_ascii[] = {
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, 43, -1, -1,
    -1, -1, -1, -1, -1, 40, 39, -1,
    29, 30, 31, 32, 33, 34, 35, 36,
    37, 38, 41, -1, -1, -1, -1, 42,
    -1,  0,  1,  2,  3,  4,  5,  6,
     7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,
     7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 24, 25, -1, -1, -1, -1, -1,

    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
  };

  int s = sprite_offset_from_ascii[c];
  if (s < 0) return;

  if (shadow) {
    draw_transp_sprite(x, y, DATA_FONT_SHADOW_BASE + s, 0, 0, shadow);
  }
  draw_transp_sprite(x, y, DATA_FONT_BASE + s, 0, 0, color);
}

/* Draw the string str at x, y in the dest frame. */
void
frame_t::draw_string(int x, int y, int color, int shadow, const char *str) {
  for (; *str != 0; x += 8) {
    if (/*string_bg*/ 0) {
      fill_rect(x, y, 8, 8, 0);
    }

    unsigned char c = *str++;
    draw_char_sprite(x, y, c, color, shadow);
  }
}

/* Draw the number n at x, y in the dest frame. */
void
frame_t::draw_number(int x, int y, int color, int shadow, int n) {
  if (n < 0) {
    draw_char_sprite(x, y, '-', color, shadow);
    x += 8;
    n *= -1;
  }

  if (n == 0) {
    draw_char_sprite(x, y, '0', color, shadow);
    return;
  }

  int digits = 0;
  for (int i = n; i > 0; i /= 10) digits += 1;

  for (int i = digits-1; i >= 0; i--) {
    draw_char_sprite(x+8*i, y, '0'+(n % 10), color, shadow);
    n /= 10;
  }
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
frame_t::draw_rect(int x, int y, int width, int height, int color) {
  uint8_t *palette =
           reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));
  color_t c = { palette[3*color+0],
                palette[3*color+1],
                palette[3*color+2],
                0xff };

  video->draw_rect(x, y, width, height, &c, this);
}

/* Draw a rectangle with color at x, y in the dest frame. */
void
frame_t::fill_rect(int x, int y, int width, int height, int color) {
  uint8_t *palette =
           reinterpret_cast<uint8_t*>(data_get_object(DATA_PALETTE_GAME, NULL));
  color_t c = { palette[3*color+0],
                palette[3*color+1],
                palette[3*color+2],
                0xff };

  video->fill_rect(x, y, width, height, &c, this);
}

/* Initialize new graphics frame. If dest is NULL a new
   backing surface is created, otherwise the same surface
   as dest is used. */
frame_t::frame_t(video_sdl_t *video, unsigned int width, unsigned int height) {
  this->video = video;
  video->frame_init(this, width, height);
}

/* Deinitialize frame and backing surface. */
frame_t::~frame_t() {
  video->frame_deinit(this);
}

/* Draw source frame from rectangle at sx, sy with given
   width and height, to destination frame at dx, dy. */
void
frame_t::draw_frame(int dx, int dy, int sx, int sy, frame_t *src,
                    int w, int h) {
  video->draw_frame(dx, dy, this, sx, sy, src, w, h);
}

frame_t *
gfx_t::create_frame(unsigned int width, unsigned int height) {
  return new frame_t(video, width, height);
}

/* Enable or disable fullscreen mode */
bool
gfx_t::set_fullscreen(bool enable) {
  return video->set_fullscreen(enable);
}

/* Check whether fullscreen mode is enabled */
bool
gfx_t::is_fullscreen() {
  return video->is_fullscreen();
}

frame_t *
gfx_t::get_screen_frame() {
  return video->get_screen_frame();
}

void
gfx_t::set_resolution(unsigned int width, unsigned int height,
                      bool fullscreen) {
  video->set_resolution(width, height, fullscreen);
}

void
gfx_t::get_resolution(unsigned int *width, unsigned int *height) {
}

void
gfx_t::swap_buffers() {
  video->swap_buffers();
}
