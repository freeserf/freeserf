/*
 * savegame.h - Loading and saving of save games
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

#ifndef SRC_SAVEGAME_H_
#define SRC_SAVEGAME_H_

#include <iostream>
#include <string>
#include <list>
#include <cstdint>

#include "src/map.h"
#include "src/resource.h"
#include "src/building.h"
#include "src/serf.h"
#include "src/debug.h"

/* Original game format */
bool load_v0_state(std::istream *is, Game *game);

/* Text format */
bool save_text_state(std::ostream *os, Game *game);
bool load_text_state(std::istream *is, Game *game);

/* Generic save/load function that will try to detect the right
   format on load and save to the best format on write. */
bool save_state(const std::string &path, Game *game);
bool load_state(const std::string &path, Game *game);

bool save_game(int autosave, Game *game);

class SaveReaderBinary {
 protected:
  uint8_t *start;
  uint8_t *current;
  uint8_t *end;

 public:
  SaveReaderBinary(const SaveReaderBinary &reader);
  SaveReaderBinary(void *data, size_t size);

  SaveReaderBinary& operator >> (uint8_t &val);
  SaveReaderBinary& operator >> (uint16_t &val);
  SaveReaderBinary& operator >> (uint32_t &val);
  SaveReaderBinary& operator = (const SaveReaderBinary& other);

  void reset() { current = start; }
  void skip(size_t count) { current += count; }
  SaveReaderBinary extract(size_t size);
  uint8_t *read(size_t size);
  bool has_data_left(size_t size) const { return current + size <= end; }
};

class SaveReaderTextValue {
 protected:
  std::string value;

 public:
  explicit SaveReaderTextValue(std::string value);

  SaveReaderTextValue& operator >> (int &val);
  SaveReaderTextValue& operator >> (unsigned int &val);
#if defined(_M_AMD64) || defined(__x86_64__)
  SaveReaderTextValue& operator >> (size_t &val);
#endif  // defined(_M_AMD64) || defined(__x86_64__)
  SaveReaderTextValue& operator >> (Direction &val);
  SaveReaderTextValue& operator >> (Resource::Type &val);
  SaveReaderTextValue& operator >> (Building::Type &val);
  SaveReaderTextValue& operator >> (Serf::State &val);
  SaveReaderTextValue& operator >> (uint16_t &val);
  SaveReaderTextValue& operator >> (std::string &val);
  SaveReaderTextValue operator[] (size_t pos);
};

class SaveWriterTextValue {
 protected:
  std::string value;

 public:
  SaveWriterTextValue() {}

  SaveWriterTextValue& operator << (int val);
  SaveWriterTextValue& operator << (unsigned int val);
#if defined(_M_AMD64) || defined(__x86_64__)
  SaveWriterTextValue& operator << (size_t val);
#endif  // defined(_M_AMD64) || defined(__x86_64__)
  SaveWriterTextValue& operator << (Direction val);
  SaveWriterTextValue& operator << (Resource::Type val);
  SaveWriterTextValue& operator << (const std::string &val);

  std::string &get_value() { return value; }
};

class SaveReaderText;

typedef std::list<SaveReaderText*> Readers;

class SaveReaderText {
 public:
  virtual ~SaveReaderText() = default;
  virtual std::string get_name() const = 0;
  virtual unsigned int get_number() const = 0;
  virtual SaveReaderTextValue value(const std::string &name) const
            throw(ExceptionFreeserf) = 0;
  virtual Readers get_sections(const std::string &name) = 0;
};

class SaveWriterText {
 public:
  virtual ~SaveWriterText() = default;
  virtual SaveWriterTextValue &value(const std::string &name) = 0;
  virtual SaveWriterText &add_section(const std::string &name,
                                      unsigned int number) = 0;
};

#endif  // SRC_SAVEGAME_H_
