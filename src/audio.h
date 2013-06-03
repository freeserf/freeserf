/*
 * audio.h - Music and sound effects playback.
 *
 * Copyright (C) 2012  Wicked_Digger <wicked_digger@mail.ru>
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


#ifndef _AUDIO_H
#define _AUDIO_H

typedef enum {
	SFX_MESSAGE = 1,
	SFX_ACCEPTED = 2,
	SFX_NOT_ACCEPTED = 4,
	SFX_UNDO = 6,
	SFX_CLICK = 8,
	SFX_FIGHT_01 = 10,
	SFX_FIGHT_02 = 14,
	SFX_FIGHT_03 = 18,
	SFX_FIGHT_04 = 22,
	SFX_RESOURCE_FOUND = 26,
	SFX_PICK_BLOW = 28,
	SFX_METAL_HAMMERING = 30,
	SFX_AX_BLOW = 32,
	SFX_TREE_FALL = 34,
	SFX_WOOD_HAMMERING = 36,
	SFX_ELEVATOR = 38,
	SFX_HAMMER_BLOW = 40,
	SFX_SAWING = 42,
	SFX_MILL_GRINDING = 43,
	SFX_BACKSWORD_BLOW = 44,
	SFX_GEOLOGIST_SAMPLING = 46,
	SFX_PLANTING = 48,
	SFX_DIGGING = 50,
	SFX_MOWING = 52,
	SFX_FISHING_ROD_REEL = 54,
	SFX_UNKNOWN_21 = 58,
	SFX_PIG_OINK = 60,
	SFX_GOLD_BOILS = 62,
	SFX_ROWING = 64,
	SFX_UNKNOWN_25 = 66,
	SFX_SERF_DYING = 69,
	SFX_BIRD_CHIRP_0 = 70,
	SFX_BIRD_CHIRP_1 = 74,
	SFX_AHHH = 76,
	SFX_BIRD_CHIRP_2 = 78,
	SFX_BIRD_CHIRP_3 = 82,
	SFX_BURNING = 84,
	SFX_UNKNOWN_28 = 86,
	SFX_UNKNOWN_29 = 88,
} sfx_t;

typedef enum {
	MIDI_TRACK_0 = 0,
	MIDI_TRACK_1 = 1,
	MIDI_TRACK_2 = 2,
	MIDI_TRACK_3 = 4,
} midi_t;

/* Common audio. */
void audio_cleanup();
int audio_volume();
void audio_set_volume(int volume);
void audio_volume_up();
void audio_volume_down();

/* Play sound. */
void sfx_play_clip(sfx_t sfx);
void sfx_enable(int enable);
int sfx_is_enabled();

/* Play music. */
void midi_play_track(midi_t midi);
void midi_enable(int enable);
int midi_is_enabled();

#endif /* ! _AUDIO_H */
