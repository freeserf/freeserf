/*
 * savegame.h - Loading and saving of save games
 *
 * Copyright (C) 2013-2018  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <vector>
#include <memory>

#include "src/map.h"
#include "src/resource.h"
#include "src/building.h"
#include "src/serf.h"
#include "src/debug.h"

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
  std::vector<SaveReaderTextValue> parts;

 public:
  explicit SaveReaderTextValue(const std::string &value);

  const SaveReaderTextValue& operator >> (int &val) const;
  const SaveReaderTextValue& operator >> (unsigned int &val) const;
#if (defined(_M_AMD64) || defined(__x86_64__)) || !defined(WIN32)
  const SaveReaderTextValue& operator >> (size_t &val) const;
#endif  // (defined(_M_AMD64) || defined(__x86_64__)) || !defined(WIN32)
  const SaveReaderTextValue& operator >> (Direction &val) const;
  const SaveReaderTextValue& operator >> (Resource::Type &val) const;
  const SaveReaderTextValue& operator >> (Building::Type &val) const;
  const SaveReaderTextValue& operator >> (Serf::State &val) const;
  const SaveReaderTextValue& operator >> (uint16_t &val) const;
  const SaveReaderTextValue& operator >> (std::string &val) const;
  const SaveReaderTextValue& operator[] (size_t pos) const;
};

class SaveWriterTextValue {
 protected:
  std::string value;

 public:
  SaveWriterTextValue() {}

  SaveWriterTextValue& operator << (int val);
  SaveWriterTextValue& operator << (unsigned int val);
#if (defined(_M_AMD64) || defined(__x86_64__)) || !defined(WIN32)
  SaveWriterTextValue& operator << (size_t val);
#endif  // (defined(_M_AMD64) || defined(__x86_64__)) || !defined(WIN32)
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
  virtual const SaveReaderTextValue &value(const std::string &name) const = 0;
  virtual Readers get_sections(const std::string &name) = 0;
};

class SaveWriterText {
 public:
  virtual ~SaveWriterText() = default;
  virtual SaveWriterTextValue &value(const std::string &name) = 0;
  virtual SaveWriterText &add_section(const std::string &name,
                                      unsigned int number) = 0;
};

class GameStore {
 public:
  class SaveInfo {
   public:
    typedef enum Type {
      Legacy,
      Regular
    } Type;

    std::string name;
    std::string path;
    Type type;
  };

 protected:
  GameStore();

  std::string folder_path;
  std::vector<SaveInfo> saved_games;

 public:
  virtual ~GameStore();

  static GameStore &get_instance();

  std::string get_folder_path() const { return folder_path; }
  bool create_folder(const std::string &path);
  bool is_folder_exists(const std::string &path);
  const std::vector<SaveInfo> &get_saved_games();

  /* Generic save/load function that will try to detect the right
   format on load and save to the best format on write. */
  bool save(const std::string &path, Game *game);
  bool load(const std::string &path, Game *game);
  bool quick_save(const std::string &prefix, Game *game);

  bool read(std::istream *is, Game *game);
  bool write(std::ostream *os, Game *game);

 protected:
  void update();
  void find_legacy();
  void find_regular();
  std::string name_from_file(const std::string &file_name);
};

#endif  // SRC_SAVEGAME_H_
