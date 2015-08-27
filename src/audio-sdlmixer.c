/*
 * audio-sdlmixer.c - Music and sound effects playback using SDL_mixer.
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

#include "audio.h"
#include "data.h"
#include "log.h"
#include "list.h"
#include "pqueue.h"
#include "freeserf_endian.h"

#include "SDL.h"
#include "SDL_mixer.h"

#include <string.h>

/* Play sound. */

typedef struct {
	list_elm_t elm;
	int num;
	Mix_Chunk *chunk;
} audio_clip_t;

typedef struct {
	list_elm_t elm;
	int num;
	Mix_Music *music;
} track_t;

static list_t sfx_clips_to_play;
static list_t midi_tracks;
static int sfx_enabled = 1;
static int midi_enabled = 1;
static midi_t current_track = MIDI_TRACK_NONE;

static void midi_track_finished();


int
audio_init()
{
	LOGI("audio-sdlmixer", "Initializing audio driver `sdlmixer'.");

	list_init(&sfx_clips_to_play);
	list_init(&midi_tracks);

	int r = Mix_Init(0);
	if (r != 0) {
		LOGE("audio-sdlmixer", "Could not init SDL_mixer: %s.", Mix_GetError());
		return -1;
	}

	r = Mix_OpenAudio(8000, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512);
	if (r < 0) {
		LOGE("audio-sdlmixer", "Could not open audio device: %s.", Mix_GetError());
		return -1;
	}

	r = Mix_AllocateChannels(16);
	if (r != 16) {
		LOGE("audio-sdlmixer", "Failed to allocate channels: %s.", Mix_GetError());
		return -1;
	}

	Mix_HookMusicFinished(midi_track_finished);

	return 0;
}

void
audio_deinit()
{
	while (!list_is_empty(&sfx_clips_to_play)) {
		list_elm_t *elm = list_remove_head(&sfx_clips_to_play);
		audio_clip_t *audio_clip = (audio_clip_t*)elm;
		Mix_FreeChunk(audio_clip->chunk);
		free(elm);
	}

	while (!list_is_empty(&midi_tracks)) {
		list_elm_t *elm = list_remove_head(&midi_tracks);
		track_t *track = (track_t*)elm;
		Mix_FreeMusic(track->music);
		free(elm);
	}

	Mix_CloseAudio();
	Mix_Quit();
}

int
audio_volume()
{
	return (int)((float)Mix_Volume(-1, -1) / (float)MIX_MAX_VOLUME * 99.f + 0.5f);
}

void
audio_set_volume(int volume)
{
	/* TODO Increment by one only works for MIX_MAX_VALUE > 50*/
	volume = (int)((float)volume / 99.f * (float)MIX_MAX_VOLUME + 0.5f);
	Mix_Volume(-1, volume);
	Mix_VolumeMusic(volume);
}

void
audio_volume_up()
{
	int volume = audio_volume();
	audio_set_volume(volume + 1);
}

void
audio_volume_down()
{
	int volume = audio_volume();
	audio_set_volume(volume - 1);
}

static char *
sfx_produce_wav(char* data, uint32_t size, size_t *new_size)
{
#define WRITE_DATA_WG(X) {memcpy(current, &X, sizeof(X)); current+=sizeof(X);};
#define WRITE_BE32_WG(X) {uint32_t val = X; val = htobe32(val); WRITE_DATA_WG(val);}
#define WRITE_LE32_WG(X) {uint32_t val = X; val = htole32(val); WRITE_DATA_WG(val);}
#define WRITE_BE16_WG(X) {uint16_t val = X; val = htobe16(val); WRITE_DATA_WG(val);}
#define WRITE_LE16_WG(X) {uint16_t val = X; val = htole16(val); WRITE_DATA_WG(val);}
#define WRITE_BYTE_WG(X) {*current = (uint8_t)X; current++;}

	*new_size = 44 + size*2;

	char *result = (char*)malloc(*new_size);
	if (result == NULL) abort();

	char *current = result;

	/* WAVE header */
	WRITE_BE32_WG(0x52494646);	/* 'RIFF' */
	WRITE_LE32_WG((uint32_t)*new_size - 8);		/* Chunks size */
	WRITE_BE32_WG(0x57415645);	/* 'WAVE' */

	/* Subchunk #1 */
	WRITE_BE32_WG(0x666d7420);	/* 'fmt ' */
	WRITE_LE32_WG(16);					/* Subchunk size */
	WRITE_LE16_WG(1);						/* Format = PCM */
	WRITE_LE16_WG(1);						/* Chanels count */
	WRITE_LE32_WG(8000);				/* Rate */
	WRITE_LE32_WG(16000);				/* Byte rate */
	WRITE_LE16_WG(2);						/* Block align */
	WRITE_LE16_WG(16);						/* Bits per sample */

	/* Subchunk #2 */
	WRITE_BE32_WG(0x64617461);	/* 'data' */
	WRITE_LE32_WG(size*2);				/* Data size */
	for (uint32_t i = 0; i < size; i++) {
		int value = *(data + i);
		value = value - 0x20;
		WRITE_BE16_WG(value*0xFF);
	}

	return result;
}

