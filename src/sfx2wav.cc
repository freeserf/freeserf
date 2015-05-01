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
#include <cstdint>

#include "src/freeserf_endian.h"

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

char *write_wave_header(char *current, size_t data_size, size_t chenals,
                        size_t rate) {
  /* WAVE header */
  WRITE_BE32_WG(0x52494646);                      /* 'RIFF' */
  WRITE_LE32_WG((uint32_t)data_size + 36);        /* Chunks size */
  WRITE_BE32_WG(0x57415645);                      /* 'WAVE' */

  /* Subchunk #1 */
  WRITE_BE32_WG(0x666d7420);                      /* 'fmt ' */
  WRITE_LE32_WG(16);                              /* Subchunk size */
  WRITE_LE16_WG(1);                               /* Format = PCM */
  WRITE_LE16_WG((uint16_t)chenals);               /* Chanels count */
  WRITE_LE32_WG((uint32_t)rate);                  /* Rate */
  WRITE_LE32_WG((uint32_t)(rate * 2 * chenals));  /* Byte rate */
  WRITE_LE16_WG((uint16_t)(2 * chenals));         /* Block align */
  WRITE_LE16_WG(16);                              /* Bits per sample */

  /* Subchunk #2 */
  WRITE_BE32_WG(0x64617461);                      /* 'data' */
  WRITE_LE32_WG((uint32_t)data_size);             /* Data size */

  return current;
}

void *
sfx2wav(void* sfx, size_t sfx_size, size_t *wav_size, int level, bool invert) {
  if (wav_size != NULL) {
    *wav_size = 0;
  }

  size_t size = 44 + sfx_size * 2;

  char *result = reinterpret_cast<char*>(malloc(size));
  if (result == NULL) {
    return NULL;
  }

  char *current = result;

  current = write_wave_header(current, sfx_size * 2, 1, 8000);

  unsigned char *source = reinterpret_cast<unsigned char*>(sfx);
  while (sfx_size) {
    int value = *(source++);
    value = value + level;
    if (invert) {
      value = 0xFF - value;
    }
    value *= 0xFF;
    WRITE_BE16_WG(value);
    sfx_size--;
  }

  if (wav_size != NULL) {
    *wav_size = size;
  }

  return result;
}

void *sfx2wav16(void* sfx, size_t sfx_size, size_t *wav_size) {
  if (wav_size != NULL) {
    *wav_size = 0;
  }

  size_t size = 44 + sfx_size;

  char *result = reinterpret_cast<char*>(malloc(size));
  if (result == NULL) {
    return NULL;
  }

  char *current = result;

  current = write_wave_header(current, sfx_size, 2, 44100);

  memcpy(current, sfx, sfx_size);

  if (wav_size != NULL) {
    *wav_size = size;
  }

  return result;
}
