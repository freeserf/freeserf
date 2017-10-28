/*
 * pcm2wav.cc - PCM to WAV converter.
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/pcm2wav.h"

#include <string>
#include <memory>

ConvertorPCM2WAV::ConvertorPCM2WAV(PBuffer _buffer,
                                   size_t _chenals,
                                   size_t _rate)
  : Convertor(_buffer)
  , chenals(_chenals)
  , rate(_rate) {
}

PBuffer
ConvertorPCM2WAV::convert() {
  result = std::make_shared<MutableBuffer>(Buffer::EndianessLittle);

  PBuffer header = create_header();
  PBuffer data = create_data(buffer);

  result->push("RIFF");
  result->push<uint32_t>(static_cast<uint32_t>(header->get_size() +
                                               data->get_size() + 20));
  result->push("WAVE");
  write_chunk("fmt ", header);
  write_chunk("data", data);

  return result;
}

void
ConvertorPCM2WAV::write_chunk(const std::string &name, PBuffer data) {
  result->push(name);
  result->push<uint32_t>(static_cast<uint32_t>(data->get_size()));
  result->push(data);
}

PBuffer
ConvertorPCM2WAV::create_header() {
  PMutableBuffer header =
                       std::make_shared<MutableBuffer>(Buffer::EndianessLittle);

  header->push<uint16_t>(1);                                    // Format = PCM
  header->push<uint16_t>(static_cast<uint16_t>(chenals));       // Chanels count
  header->push<uint32_t>(static_cast<uint32_t>(rate));                // Rate
  header->push<uint32_t>(static_cast<uint32_t>(rate * chenals * 2));  // Bytes
  header->push<uint16_t>(static_cast<uint16_t>(chenals * 2));   // Block align
  header->push<uint16_t>(16);                                 // Bits per sample

  return header;
}

PBuffer
ConvertorPCM2WAV::create_data(PBuffer data) {
  return data;
}
