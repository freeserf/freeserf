/*
 * pcm2wav.h - PCM to WAV converter.
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

#ifndef SRC_PCM2WAV_H_
#define SRC_PCM2WAV_H_

#include <string>

#include "src/convertor.h"

class ConvertorPCM2WAV : public Convertor {
 protected:
  PMutableBuffer result;
  size_t chenals;
  size_t rate;

 public:
  ConvertorPCM2WAV(PBuffer buffer, size_t chenals, size_t rate);

  virtual PBuffer convert();

 protected:
  void write_chunk(std::string name, PBuffer data);
  PBuffer create_header();
  virtual PBuffer create_data(PBuffer data);
};

#endif  // SRC_PCM2WAV_H_
