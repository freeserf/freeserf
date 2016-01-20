/*
 * audio-dummy.h - Music and sound without playback.
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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


#ifndef SRC_AUDIO_DUMMY_H_
#define SRC_AUDIO_DUMMY_H_

#include "src/audio.h"

class audio_dummy_t : public audio_t {
 public:
  /* Common audio. */
  audio_dummy_t() {}
  virtual ~audio_dummy_t() {}

  virtual audio_volume_controller_t *get_volume_controller() { return NULL; }
  virtual audio_player_t *get_sound_player() { return NULL; }
  virtual audio_player_t *get_music_player() { return NULL; }
};

#endif  // SRC_AUDIO_DUMMY_H_
