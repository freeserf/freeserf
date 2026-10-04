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

/* Sound effect parameters of the original (Amiga sfx_table, 0x2f334):
   base period, period random mask, base volume (0..64), volume random
   mask, and the frames (1/50 s) the channel stays reserved. */
typedef struct SfxParams {
  int period;
  int period_mask;
  int volume;
  int volume_mask;
  int duration;
} SfxParams;

static const SfxParams sfx_params[Audio::kSfxCount] = {
  {0, 0, 0, 0, 0}, {427, 0, 64, 0, 96}, {427, 0, 48, 0, 40},  /* 0 */
  {0, 0, 0, 0, 0}, {427, 0, 64, 0, 86}, {0, 0, 0, 0, 0},  /* 3 */
  {427, 0, 20, 0, 36}, {0, 0, 0, 0, 0}, {220, 0, 25, 0, 1},  /* 6 */
  {0, 0, 0, 0, 0}, {412, 31, 25, 15, 75}, {0, 0, 0, 0, 0},  /* 9 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {412, 31, 25, 15, 94},  /* 12 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 15 */
  {412, 31, 25, 15, 80}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 18 */
  {0, 0, 0, 0, 0}, {412, 31, 25, 15, 51}, {0, 0, 0, 0, 0},  /* 21 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {412, 31, 29, 7, 147},  /* 24 */
  {0, 0, 0, 0, 0}, {396, 63, 23, 15, 32}, {0, 0, 0, 0, 0},  /* 27 */
  {420, 15, 9, 7, 54}, {0, 0, 0, 0, 0}, {396, 63, 25, 15, 39},  /* 30 */
  {0, 0, 0, 0, 0}, {1774, 31, 25, 15, 266}, {0, 0, 0, 0, 0},  /* 33 */
  {412, 31, 9, 7, 39}, {0, 0, 0, 0, 0}, {412, 31, 17, 7, 27},  /* 36 */
  {0, 0, 0, 0, 0}, {396, 63, 25, 15, 17}, {0, 0, 0, 0, 0},  /* 39 */
  {879, 31, 9, 3, 145}, {729, 31, 9, 3, 145}, {412, 31, 7, 3, 32},  /* 42 */
  {0, 0, 0, 0, 0}, {205, 31, 25, 15, 8}, {0, 0, 0, 0, 0},  /* 45 */
  {412, 31, 25, 15, 21}, {0, 0, 0, 0, 0}, {945, 31, 25, 15, 152},  /* 48 */
  {0, 0, 0, 0, 0}, {492, 31, 17, 7, 49}, {0, 0, 0, 0, 0},  /* 51 */
  {248, 0, 32, 0, 6}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 54 */
  {0, 0, 0, 0, 0}, {412, 31, 25, 15, 24}, {0, 0, 0, 0, 0},  /* 57 */
  {362, 255, 12, 7, 48}, {0, 0, 0, 0, 0}, {1156, 63, 7, 3, 228},  /* 60 */
  {0, 0, 0, 0, 0}, {825, 63, 9, 7, 243}, {0, 0, 0, 0, 0},  /* 63 */
  {887, 15, 7, 3, 200}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 66 */
  {465, 63, 13, 7, 139}, {205, 31, 5, 15, 17}, {0, 0, 0, 0, 0},  /* 69 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {205, 31, 5, 15, 65},  /* 72 */
  {0, 0, 0, 0, 0}, {396, 63, 17, 7, 210}, {0, 0, 0, 0, 0},  /* 75 */
  {251, 31, 5, 15, 99}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 78 */
  {0, 0, 0, 0, 0}, {205, 31, 5, 15, 135}, {0, 0, 0, 0, 0},  /* 81 */
  {412, 31, 9, 7, 405}, {0, 0, 0, 0, 0}, {831, 127, 0, 0, 1114},  /* 84 */
  {0, 0, 0, 0, 0}, {533, 127, 0, 0, 764}, {0, 0, 0, 0, 0},  /* 87 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 90 */
  {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},  /* 93 */
};

Audio::Audio()
  : volume(0.75f)
  , sfx_random(0x5a5a, 0x1234, 0x8765) {
  for (int &sfx : sfx_queue) sfx = 0xff;
  for (int i = 0; i < kSfxChannels; i++) {
    sfx_channel[i] = 0xff;
    sfx_timer[i] = 0;
  }
  for (int i = 0; i < kSfxCount; i++) sfx_volume[i] = sfx_params[i].volume;
}

/* Insert into the sorted queue, a lower id takes precedence (Amiga
   enqueue_sfx_clip @0x1beb0). */
void
Audio::enqueue_sfx(int sfx) {
  if (sfx < 0 || sfx >= kSfxCount || sfx_params[sfx].period == 0) return;

  for (int i = 0; i < 4; i++) {
    if (sfx == sfx_queue[i]) return;
    if (sfx < sfx_queue[i]) {
      for (int j = 3; j > i; j--) sfx_queue[j] = sfx_queue[j-1];
      sfx_queue[i] = sfx;
      return;
    }
  }
}

/* Base volume of a sound, the ambient sounds set it before playing. */
void
Audio::set_sfx_volume(int sfx, int _volume) {
  sfx_volume[sfx] = _volume;
}

/* Called every 1/50 s: the queued sounds take the channels holding the
   least important sounds, a sound replaces one of the same or a higher
   id. The queue is emptied (Amiga audio_vbl_update @0x2dd6). */
void
Audio::update_sfx() {
  for (int i = 0; i < kSfxChannels; i++) {
    if (sfx_timer[i] != 0 && --sfx_timer[i] == 0) sfx_channel[i] = 0xff;
  }

  PPlayer player = get_sound_player();
  if (!player || !player->is_enabled() || sfx_queue[0] == 0xff) {
    for (int &sfx : sfx_queue) sfx = 0xff;
    return;
  }

  /* Channels by the id of their sound, free ones first. */
  int order[kSfxChannels];
  for (int i = 0; i < kSfxChannels; i++) order[i] = i;
  std::stable_sort(order, order + kSfxChannels, [this](int a, int b) {
    return sfx_channel[a] > sfx_channel[b];
  });

  for (int i = 0; i < 4 && sfx_queue[i] != 0xff; i++) {
    int channel = order[i];
    int sfx = sfx_queue[i];
    if (sfx_channel[channel] < sfx) break;

    const SfxParams &params = sfx_params[sfx];
    int r = sfx_random.random();
    int period = params.period + (r & params.period_mask);
    int vol = std::min(64, (sfx_volume[sfx] + (r & params.volume_mask)) &
                           0xff);

    sfx_channel[channel] = sfx;
    sfx_timer[channel] = params.duration;
    player->play_track_on_channel(sfx, channel, vol / 64.f,
                                  static_cast<float>(params.period) / period);
  }

  for (int &sfx : sfx_queue) sfx = 0xff;
}

Audio::Player::Player() {
  enabled = true;
}

Audio::Player::~Player() {
}

Audio::PTrack
Audio::Player::get_track(int track_id) {
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

  return track;
}

Audio::PTrack
Audio::Player::play_track(int track_id) {
  if (!is_enabled()) {
    return nullptr;
  }

  Audio::PTrack track = get_track(track_id);
  if (track != nullptr) {
    track->play();
  }

  return track;
}

Audio::PTrack
Audio::Player::play_track_on_channel(int track_id, int channel, float vol,
                                     float ratio) {
  if (!is_enabled()) {
    return nullptr;
  }

  Audio::PTrack track = get_track(track_id);
  if (track != nullptr) {
    track->play_on_channel(channel, vol, ratio);
  }

  return track;
}
