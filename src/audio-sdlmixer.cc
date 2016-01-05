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

#include "src/audio-sdlmixer.h"

#include <cstring>
#include <cassert>
#include <algorithm>

#include <SDL.h>
#include <SDL_mixer.h>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
END_EXT_C
#include "src/data.h"
#include "src/sfx2wav.h"
#include "src/xmi2mid.h"

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

audio_t *
audio_t::get_instance() {
  if (instance == NULL) {
    instance = new audio_sdlmixer_t();
  }
  return instance;
}

audio_sdlmixer_t::audio_sdlmixer_t() {
  LOGI("audio-sdlmixer", "Initializing audio driver `sdlmixer'.");

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    LOGE("audio-sdlmixer", "Could not init SDL audio: %s.", SDL_GetError());
    assert(false);
  }

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

  volume = 1.f;

  sfx_player = new sfx_player_t();
  midi_player = new midi_player_t();
}

audio_sdlmixer_t::~audio_sdlmixer_t() {
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

float
audio_sdlmixer_t::get_volume() {
  return volume;
}

void
audio_sdlmixer_t::set_volume(float volume) {
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
audio_sdlmixer_t::volume_up() {
  float volume = get_volume();
  set_volume(volume + 0.1f);
}

void
audio_sdlmixer_t::volume_down() {
  float volume = get_volume();
  set_volume(volume - 0.1f);
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
  float mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_Volume(-1, static_cast<int>(mix_volume));
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

midi_player_t::midi_player_t() {
  if (current_midi_player != NULL) {
    LOGE("audio-sdlmixer", "Only one midi player is allowed.");
    assert(0);
  }
  current_track = MIDI_TRACK_NONE;
  current_midi_player = this;
  Mix_HookMusicFinished(music_finished_hook);
}

midi_player_t::~midi_player_t() {
  Mix_HookMusicFinished(NULL);
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
midi_player_t::play_track(int track_id) {
  if ((track_id <= MIDI_TRACK_NONE) || (track_id > MIDI_TRACK_LAST)) {
    track_id = MIDI_TRACK_0;
  }
  current_track = static_cast<midi_t>(track_id);
  audio_player_t::play_track(track_id);
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
  float mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_VolumeMusic(static_cast<int>(mix_volume));
}

void
midi_player_t::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
midi_player_t::volume_down() {
  set_volume(get_volume() - 0.1f);
}

midi_player_t *midi_player_t::current_midi_player = NULL;

void
midi_player_t::music_finished_hook() {
  if (current_midi_player != NULL) {
    event_loop_t *event_loop = event_loop_t::get_instance();
    event_loop->deferred_call(current_midi_player, NULL);
  }
}

void
midi_player_t::deffered_call(void *data) {
  music_finished();
}

void
midi_player_t::music_finished() {
  if (is_enabled()) {
    play_track(current_track + 1);
  }
}

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
