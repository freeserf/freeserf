/*
 * audio-dummy.cc - Dummy music and sound effects playback.
 *
 * Copyright (C) 2013-2015  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/audio.h"

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
END_EXT_C

/* Common audio. */
audio_t::audio_t() {
  LOGI("audio-dummy", "Initializing audio driver `dummy'.");
  sfx_player = NULL;
  midi_player = NULL;
  volume = 0.f;
}

audio_t::~audio_t() {
}

audio_t *audio_t::instance = NULL;

audio_t *
audio_t::get_instance() {
  if (instance == NULL) {
    instance = new audio_t();
  }

  return instance;
}

float
audio_t::get_volume() {
  return 0.f;
}

void
audio_t::set_volume(float volume) {
  LOGV("audio-dummy", "Request to set volume to %i", volume);
}

void
audio_t::volume_up() {
  LOGV("audio-dummy", "Request to increase volume");
}

void
audio_t::volume_down() {
  LOGV("audio-dummy", "Request to decrease volume");
}
