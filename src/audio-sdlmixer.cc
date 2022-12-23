/*
 * audio-sdlmixer.cc - Music and sound effects playback using SDL_mixer.
 *
 * Copyright (C) 2012-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include <algorithm>
#include <memory>
#include <string>

#include <SDL.h>
#include <SDL_mixer.h>

#include "src/log.h"
#include "src/data.h"

ExceptionSDLmixer::ExceptionSDLmixer(const std::string &_description)
  : ExceptionAudio(_description) {
  sdl_error = SDL_GetError();
  description += " (" + sdl_error + ")";
}

Audio &
Audio::get_instance() {
  static AudioSDL audio_sdl;
  return audio_sdl;
}

AudioSDL::AudioSDL() {
  Log::Info["audio"] << "Initializing \"sdlmixer\".";

  Log::Info["audio"] << "Available drivers:";
  int num_drivers = SDL_GetNumAudioDrivers();
  for (int i = 0; i < num_drivers; ++i) {
    Log::Info["audio"] << "\t" << SDL_GetAudioDriver(i);
  }

  if (SDL_AudioInit(NULL) != 0) {
    throw ExceptionSDLmixer("Could not init SDL audio");
  }

  SDL_version version;
  SDL_GetVersion(&version);
  Log::Info["audio"] << "Initialized with SDL "
                     << static_cast<int>(version.major) << '.'
                     << static_cast<int>(version.minor) << '.'
                     << static_cast<int>(version.patch)
                     << " (driver: " << SDL_GetCurrentAudioDriver() << ")";

  const SDL_version *mversion = Mix_Linked_Version();
  Log::Info["audio:SDL_mixer"] << "Initializing SDL_mixer "
                               << static_cast<int>(mversion->major) << '.'
                               << static_cast<int>(mversion->minor) << '.'
                               << static_cast<int>(mversion->patch);
  int r = Mix_Init(0);
  if (r != 0) {
    throw ExceptionSDLmixer("Could not init SDL_mixer");
  }

  // this crashes randomly, very often, on startup on Linux for me
  //  can't figure out why, tried a try/catch block but that
  //  doesn't prevent it from causing a crash.  will try updating my
  //  pulseaudio version
  //try {
      r = Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512);
  //} catch (...) {
  //  throw ExceptionSDLmixer("inside AudioSDL::AudioSDL() constructor, Mix_OpenAudio call failed");
  //}
  if (r < 0) {
    throw ExceptionSDLmixer("Could not open audio device");
  }

  r = Mix_AllocateChannels(128);
  if (r != 128) {
    throw ExceptionSDLmixer("Failed to allocate channels");
  }

  volume = 1.f;

  sfx_player = std::make_shared<AudioSDL::PlayerSFX>();
  midi_player = std::make_shared<AudioSDL::PlayerMIDI>();

  Log::Info["audio:SDL_mixer"] << "Initialized";
}

AudioSDL::~AudioSDL() {
  sfx_player = nullptr;
  midi_player = nullptr;

  Mix_CloseAudio();
  Mix_Quit();
  SDL_AudioQuit();
}

float
AudioSDL::get_volume() {
  return volume;
}

void
AudioSDL::set_volume(float _volume) {
  _volume = std::max(0.f, std::min(_volume, 1.f));
  if (fabs(volume - _volume) < 0.01f) {
    return;
  }
  volume = _volume;

  if (midi_player != nullptr) {
    Audio::PVolumeController volume_controller =
                                           midi_player->get_volume_controller();
    if (volume_controller) {
      volume_controller->set_volume(volume);
    }
  }

  if (sfx_player != nullptr) {
    Audio::PVolumeController volume_controller =
                                            sfx_player->get_volume_controller();
    if (volume_controller) {
      volume_controller->set_volume(volume);
    }
  }
}

void
AudioSDL::volume_up() {
  float vol = get_volume();
  set_volume(vol + 0.1f);
}

void
AudioSDL::volume_down() {
  float vol = get_volume();
  set_volume(vol - 0.1f);
}

/*
Audio::PTrack
AudioSDL::PlayerSFX::create_track(int track_id) {
  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();

  PBuffer wav = data_source->get_sound(track_id);
  if (!wav) {
    return nullptr;
  }

  SDL_RWops *rw = SDL_RWFromMem(wav->get_data(),
                                static_cast<int>(wav->get_size()));
  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
  if (chunk == nullptr) {
    Log::Error["audio:SDL_mixer"] << "Mix_LoadWAV_RW: " << Mix_GetError();
    return nullptr;
  }

  return std::make_shared<AudioSDL::TrackSFX>(chunk);
}
*/

