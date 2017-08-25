/*
 * freeserf_endian.h - Endianness conversions
 *
 * Copyright (C) 2007-2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_FREESERF_ENDIAN_H_
#define SRC_FREESERF_ENDIAN_H_

#include <cstdint>

inline bool
is_big_endian() {
  union {
    uint32_t i;
    char c[4];
  } bint = {0x01020304};

  return (bint.c[0] == 1);
}

template <typename T>
T byte_swap(T x) {
  union {
    T x;
    unsigned char x8[sizeof(T)];
  } source, dest;

  source.x = x;

  for (size_t i = 0; i < sizeof(T); i++) {
    dest.x8[i] = source.x8[sizeof(T) - i - 1];
  }

  return dest.x;
}

template <typename T>
T betoh(T x) {
  return is_big_endian() ? x : byte_swap(x);
}

template <typename T>
T letoh(T x) {
  return is_big_endian() ? byte_swap(x) : x;
}

template <typename T>
T htobe(T x) {
  return is_big_endian() ? x : byte_swap(x);
}

template <typename T>
T htole(T x) {
  return is_big_endian() ? byte_swap(x) : x;
}

#endif  // SRC_FREESERF_ENDIAN_H_
