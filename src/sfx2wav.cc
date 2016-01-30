/*
 * sfx2wav.cc - SFX to WAV converter.
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

#include "src/sfx2wav.h"

#include <cstdlib>
#include <cstring>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "src/misc.h"
#include "src/freeserf_endian.h"

void *
sfx2wav(void* sfx, size_t sfx_size, size_t *wav_size, int level, bool invert) {
#define WRITE_DATA_WG(X) {memcpy(current, &X, sizeof(X)); current+=sizeof(X);};
#define WRITE_BE32_WG(X) {uint32_t val = X; val = htobe32(val); \
                          WRITE_DATA_WG(val);}
#define WRITE_LE32_WG(X) {uint32_t val = X; val = htole32(val); \
                          WRITE_DATA_WG(val);}
#define WRITE_BE16_WG(X) {uint16_t val = X; val = htobe16(val); \
                          WRITE_DATA_WG(val);}
#define WRITE_LE16_WG(X) {uint16_t val = X; val = htole16(val); \
                          WRITE_DATA_WG(val);}
#define WRITE_BYTE_WG(X) {*current = (uint8_t)X; current++;}

  if (wav_size != NULL) {
    *wav_size = 0;
  }

  size_t size = 44 + sfx_size * 2;

  char *result = reinterpret_cast<char*>(malloc(size));
  if (result == NULL) {
    return NULL;
  }

  char *current = result;

  /* WAVE header */
  WRITE_BE32_WG(0x52494646);              /* 'RIFF' */
  WRITE_LE32_WG((uint32_t)size - 8);      /* Chunks size */
  WRITE_BE32_WG(0x57415645);              /* 'WAVE' */

  /* Subchunk #1 */
  WRITE_BE32_WG(0x666d7420);              /* 'fmt ' */
  WRITE_LE32_WG(16);                      /* Subchunk size */
  WRITE_LE16_WG(1);                       /* Format = PCM */
  WRITE_LE16_WG(1);                       /* Chanels count */
  WRITE_LE32_WG(8000);                    /* Rate */
  WRITE_LE32_WG(16000);                   /* Byte rate */
  WRITE_LE16_WG(2);                       /* Block align */
  WRITE_LE16_WG(16);                      /* Bits per sample */

  /* Subchunk #2 */
  WRITE_BE32_WG(0x64617461);              /* 'data' */
  WRITE_LE32_WG((uint32_t)sfx_size*2);    /* Data size */

  unsigned char *source = reinterpret_cast<unsigned char*>(sfx);
  while (sfx_size) {
    int value = *(source++);
    value = value + level;
    if (invert) {
      value = 0xFF - value;
    }
    WRITE_BE16_WG(value*0xFF);
    sfx_size--;
  }

  if (wav_size != NULL) {
    *wav_size = size;
  }

  return result;
}