// adding support for multiple data sources
Audio::PTrack
AudioSDL::PlayerSFX::create_track(int track_id, int source_type) {
  Data &data = Data::get_instance();
  //Data::PSource data_source = data.get_data_source();
  Data::PSource data_source;
  if (source_type == 0)
    data_source = data.get_data_source_Amiga();
  if (source_type == 1)
    data_source = data.get_data_source_DOS();
  if (source_type == 2)
    data_source = data.get_data_source_Custom();

  PBuffer wav = data_source->get_sound(track_id);
  if (!wav) {
    return nullptr;
  }

  SDL_RWops *rw = SDL_RWFromMem(wav->get_data(),
                                static_cast<int>(wav->get_size()));
  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
  if (chunk == nullptr) {
    Log::Error["audio:SDL_mixer"] << "Mix_LoadWAV_RW: " << Mix_GetError();
    return nullptr;
  //}else{
    //Log::Debug["audio-sldmixer.cc"] << "inside AudioSDL::PlayerSFX::create_track, successfully loaded WAV for source " << source_type << ", track #" << track_id;
  }

  // the boiling sounds from SteelSmelter and GoldSmelter are obnoxious
  //  reduce their volume by 30%
  if (track_id == Audio::TypeSfxGoldBoils){
    int current_boil_volume = Mix_VolumeChunk(chunk, -1);  // -1 means query current volume
    Mix_VolumeChunk(chunk, current_boil_volume * 0.65f);  // set new volume, only affects this track/chunk/sound
  }



  return std::make_shared<AudioSDL::TrackSFX>(chunk);
}

void
AudioSDL::PlayerSFX::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }else{
    start();
  }
}

// change stop to pause and halt to stop

void
AudioSDL::PlayerSFX::stop() {
  //Mix_HaltChannel(-1);
  Mix_Pause(-1);
}

void
AudioSDL::PlayerSFX::halt() {
  Mix_HaltChannel(-1);
  //Mix_Pause(-1);
}

