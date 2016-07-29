/*
 * audio-sdlmixer.h - Music and sound effects playback using SDL_mixer.
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


#ifndef SRC_AUDIO_SDLMIXER_H_
#define SRC_AUDIO_SDLMIXER_H_

#include "src/audio.h"

#include <SDL_mixer.h>

#include "src/event_loop.h"

class AudioSDL : public Audio, public Audio::VolumeController {
 protected:
  class TrackSFX : public Audio::Track {
   protected:
    Mix_Chunk *chunk;

   public:
    explicit TrackSFX(Mix_Chunk *chunk);
    virtual ~TrackSFX();

    virtual void play();
  };

  class PlayerSFX : public Audio::Player, public Audio::VolumeController {
   public:
    virtual void enable(bool enable);
    virtual Audio::VolumeController *get_volume_controller() { return this; }

   protected:
    virtual Audio::Track *create_track(int track_id);
    virtual void stop();

   public:
    virtual float get_volume();
    virtual void set_volume(float volume);
    virtual void volume_up();
    virtual void volume_down();
  };

  class TrackMIDI : public Audio::Track {
   protected:
    Mix_Music *chunk;

   public:
    explicit TrackMIDI(Mix_Music *chunk);
    virtual ~TrackMIDI();

    virtual void play();
  };

  class PlayerMIDI : public Audio::Player, public Audio::VolumeController,
                     public DeferredCallee {
   protected:
    TypeMidi current_track;

   public:
    PlayerMIDI();
    virtual ~PlayerMIDI();

    virtual void play_track(int track_id);
    virtual void enable(bool enable);
    virtual Audio::VolumeController *get_volume_controller() { return this; }

   protected:
    virtual Audio::Track *create_track(int track_id);
    virtual void stop();

   public:
    virtual float get_volume();
    virtual void set_volume(float volume);
    virtual void volume_up();
    virtual void volume_down();

   public:
    virtual void deferred_call(void *data);

   protected:
    static PlayerMIDI *current_midi_player;
    static void music_finished_hook();
    void music_finished();
  };

 protected:
  Audio::Player *sfx_player;
  Audio::Player *midi_player;

  float volume;

 public:
  /* Common audio. */
  AudioSDL();
  virtual ~AudioSDL();

  virtual Audio::VolumeController *get_volume_controller() { return this; }
  virtual Audio::Player *get_sound_player() { return sfx_player; }
  virtual Audio::Player *get_music_player() { return midi_player; }

 public:
  virtual float get_volume();
  virtual void set_volume(float volume);
  virtual void volume_up();
  virtual void volume_down();
};

#endif  // SRC_AUDIO_SDLMIXER_H_
