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

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <string>

#include "src/log.h"
#include "src/data.h"

ExceptionSDLmixer::ExceptionSDLmixer(const std::string &_description)
  : ExceptionAudio(_description) {
  sdl_error = SDL_GetError();
  description += " (" + sdl_error + ")";
}

/* Tracks of the sound effect channels and their sounds' volume. */
#define SFX_TRACKS  Audio::kSfxChannels

static MIX_Mixer *mixer = nullptr;
static MIX_Track *sfx_tracks[SFX_TRACKS];
static float sfx_track_volume[SFX_TRACKS];
static float sfx_master_volume = 1.f;
static MIX_Track *music_track = nullptr;

/* SoundFont for the MIDI music: SDL_SOUNDFONTS, else the TimGM6mb
   SoundFont installed with the game (next to the program, in the
   resources of the macOS bundle) or put next to the game data. */
static std::string
find_soundfont() {
  const char *env = SDL_getenv("SDL_SOUNDFONTS");
  if (env != nullptr) {
    return env;
  }

  std::list<std::string> dirs;
  const char *base = SDL_GetBasePath();
  if (base != nullptr) {
    dirs.push_back(base);
  }
  Data::PSource data_source = Data::get_instance().get_data_source();
  if (data_source) {
    std::string data_path = data_source->get_path();
    size_t sep = data_path.find_last_of("/\\");
    if (sep != std::string::npos) {
      dirs.push_back(data_path.substr(0, sep + 1));
    }
  }

  for (const std::string &dir : dirs) {
    std::string path = dir + "TimGM6mb.sf2";
    if (SDL_GetPathInfo(path.c_str(), nullptr)) {
      return path;
    }
  }

  return std::string();
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

  if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
    throw ExceptionSDLmixer("Could not init SDL audio");
  }

  int version = SDL_GetVersion();
  Log::Info["audio"] << "Initialized with SDL "
                     << SDL_VERSIONNUM_MAJOR(version) << '.'
                     << SDL_VERSIONNUM_MINOR(version) << '.'
                     << SDL_VERSIONNUM_MICRO(version)
                     << " (driver: " << SDL_GetCurrentAudioDriver() << ")";

  int mversion = MIX_Version();
  Log::Info["audio:SDL_mixer"] << "Initializing SDL_mixer "
                               << SDL_VERSIONNUM_MAJOR(mversion) << '.'
                               << SDL_VERSIONNUM_MINOR(mversion) << '.'
                               << SDL_VERSIONNUM_MICRO(mversion);

  if (!MIX_Init()) {
    throw ExceptionSDLmixer("Could not init SDL_mixer");
  }

  SDL_AudioSpec spec;
  spec.format = SDL_AUDIO_S16;
  spec.channels = 2;
  spec.freq = 44100;
  mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
  if (mixer == nullptr) {
    throw ExceptionSDLmixer("Could not open audio device");
  }

  for (int i = 0; i < SFX_TRACKS; i++) {
    sfx_track_volume[i] = 1.f;
    sfx_tracks[i] = MIX_CreateTrack(mixer);
    if (sfx_tracks[i] == nullptr) {
      throw ExceptionSDLmixer("Failed to allocate tracks");
    }
  }

  music_track = MIX_CreateTrack(mixer);
  if (music_track == nullptr) {
    throw ExceptionSDLmixer("Failed to allocate tracks");
  }

  volume = 1.f;

  sfx_player = std::make_shared<AudioSDL::PlayerSFX>();
  midi_player = std::make_shared<AudioSDL::PlayerMIDI>();

  Log::Info["audio:SDL_mixer"] << "Initialized";
}

