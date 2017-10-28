/*
 * mod2wav.cc - MOD to WAV converter.
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

#include "src/mod2wav.h"

#include <xmp.h>
#include <string>
#include <memory>

#include "src/pcm2wav.h"
#include "src/debug.h"

ConvertorMOD2WAV::ConvertorMOD2WAV(PBuffer _buffer) : Convertor(_buffer) {
  PBuffer id = buffer->get_subbuffer(1080, 4);
  if ((std::string)*id.get() != "M!K!") {
    throw ExceptionFreeserf("mod2wav: wrong input data format");
  }
}

PBuffer
ConvertorMOD2WAV::convert() {
  PMutableBuffer result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);
  xmp_context ctx = xmp_create_context();
  if (xmp_load_module_from_memory(ctx, buffer->get_data(),
                              static_cast<uint32_t>(buffer->get_size())) == 0) {
    if (xmp_start_player(ctx, 44100, 0) == 0) {
      struct xmp_frame_info fi;
      while (xmp_play_frame(ctx) == 0) {
        xmp_get_frame_info(ctx, &fi);
        if (fi.loop_count > 0) {
          break;
        }
        result->push(fi.buffer, fi.buffer_size);
      }
      xmp_end_player(ctx);
    }
    xmp_release_module(ctx);
  }
  xmp_free_context(ctx);

  ConvertorPCM2WAV converter(result, 2, 44100);
  return converter.convert();
}

