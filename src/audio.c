/* audio.c */

#include "audio.h"
#include "gfx.h"
#include "data.h"
#include "log.h"
#include "list.h"
#include "freeserf_endian.h"

#include "SDL.h"
#include "SDL_mixer.h"

#include <time.h>

/* Play sound. */

typedef struct {
	list_elm_t elm;
	int num;
	Mix_Chunk chunk;
} audio_clip_t;

static list_t sfx_clips_to_play;
static int initialized = 0;
static int sfx_enabled = 1;
static int midi_enabled = 1;

static void
midi_track_finished();

static void
sfx_init()
{
	list_init(&sfx_clips_to_play);

	int r = Mix_OpenAudio(8000, AUDIO_U8, 1, 4096);
	if (r < 0) {
		LOGE("Could not open audio device: %s\n", Mix_GetError());
		return;
	}
	
	Mix_HookMusicFinished(midi_track_finished);

	initialized = 1;
}

int
list_less_func_sfx(const list_elm_t *e1, const list_elm_t *e2)
{
	return ((audio_clip_t*)e1)->num < ((audio_clip_t*)e2)->num;
}

void
enqueue_sfx_clip(sfx_t sfx)
{
	if (0 == sfx_enabled) {
		return;
	}

	if (0 == initialized) {
		sfx_init();
		if (0 == initialized) {
			return;
		}
	}

	audio_clip_t *audio_clip = NULL;
	list_elm_t *elm;
	list_foreach(&sfx_clips_to_play, elm) {
		if (sfx == ((audio_clip_t*)elm)->num) {
			audio_clip = (audio_clip_t*)elm;
		}
	}

	if (NULL == audio_clip) {
		audio_clip = malloc(sizeof(audio_clip_t));
		if (audio_clip == NULL) abort();
		audio_clip->num = sfx;

		size_t size = 0;
		audio_clip->chunk.abuf = gfx_get_data_object(DATA_SFX_BASE + sfx, &size);
		audio_clip->chunk.alen = (int)size;
		audio_clip->chunk.allocated = 0;
		audio_clip->chunk.volume = 128;

		list_insert_sorted(&sfx_clips_to_play, (list_elm_t*)audio_clip, list_less_func_sfx);
	}

	int r = Mix_PlayChannel(-1, &(audio_clip->chunk), 0);
	if (r < 0) {
		LOGE("Could not play SFX clip: %s\n", Mix_GetError());
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

typedef struct {
	list_t nodes;
	Uint32 tempo;
	Uint8 *data;
	Uint64 size;
} midi_file_t;

static int xmi_process_subchunks(char *data, int length, midi_file_t *midi);
static int xmi_process_INFO(char *data, int length, midi_file_t *midi);
static int xmi_process_TIMB(char *data, int length, midi_file_t *midi);
static int xmi_process_EVNT(char *data, int length, midi_file_t *midi);
static int xmi_process_multi(char *data, int length, midi_file_t *midi);
static int xmi_process_single(char *data, int length, midi_file_t *midi);

typedef struct {
	Uint32 name;
	int (*processor)(char *, int, midi_file_t*);
} chunk_info_t;

static chunk_info_t xmi_processors[] = {
	{ 0x464F524D, xmi_process_multi  },	// 'FORM'
	{ 0x58444952, xmi_process_single },	// 'XDIR'
	{ 0x494E464F, xmi_process_INFO   },	// 'INFO'
	{ 0x43415420, xmi_process_multi  },	// 'CAT '
	{ 0x584D4944, xmi_process_single },	// 'XMID'
	{ 0x54494D42, xmi_process_TIMB   },	// 'TIMB'
	{ 0x45564E54, xmi_process_EVNT   },	// 'EVNT'
} ;

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
	char string[5] = {0};
	*((Uint32*)string) = htobe32(name);
	LOGI("Processing XMI chunk: %s", string);

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
		LOGW("Unknown XMI chunk: %s (0x%X)", string, name);
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
	return 6;
}

#define READ_DATA(X) {X = *data; data += sizeof(X); balance -= sizeof(X); };

static int
xmi_process_TIMB(char *data, int length, midi_file_t *midi)
{
	Uint32 size = *(Uint32*)data;
	size = be32toh(size);
	return size + 4;
}

typedef struct {
	list_elm_t elm;
	Uint64 time;
	Uint8  type;
	Uint8  data1;
	Uint8  data2;
} midi_node_t;

int
list_less_func_midi_node(const list_elm_t *e1, const list_elm_t *e2)
{
	return ((midi_node_t*)e1)->time < ((midi_node_t*)e2)->time;
}

static int
xmi_process_EVNT(char *data, int length, midi_file_t *midi)
{
	int balance = length;
	Uint64 time = 0;

	Uint32 unknown = 0;
	READ_DATA(unknown);
	
	while (balance) {
		Uint8 type = 0;
		READ_DATA(type);
		
		if (type & 0x80) {
			midi_node_t *node = malloc(sizeof(midi_node_t));
			node->time = time;
			node->type = type;
			switch (type & 0xF0) {
				case 0x80:
				case 0x90:
				case 0xA0:
				case 0xB0:
				case 0xE0: {
					READ_DATA(node->data1);
					READ_DATA(node->data2);
					if (0x90 == (type & 0xF0)) {
						Uint8 data1 = node->data1;
						list_insert_sorted(&midi->nodes, (list_elm_t*)node, list_less_func_midi_node);
						node = malloc(sizeof(midi_node_t));
						node->type = type;
						READ_DATA(type);
						node->time = type + time;
						node->data1 = data1;
						node->data2 = 0;
					}
					break;
				}
				case 0xC0:
				case 0xD0: {
					READ_DATA(node->data1);
					node->data2 = 0;
					break;
				}
				case 0xF0: {
					free(node);
					node = NULL;
					if (0xFF == type) {
						Uint8 length = 0;
						READ_DATA(type);
						READ_DATA(length);
						if (0x51 == type) {
							Uint32 tempo = 0;
							for (int i = 0; i < length; i++) {
								tempo = tempo << 8;
								Uint8 byte = 0;
								READ_DATA(byte);
								tempo |= byte;
							}
							if (0 == midi->tempo) {
								midi->tempo = tempo;
							}
						}
						else {
							data += length;
							balance -= length;
							LOGW("Ignored extended node 0x%X", type);
						}
					}
					break;
				}
			}
			if (NULL != node) {
				list_insert_sorted(&midi->nodes, (list_elm_t*)node, list_less_func_midi_node);
			}
		}
		else {
			time += type;
		}
	}

	return length;
}

static void
midi_grow(midi_file_t *midi, Uint8 **current)
{
	Uint8 *data = malloc(midi->size + 1024);
	Uint64 pos = *current - midi->data;
	if (midi->data) {
		memcpy(data, midi->data, midi->size);
		free(midi->data);
	}
	*current = data + pos;
	midi->data = data;
	midi->size += 1024;
}

#define WRITE_DATA(X) {if(midi->size <= (current-midi->data) + sizeof(X)) midi_grow(midi,&current); memcpy(current, &X, sizeof(X)); current+=sizeof(X);};
#define WRITE_BE32(X) {Uint32 val = X; val = htobe32(val); WRITE_DATA(val);}
#define WRITE_LE32(X) {Uint32 val = X; val = htole32(val); WRITE_DATA(val);}
#define WRITE_BE16(X) {Uint16 val = X; val = htobe16(val); WRITE_DATA(val);}
#define WRITE_BYTE(X) {if(midi->size <= (current-midi->data) + sizeof(X)) midi_grow(midi,&current); *current = (Uint8)X; current++;}

static Uint32
midi_write_variable_size(midi_file_t *midi, Uint8 **current, Uint64 val)
{
	Uint32 count = 1;
	Uint32 buf = val & 0x7F;
	for (; val >>= 7; ++count) {
		buf = (buf << 8) | 0x80 | (val & 0x7F);
	}
	if (midi->size <= (*current-midi->data) + count) {
		midi_grow(midi,current);	
	}
	for (int i = 0; i < count; ++i) {
		**current = (Uint8)(buf & 0xFF);
		(*current)++;
		buf >>= 8;
	}
  return count;
}

static void*
midi_produce(midi_file_t *midi, size_t *size)
{
	Uint8 *current = midi->data;

	// Header
	WRITE_BE32(0x4D546864);		// 'MThd'
	WRITE_BE32(6);						// Header size
	WRITE_BE16(0);						// File type
	WRITE_BE16(1);						// Track count
	WRITE_BE16(midi->tempo * 3 / 25000);	// Time division

	// First track
	WRITE_BE32(0x4D54726B);		// 'MTrk'
	Uint64 size_pos = current - midi->data;
	WRITE_BE32(0);					// Size reserved
	WRITE_BYTE(0);					// Time
	WRITE_BYTE(0xFF);				// Type
	WRITE_BYTE(0x51);				// Extended type
	WRITE_BE32(midi->tempo); *(current-4) = 3; // Tempo length and value

	list_elm_t *elm;
	Uint64 time = 0;
	int i = 0;
	list_foreach(&midi->nodes, elm) {
		i++;
		midi_node_t *node = (midi_node_t*)elm;
		midi_write_variable_size(midi, &current, node->time - time);
		time = node->time;
		WRITE_BYTE(node->type);
		WRITE_BYTE(node->data1);
		if (((node->type & 0xF0) != 0xC0) && ((node->type & 0xF0) != 0xD0)){
			WRITE_BYTE(node->data2);
		}
	}

	WRITE_BYTE(0);					// Time
	WRITE_BYTE(0xFF);				// Type
	WRITE_BYTE(0x2F);				// Extended type
	WRITE_BYTE(0x00);				// Length
	*size = (Uint32)(current - midi->data);
	Uint32 data_size = (Uint32)(*size - size_pos - 4);
	current = midi->data + size_pos;
	WRITE_BE32(data_size);	// Write correct size

	return midi->data;
}

void
midi_play_track(midi_t midi)
{
	if (0 == midi_enabled) {
		return;
	}

	if (0 == initialized) {
		sfx_init();
		if (0 == initialized) {
			return;
		}
	}

	size_t size = 0;
	char *data = gfx_get_data_object(DATA_MUSIC_GAME + midi, &size);
	if (NULL == data) {
		return;
	}

	midi_file_t midi_file;
	list_init(&midi_file.nodes);
	midi_file.tempo = 0;
	midi_file.data = NULL;
	midi_file.size = 0;

	xmi_process_subchunks(data, (int)size, &midi_file);
	data = midi_produce(&midi_file, &size);

	SDL_RWops *rw = SDL_RWFromMem(data, (int)size);
	Mix_Music *music = Mix_LoadMUS_RW(rw);
	if (NULL == music) {
		return;
	}
	int r = Mix_PlayMusic(music, 0);
	if (r < 0) {
		LOGE("Could not play MIDI track: %s\n", Mix_GetError());
		return;
	}

	return;
}

void
midi_enable(int enable)
{
	midi_enabled = enable;
	if (0 != enable) {
		midi_start_play_randomly();
	}
	else {
		Mix_HaltMusic();
	}
}

int
midi_is_enabled()
{
	return midi_enabled;
}

void
midi_start_play_randomly()
{
	srand((unsigned int)time(NULL));
	int track = rand() % 5;
	track = (track == 5) ? 3 : track;

	midi_play_track(track);
}

static void
midi_track_finished()
{
	if (midi_enabled) {
		midi_start_play_randomly();
	}
}
