/*
 * audio.h - Music and sound effects playback base.
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


#ifndef SRC_AUDIO_H_
#define SRC_AUDIO_H_

#include <map>
#include <memory>
#include <string>

#include "src/debug.h"

class ExceptionAudio : public ExceptionFreeserf {
 public:
  explicit ExceptionAudio(const std::string &description) throw();
  virtual ~ExceptionAudio() throw();

  virtual std::string get_description() const;
  virtual std::string get_platform() const { return "Abstract"; }
  virtual std::string get_system() const { return "audio"; }
};

class Audio {
 public:
  typedef enum TypeSfx {
    TypeSfxMessage = 1,
    TypeSfxAccepted = 2,
    TypeSfxNotAccepted = 4,
    TypeSfxUndo = 6,
    TypeSfxClick = 8,
    TypeSfxFight01 = 10,
    TypeSfxFight02 = 14,
    TypeSfxFight03 = 18,
    TypeSfxFight04 = 22,
    TypeSfxResourceFound = 26,
    TypeSfxPickBlow = 28,
    TypeSfxMetalHammering = 30,
    TypeSfxAxBlow = 32,
    TypeSfxTreeFall = 34,
    TypeSfxWoodHammering = 36,
    TypeSfxElevator = 38,
    TypeSfxHammerBlow = 40,
    TypeSfxSawing = 42,
    TypeSfxMillGrinding = 43,
    TypeSfxBackswordBlow = 44,
    TypeSfxGeologistSampling = 46,
    TypeSfxPlanting = 48,
    TypeSfxDigging = 50,
    TypeSfxMowing = 52,
    TypeSfxFishingRodReel = 54,
    TypeSfxUnknown21 = 58,
    TypeSfxPigOink = 60,
    TypeSfxGoldBoils = 62,
    TypeSfxRowing = 64,
    TypeSfxUnknown25 = 66,
    TypeSfxSerfDying = 69,
    TypeSfxBirdChirp0 = 70,
    TypeSfxBirdChirp1 = 74,
    TypeSfxAhhh = 76,
    TypeSfxBirdChirp2 = 78,
    TypeSfxBirdChirp3 = 82,
    TypeSfxBurning = 84,
    TypeSfxUnknown28 = 86,
    TypeSfxUnknown29 = 88,
  } TypeSfx;

  typedef enum TypeMidi {
    TypeMidiNone = -1,
    TypeMidiTrack0 = 0,
    TypeMidiTrack1 = 1,
    TypeMidiTrack2 = 2,
    TypeMidiTrack3 = 4,
    TypeMidiTrackLast = TypeMidiTrack3,
  } TypeMidi;

  class VolumeController {
   public:
    virtual float get_volume() = 0;
    virtual void set_volume(float volume) = 0;
    virtual void volume_up() = 0;
    virtual void volume_down() = 0;
  };
  typedef std::shared_ptr<VolumeController> PVolumeController;

  class Track {
   public:
    virtual ~Track() {}

    virtual void play() = 0;
  };
  typedef std::shared_ptr<Track> PTrack;

  class Player {
   protected:
    typedef std::map<int, PTrack> TrackCache;
    TrackCache track_cache;
    bool enabled;

   public:
    Player();
    virtual ~Player();

    virtual void play_track(int track_id);
    virtual void enable(bool enable) = 0;
    virtual bool is_enabled() const { return enabled; }
    virtual PVolumeController get_volume_controller() = 0;

   protected:
    virtual PTrack create_track(int track_id) = 0;
    virtual void stop() = 0;
  };
  typedef std::shared_ptr<Player> PPlayer;

 protected:
  static Audio *instance;

  float volume;

  Audio() {}

 public:
  /* Common audio. */
  virtual ~Audio() {}

  static Audio *get_instance();

  virtual VolumeController *get_volume_controller() = 0;
  virtual PPlayer get_sound_player() = 0;
  virtual PPlayer get_music_player() = 0;
};

#endif  // SRC_AUDIO_H_
