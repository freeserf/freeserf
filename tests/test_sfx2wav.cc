/*
 * test_sfx2wav.cc - SFX to WAV conversion tests
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
#include <vector>

#include "src/sfx2wav.h"

static std::vector<int16_t>
convert(std::vector<uint8_t> sfx, int level, bool is_signed) {
  PBuffer buffer = std::make_shared<Buffer>(sfx.data(), sfx.size());
  ConvertorSFX2WAV convertor(buffer, level, false, 8000, is_signed);
  PBuffer wav = convertor.convert();

  // Samples follow the 44 bytes of the RIFF, fmt and data headers.
  const uint8_t *data = reinterpret_cast<const uint8_t*>(wav->get_data());
  EXPECT_EQ(44 + sfx.size() * 2, wav->get_size());
  std::vector<int16_t> samples;
  for (size_t i = 0; i < sfx.size(); i++) {
    samples.push_back(static_cast<int16_t>(data[44 + i * 2] |
                                           (data[44 + i * 2 + 1] << 8)));
  }
  return samples;
}

TEST(SFX2WAV, SignedSamples) {
  // Amiga samples are signed 8-bit.
  std::vector<int16_t> expected{0, 256, 32512, -32768, -256};
  EXPECT_EQ(expected, convert({0x00, 0x01, 0x7f, 0x80, 0xff}, 0, true));
}

TEST(SFX2WAV, UnsignedSamplesWithLevel) {
  // DOS samples are unsigned and centred at 32.
  std::vector<int16_t> expected{0, -8192, 7936};
  EXPECT_EQ(expected, convert({32, 0, 63}, -32, false));
}