void
sfx_play_clip(sfx_t sfx)
{
	if (0 == sfx_enabled) {
		return;
	}

	audio_clip_t *audio_clip = NULL;
	list_elm_t *elm;
	list_foreach(&sfx_clips_to_play, elm) {
		if (sfx == ((audio_clip_t*)elm)->num) {
			audio_clip = (audio_clip_t*)elm;
		}
	}

	if (NULL == audio_clip) {
		audio_clip = (audio_clip_t*)malloc(sizeof(audio_clip_t));
		if (audio_clip == NULL) abort();
		audio_clip->num = sfx;

		size_t size = 0;
		char *data = (char*)data_get_object(DATA_SFX_BASE + sfx, &size);

		char *wav = sfx_produce_wav(data, (int)size, &size);

		SDL_RWops *rw = SDL_RWFromMem(wav, (int)size);
		audio_clip->chunk = Mix_LoadWAV_RW(rw, 0);
		free(wav);
		if (!audio_clip->chunk) {
			LOGE("audio-sdlmixer", "Mix_LoadWAV_RW: %s.", Mix_GetError());
			free(audio_clip);
			return;
		}

		list_prepend(&sfx_clips_to_play, (list_elm_t*)audio_clip);
	}

	int r = Mix_PlayChannel(-1, audio_clip->chunk, 0);
	if (r < 0) {
		LOGE("audio-sdlmixer", "Could not play SFX clip: %s.", Mix_GetError());
		return;
	}
}

void
sfx_enable(int enable)
{
	sfx_enabled = enable;
}

int
sfx_is_enabled()
{
	return sfx_enabled;
}


/* Play music. */

/* Midi node. */
typedef struct {
	uint64_t time;
	uint index;
	uint8_t type;
	uint8_t data1;
	uint8_t data2;
	char *buffer;
} midi_node_t;

typedef struct {
	pqueue_t nodes;
	uint32_t tempo;
	uint8_t *data;
	uint64_t size;
} midi_file_t;


/* Return true if first midi node should come before second. */
static int
midi_node_less(const midi_node_t *m1, const midi_node_t *m2)
{
	if (m1->time != m2->time) {
		return m1->time < m2->time;
	}

	if (m1->index != m2->index) {
		return m1->index < m2->index;
	}

	return 0;
}



static int xmi_process_subchunks(char *data, int length, midi_file_t *midi);
static int xmi_process_INFO(char *data, int length, midi_file_t *midi);
static int xmi_process_TIMB(char *data, int length, midi_file_t *midi);
static int xmi_process_EVNT(char *data, int length, midi_file_t *midi);
static int xmi_process_multi(char *data, int length, midi_file_t *midi);
static int xmi_process_single(char *data, int length, midi_file_t *midi);

typedef struct {
	uint32_t name;
	int (*processor)(char *, int, midi_file_t*);
} chunk_info_t;

static chunk_info_t xmi_processors[] = {
	{ 0x464F524D, xmi_process_multi  },	/* 'FORM' */
	{ 0x58444952, xmi_process_single },	/* 'XDIR' */
	{ 0x494E464F, xmi_process_INFO   },	/* 'INFO' */
	{ 0x43415420, xmi_process_multi  },	/* 'CAT ' */
	{ 0x584D4944, xmi_process_single },	/* 'XMID' */
	{ 0x54494D42, xmi_process_TIMB   },	/* 'TIMB' */
	{ 0x45564E54, xmi_process_EVNT   },	/* 'EVNT' */
};

static int
xmi_process_multi(char *data, int length, midi_file_t *midi)
{
	int size = be32toh(*(int*)data);
	data += 4;
	xmi_process_subchunks(data, size, midi);
	return 4+size;
}

static int
xmi_process_single(char *data, int length, midi_file_t *midi)
{
	int size = 0;
	int name = be32toh(*(int*)data);
	uint32_t string = htobe32(name);

	LOGV("audio-sdlmixer", "Processing XMI chunk: %.4s", (char *)&string);

	data += 4;
	int processed = 0;
	for (int i = 0; i < sizeof(xmi_processors)/sizeof(chunk_info_t); i++) {
		if (xmi_processors[i].name == name) {
			if (NULL != xmi_processors[i].processor) {
				size = xmi_processors[i].processor(data, length-4, midi);
			}
			processed = 1;
			break;
		}
	}
	if (0 == processed) {
		LOGD("audio-sdlmixer", "Unknown XMI chunk: %s (0x%X)", string, name);
	}
	return size+4;
}

