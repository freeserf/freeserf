/*
 * audio-dummy.c - Dummy music and sound effects playback.
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "audio.h"
#include "log.h"


/* Common audio. */
int
audio_init()
{
	LOGI("audio-dummy", "Initializing audio driver `dummy'.");
	return 0;
}

void
audio_deinit()
{
}

int
audio_volume()
{
	return 0;
}

void
audio_set_volume(int volume)
{
	LOGV("audio-dummy", "Request to set volume to %i", volume);
}

void
audio_volume_up()
{
	LOGV("audio-dummy", "Request to increase volume");
}

void
audio_volume_down()
{
	LOGV("audio-dummy", "Request to decrease volume");
}


/* Play sound. */
void
sfx_play_clip(sfx_t sfx)
{
	LOGV("audio-dummy", "Request to play SFX clip %i", sfx);
}

void
sfx_enable(int enable)
{
}

int
sfx_is_enabled()
{
	return 0;
}


/* Play music. */
void
midi_play_track(midi_t midi)
{
	LOGV("audio-dummy", "Request to play MIDI track %i", midi);
}

void
midi_enable(int enable)
{
}

int
midi_is_enabled()
{
	return 0;
}
