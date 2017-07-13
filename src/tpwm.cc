/*
 * tpwm.cc - Uncomprassing TPWM'ed content
 *
 * Copyright (C) 2015-2017  Wicked_Digger  <wicked_digger@mail.ru>
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

#include <string>
#include <memory>

#include "src/freeserf_endian.h"

UnpackerTPWM::UnpackerTPWM(PBuffer _buffer) : Convertor(_buffer) {
  if (buffer->get_size() < 8) {
    throw ExceptionFreeserf("Data is not TPWM archive");
  }

  PBuffer id = buffer->pop(4);
  if ((std::string)*id.get() != "TPWM") {
    throw ExceptionFreeserf("Data is not TPWM archive");
  }
}

PBuffer
UnpackerTPWM::convert() {
  size_t res_size = buffer->pop16le();
  PMutableBuffer result = std::make_shared<MutableBuffer>();

  try {
    while (buffer->readable()) {
      size_t flag = buffer->pop();
      for (int i = 0 ; i < 8 ; i++) {
        flag <<= 1;
        if (flag & ~0xFF) {
          flag &= 0xFF;
          size_t temp = buffer->pop();
          size_t stamp_size = (temp & 0x0F) + 3;
          size_t stamp_offset = buffer->pop();
          stamp_offset |= ((temp << 4) & 0x0F00);
          PBuffer stamp = result->get_subbuffer(result->get_size() -
                                                  stamp_offset,
                                                stamp_size);
          result->push(stamp);
        } else {
          result->push(buffer->pop());
        }
      }
    }
  } catch (ExceptionFreeserf e) {
    throw ExceptionFreeserf("TPWM source data corrupted");
  }

  if (result->get_size() != res_size) {
    throw ExceptionFreeserf("TPWM source data corrupted");
  }

  return result;
}
