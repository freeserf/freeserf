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

#include <cassert>
#include <algorithm>

#include <SDL.h>
#include <SDL_mixer.h>

#include "src/log.h"
#include "src/data.h"
#include "src/data-source.h"

Audio *
Audio::get_instance() {
  if (instance == NULL) {
    instance = new AudioSDL();
  }
  return instance;
}

AudioSDL::AudioSDL() {
  Log::Info["audio-sdlmixer"] << "Initializing audio driver `sdlmixer'.";

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
    Log::Error["audio-sdlmixer"] << "Could not init SDL audio: "
                                 << SDL_GetError();
    assert(false);
  }

  int r = Mix_Init(0);
  if (r != 0) {
    Log::Error["audio-sdlmixer"] << "Could not init SDL_mixer: "
                                 << Mix_GetError();
    assert(false);
  }

  r = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512);
  if (r < 0) {
    Log::Error["audio-sdlmixer"] << "Could not open audio device: "
                                 << Mix_GetError();
    assert(false);
  }

  r = Mix_AllocateChannels(16);
  if (r != 16) {
    Log::Error["audio-sdlmixer"] << "Failed to allocate channels: %s."
                                 << Mix_GetError();
    assert(false);
  }

  volume = 1.f;

  sfx_player = new AudioSDL::PlayerSFX();
  midi_player = new AudioSDL::PlayerMIDI();
}

AudioSDL::~AudioSDL() {
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
AudioSDL::get_volume() {
  return volume;
}

void
AudioSDL::set_volume(float volume) {
  volume = std::max(0.f, std::min(volume, 1.f));
  this->volume = volume;

  if (midi_player != NULL) {
    Audio::VolumeController *volume_controller =
                                           midi_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume);
    }
  }

  if (sfx_player != NULL) {
    Audio::VolumeController *volume_controller =
                                            sfx_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume);
    }
  }
}

void
AudioSDL::volume_up() {
  float volume = get_volume();
  set_volume(volume + 0.1f);
}

void
AudioSDL::volume_down() {
  float volume = get_volume();
  set_volume(volume - 0.1f);
}

Audio::Track *
AudioSDL::PlayerSFX::create_track(int track_id) {
  Data *data = Data::get_instance();
  DataSource *data_source = data->get_data_source();

  size_t size = 0;
  void *wav = data_source->get_sound(track_id, &size);
  if (wav == NULL) {
    return NULL;
  }

  SDL_RWops *rw = SDL_RWFromMem(wav, static_cast<int>(size));
  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
  free(wav);
  if (chunk == NULL) {
    Log::Error["audio-sdlmixer"] << "Mix_LoadWAV_RW: " << Mix_GetError();
    return NULL;
  }

  return new AudioSDL::TrackSFX(chunk);
}

void
AudioSDL::PlayerSFX::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
AudioSDL::PlayerSFX::stop() {
  Mix_HaltChannel(-1);
}

float
AudioSDL::PlayerSFX::get_volume() {
  return static_cast<float>(Mix_Volume(-1, -1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
AudioSDL::PlayerSFX::set_volume(float volume) {
  volume = std::max(0.f, std::min(volume, 1.f));
  float mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_Volume(-1, static_cast<int>(mix_volume));
}

void
AudioSDL::PlayerSFX::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
AudioSDL::PlayerSFX::volume_down() {
  set_volume(get_volume() - 0.1f);
}

AudioSDL::TrackSFX::TrackSFX(Mix_Chunk *chunk) {
  this->chunk = chunk;
}

AudioSDL::TrackSFX::~TrackSFX() {
  Mix_FreeChunk(chunk);
}

void
AudioSDL::TrackSFX::play() {
  int r = Mix_PlayChannel(-1, chunk, 0);
  if (r < 0) {
    Log::Error["audio-sdlmixer"] << "Could not play SFX clip: "
                                 << Mix_GetError();
  }
}

AudioSDL::PlayerMIDI::PlayerMIDI() {
  if (current_midi_player != NULL) {
    Log::Error["audio-sdlmixer"] << "Only one midi player is allowed.";
    assert(0);
  }
  current_track = TypeMidiNone;
  current_midi_player = this;
  Mix_HookMusicFinished(music_finished_hook);
}

AudioSDL::PlayerMIDI::~PlayerMIDI() {
  Mix_HookMusicFinished(NULL);
}

Audio::Track *
AudioSDL::PlayerMIDI::create_track(int track_id) {
  Data *data = Data::get_instance();
  DataSource *data_source = data->get_data_source();

  size_t size = 0;
  void *midi = data_source->get_music(track_id, &size);
  if (midi == NULL) {
    return NULL;
  }

  SDL_RWops *rw = SDL_RWFromMem(midi, static_cast<int>(size));
  Mix_Music *music = Mix_LoadMUS_RW(rw, 0);
  if (music == NULL) {
    return NULL;
  }

  return new AudioSDL::TrackMIDI(midi, music);
}

void
AudioSDL::PlayerMIDI::play_track(int track_id) {
  if ((track_id <= TypeMidiNone) || (track_id > TypeMidiTrackLast)) {
    track_id = TypeMidiTrack0;
  }
  current_track = static_cast<TypeMidi>(track_id);
  Audio::Player::play_track(track_id);
}

void
AudioSDL::PlayerMIDI::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
AudioSDL::PlayerMIDI::stop() {
  Mix_HaltMusic();
}

float
AudioSDL::PlayerMIDI::get_volume() {
  return static_cast<float>(Mix_VolumeMusic(-1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
AudioSDL::PlayerMIDI::set_volume(float volume) {
  volume = std::max(0.f, std::min(volume, 1.f));
  float mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_VolumeMusic(static_cast<int>(mix_volume));
}

void
AudioSDL::PlayerMIDI::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
AudioSDL::PlayerMIDI::volume_down() {
  set_volume(get_volume() - 0.1f);
}

AudioSDL::PlayerMIDI *
AudioSDL::PlayerMIDI::current_midi_player = NULL;

void
AudioSDL::PlayerMIDI::music_finished_hook() {
  if (current_midi_player != NULL) {
    EventLoop *event_loop = EventLoop::get_instance();
    event_loop->deferred_call(current_midi_player, NULL);
  }
}

void
AudioSDL::PlayerMIDI::deferred_call(void *data) {
  music_finished();
}

void
AudioSDL::PlayerMIDI::music_finished() {
  if (is_enabled()) {
    play_track(current_track + 1);
  }
}

AudioSDL::TrackMIDI::TrackMIDI(void *_data, Mix_Music *_chunk) {
  data = _data;
  chunk = _chunk;
}

AudioSDL::TrackMIDI::~TrackMIDI() {
  Mix_FreeMusic(chunk);
  free(data);
}

void
AudioSDL::TrackMIDI::play() {
  int r = Mix_PlayMusic(chunk, 0);
  if (r < 0) {
    Log::Warn["audio-sdlmixer"] << "Could not play MIDI track: "
                                << Mix_GetError();
  }
}