static int
xmi_process_subchunks(char *data, int length, midi_file_t *midi)
{
	int balance = length;

	while (balance) {
		int size = xmi_process_single(data, balance, midi);
		data += size;
		balance -= size;
	}

	return length;
}

static int
xmi_process_INFO(char *data, int length, midi_file_t *midi)
{
	uint32_t size = *(uint32_t*)data;
	data += 4;
	size = be32toh(size);
	if (size != 2) {
		LOGD("audio-sdlmixer", "\tInconsistent INFO block.");
	} else {
		uint16_t track_count = *(uint16_t*)data;
		LOGV("audio-sdlmixer", "\tXMI contains %d track(s)", track_count);
	}
	return size + 4;
}

static int
xmi_process_TIMB(char *data, int length, midi_file_t *midi)
{
	uint32_t size = *(uint32_t*)data;
	data += 4;
	size = be32toh(size);
	uint16_t count = *(uint16_t*)data;
	data += 2;
	if (count*2 + 2 != size) {
		LOGD("audio-sdlmixer", "\tInconsistent TIMB block.");
	} else {
		for (int i = 0; i < count; i++) {
			uint8_t num = *data++;
			uint8_t bank = *data++;
			LOGV("audio-sdlmixer", "\tTIMB entry %02d: %d, %d", i, (int)num, (int)bank);
		}
	}
	return size + 4;
}

static int
xmi_process_EVNT(char *data, int length, midi_file_t *midi)
{
#define READ_DATA(X) {X = *data; data += sizeof(X); balance -= sizeof(X); };

	int balance = length;
	uint64_t time = 0;
	uint time_index = 0;

	uint32_t unknown = 0;
	READ_DATA(unknown);

	while (balance) {
		uint8_t type = 0;
		READ_DATA(type);

		if (type & 0x80) {
			midi_node_t *node = (midi_node_t*)malloc(sizeof(midi_node_t));
			if (node == NULL) abort();

			node->time = time;
			node->index = time_index++;
			node->type = type;
			node->buffer = NULL;

			switch (type & 0xF0) {
			case 0x80:
			case 0x90:
			case 0xA0:
			case 0xB0:
			case 0xE0:
				READ_DATA(node->data1);
				READ_DATA(node->data2);
				if (0x90 == (type & 0xF0)) {
					uint8_t data1 = node->data1;
					pqueue_insert(&midi->nodes, node);

					node = (midi_node_t*)malloc(sizeof(midi_node_t));
					if (node == NULL) abort();

					node->type = type;

					/* Decode variable length duration. */
					uint64_t length = 0;
					READ_DATA(type);
					while (type & 0x80) {
						length = (length << 7) | (type & 0x7f);
						READ_DATA(type);
					}
					length = (length << 7) | (type & 0x7f);

					/* Generate on note with velocity zero
					   corresponding to off note. */
					node->time = time + length;
					node->index = time_index++;
					node->data1 = data1;
					node->data2 = 0;
				}
				break;
			case 0xC0:
			case 0xD0:
				READ_DATA(node->data1);
				node->data2 = 0;
				break;
			case 0xF0:
				if (0xFF == type) {
					/* Meta message */
					READ_DATA(node->data1);
					READ_DATA(node->data2);
					node->buffer = data;
					if (0x51 == node->data1) {
						node->buffer = data;
						uint32_t tempo = 0;
						for (int i = 0; i < node->data2; i++) {
							tempo = tempo << 8;
							uint8_t byte = 0;
							READ_DATA(byte);
							tempo |= byte;
						}

						if (0 == midi->tempo) {
							midi->tempo = tempo;
						}
					} else {
						data += node->data2;
						balance -= node->data2;
					}
				}
				break;
			}

			pqueue_insert(&midi->nodes, node);
		} else {
			time += type;
		}
	}

	return length;
}

static void
midi_grow(midi_file_t *midi, uint8_t **current)
{
	uint8_t *data = (uint8_t*)malloc((size_t)(midi->size + 1024));
	if (data == NULL) abort();

	Uint64 pos = *current - midi->data;
	if (midi->data) {
		memcpy(data, midi->data, (size_t)midi->size);
		free(midi->data);
	}
	*current = data + pos;
	midi->data = data;
	midi->size += 1024;
}

