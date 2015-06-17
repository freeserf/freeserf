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

#include <cstdio>
#include <string>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

#include "src/map.h"
#include "src/resource.h"

/* Original game format */
int load_v0_state(FILE *f);

/* Text format */
int save_text_state(FILE *f);
int load_text_state(FILE *f);

/* Generic save/load function that will try to detect the right
   format on load and save to the best format on write. */
int save_state(const char *path);
int load_state(const char *path);

int save_game(int autosave);

class save_reader_binary_t {
 protected:
  uint8_t *start;
  uint8_t *current;
  uint8_t *end;

 public:
  save_reader_binary_t(const save_reader_binary_t &reader);
  save_reader_binary_t(void *data, size_t size);

  save_reader_binary_t& operator >> (uint8_t &val);
  save_reader_binary_t& operator >> (uint16_t &val);
  save_reader_binary_t& operator >> (uint32_t &val);
  save_reader_binary_t& operator = (const save_reader_binary_t& other);

  void reset() { current = start; }
  void skip(size_t count) { current += count; }
  save_reader_binary_t extract(size_t size);
  uint8_t *read(size_t size);
};

class save_reader_text_value_t {
 protected:
  std::string value;

 public:
  explicit save_reader_text_value_t(std::string value);

  save_reader_text_value_t& operator >> (int &val);
  save_reader_text_value_t& operator >> (unsigned int &val);
  save_reader_text_value_t& operator >> (dir_t &val);
  save_reader_text_value_t& operator >> (resource_type_t &val);
  save_reader_text_value_t& operator >> (uint16_t &val);
  save_reader_text_value_t& operator >> (std::string &val);
  save_reader_text_value_t operator[] (size_t pos);
};

class save_writer_text_value_t {
 protected:
  std::string value;

 public:
  save_writer_text_value_t() {}

  save_writer_text_value_t& operator << (int val);
  save_writer_text_value_t& operator << (unsigned int val);
  save_writer_text_value_t& operator << (dir_t val);
  save_writer_text_value_t& operator << (resource_type_t val);
  save_writer_text_value_t& operator << (const std::string &val);

  std::string &get_value() { return value; }
};

class save_reader_text_t {
 public:
  virtual save_reader_text_value_t value(std::string name) const = 0;
};

class save_writer_text_t {
 public:
  virtual save_writer_text_value_t &value(std::string name) = 0;
};

#endif  // SRC_SAVEGAME_H_
