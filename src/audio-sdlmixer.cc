/*
 * audio-sdlmixer.cc - Music and sound effects playback using SDL_mixer.
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

#include "src/audio.h"

#include <cstring>
#include <cassert>

#include <SDL.h>
#include <SDL_mixer.h>

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/data.h"
  #include "src/log.h"
  #include "src/list.h"
  #include "src/pqueue.h"
  #include "src/freeserf_endian.h"
END_EXT_C


audio_t *audio_t::instance = NULL;

audio_t::audio_t() {
  LOGI("audio-sdlmixer", "Initializing audio driver `sdlmixer'.");

  int r = Mix_Init(0);
  if (r != 0) {
    LOGE("audio-sdlmixer", "Could not init SDL_mixer: %s.", Mix_GetError());
    assert(false);
  }

  r = Mix_OpenAudio(8000, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 512);
  if (r < 0) {
    LOGE("audio-sdlmixer", "Could not open audio device: %s.", Mix_GetError());
    assert(false);
  }

  r = Mix_AllocateChannels(16);
  if (r != 16) {
    LOGE("audio-sdlmixer", "Failed to allocate channels: %s.", Mix_GetError());
    assert(false);
  }
/*
  Mix_HookMusicFinished(midi_track_finished);
*/
  volume = 1.f;

  sfx_player = new sfx_player_t();
  midi_player = new midi_player_t();
}

audio_t::~audio_t() {
  if (sfx_player != NULL) {
    delete sfx_player;
    sfx_player = NULL;
  }

  if (midi_player != NULL) {
    delete midi_player;
    midi_player = NULL;
  }

  Mix_CloseAudio();
  Mix_Quit();
}

audio_t *
audio_t::get_instance() {
  if (instance == NULL) {
    instance = new audio_t();
  }
  return instance;
}

float
audio_t::get_volume() {
  return volume;
}

void
audio_t::set_volume(float volume) {
  this->volume = volume;

  if (midi_player != NULL) {
    audio_volume_controller_t *volume_controller =
                                           midi_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume_controller->get_volume() * volume);
    }
  }

  if (sfx_player != NULL) {
    audio_volume_controller_t *volume_controller =
                                            sfx_player->get_volume_controller();
    if (volume_controller != NULL) {
      volume_controller->set_volume(volume_controller->get_volume() * volume);
    }
  }
}

void
audio_t::volume_up() {
  float volume = get_volume();
  set_volume(volume + 0.1f);
}

void
audio_t::volume_down() {
  float volume = get_volume();
  set_volume(volume - 0.1f);
}

static char *
sfx_produce_wav(void* data, size_t size, size_t *new_size) {
#define WRITE_DATA_WG(X) { memcpy(current, &X, sizeof(X)); \
                           current+=sizeof(X); };
#define WRITE_BE32_WG(X) { uint32_t val = X; \
                           val = htobe32(val); \
                           WRITE_DATA_WG(val); }
#define WRITE_LE32_WG(X) { uint32_t val = X; \
                           val = htole32(val); \
                           WRITE_DATA_WG(val); }
#define WRITE_BE16_WG(X) { uint16_t val = X; \
                           val = htobe16(val); \
                           WRITE_DATA_WG(val); }
#define WRITE_LE16_WG(X) { uint16_t val = X; \
                           val = htole16(val); \
                           WRITE_DATA_WG(val); }
#define WRITE_BYTE_WG(X) { *current = (uint8_t)X; \
                           current++; }

  char *bytes = reinterpret_cast<char*>(data);

  *new_size = 44 + size*2;

  char *result = reinterpret_cast<char*>(malloc(*new_size));
  if (result == NULL) abort();

  char *current = result;

  /* WAVE header */
  WRITE_BE32_WG(0x52494646);  /* 'RIFF' */
  WRITE_LE32_WG((uint32_t)*new_size - 8);    /* Chunks size */
  WRITE_BE32_WG(0x57415645);  /* 'WAVE' */

  /* Subchunk #1 */
  WRITE_BE32_WG(0x666d7420);  /* 'fmt ' */
  WRITE_LE32_WG(16);          /* Subchunk size */
  WRITE_LE16_WG(1);            /* Format = PCM */
  WRITE_LE16_WG(1);            /* Chanels count */
  WRITE_LE32_WG(8000);        /* Rate */
  WRITE_LE32_WG(16000);        /* Byte rate */
  WRITE_LE16_WG(2);            /* Block align */
  WRITE_LE16_WG(16);            /* Bits per sample */

  /* Subchunk #2 */
  WRITE_BE32_WG(0x64617461);  /* 'data' */
  WRITE_LE32_WG(static_cast<uint32_t>(size*2));        /* Data size */
  for (uint32_t i = 0; i < size; i++) {
    int value = *(bytes + i);
    value = value - 0x20;
    WRITE_BE16_WG(value*0xFF);
  }

  return result;
}

audio_player_t::audio_player_t() {
  enabled = true;
}

audio_player_t::~audio_player_t() {
  while (track_cache.size()) {
    audio_track_t *track = track_cache.begin()->second;
    track_cache.erase(track_cache.begin());
    delete track;
  }
}

