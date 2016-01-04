/*
 * xmi2mid.cc - XMI to MID converter.
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

#include "src/xmi2mid.h"

#include <cstring>
#include <queue>
#include <vector>

#include "src/freeserf_endian.h"
#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
END_EXT_C

/* Midi node. */
typedef struct {
  uint64_t time;
  unsigned int index;
  uint8_t type;
  uint8_t data1;
  uint8_t data2;
  char *buffer;
} midi_node_t;

typedef midi_node_t* midi_node_p;

class compare_by_time {
 public:
  /* Return true if first midi node should come before second. */
  bool operator()(const midi_node_p &m1, const midi_node_p &m2) const {
    if (m1->time != m2->time) {
      return m1->time > m2->time;
    }

    if (m1->index != m2->index) {
      return m1->index > m2->index;
    }

    return false;
  }
};

typedef struct {
  std::priority_queue<midi_node_t*, std::vector<midi_node_t*>,
                      compare_by_time> nodes;
  uint32_t tempo;
  uint8_t *data;
  uint64_t size;
} midi_file_t;

static size_t xmi_process_subchunks(char *data, size_t length,
                                    midi_file_t *midi);
static size_t xmi_process_INFO(char *data, size_t length, midi_file_t *midi);
static size_t xmi_process_TIMB(char *data, size_t length, midi_file_t *midi);
static size_t xmi_process_EVNT(char *data, size_t length, midi_file_t *midi);
static size_t xmi_process_multi(char *data, size_t length, midi_file_t *midi);
static size_t xmi_process_single(char *data, size_t length, midi_file_t *midi);

