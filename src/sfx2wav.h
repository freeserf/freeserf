/*
 * sfx2wav.h - SFX to WAV converter.
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

#ifndef SRC_SFX2WAV_H_
#define SRC_SFX2WAV_H_

#include "src/pcm2wav.h"

class ConvertorSFX2WAV : public ConvertorPCM2WAV {
 protected:
  int level;
  bool invert;

 public:
  explicit ConvertorSFX2WAV(PBuffer buffer, int level = 0, bool invert = false);

 protected:
  virtual PBuffer create_data(PBuffer data);
};

#endif  // SRC_SFX2WAV_H_
