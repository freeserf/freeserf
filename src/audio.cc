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

// this function is used both for music (midi) and sounds (wav)
// and the does not seem to be any way to distinguish between the
//  two from inside this function, maybe add an indentifier variable inside?
Audio::PTrack
//Audio::Player::play_track(int track_id) {
Audio::Player::play_track(int track_id, int source_type) { 
  //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, track_id " << track_id << ", source_type " << source_type; 
  if (!is_enabled()) {
    return nullptr;
  }
  //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, is_enabled true";

  Audio::PTrack track;
  TrackCache::iterator it = track_cache.find(track_id);
  //if (it == track_cache.end()) {
  if (it == track_cache.end() || track == nullptr){
    //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, trying to create_track";
    //track = create_track(track_id);
    track = create_track(track_id, source_type); // 0=Amiga, 1=DOS, 2=Custom
    if (track != nullptr) {
      //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, successfuly created track";
      track_cache[track_id] = track;
    }else{
      Log::Warn["audio.cc"] << "inside Audio::Player::play_track, failed to create sound or midi track! track_id " << track_id << ", source_type " << source_type;
    }
  }
  //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, foo";

  if (track != nullptr) {
    //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, playing track";
    track->play();
  }else{
    Log::Warn["audio.cc"] << "inside Audio::Player::play_track, track is nullptr!";
  }

  //Log::Debug["audio.cc"] << "inside Audio::Player::play_track, foo2";

  return track;
}
