/*
 * sfx2wav.cc - SFX to WAV converter.
 *
 * Copyright (C) 2015-2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include <memory>

ConvertorSFX2WAV::ConvertorSFX2WAV(PBuffer _buffer, int _level, bool _invert)
  : ConvertorPCM2WAV(_buffer, 1, 8000)
  , level(_level)
  , invert(_invert) {
}

PBuffer
ConvertorSFX2WAV::create_data(PBuffer data) {
  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  while (data->readable()) {
    int value = data->pop<uint8_t>();
    value = value + level;
    if (invert) {
      value = 0xFF - value;
    }
    value *= 0xFF;
    result->push<int16_t>(value);
  }

  return result;
}

