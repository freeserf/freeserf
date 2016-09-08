/*
 * random.cc - Random number generator
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/random.h"

#include <ctime>
#include <cstdlib>
#include <sstream>
#include <cstdint>

Random::Random() {
  srand((unsigned int)time(NULL));
  state[0] = std::rand();
  state[1] = std::rand();
  state[2] = std::rand();
  random();
}

Random::Random(const uint16_t &value) {
  state[0] = value;
  state[1] = value;
  state[2] = value;
}

Random::Random(const Random &random_state) {
  state[0] = random_state.state[0];
  state[1] = random_state.state[1];
  state[2] = random_state.state[2];
}

Random::Random(const std::string &string) {
  uint64_t tmp = 0;

  for (int i = 15; i >= 0; i--) {
    tmp <<= 3;
    uint8_t c = string[i] - '0' - 1;
    tmp |= c;
  }

  state[0] = tmp & 0xFFFF;
  tmp >>= 16;
  state[1] = tmp & 0xFFFF;
  tmp >>= 16;
  state[2] = tmp & 0xFFFF;
}

Random::Random(uint16_t base_0, uint16_t base_1, uint16_t base_2) {
  state[0] = base_0;
  state[1] = base_1;
  state[2] = base_2;
}

uint16_t
Random::random() {
  uint16_t *rnd = state;
  uint16_t r = (rnd[0] + rnd[1]) ^ rnd[2];
  rnd[2] += rnd[1];
  rnd[1] ^= rnd[2];
  rnd[1] = (rnd[1] >> 1) | (rnd[1] << 15);
  rnd[2] = (rnd[2] >> 1) | (rnd[2] << 15);
  rnd[0] = r;

  return r;
}

Random::operator std::string() const {
  uint64_t tmp0 = state[0];
  uint64_t tmp1 = state[1];
  uint64_t tmp2 = state[2];

  std::stringstream ss;

  uint64_t tmp = tmp0;
  tmp |= tmp1 << 16;
  tmp |= tmp2 << 32;

  for (int i = 0; i < 16; i++) {
    uint8_t c = tmp & 0x07;
    c += '1';
    ss << c;
    tmp >>= 3;
  }

  return ss.str();
}

Random&
operator^=(Random& left, const Random& right) {  // NOLINT
  left.state[0] ^= right.state[0];
  left.state[1] ^= right.state[1];
  left.state[2] ^= right.state[2];

  return left;
}
