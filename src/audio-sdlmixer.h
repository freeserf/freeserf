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

class sfx_track_t : public audio_track_t {
 protected:
  Mix_Chunk *chunk;

 public:
  explicit sfx_track_t(Mix_Chunk *chunk);
  virtual ~sfx_track_t();

  virtual void play();
};

class sfx_player_t : public audio_player_t, public audio_volume_controller_t {
 public:
  virtual void enable(bool enable);
  virtual audio_volume_controller_t *get_volume_controller() { return this; }

 protected:
  virtual audio_track_t *create_track(int track_id);
  virtual void stop();

 public:
  virtual float get_volume();
  virtual void set_volume(float volume);
  virtual void volume_up();
  virtual void volume_down();
};

class midi_track_t : public audio_track_t {
 protected:
  Mix_Music *chunk;

 public:
  explicit midi_track_t(Mix_Music *chunk);
  virtual ~midi_track_t();

  virtual void play();
};

class midi_player_t : public audio_player_t, public audio_volume_controller_t,
                      public deferred_callee_t {
 protected:
  midi_t current_track;

 public:
  midi_player_t();
  virtual ~midi_player_t();

  virtual void play_track(int track_id);
  virtual void enable(bool enable);
  virtual audio_volume_controller_t *get_volume_controller() { return this; }

 protected:
  virtual audio_track_t *create_track(int track_id);
  virtual void stop();

 public:
  virtual float get_volume();
  virtual void set_volume(float volume);
  virtual void volume_up();
  virtual void volume_down();

 public:
  virtual void deferred_call(void *data);

 protected:
  static midi_player_t *current_midi_player;
  static void music_finished_hook();
  void music_finished();
};

class audio_sdlmixer_t : public audio_t, public audio_volume_controller_t {
 protected:
  audio_player_t *sfx_player;
  audio_player_t *midi_player;

  float volume;

 public:
  /* Common audio. */
  audio_sdlmixer_t();
  virtual ~audio_sdlmixer_t();

  virtual audio_volume_controller_t *get_volume_controller() { return this; }
  virtual audio_player_t *get_sound_player() { return sfx_player; }
  virtual audio_player_t *get_music_player() { return midi_player; }

 public:
  virtual float get_volume();
  virtual void set_volume(float volume);
  virtual void volume_up();
  virtual void volume_down();
};

#endif  // SRC_AUDIO_SDLMIXER_H_
