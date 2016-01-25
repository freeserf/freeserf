/*
 * tpwm.cc - Uncomprassing TPWM'ed content
 *
 * Copyright (C) 2015  Wicked_Digger  <wicked_digger@mail.ru>
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

#include "src/tpwm.h"

#include <cstring>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "src/freeserf_endian.h"

static const uint8_t tpwm_sign[4] = {'T', 'P', 'W', 'M'};

bool
tpwm_is_compressed(void *src_data, size_t src_size) {
  if (src_size < 8) {
    return false;
  }

  if (0 != memcmp(src_data, tpwm_sign, 4)) {
    return false;
  }

  return true;
}

bool
tpwm_uncompress(void *src_data, size_t src_size,
                void **res_data, size_t *res_size, const char **error) {
  if ((res_data == NULL) || (res_size == NULL)) {
    *error = "TPWM: bad parameter";
    return false;
  }

  if (!tpwm_is_compressed(src_data, src_size)) {
    *error = "TPWM: source buffer is not tpwm packed";
    return false;
  }

  *res_data = NULL;
  *res_size = 0;

  uint32_t *header = reinterpret_cast<uint32_t*>(src_data);
  *res_size = le32toh(header[1]);
  *res_data = malloc(*res_size);
  if (*res_data == NULL) {
    *error = "TPWM: unable to allocate target buffer";
    return false;
  }

  uint8_t *src_pos = reinterpret_cast<uint8_t*>(src_data) + 8;
  uint8_t *src_end = src_pos+src_size - 8;
  uint8_t *res_pos = reinterpret_cast<uint8_t*>(*res_data);
  uint8_t *res_end = res_pos + *res_size;
  bool result = true;

  while ((src_pos < src_end) && (res_pos < res_end)) {
    size_t flag = *src_pos++;
    if (src_pos >= src_end) { result = false; break; }
    for (int i = 0 ; i < 8 ; i++) {
      flag <<= 1;
      if (flag & ~0xFF) {
        flag &= 0xFF;
        size_t temp = *src_pos++;
        if (src_pos >= src_end) { result = false; break; }
        size_t repeater = (temp & 0x0F) + 3;
        size_t stamp_offset = *src_pos++ | ((temp << 4) & 0x0F00);
        uint8_t *stamp = res_pos - stamp_offset;
        while (repeater--) {
          if ((res_pos >= res_end) || (stamp >= res_end)) {
            result = false; break;
          }
          *res_pos++ = *stamp++;
        }
      } else {
        *res_pos++ = *src_pos++;
      }
    }
  }

  if (!result) {
    *error = "TPWM: unable to unpack, source data corrupted";
    free(*res_data);
    *res_data = NULL;
    *res_size = 0;
  }

  return result;
}
