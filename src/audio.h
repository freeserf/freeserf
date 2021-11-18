/*
 * audio.h - Music and sound effects playback base.
 *
 * Copyright (C) 2012-2018  Wicked_Digger <wicked_digger@mail.ru>
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
  explicit ExceptionAudio(const std::string &description);
  virtual ~ExceptionAudio();

  virtual std::string get_description() const;
  virtual std::string get_platform() const { return "Abstract"; }
  virtual std::string get_system() const { return "audio"; }
};

class Audio {
 public:
  typedef enum TypeSfx {
    // the numbers that are not represented here appear to be unreadable/nonexistant?
    //  if you try to export DOS resources using FSStudio it says Error [data] Could not extract SFX clip: #n
    //  for all of the ones missing below
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
    TypeSfxTreeFall = 34,  // the DOS vs Amiga sounds are very different, I think DOS is better
    TypeSfxWoodHammering = 36,
    TypeSfxElevator = 38,
    TypeSfxHammerBlow = 40,
    TypeSfxSawing = 42,   // the DOS vs Amiga sounds are very different, I think Amiga is better
    TypeSfxMillGrinding = 43,  // the DOS vs Amiga sounds are very different, I think Amiga is better
    TypeSfxBackswordBlow = 44,
    TypeSfxGeologistSampling = 46,  // the DOS vs Amiga sounds are slightly different, I think Amiga is a bit better
    TypeSfxPlanting = 48,  // the DOS vs Amiga sounds are slightly different, I can't say which I prefer
    TypeSfxDigging = 50,  // DOS vs Amiga sounds are slightly different, I can't say which I prefer
    TypeSfxMowing = 52,
    TypeSfxFishingRodReel = 54,  // the DOS sample repeats 4x times, while Amiga is a single non-repeating sound (very similar sound though)
    TypeSfxUnknown21 = 58,  // sounds like "hup"  or hopping-over-something quick grunt sound
    TypeSfxPigOink = 60,
    TypeSfxGoldBoils = 62,  // the DOS vs Amiga sounds are very different, I think Amiga is better. Also, in Freeserf the DOS sound #62 is buggy (pop, static)
    TypeSfxRowing = 64,     // the DOS vs Amiga sounds are very different, I think DOS is better
    TypeSfxUnknown25 = 66,  // sounds like a fainter burning sound, could be an alternate building burn sound?  the DOS vs Amiga sounds are very different, Amiga sounds like a motor
    TypeSfxSerfDying = 69,  
    TypeSfxBirdChirp0 = 70, // DOS vs Amiga sounds are quite different, I can't say which I prefer, I think Amiga
    TypeSfxBirdChirp1 = 74, // DOS vs Amiga sounds are quite different, I can't say which I prefer, I think Amiga
    TypeSfxAhhh = 76,
    TypeSfxBirdChirp2 = 78, // DOS vs Amiga sounds are VERY different, Amiga is way better
    TypeSfxBirdChirp3 = 82, // DOS vs Amiga sounds are VERY different, Amiga is probably better
    TypeSfxBurning = 84,
    TypeSfxUnknown28 = 86,  // ocean sound, use for ambient water sounds. DOS vs Amiga sounds are quite different, I think DOS is better
    TypeSfxUnknown29 = 88,  // whistling wind sound, user for ambient desert sound.  DOS vs Amiga sounds are slightly different, I can't say which I prefer
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
    virtual ~VolumeController() {}
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

    //virtual PTrack play_track(int track_id);
    virtual PTrack play_track(int track_id, int source_type);   // 0=Amiga, 1=DOS, 2=Custom
    virtual void enable(bool enable) = 0;
    virtual bool is_enabled() const { return enabled; }
    virtual PVolumeController get_volume_controller() = 0;

   protected:
    //virtual PTrack create_track(int track_id) = 0;
    virtual PTrack create_track(int track_id, int source_type) = 0;
    virtual void stop() = 0;
  };
  typedef std::shared_ptr<Player> PPlayer;

 protected:
  static Audio *instance;

  float volume;

  Audio() : volume(0.75f) {}

 public:
  /* Common audio. */
  virtual ~Audio() {}

  static Audio &get_instance();

  virtual VolumeController *get_volume_controller() = 0;
  virtual PPlayer get_sound_player() = 0;
  virtual PPlayer get_music_player() = 0;
};

#endif  // SRC_AUDIO_H_
