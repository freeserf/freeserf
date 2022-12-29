/*
 * xmi2mid.cc - XMI to MID converter.
 *
 * Copyright (C) 2015-2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include <queue>
#include <algorithm>
#include <memory>
#include <string>
#include <map>
//F#include <bitset> // for debugging midi, testing instrument changes

#include "src/freeserf_endian.h"
#include "src/log.h"
#include "src/game-options.h"  // for option_RandomizeInstruments

typedef struct MidiFile {
  std::vector<ConvertorXMI2MID::MidiNode> nodes;
  uint32_t tempo;
} MidiFile;

static size_t xmi_process_chunks(PBuffer buffer, MidiFile *midi);
static size_t xmi_process_INFO(PBuffer buffer, MidiFile *midi);
static size_t xmi_process_TIMB(PBuffer buffer, MidiFile *midi);
static size_t xmi_process_EVNT(PBuffer buffer, MidiFile *midi);
static size_t xmi_process_multi(PBuffer buffer, MidiFile *midi);

typedef size_t (*ChunkProcessor)(PBuffer, MidiFile*);
typedef std::map<std::string, ChunkProcessor> ChunkProcessors;
static ChunkProcessors xmi_processors = {
  { "FORM", xmi_process_multi  },
  { "INFO", xmi_process_INFO   },
  { "CAT ", xmi_process_multi  },
  { "TIMB", xmi_process_TIMB   },
  { "EVNT", xmi_process_EVNT   },
};

static size_t
xmi_process_chunk(PBuffer buffer, MidiFile *midi) {
  PBuffer id = buffer->pop(4);
  std::string name = *id;
  size_t size = buffer->pop<uint32_t>();
  Log::Verbose["xmi2mid"] << "Processing XMI chunk: " << name
                          << " (size: " << size << ")";

  ChunkProcessors::iterator it = xmi_processors.find(name);
  if (it == xmi_processors.end()) {
    buffer->pop(size);
    Log::Debug["xmi2mid"] << "Unknown XMI chunk: " << id.get();
  } else {
    PBuffer content = buffer->pop(size);
    it->second(content, midi);
  }

  return size + 8;
}

static size_t
xmi_process_chunks(PBuffer buffer, MidiFile *midi) {
  size_t length = buffer->get_size();

  while (buffer->get_size()) {
    size_t size = xmi_process_chunk(buffer, midi);
    buffer = buffer->get_tail(size);
  }

  return length;
}

static size_t
xmi_process_multi(PBuffer buffer, MidiFile *midi) {
  PBuffer type = buffer->pop(4);
  std::string name = *type.get();
  Log::Verbose["xmi2mid"] << "\tchunk subtype: " << name;
  xmi_process_chunks(buffer->get_tail(4), midi);
  return buffer->get_size();
}

static size_t
xmi_process_INFO(PBuffer buffer, MidiFile * /*midi*/) {
  if (buffer->get_size() != 2) {
    Log::Debug["xmi2mid"] << "\tInconsistent INFO block.";
  } else {
    uint16_t track_count = buffer->pop<uint16_t>();
    Log::Verbose["xmi2mid"] << "\tXMI contains " << track_count << " track(s)";
  }
  return buffer->get_size();
}

static size_t
xmi_process_TIMB(PBuffer buffer, MidiFile * /*midi*/) {
  buffer->set_endianess(Buffer::EndianessLittle);
  size_t count = buffer->pop<uint16_t>();
  if (count*2 + 2 != buffer->get_size()) {
    Log::Debug["xmi2mid"] << "\tInconsistent TIMB block.";
  } else {
    for (size_t i = 0; i < count; i++) {
      uint8_t patch = buffer->pop<uint8_t>();
      uint8_t bank = buffer->pop<uint8_t>();
      Log::Verbose["xmi2mid"] << "\tTIMB entry " << i << ": "
                              << static_cast<int>(patch) << ", "
                              << static_cast<int>(bank);
    }
  }
  return buffer->get_size();
}

// Decode variable length duration.
static uint64_t
midi_read_variable_size(PBuffer buffer) {
  uint64_t val = 0;

  uint8_t data = buffer->pop<uint8_t>();
  while (data & 0x80) {
    val = (val << 7) | (data & 0x7f);
    data = buffer->pop<uint8_t>();
  }
  val = (val << 7) | (data & 0x7f);

  return val;
}