void
AudioSDL::PlayerSFX::start() {
  Mix_Resume(-1);
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

AudioSDL::TrackSFX::TrackSFX(Mix_Chunk *_chunk) {
  chunk = _chunk;
}

AudioSDL::TrackSFX::~TrackSFX() {
  Mix_FreeChunk(chunk);
}

void
AudioSDL::TrackSFX::play() {
  int r = Mix_PlayChannel(-1, chunk, 0);
  if (r < 0) {
    Log::Error["audio:SDL_mixer"] << "Could not play SFX clip: "
                                  << Mix_GetError();
  }
}

AudioSDL::PlayerMIDI::PlayerMIDI() {
  if (current_midi_player != nullptr) {
    throw ExceptionSDLmixer("Only one midi player is allowed");
  }
  current_track = TypeMidiNone;
  current_midi_player = this;
  Mix_HookMusicFinished(music_finished_hook);
}

AudioSDL::PlayerMIDI::~PlayerMIDI() {
  current_midi_player = nullptr;
  Mix_HookMusicFinished(nullptr);
}

/*
Audio::PTrack
AudioSDL::PlayerMIDI::create_track(int track_id) {
  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();

  if (data_source->get_music_format() == Data::MusicFormatMod) {
    if (MIX_INIT_MOD != Mix_Init(MIX_INIT_MOD)) {
      Log::Error["audio:SDL_mixer"] << "Failed to initialize MOD music decoder";
      return nullptr;
    }
  }

  PBuffer midi = data_source->get_music(track_id);
  if (!midi) {
    return nullptr;
  }

  SDL_RWops *rw = SDL_RWFromMem(midi->get_data(),
                                static_cast<int>(midi->get_size()));
  Mix_Music *music = Mix_LoadMUS_RW(rw, 0);
  if (music == nullptr) {
    return nullptr;
  }

  return std::make_shared<AudioSDL::TrackMIDI>(midi, music);
}
*/

// adding support for multiple data sources
Audio::PTrack
AudioSDL::PlayerMIDI::create_track(int track_id, int source_type) {
  Data &data = Data::get_instance();
  //Data::PSource data_source = data.get_data_source();
  Data::PSource data_source;
  if (source_type == 0)
    data_source = data.get_data_source_Amiga();
  if (source_type == 1)
    data_source = data.get_data_source_DOS();
  if (source_type == 2)
    data_source = data.get_data_source_Custom();

  if (data_source->get_music_format() == Data::MusicFormatMod) {
    if (MIX_INIT_MOD != Mix_Init(MIX_INIT_MOD)) {
      Log::Error["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::create_track, Failed to initialize MOD music decoder, SDL_GetError is " << SDL_GetError();
      return nullptr;
    }
  }

  PBuffer midi = data_source->get_music(track_id);
  if (!midi) {
    Log::Error["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::create_track, midi is nullptr, SDL_GetError is " << SDL_GetError();
    return nullptr;
  }

  SDL_RWops *rw = SDL_RWFromMem(midi->get_data(),
                                static_cast<int>(midi->get_size()));
  Mix_Music *music = Mix_LoadMUS_RW(rw, 0);
  if (music == nullptr) {
    Log::Error["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::create_track, music is nullptr!, SDL_GetError is " << SDL_GetError();
    Log::Error["audio-sdlmixer.cc"] << "note, if you are running linux and geting error about timidity/freepats.cfg, you might need 'timidity' and/or 'freepats' package, see https://github.com/freeserf/freeserf/issues/508";
    return nullptr;
  }
  return std::make_shared<AudioSDL::TrackMIDI>(midi, music);
}

// this function should be called play_next_track
Audio::PTrack
//AudioSDL::PlayerMIDI::play_track(int track_id) {
AudioSDL::PlayerMIDI::play_track(int track_id, int source_type) {
  //Log::Debug["audio-sdlmixer.cc"] << "start of AudioSDL::PlayerMIDI::play_track, track_id " << track_id << ", source " << source_type;
  Audio::PTrack track;
  bool have_track = false;
  while (!track) {
    //Log::Debug["audio-sdlmixer.cc"] << "start of AudioSDL::PlayerMIDI::play_track, start of while !track loop";
    if ((track_id <= TypeMidiNone) || (track_id > TypeMidiTrackLast)) {
      //Log::Debug["audio-sdlmixer.cc"] << "start of AudioSDL::PlayerMIDI::play_track, track is not valid range";
      if (!have_track) {
        //Log::Debug["audio-sdlmixer.cc"] << "start of AudioSDL::PlayerMIDI::play_track, track is not valid range, !have_track";
        break;
      }
      //Log::Debug["audio-sdlmixer.cc"] << "start of AudioSDL::PlayerMIDI::play_track, track is not valid range, setting track_id to TypeMidiTrack0";
      track_id = TypeMidiTrack0;
    }
    current_track = static_cast<TypeMidi>(track_id);
    current_track_id = track_id;
    current_source = source_type;
    //Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::play_track, Playing MIDI track: " << current_track;
    //track = Audio::Player::play_track(track_id);
    track = Audio::Player::play_track(track_id, source_type);
    if (track) {
      //Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::play_track, have_track is true";
      have_track = true;
    }
    track_id++;
    // keep playing music from the start after last track ends
    if (track_id > TypeMidiTrack3){
      track_id = 0;
    }
  }
  return track;
}

void
AudioSDL::PlayerMIDI::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }else{
    start();
  }
}

int
AudioSDL::PlayerMIDI::get_current_track_id(){
  Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::get_current_track(), returning current_track " << current_track;
  return current_track_id;
}
    
//void
//AudioSDL::PlayerMIDI::set_current_track_id(Audio::TypeMidi tracknum){
//  Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::set_current_track(), tracknum " << tracknum;
//  current_track = tracknum;
//}

int
AudioSDL::PlayerMIDI::get_current_source(){
  Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::get_current_source(), returning current_source " << current_source;
  return current_source;
}

void
AudioSDL::PlayerMIDI::set_current_source(int sourcenum){
  Log::Debug["audio-sdlmixer.cc"] << "inside AudioSDL::PlayerMIDI::set_current_source(), sourcenum " << sourcenum;
  current_source = sourcenum;
}

// change the code so stop becomes pause and halt becomes stop

void
AudioSDL::PlayerMIDI::stop() {
  //Mix_HaltMusic();
  Mix_PauseMusic();
}

void
AudioSDL::PlayerMIDI::halt() {
  Mix_HaltMusic();
  //Mix_PauseMusic();
}

void
AudioSDL::PlayerMIDI::start() {
  Mix_ResumeMusic();
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
AudioSDL::PlayerMIDI::current_midi_player = nullptr;

void
AudioSDL::PlayerMIDI::music_finished_hook() {
  if (current_midi_player != nullptr) {
    EventLoop &event_loop = EventLoop::get_instance();
    event_loop.deferred_call([](void*){
      current_midi_player->music_finished();
    }, nullptr);
  }
}

void
AudioSDL::PlayerMIDI::music_finished() {
  if (is_enabled()) {
    //Audio::PTrack t = play_track(current_track + 1);
    Audio::PTrack t = play_track(current_track + 1, current_source);
  }
}

AudioSDL::TrackMIDI::TrackMIDI(PBuffer _data, Mix_Music *_chunk)
  : data(_data)
  , chunk(_chunk) {
}

AudioSDL::TrackMIDI::~TrackMIDI() {
  Mix_FreeMusic(chunk);
}

void
AudioSDL::TrackMIDI::play() {
  int r = Mix_PlayMusic(chunk, 0);
  if (r < 0) {
    Log::Warn["audio:SDL_mixer"] << "Could not play MIDI track: "
                                 << Mix_GetError();
    PlayerMIDI::music_finished_hook();
  }
}