void
audio_player_t::play_track(int track_id) {
  if (!is_enabled()) {
    return;
  }

  audio_track_t *track = NULL;
  track_cache_t::iterator it = track_cache.find(track_id);
  if (it == track_cache.end()) {
    track = create_track(track_id);
    if (track != NULL) {
      track_cache[track_id] = track;
    }
  } else {
    track = it->second;
  }

  if (track != NULL) {
    track->play();
  }
}

audio_track_t *
sfx_player_t::create_track(int track_id) {
  size_t size = 0;
  void *data = data_get_object(DATA_SFX_BASE + track_id, &size);
  if (data == NULL) {
    return NULL;
  }

  void *wav = sfx_produce_wav(data, size, &size);
  if (wav == NULL) {
    return NULL;
  }

  SDL_RWops *rw = SDL_RWFromMem(wav, static_cast<int>(size));
  Mix_Chunk *chunk = Mix_LoadWAV_RW(rw, 0);
  free(wav);
  if (chunk == NULL) {
    LOGE("audio-sdlmixer", "Mix_LoadWAV_RW: %s.", Mix_GetError());
    return NULL;
  }

  return new sfx_track_t(chunk);
}

void
sfx_player_t::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
sfx_player_t::stop() {
  Mix_HaltChannel(-1);
}