static size_t
xmi_process_EVNT(PBuffer buffer, MidiFile *midi) {
  uint64_t time = 0;
  unsigned int time_index = 0;

  while (buffer->readable()) {
    uint8_t type = buffer->pop<uint8_t>();

    if (type & 0x80) {
      ConvertorXMI2MID::MidiNode node;
      node.time = time;
      node.index = time_index++;
      node.type = type;

      switch (type & 0xF0) {
        case 0x80:
        case 0x90:
        case 0xA0:
        case 0xB0:
        case 0xE0:
          node.data1 = buffer->pop<uint8_t>();
          node.data2 = buffer->pop<uint8_t>();
          if ((type & 0xF0) == 0x90) {
            uint8_t data1 = node.data1;
            midi->nodes.push_back(node);

            ConvertorXMI2MID::MidiNode subnode;
            subnode.type = type;
            uint64_t length = midi_read_variable_size(buffer);

            // Generate on note with velocity zero corresponding to off note.
            subnode.time = time + length;
            subnode.index = time_index++;
            subnode.data1 = data1;
            subnode.data2 = 0;
            node = subnode;
          }
          break;
        case 0xC0:   //https://www.cs.cmu.edu/~music/cmsip/readings/Standard-MIDI-file-format-updated.pdf
        case 0xD0:
          node.data1 = buffer->pop<uint8_t>();
          node.data2 = 0;
          if (option_RandomizeInstruments){
            // 0x3F is the binary inverse of 0xC0
            //uint8_t midi_instrument = node.data1 + 1;
            //Log::Debug["xmi2mid.cc"] << "inside xmi_process_EVNT() 0xC0/0xD0, node.type is " << std::bitset<16>(node.type) << ", mask " << std::bitset<16>(0x3F) << ", foo " << std::bitset<16>(node.type & 0x3F) << ", node.type int is " << int(node.type & 0x3F) << ", node.data1 int " << int(node.data1) << " node.data1 bin " << std::bitset<16>(node.data1);
            Log::Debug["xmi2mid.cc"] << "inside xmi_process_EVNT() 0xC0/0xD0, found instrument " << node.data1 + 1; 
            //if (node.data1 == 100){ // #49 String Ensemble 1
            //  node.data1 = 101;
            //}
            //node.data1 = 116;

            //uint8_t random_instrument = (rand() % static_cast<int>(80));
            // don't use or mess with instruments >80, they are weird and won't work well
            if (node.data1 < 81){
              node.data1 = rand() % static_cast<int>(80);
              Log::Debug["xmi2mid.cc"] << "inside xmi_process_EVNT() 0xC0/0xD0, changing instrument to " << node.data1 + 1;
            }

            // found instruments
            //  45 Tremolo Strings
            //  46 Pizzicato Strings
            //  49 String Ensemble 1
            //  61 French Horn
            //  70 English Horn
            //  71 Bassoon
            //  74 Flute
            //  89 Synth Pad 1 (new age)
            // 101 Synth FX 5 (brightness)
            // 117 Taiko Drum
            //
            // it seems there might be a few other instruments that aren't appearing here?  and changing node.data1 here doesn't affect them
            //
          }
          break;
        case 0xF0:
          if (type == 0xFF) {  // Meta message.
            node.data1 = buffer->pop<uint8_t>();
            node.data2 = buffer->pop<uint8_t>();
            node.buffer = buffer->pop(node.data2);
            if (node.data1 == 0x51) {
              if (midi->tempo == 0) {
                for (int i = 0; i < node.data2; i++) {
                  midi->tempo = midi->tempo << 8;
                  midi->tempo |= node.buffer->pop<uint8_t>();
                }
              }
            }
          }
          break;
      }

      //Log::Debug["xmi2mid.cc"] << "inside xmi_process_EVNT(), node.type is " << node.type;

      midi->nodes.push_back(node);
    } else {
      time += type;
    }
  }

  return buffer->get_size();
}

static void
midi_write_variable_size(uint64_t val, PMutableBuffer data) {
  uint32_t count = 1;
  uint32_t buf = val & 0x7F;
  for (; val >>= 7; ++count) {
    buf = (buf << 8) | 0x80 | (val & 0x7F);
  }
  for (uint32_t i = 0; i < count; ++i) {
    data->push<uint8_t>(buf & 0xFF);
    buf >>= 8;
  }
}

ConvertorXMI2MID::ConvertorXMI2MID(PBuffer _buffer) : Convertor(_buffer) {
  buffer->set_endianess(Buffer::EndianessBig);
}

PBuffer
ConvertorXMI2MID::convert() {
  MidiFile midi_file;
  midi_file.tempo = 0;

  xmi_process_chunks(buffer, &midi_file);

  result = std::make_shared<MutableBuffer>(Buffer::EndianessBig);

  PMutableBuffer header = std::make_shared<MutableBuffer>(Buffer::EndianessBig);
  header->push<int16_t>(0);                            // File type
  header->push<int16_t>(1);                            // Track count
  header->push<int16_t>(midi_file.tempo * 3 / 25000);  // Time division
  write_chunk("MThd", header);

  write_chunk("MTrk", create_track(&midi_file.nodes));

  return result;
}

void
ConvertorXMI2MID::write_chunk(const std::string &id, PBuffer data) {
  result->push(id);
  result->push<uint32_t>(static_cast<uint32_t>(data->get_size()));
  result->push(data);
}

PBuffer
ConvertorXMI2MID::create_track(MidiNodes *nodes) {
  class CompareByTime {
   public:
    // Return true if first midi node should come before second.
    bool operator()(const MidiNode &m1, const MidiNode &m2) const {
      if (m1.time != m2.time) {
        return m1.time > m2.time;
      }
      return false;
    }
  };

  // Sort the nodes.
  std::sort(nodes->begin(), nodes->end(), CompareByTime());

  PMutableBuffer data = std::make_shared<MutableBuffer>(Buffer::EndianessBig);
  uint64_t time = 0;
  while (!nodes->empty()) {
    MidiNode node = nodes->back();
    nodes->pop_back();

    midi_write_variable_size(node.time - time, data);
    time = node.time;
    data->push<uint8_t>(node.type);
    data->push<uint8_t>(node.data1);
    if (((node.type & 0xF0) != 0xC0) && ((node.type & 0xF0) != 0xD0)) {
      data->push<uint8_t>(node.data2);
      if (node.type == 0xFF) {
        if (node.data2 > 0) {
          data->push(node.buffer);
        }
      }
    }
  }

  return data;
}