AudioSDL::~AudioSDL() {
  sfx_player = nullptr;
  midi_player = nullptr;

  /* Destroys the tracks too. */
  MIX_DestroyMixer(mixer);
  mixer = nullptr;
  music_track = nullptr;
  MIX_Quit();
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
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

AudioSDL::PlayerSFX::PlayerSFX()
  : volume(1.f) {
}

Audio::PTrack
AudioSDL::PlayerSFX::create_track(int track_id) {
  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();

  PBuffer wav = data_source->get_sound(track_id);
  if (!wav) {
    return nullptr;
  }

  SDL_IOStream *io = SDL_IOFromConstMem(wav->get_data(), wav->get_size());
  MIX_Audio *chunk = MIX_LoadAudio_IO(mixer, io, true, true);
  if (chunk == nullptr) {
    Log::Error["audio:SDL_mixer"] << "MIX_LoadAudio_IO: " << SDL_GetError();
    return nullptr;
  }

  return std::make_shared<AudioSDL::TrackSFX>(chunk);
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
  for (int i = 0; i < SFX_TRACKS; i++) {
    MIX_StopTrack(sfx_tracks[i], 0);
  }
}

float
AudioSDL::PlayerSFX::get_volume() {
  return volume;
}

void
AudioSDL::PlayerSFX::set_volume(float _volume) {
  volume = std::max(0.f, std::min(_volume, 1.f));
  sfx_master_volume = volume;
  for (int i = 0; i < SFX_TRACKS; i++) {
    MIX_SetTrackGain(sfx_tracks[i], volume * sfx_track_volume[i]);
  }
}

void
AudioSDL::PlayerSFX::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
AudioSDL::PlayerSFX::volume_down() {
  set_volume(get_volume() - 0.1f);
}

AudioSDL::TrackSFX::TrackSFX(MIX_Audio *_chunk) {
  chunk = _chunk;
}

AudioSDL::TrackSFX::~TrackSFX() {
  MIX_DestroyAudio(chunk);
}

void
AudioSDL::TrackSFX::play() {
  /* Play on the first free track. */
  for (int i = 0; i < SFX_TRACKS; i++) {
    MIX_Track *track = sfx_tracks[i];
    if (MIX_TrackPlaying(track)) {
      continue;
    }
    if (!MIX_SetTrackAudio(track, chunk) || !MIX_PlayTrack(track, 0)) {
      Log::Error["audio:SDL_mixer"] << "Could not play SFX clip: "
                                    << SDL_GetError();
    }
    return;
  }

  Log::Warn["audio:SDL_mixer"] << "Could not play SFX clip: no free track";
}

void
AudioSDL::TrackSFX::play_on_channel(int channel, float volume, float ratio) {
  MIX_Track *track = sfx_tracks[channel];
  MIX_StopTrack(track, 0);
  sfx_track_volume[channel] = volume;
  MIX_SetTrackGain(track, sfx_master_volume * volume);
  MIX_SetTrackFrequencyRatio(track, ratio);
  if (!MIX_SetTrackAudio(track, chunk) || !MIX_PlayTrack(track, 0)) {
    Log::Error["audio:SDL_mixer"] << "Could not play SFX clip: "
                                  << SDL_GetError();
  }
}

AudioSDL::PlayerMIDI::PlayerMIDI() {
  if (current_midi_player != nullptr) {
    throw ExceptionSDLmixer("Only one midi player is allowed");
  }
  current_track = TypeMidiNone;
  current_midi_player = this;
  MIX_SetTrackStoppedCallback(music_track, music_finished_hook, nullptr);
}

AudioSDL::PlayerMIDI::~PlayerMIDI() {
  current_midi_player = nullptr;
  if (music_track != nullptr) {
    MIX_SetTrackStoppedCallback(music_track, nullptr, nullptr);
  }
}

Audio::PTrack
AudioSDL::PlayerMIDI::create_track(int track_id) {
  Data &data = Data::get_instance();
  Data::PSource data_source = data.get_data_source();

  PBuffer midi = data_source->get_music(track_id);
  if (!midi) {
    return nullptr;
  }

  /* MIDI (DOS data) is played by FluidSynth, which needs a SoundFont (see
     find_soundfont()). MOD music (Amiga data) needs no setup. */
  SDL_PropertiesID props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER,
                         SDL_IOFromConstMem(midi->get_data(),
                                            midi->get_size()));
  SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true);
  SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER,
                         mixer);
  static const std::string soundfont = find_soundfont();
  if (!soundfont.empty()) {
    SDL_SetStringProperty(props, "SDL_mixer.decoder.fluidsynth.soundfont_path",
                          soundfont.c_str());
  }
  MIX_Audio *music = MIX_LoadAudioWithProperties(props);
  SDL_DestroyProperties(props);
  if (music == nullptr) {
    Log::Warn["audio:SDL_mixer"] << "Could not load music track: "
                                 << SDL_GetError();
    return nullptr;
  }

  return std::make_shared<AudioSDL::TrackMIDI>(midi, music);
}

Audio::PTrack
AudioSDL::PlayerMIDI::play_track(int track_id) {
  /* Skip missing tracks; after the last one start again with the first,
     but only once, so that it ends when no track can be played. */
  Audio::PTrack track;
  bool wrapped = false;
  while (!track) {
    if ((track_id <= TypeMidiNone) || (track_id > TypeMidiTrackLast)) {
      if (wrapped) {
        break;
      }
      wrapped = true;
      track_id = TypeMidiTrack0;
    }
    current_track = static_cast<TypeMidi>(track_id);
    Log::Info["audio:SDL_mixer"] << "Playing MIDI track: " << current_track;
    track = Audio::Player::play_track(track_id);
    track_id++;
  }
  return track;
}

void
AudioSDL::PlayerMIDI::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  } else if (!MIX_TrackPlaying(music_track)) {
    /* Start the music again where it was stopped. */
    play_track((current_track == TypeMidiNone) ? TypeMidiTrack0 :
                                                 current_track);
  }
}

void
AudioSDL::PlayerMIDI::stop() {
  MIX_StopTrack(music_track, 0);
}

float
AudioSDL::PlayerMIDI::get_volume() {
  return MIX_GetTrackGain(music_track);
}

void
AudioSDL::PlayerMIDI::set_volume(float volume) {
  volume = std::max(0.f, std::min(volume, 1.f));
  MIX_SetTrackGain(music_track, volume);
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
AudioSDL::PlayerMIDI::music_finished_hook(void * /*userdata*/,
                                          MIX_Track * /*track*/) {
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
    Audio::PTrack t = play_track(current_track + 1);
  }
}

AudioSDL::TrackMIDI::TrackMIDI(PBuffer _data, MIX_Audio *_chunk)
  : data(_data)
  , chunk(_chunk) {
}

AudioSDL::TrackMIDI::~TrackMIDI() {
  MIX_DestroyAudio(chunk);
}

void
AudioSDL::TrackMIDI::play() {
  /* Replacing the playing track must not run the finished hook. */
  MIX_LockMixer(mixer);
  MIX_SetTrackStoppedCallback(music_track, nullptr, nullptr);
  MIX_StopTrack(music_track, 0);
  bool r = MIX_SetTrackAudio(music_track, chunk) &&
           MIX_PlayTrack(music_track, 0);
  MIX_SetTrackStoppedCallback(music_track, PlayerMIDI::music_finished_hook,
                              nullptr);
  MIX_UnlockMixer(mixer);
  if (!r) {
    Log::Warn["audio:SDL_mixer"] << "Could not play MIDI track: "
                                 << SDL_GetError();
    PlayerMIDI::music_finished_hook(nullptr, nullptr);
  }
}