typedef struct {
  uint32_t name;
  size_t (*processor)(char *, size_t, midi_file_t*);
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

static size_t
xmi_process_multi(char *data, size_t /*length*/, midi_file_t *midi) {
  int size = be32toh(*reinterpret_cast<int*>(data));
  data += 4;
  xmi_process_subchunks(data, size, midi);
  return 4+size;
}

static size_t
xmi_process_single(char *data, size_t length, midi_file_t *midi) {
  size_t size = 0;
  int name = be32toh(*reinterpret_cast<int*>(data));
  uint32_t string_ = htobe32(name);

  LOGV("xmi2mid", "Processing XMI chunk: %.4s",
       reinterpret_cast<char*>(&string_));

  data += 4;
  int processed = 0;
  for (int i = 0; i < (sizeof(xmi_processors)/sizeof(chunk_info_t)); i++) {
    if (xmi_processors[i].name == (uint32_t)name) {
      if (NULL != xmi_processors[i].processor) {
        size = xmi_processors[i].processor(data, length-4, midi);
      }
      processed = 1;
      break;
    }
  }
  if (0 == processed) {
    LOGD("xmi2mid", "Unknown XMI chunk: %s (0x%X)", string_, name);
  }
  return size+4;
}

static size_t
xmi_process_subchunks(char *data, size_t length, midi_file_t *midi) {
  size_t balance = length;

  while (balance) {
    size_t size = xmi_process_single(data, balance, midi);
    data += size;
    balance -= size;
  }

  return length;
}

static size_t
xmi_process_INFO(char *data, size_t /*length*/, midi_file_t * /*midi*/) {
  uint32_t size = *reinterpret_cast<uint32_t*>(data);
  data += 4;
  size = be32toh(size);
  if (size != 2) {
    LOGD("xmi2mid", "\tInconsistent INFO block.");
  } else {
    uint16_t track_count = *reinterpret_cast<uint16_t*>(data);
    LOGV("xmi2mid", "\tXMI contains %d track(s)", track_count);
  }
  return size + 4;
}

static size_t
xmi_process_TIMB(char *data, size_t /*length*/, midi_file_t * /*midi*/) {
  uint32_t size = *reinterpret_cast<uint32_t*>(data);
  data += 4;
  size = be32toh(size);
  uint16_t count = *reinterpret_cast<uint16_t*>(data);
  data += 2;
  if (count*2 + 2 != size) {
    LOGD("xmi2mid", "\tInconsistent TIMB block.");
  } else {
    for (int i = 0; i < count; i++) {
      uint8_t num = *data++;
      uint8_t bank = *data++;
      LOGV("xmi2mid", "\tTIMB entry %02d: %d, %d", i, (int)num, (int)bank);
    }
  }
  return size + 4;
}

static size_t
xmi_process_EVNT(char *data, size_t length, midi_file_t *midi) {
#define READ_DATA(X) {X = *data; data += sizeof(X); balance -= sizeof(X); };

  size_t balance = length;
  uint64_t time = 0;
  unsigned int time_index = 0;

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
            midi->nodes.push(node);

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

      midi->nodes.push(node);
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

  uint64_t pos = *current - midi->data;
  if (midi->data) {
    memcpy(data, midi->data, (size_t)midi->size);
    free(midi->data);
  }
  *current = data + pos;
  midi->data = data;
  midi->size += 1024;
}

static uint32_t
midi_write_variable_size(midi_file_t *midi, uint8_t **current, uint64_t val) {
  uint32_t count = 1;
  uint32_t buf = val & 0x7F;
  for (; val >>= 7; ++count) {
    buf = (buf << 8) | 0x80 | (val & 0x7F);
  }
  if (midi->size <= (uint64_t)((*current-midi->data) + count)) {
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
#define WRITE_DATA(X) {if (midi->size <= (current-midi->data) + sizeof(X)) \
                       midi_grow(midi, &current); \
                       memcpy(current, &X, sizeof(X)); current+=sizeof(X);};
#define WRITE_BE32(X) {uint32_t val = X; val = htobe32(val); WRITE_DATA(val);}
#define WRITE_LE32(X) {uint32_t val = X; val = htole32(val); WRITE_DATA(val);}
#define WRITE_BE16(X) {uint16_t val = X; val = htobe16(val); WRITE_DATA(val);}
#define WRITE_BYTE(X) {if (midi->size <= (current-midi->data) + sizeof(X)) \
                       midi_grow(midi, &current); \
                       *current = (uint8_t)X; current++;}

  uint8_t *current = midi->data;

  /* Header */
  WRITE_BE32(0x4D546864);               /* 'MThd' */
  WRITE_BE32(6);                        /* Header size */
  WRITE_BE16(0);                        /* File type */
  WRITE_BE16(1);                        /* Track count */
  WRITE_BE16(midi->tempo * 3 / 25000);  /* Time division */

  /* First track */
  WRITE_BE32(0x4D54726B);    /* 'MTrk' */
  uint64_t size_pos = current - midi->data;
  WRITE_BE32(0);          /* Size reserved */

  uint64_t time = 0;
  int i = 0;
  while (!midi->nodes.empty()) {
    i++;
    midi_node_t *node = midi->nodes.top();
    midi->nodes.pop();
    midi_write_variable_size(midi, &current, node->time - time);
    time = node->time;
    WRITE_BYTE(node->type);
    WRITE_BYTE(node->data1);
    if (((node->type & 0xF0) != 0xC0) && ((node->type & 0xF0) != 0xD0)) {
      WRITE_BYTE(node->data2);
      if (node->type == 0xFF) {
        if (node->data2 > 0) {
          if (midi->size <= (uint64_t)((current-midi->data) + node->data2)) {
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

void *
xmi2mid(void *xmi, size_t xmi_size, size_t *mid_size) {
  midi_file_t midi_file;
  midi_file.tempo = 0;
  midi_file.data = NULL;
  midi_file.size = 0;

  xmi_process_subchunks(reinterpret_cast<char*>(xmi), xmi_size, &midi_file);
  void *mid = midi_produce(&midi_file, mid_size);

  return mid;
}
