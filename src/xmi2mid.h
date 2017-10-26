/*
 * xmi2mid.h - XMI to MID converter.
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

#ifndef SRC_XMI2MID_H_
#define SRC_XMI2MID_H_

#include <vector>
#include <string>

#include "src/convertor.h"

class ConvertorXMI2MID : public Convertor {
 public:
  typedef struct MidiNode {
    uint64_t time;
    unsigned int index;
    uint8_t type;
    uint8_t data1;
    uint8_t data2;
    PBuffer buffer;
  } MidiNode;

  typedef std::vector<MidiNode> MidiNodes;

 protected:
  PMutableBuffer result;

 public:
  explicit ConvertorXMI2MID(PBuffer buffer);

  virtual PBuffer convert();

 protected:
  void write_chunk(std::string id, PBuffer data);
  PBuffer create_track(MidiNodes *nodes);
};

#endif  // SRC_XMI2MID_H_
