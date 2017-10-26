/*
 * audio.cc - Music and sound effects playback base.
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

#include "src/audio.h"

#include <algorithm>
#include <string>

#include "src/log.h"

ExceptionAudio::ExceptionAudio(const std::string &description) :
  ExceptionFreeserf(description) {
}

ExceptionAudio::~ExceptionAudio() {
}

std::string
ExceptionAudio::get_description() const {
  return "[" + get_system() + ":" + get_platform() + "] " + description.c_str();
}

Audio *
Audio::instance = nullptr;

Audio::Player::Player() {
  enabled = true;
}

Audio::Player::~Player() {
}

void
Audio::Player::play_track(int track_id) {
  if (!is_enabled()) {
    return;
  }

  Audio::PTrack track;
  TrackCache::iterator it = track_cache.find(track_id);
  if (it == track_cache.end()) {
    track = create_track(track_id);
    if (track != nullptr) {
      track_cache[track_id] = track;
    }
  } else {
    track = it->second;
  }

  if (track != nullptr) {
    track->play();
  }
}
