/*
 * audio-sdlmixer.cc - Music and sound effects playback using SDL_mixer.
 *
 * Copyright (C) 2012-2015  Wicked_Digger <wicked_digger@mail.ru>
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

#include <cstring>
#include <cassert>

#include <SDL.h>
#include <SDL_mixer.h>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/data.h"
  #include "src/log.h"
END_EXT_C
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

audio_t *audio_t::instance = NULL;

audio_t::audio_t() {
  LOGI("audio-sdlmixer", "Initializing audio driver `sdlmixer'.");

  int r = Mix_Init(0);
  if (r != 0) {
    LOGE("audio-sdlmixer", "Could not init SDL_mixer: %s.", Mix_GetError());
    assert(false);
  }

  r = Mix_OpenAudio(8000, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512);
  if (r < 0) {
    LOGE("audio-sdlmixer", "Could not open audio device: %s.", Mix_GetError());
    assert(false);
  }

  r = Mix_AllocateChannels(16);
  if (r != 16) {
    LOGE("audio-sdlmixer", "Failed to allocate channels: %s.", Mix_GetError());
    assert(false);
  }
/*
  Mix_HookMusicFinished(midi_track_finished);
*/
  volume = 1.f;

  sfx_player = new sfx_player_t();
  midi_player = new midi_player_t();
}

audio_t::~audio_t() {
  if (sfx_player != NULL) {
    delete sfx_player;
    sfx_player = NULL;
  }

  if (midi_player != NULL) {
    delete midi_player;
    midi_player = NULL;
  }

  Mix_CloseAudio();
  Mix_Quit();
}

audio_t *
audio_t::get_instance() {
  if (instance == NULL) {
    instance = new audio_t();
  }
  return instance;
}

float
audio_t::get_volume() {
  return volume;
}

void
audio_t::set_volume(float volume) {
  this->volume = volume;

  if (midi_player != NULL) {
    audio_volume_controller_t *volume_controller =
                                           midi_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume_controller->get_volume() * volume);
    }
  }

  if (sfx_player != NULL) {
    audio_volume_controller_t *volume_controller =
                                            sfx_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume_controller->get_volume() * volume);
    }
  }
}

void
audio_t::volume_up() {
  float volume = get_volume();
  set_volume(volume + 0.1f);
}

void
audio_t::volume_down() {
  float volume = get_volume();
  set_volume(volume - 0.1f);
}

audio_player_t::audio_player_t() {
  enabled = true;
}

audio_player_t::~audio_player_t() {
  while (track_cache.size()) {
    audio_track_t *track = track_cache.begin()->second;
    track_cache.erase(track_cache.begin());
    delete track;
  }
}

void
audio_player_t::play_track(int track_id) {
  if (!is_enabled()) {
    return;
  }

  audio_track_t *track = NULL;
  track_cache_t::iterator it = track_cache.find(track_id);
  if (it == track_cache.end()) {
    track = create_track(track_id);
    if (track != NULL) {
      track_cache[track_id] = track;
    }
  } else {
    track = it->second;
  }

  if (track != NULL) {
    track->play();
  }
}

audio_track_t *
sfx_player_t::create_track(int track_id) {
  size_t size = 0;
  void *data = data_get_object(DATA_SFX_BASE + track_id, &size);
  if (data == NULL) {
    return NULL;
  }

  void *wav = sfx2wav(data, size, &size, -0x20);
  if (wav == NULL) {
    return NULL;
  }

  SDL_RWops *rw = SDL_RWFromMem(wav, static_cast<int>(size));
  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
  free(wav);
  if (chunk == NULL) {
    LOGE("audio-sdlmixer", "Mix_LoadWAV_RW: %s.", Mix_GetError());
    return NULL;
  }

  return new sfx_track_t(chunk);
}

void
sfx_player_t::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
sfx_player_t::stop() {
  Mix_HaltChannel(-1);
}

float
sfx_player_t::get_volume() {
  return static_cast<float>(Mix_Volume(-1, -1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
sfx_player_t::set_volume(float volume) {
  volume = std::max(std::min(0.f, volume), 1.f);
  int mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_Volume(-1, mix_volume);
}

void
sfx_player_t::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
sfx_player_t::volume_down() {
  set_volume(get_volume() - 0.1f);
}

sfx_track_t::sfx_track_t(Mix_Chunk *chunk) {
  this->chunk = chunk;
}

sfx_track_t::~sfx_track_t() {
  Mix_FreeChunk(chunk);
}

void
sfx_track_t::play() {
  int r = Mix_PlayChannel(-1, chunk, 0);
  if (r < 0) {
    LOGE("audio-sdlmixer", "Could not play SFX clip: %s.", Mix_GetError());
  }
}

audio_track_t *
midi_player_t::create_track(int track_id) {
  size_t size = 0;
  void *data = data_get_object(DATA_MUSIC_GAME + track_id, &size);
  if (NULL == data) {
    return NULL;
  }

  void *midi = xmi2mid(data, size, &size);
  SDL_RWops *rw = SDL_RWFromMem(midi, static_cast<int>(size));
  Mix_Music *music = Mix_LoadMUS_RW(rw, 0);
  free(midi);
  if (music == NULL) {
    return NULL;
  }

  return new midi_track_t(music);
}

void
midi_player_t::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
midi_player_t::stop() {
  Mix_HaltMusic();
}

float
midi_player_t::get_volume() {
  return static_cast<float>(Mix_VolumeMusic(-1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
midi_player_t::set_volume(float volume) {
  volume = std::max(std::min(0.f, volume), 1.f);
  int mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_VolumeMusic(mix_volume);
}

void
midi_player_t::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
midi_player_t::volume_down() {
  set_volume(get_volume() - 0.1f);
}

/*
static void
midi_track_finished() {
  if (midi_enabled) {
    midi_play_track(current_track);
  }
}
*/

midi_track_t::midi_track_t(Mix_Music *chunk) {
  this->chunk = chunk;
}

midi_track_t::~midi_track_t() {
  Mix_FreeMusic(chunk);
}

void
midi_track_t::play() {
  int r = Mix_PlayMusic(chunk, 0);
  if (r < 0) {
    LOGW("audio-sdlmixer", "Could not play MIDI track: %s\n", Mix_GetError());
  }
}
