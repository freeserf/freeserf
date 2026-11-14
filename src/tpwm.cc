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

#include <cstring>
#include <memory>
#include <vector>

#include "src/freeserf_endian.h"
#include "src/debug.h"

UnpackerTPWM::UnpackerTPWM(PBuffer _buffer) : Convertor(_buffer) {
  if (!is_packed(buffer)) {
    throw ExceptionFreeserf("Data is not TPWM archive");
  }
  buffer->pop(4);
}

bool
UnpackerTPWM::is_packed(PBuffer buffer) {
  return (buffer->get_size() >= 8) &&
         (std::memcmp(buffer->get_data(), "TPWM", 4) == 0);
}

PBuffer
UnpackerTPWM::convert() {
  /* The size of the unpacked data: 32 bits, little endian. */
  buffer->set_endianess(Buffer::EndianessLittle);
  size_t res_size = buffer->pop<uint32_t>();
  std::vector<uint8_t> result;
  result.reserve(res_size);

  /* Buffer::pop does not check the end. */
  auto pop_byte = [this]() -> size_t {
    if (!buffer->readable()) {
      throw ExceptionFreeserf("TPWM source data corrupted");
    }
    return buffer->pop<uint8_t>();
  };

  try {
    /* The last flags byte may have bits left over the end. */
    while (result.size() < res_size) {
      size_t flag = pop_byte();
      for (int i = 0 ; (i < 8) && (result.size() < res_size) ; i++) {
        flag <<= 1;
        if (flag & ~0xFF) {
          flag &= 0xFF;
          size_t temp = pop_byte();
          size_t stamp_size = (temp & 0x0F) + 3;
          size_t stamp_offset = pop_byte();
          stamp_offset |= ((temp << 4) & 0x0F00);
          if ((stamp_offset == 0) || (stamp_offset > result.size())) {
            throw ExceptionFreeserf("TPWM source data corrupted");
          }
          /* Byte by byte: the copy may overlap what it adds. */
          size_t from = result.size() - stamp_offset;
          for (size_t j = 0; j < stamp_size; j++) {
            result.push_back(result[from + j]);
          }
        } else {
          result.push_back(static_cast<uint8_t>(pop_byte()));
        }
      }
    }
  } catch (...) {
    throw ExceptionFreeserf("TPWM source data corrupted");
  }

  /* An overlapping copy may go over the end. */
  result.resize(res_size);
  /* Reserved, so that reading starts at the data; the data of the
     original is little endian. */
  PMutableBuffer unpacked =
    std::make_shared<MutableBuffer>(res_size, Buffer::EndianessLittle);
  unpacked->push(static_cast<const void*>(result.data()), result.size());
  return unpacked;
}