static uint32_t
midi_write_variable_size(midi_file_t *midi, uint8_t **current, Uint64 val)
{
	uint32_t count = 1;
	uint32_t buf = val & 0x7F;
	for (; val >>= 7; ++count) {
		buf = (buf << 8) | 0x80 | (val & 0x7F);
	}
	if (midi->size <= (*current-midi->data) + count) {
		midi_grow(midi,current);
	}
	for (uint32_t i = 0; i < count; ++i) {
		**current = (uint8_t)(buf & 0xFF);
		(*current)++;
		buf >>= 8;
	}

	return count;
}

static void*
midi_produce(midi_file_t *midi, size_t *size)
{
#define WRITE_DATA(X) {if (midi->size <= (current-midi->data) + sizeof(X)) midi_grow(midi,&current); memcpy(current, &X, sizeof(X)); current+=sizeof(X);};
#define WRITE_BE32(X) {uint32_t val = X; val = htobe32(val); WRITE_DATA(val);}
#define WRITE_LE32(X) {uint32_t val = X; val = htole32(val); WRITE_DATA(val);}
#define WRITE_BE16(X) {uint16_t val = X; val = htobe16(val); WRITE_DATA(val);}
#define WRITE_BYTE(X) {if (midi->size <= (current-midi->data) + sizeof(X)) midi_grow(midi,&current); *current = (uint8_t)X; current++;}

	uint8_t *current = midi->data;

	/* Header */
	WRITE_BE32(0x4D546864);		/* 'MThd' */
	WRITE_BE32(6);						/* Header size */
	WRITE_BE16(0);						/* File type */
	WRITE_BE16(1);						/* Track count */
	WRITE_BE16(midi->tempo * 3 / 25000);	/* Time division */

	/* First track */
	WRITE_BE32(0x4D54726B);		/* 'MTrk' */
	uint64_t size_pos = current - midi->data;
	WRITE_BE32(0);					/* Size reserved */

	uint64_t time = 0;
	int i = 0;
	while (!pqueue_is_empty(&midi->nodes)) {
		i++;
		midi_node_t *node = (midi_node_t*)pqueue_pop(&midi->nodes);
		midi_write_variable_size(midi, &current, node->time - time);
		time = node->time;
		WRITE_BYTE(node->type);
		WRITE_BYTE(node->data1);
		if (((node->type & 0xF0) != 0xC0) && ((node->type & 0xF0) != 0xD0)) {
			WRITE_BYTE(node->data2);
			if (node->type == 0xFF) {
				if (node->data2 > 0) {
					if (midi->size <= (current-midi->data) + node->data2) {
						midi_grow(midi,&current);
					}
					memcpy(current, node->buffer, node->data2);
					current += node->data2;
				}
			}
		}
		free(node);
	}

	*size = (uint32_t)(current - midi->data);
	uint32_t data_size = (uint32_t)(*size - size_pos - 4);
	current = midi->data + size_pos;
	WRITE_BE32(data_size);	/* Write correct size */

	return midi->data;
}

void
midi_play_track(midi_t midi)
{
	if (0 == midi_enabled) {
		return;
	}

	track_t *track = NULL;
	list_elm_t *elm;
	list_foreach(&midi_tracks, elm) {
		if (midi == ((track_t*)elm)->num) {
			track = (track_t*)elm;
		}
	}

	current_track = midi;

	if (NULL == track) {
		track = (track_t *)malloc(sizeof(track_t));
		if (track == NULL) abort();

		track->num = midi;

		size_t size = 0;
		char *data = (char*)data_get_object(DATA_MUSIC_GAME + midi, &size);
		if (NULL == data) {
			free(track);
			return;
		}

		midi_file_t midi_file;
		pqueue_init(&midi_file.nodes, 16*1024,
			    (pqueue_less_func *)midi_node_less);
		midi_file.tempo = 0;
		midi_file.data = NULL;
		midi_file.size = 0;

		xmi_process_subchunks(data, (int)size, &midi_file);
		data = (char*)midi_produce(&midi_file, &size);

		pqueue_deinit(&midi_file.nodes);

		SDL_RWops *rw = SDL_RWFromMem(data, (int)size);
		track->music = Mix_LoadMUS_RW(rw, 0);
		if (NULL == track->music) {
			free(track);
			return;
		}

		list_append(&midi_tracks, (list_elm_t*)track);
	}

	int r = Mix_PlayMusic(track->music, 0);
	if (r < 0) {
		LOGW("audio-sdlmixer", "Could not play MIDI track: %s\n", Mix_GetError());
		return;
	}

	return;
}

void
midi_enable(int enable)
{
	midi_enabled = enable;
	if (0 != enable) {
		if (current_track != -1) {
			midi_play_track(current_track);
		}
	} else {
		Mix_HaltMusic();
	}
}

int
midi_is_enabled()
{
	return midi_enabled;
}

static void
midi_track_finished()
{
	if (midi_enabled) {
		midi_play_track(current_track);
	}
}
