/*
 * test_tpwm.cc - TPWM unpacking tests
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "src/tpwm.h"
#include "src/debug.h"

/* "TPWM", the unpacked size (32 bits, little endian), the packed data. */
static std::vector<uint8_t>
packed(size_t size, const std::vector<uint8_t> &data) {
  std::vector<uint8_t> file = { 'T', 'P', 'W', 'M',
                                static_cast<uint8_t>(size),
                                static_cast<uint8_t>(size >> 8),
                                static_cast<uint8_t>(size >> 16),
                                static_cast<uint8_t>(size >> 24) };
  file.insert(file.end(), data.begin(), data.end());
  return file;
}

static std::string
unpack(std::vector<uint8_t> file) {
  PBuffer buffer = std::make_shared<Buffer>(file.data(), file.size());
  UnpackerTPWM unpacker(buffer);
  PBuffer result = unpacker.convert();
  return std::string(reinterpret_cast<const char*>(result->get_data()),
                     result->get_size());
}

/* Two literals, then a copy of 3 bytes from 2 back, which overlaps what it
   adds; the flag bits left after the end are not used. */
TEST(TPWM, OverlappingCopy) {
  EXPECT_EQ("ababa", unpack(packed(5, { 0x20, 'a', 'b', 0x00, 0x02 })));
}

/* The size has 32 bits: data over 64 KiB. */
TEST(TPWM, LargeSize) {
  const size_t size = 70000;
  std::vector<uint8_t> data;
  for (size_t i = 0; i < size; i++) {
    if (i % 8 == 0) data.push_back(0x00);
    data.push_back(static_cast<uint8_t>(i));
  }
  std::string result = unpack(packed(size, data));
  ASSERT_EQ(size, result.size());
  EXPECT_EQ(static_cast<char>(69999 & 0xff), result[69999]);
}

TEST(TPWM, Truncated) {
  EXPECT_THROW(unpack(packed(10, { 0x00, 'a', 'b' })), ExceptionFreeserf);
}

TEST(TPWM, CopyBeforeStart) {
  EXPECT_THROW(unpack(packed(5, { 0x80, 0x00, 0x02 })), ExceptionFreeserf);
}