float
sfx_player_t::get_volume() {
  return static_cast<float>(Mix_Volume(-1, -1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
sfx_player_t::set_volume(float volume) {
  volume = std::max(std::min(0.f, volume), 1.f);
  int mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_Volume(-1, mix_volume);
}

void
sfx_player_t::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
sfx_player_t::volume_down() {
  set_volume(get_volume() - 0.1f);
}

sfx_track_t::sfx_track_t(Mix_Chunk *chunk) {
  this->chunk = chunk;
}

sfx_track_t::~sfx_track_t() {
  Mix_FreeChunk(chunk);
}

void
sfx_track_t::play() {
  int r = Mix_PlayChannel(-1, chunk, 0);
  if (r < 0) {
    LOGE("audio-sdlmixer", "Could not play SFX clip: %s.", Mix_GetError());
  }
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
midi_node_less(const midi_node_t *m1, const midi_node_t *m2) {
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
  { 0x464F524D, xmi_process_multi  },  /* 'FORM' */
  { 0x58444952, xmi_process_single },  /* 'XDIR' */
  { 0x494E464F, xmi_process_INFO   },  /* 'INFO' */
  { 0x43415420, xmi_process_multi  },  /* 'CAT ' */
  { 0x584D4944, xmi_process_single },  /* 'XMID' */
  { 0x54494D42, xmi_process_TIMB   },  /* 'TIMB' */
  { 0x45564E54, xmi_process_EVNT   },  /* 'EVNT' */
};

static int
xmi_process_multi(char *data, int length, midi_file_t *midi) {
  int size = be32toh(*reinterpret_cast<int*>(data));
  data += 4;
  xmi_process_subchunks(data, size, midi);
  return 4+size;
}

static int
xmi_process_single(char *data, int length, midi_file_t *midi) {
  int size = 0;
  int name = be32toh(*reinterpret_cast<int*>(data));
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
xmi_process_subchunks(char *data, int length, midi_file_t *midi) {
  int balance = length;

  while (balance) {
    int size = xmi_process_single(data, balance, midi);
    data += size;
    balance -= size;
  }

  return length;
}

static int
xmi_process_INFO(char *data, int length, midi_file_t *midi) {
  uint32_t size = *reinterpret_cast<uint32_t*>(data);
  data += 4;
  size = be32toh(size);
  if (size != 2) {
    LOGD("audio-sdlmixer", "\tInconsistent INFO block.");
  } else {
    uint16_t track_count = *reinterpret_cast<uint16_t*>(data);
    LOGV("audio-sdlmixer", "\tXMI contains %d track(s)", track_count);
  }
  return size + 4;
}

static int
xmi_process_TIMB(char *data, int length, midi_file_t *midi) {
  uint32_t size = *reinterpret_cast<uint32_t*>(data);
  data += 4;
  size = be32toh(size);
  uint16_t count = *reinterpret_cast<uint16_t*>(data);
  data += 2;
  if (count*2 + 2 != size) {
    LOGD("audio-sdlmixer", "\tInconsistent TIMB block.");
  } else {
    for (int i = 0; i < count; i++) {
      int num = *data++;
      int bank = *data++;
      LOGV("audio-sdlmixer", "\tTIMB entry %02d: %d, %d", i, num, bank);
    }
  }
  return size + 4;
}

static int
xmi_process_EVNT(char *data, int length, midi_file_t *midi) {
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
      midi_node_t *node =
                    reinterpret_cast<midi_node_t*>(malloc(sizeof(midi_node_t)));
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

          node = reinterpret_cast<midi_node_t*>(malloc(sizeof(midi_node_t)));
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
midi_grow(midi_file_t *midi, uint8_t **current) {
  uint8_t *data =
             reinterpret_cast<uint8_t*>(malloc((size_t)(midi->size + 1024)));
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
midi_write_variable_size(midi_file_t *midi, uint8_t **current, Uint64 val) {
  uint32_t count = 1;
  uint32_t buf = val & 0x7F;
  for (; val >>= 7; ++count) {
    buf = (buf << 8) | 0x80 | (val & 0x7F);
  }
  if (midi->size <= (*current-midi->data) + count) {
    midi_grow(midi, current);
  }
  for (uint32_t i = 0; i < count; ++i) {
    **current = (uint8_t)(buf & 0xFF);
    (*current)++;
    buf >>= 8;
  }

  return count;
}

static void*
midi_produce(midi_file_t *midi, size_t *size) {
#define WRITE_DATA(X) { if (midi->size <= (current-midi->data) + sizeof(X)) \
                          midi_grow(midi, &current); \
                        memcpy(current, &X, sizeof(X)); \
                        current+=sizeof(X); };
#define WRITE_BE32(X) { uint32_t val = X; val = htobe32(val); WRITE_DATA(val);}
#define WRITE_LE32(X) { uint32_t val = X; val = htole32(val); WRITE_DATA(val);}
#define WRITE_BE16(X) { uint16_t val = X; val = htobe16(val); WRITE_DATA(val);}
#define WRITE_BYTE(X) { if (midi->size <= (current-midi->data) + sizeof(X)) \
                          midi_grow(midi, &current); \
                        *current = (uint8_t)X; \
                        current++; }

  uint8_t *current = midi->data;

  /* Header */
  WRITE_BE32(0x4D546864);    /* 'MThd' */
  WRITE_BE32(6);            /* Header size */
  WRITE_BE16(0);            /* File type */
  WRITE_BE16(1);            /* Track count */
  WRITE_BE16(midi->tempo * 3 / 25000);  /* Time division */

  /* First track */
  WRITE_BE32(0x4D54726B);    /* 'MTrk' */
  uint64_t size_pos = current - midi->data;
  WRITE_BE32(0);          /* Size reserved */

  uint64_t time = 0;
  int i = 0;
  while (!pqueue_is_empty(&midi->nodes)) {
    i++;
    midi_node_t *node =
                   reinterpret_cast<midi_node_t*>(pqueue_pop(&midi->nodes));
    midi_write_variable_size(midi, &current, node->time - time);
    time = node->time;
    WRITE_BYTE(node->type);
    WRITE_BYTE(node->data1);
    if (((node->type & 0xF0) != 0xC0) && ((node->type & 0xF0) != 0xD0)) {
      WRITE_BYTE(node->data2);
      if (node->type == 0xFF) {
        if (node->data2 > 0) {
          if (midi->size <= (current-midi->data) + node->data2) {
            midi_grow(midi, &current);
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
  WRITE_BE32(data_size);  /* Write correct size */

  return midi->data;
}

audio_track_t *
midi_player_t::create_track(int track_id) {
  size_t size = 0;
  void *data = data_get_object(DATA_MUSIC_GAME + track_id, &size);
  if (NULL == data) {
    return NULL;
  }

  midi_file_t midi_file;
  pqueue_init(&midi_file.nodes, 16*1024,
              reinterpret_cast<pqueue_less_func*>(midi_node_less));
  midi_file.tempo = 0;
  midi_file.data = NULL;
  midi_file.size = 0;

  xmi_process_subchunks(reinterpret_cast<char*>(data), static_cast<int>(size),
                        &midi_file);
  data = midi_produce(&midi_file, &size);

  pqueue_deinit(&midi_file.nodes);

  SDL_RWops *rw = SDL_RWFromMem(data, static_cast<int>(size));
  Mix_Music *music = Mix_LoadMUS_RW(rw, 0);
  free(data);
  if (music == NULL) {
    return NULL;
  }

  return new midi_track_t(music);
}

void
midi_player_t::enable(bool enable) {
  enabled = enable;
  if (!enabled) {
    stop();
  }
}

void
midi_player_t::stop() {
  Mix_HaltMusic();
}

float
midi_player_t::get_volume() {
  return static_cast<float>(Mix_VolumeMusic(-1)) /
         static_cast<float>(MIX_MAX_VOLUME);
}

void
midi_player_t::set_volume(float volume) {
  volume = std::max(std::min(0.f, volume), 1.f);
  int mix_volume = static_cast<float>(MIX_MAX_VOLUME) * volume;
  Mix_VolumeMusic(mix_volume);
}

void
midi_player_t::volume_up() {
  set_volume(get_volume() + 0.1f);
}

void
midi_player_t::volume_down() {
  set_volume(get_volume() - 0.1f);
}

/*
static void
midi_track_finished() {
  if (midi_enabled) {
    midi_play_track(current_track);
  }
}
*/

midi_track_t::midi_track_t(Mix_Music *chunk) {
  this->chunk = chunk;
}

midi_track_t::~midi_track_t() {
  Mix_FreeMusic(chunk);
}

void
midi_track_t::play() {
  int r = Mix_PlayMusic(chunk, 0);
  if (r < 0) {
    LOGW("audio-sdlmixer", "Could not play MIDI track: %s\n", Mix_GetError());
  }
}
