/*
 * savegame.cc - Loading and saving of save games
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

#include "src/savegame.h"

#include <sstream>
#include <vector>
#include <map>
#include <fstream>

#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"

/* Load a save game. */
bool
load_v0_state(FILE *f) {
  if (fseek(f, 0, SEEK_END) != 0) {
    return false;
  }
  int last = static_cast<int>(ftell(f));
  if (last == -1) {
    return false;
  }
  size_t size = last;
  if (fseek(f, 0, SEEK_SET) != 0) {
    return false;
  }
  uint8_t *data = new uint8_t[size];
  if (fread(data, 1, size, f) != size) {
    return false;
  }

  save_reader_binary_t reader(data, 8628);
  game_t *game = new game_t(0);
  reader >> *game;

  delete[] data;

  return true;
}


class save_writer_text_section_t : public save_writer_text_t {
 protected:
  typedef std::map<std::string, save_writer_text_value_t> values_t;
  typedef std::vector<save_writer_text_section_t*> sections_t;

 protected:
  std::string name;
  unsigned int number;
  values_t values;
  sections_t sections;

 public:
  save_writer_text_section_t(std::string name, unsigned int number) {
    this->name = name;
    this->number = number;
  }

  virtual save_writer_text_value_t &value(const std::string &name) {
    values_t::iterator i = values.find(name);
    if (i != values.end()) {
      return i->second;
    }

    values[name] = save_writer_text_value_t();
    i = values.find(name);
    return i->second;
  }

  bool save(FILE *file) {
    fprintf(file, "[%s %i]\n", name.c_str(), number);

    for (values_t::iterator i = values.begin(); i != values.end(); ++i) {
      fprintf(file, "%s=%s\n", i->first.c_str(), i->second.get_value().c_str());
    }

    fprintf(file, "\n");

    for (sections_t::iterator i = sections.begin(); i != sections.end(); ++i) {
      (*i)->save(file);
    }

    return true;
  }

  save_writer_text_t &add_section(const std::string &name,
                                  unsigned int number) {
    save_writer_text_section_t *new_section =
                                   new save_writer_text_section_t(name, number);

    sections.push_back(new_section);

    return *new_section;
  }
};

bool
save_text_state(FILE *f, game_t *game) {
  save_writer_text_section_t writer("game", 0);

  writer << *game;

  return writer.save(f);
}

class save_reader_text_section_t : public save_reader_text_t {
 protected:
  typedef std::map<std::string, std::string> values_t;
  std::string name;
  int number;
  values_t values;
  readers_t readers_stub;

 public:
  explicit save_reader_text_section_t(const std::string &header) {
    name = header.substr(1, header.length() - 2);
    size_t pos = name.find(' ');
    if (pos != std::string::npos) {
      std::string value = name.substr(pos + 1, name.length() - pos - 1);
      name = name.substr(0, pos);
      std::stringstream ss;
      ss << value;
      ss >> number;
    }
  }

  virtual std::string get_name() const {
    return name;
  }

  virtual unsigned int get_number() const {
    return number;
  }

  virtual save_reader_text_value_t
  value(const std::string &name) const throw(Freeserf_Exception) {
    values_t::const_iterator it = values.find(name);
    if (it == values.end()) {
      throw Freeserf_Exception("failed to load value");
    }

    return save_reader_text_value_t(it->second);
  }

  virtual readers_t get_sections(const std::string &name) {
    throw Freeserf_Exception("Recursive sections are not allowed");
    return readers_stub;
  }

  void add_value(std::string line) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      throw Freeserf_Exception("Wrong save file format");
    }
    std::string name = line.substr(0, pos);
    std::string value = line.substr(pos + 1, line.length() - pos - 1);
    values[name] = value;
  }
};

typedef std::list<save_reader_text_section_t*> reader_sections_t;

class save_reader_text_file_t : public save_reader_text_t {
 protected:
  reader_sections_t sections;

 public:
  explicit save_reader_text_file_t(std::istream *file) {
    save_reader_text_section_t *section =
                                       new save_reader_text_section_t("[main]");
    sections.push_back(section);
    while (!file->eof()) {
      char c = file->peek();
      while (c == ' ' || c == '\t' || c == '\n') {
        file->get(c);
        c = file->peek();
      }

      std::string line;
      getline(*file, line);

      if (c == '[') {
        section = new save_reader_text_section_t(line);
        sections.push_back(section);
      } else {
        if (line.length() != 0) {
          section->add_value(line);
        }
      }
    }
  }

  virtual std::string get_name() const {
    return std::string();
  }

  virtual unsigned int get_number() const {
    return 0;
  }

  virtual save_reader_text_value_t
  value(const std::string &name) const throw(Freeserf_Exception) {
    reader_sections_t result;

    reader_sections_t::const_iterator it = sections.begin();
    for (; it != sections.end(); ++it) {
      if ((*it)->get_name() == "main") {
        return (*it)->value(name);
      }
    }

    throw Freeserf_Exception("Value \"" + name + "\" not found");

    return save_reader_text_value_t("");
  }

  virtual readers_t get_sections(const std::string &name) {
    readers_t result;

    reader_sections_t::const_iterator it = sections.begin();
    for (; it != sections.end(); ++it) {
      if ((*it)->get_name() == name) {
        result.push_back(*it);
      }
    }

    return result;
  }
};

bool
load_state(const std::string &path, game_t *game) {
  std::ifstream file;
  file.open(path.c_str());

  if (!file.is_open()) {
    LOGE("savegame", "Unable to open save game file: `%s'.", path.c_str());
    return false;
  }

  try {
    save_reader_text_file_t reader_text(&file);
    reader_text >> *game;
    file.close();
  } catch (...) {
    file.close();
    LOGW("savegame", "Unable to load save game, trying compatability mode...");
    std::ifstream input(path.c_str(), std::ios::binary);
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)),
                             (std::istreambuf_iterator<char>()));
    save_reader_binary_t reader(&buffer[0], buffer.size());
    try {
      reader >> *game;
    } catch (...) {
      LOGE("savegame", "Failed to load save game.");
      return false;
    }
  }

  return true;
}

bool
save_state(const std::string &path, game_t *game) {
  FILE *f = fopen(path.c_str(), "wb");
  if (f == NULL) return false;

  bool r = save_text_state(f, game);
  fclose(f);

  return r;
}

save_reader_binary_t::save_reader_binary_t(const save_reader_binary_t &reader) {
  start = reader.start;
  current = reader.current;
  end = reader.end;
}

save_reader_binary_t::save_reader_binary_t(void *data, size_t size) {
  start = current = reinterpret_cast<uint8_t*>(data);
  end = start + size;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint8_t &val) {
  val = *current;
  current++;
  return *this;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint16_t &val) {
  val = *reinterpret_cast<uint16_t*>(current);
  current += 2;
  return *this;
}

save_reader_binary_t& save_reader_binary_t::operator >> (uint32_t &val) {
  val = *reinterpret_cast<uint32_t*>(current);
  current += 4;
  return *this;
}

save_reader_binary_t&
save_reader_binary_t::operator = (const save_reader_binary_t& other) {
  start = other.start;
  current = other.current;
  end = other.end;
  return *this;
}

save_reader_binary_t
save_reader_binary_t::extract(size_t size) {
  save_reader_binary_t new_reader(current, size);
  current += size;
  return new_reader;
}

uint8_t *
save_reader_binary_t::read(size_t size) {
  if (current + size > end) {
    return NULL;
  }
  uint8_t *data = current;
  current += size;
  return data;
}

save_reader_text_value_t::save_reader_text_value_t(std::string value) {
  this->value = value;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (unsigned int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

#if defined(_M_AMD64) || defined(__x86_64__)
save_reader_text_value_t&
save_reader_text_value_t::operator >> (size_t &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}
#endif  // defined(_M_AMD64) || defined(__x86_64__)

save_reader_text_value_t&
save_reader_text_value_t::operator >> (dir_t &val) {
  int result = atoi(value.c_str());
  val = (dir_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (resource_type_t &val) {
  int result = atoi(value.c_str());
  val = (resource_type_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (building_type_t &val) {
  int result = atoi(value.c_str());
  val = (building_type_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (serf_state_t &val) {
  int result = atoi(value.c_str());
  val = (serf_state_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (uint16_t &val) {
  int result = atoi(value.c_str());
  val = (uint16_t)result;

  return *this;
}

save_reader_text_value_t&
save_reader_text_value_t::operator >> (std::string &val) {
  val = value;
  return *this;
}

save_reader_text_value_t
save_reader_text_value_t::operator[] (size_t pos) {
  std::vector<std::string> parts;
  std::istringstream iss(value);
  std::string item;
  while (std::getline(iss, item, ',')) {
    parts.push_back(item);
  }

  if (pos >= parts.size()) {
    return save_reader_text_value_t("");
  }

  return save_reader_text_value_t(parts[pos]);
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (unsigned int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

#if defined(_M_AMD64) || defined(__x86_64__)
save_writer_text_value_t&
save_writer_text_value_t::operator << (size_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}
#endif  // defined(_M_AMD64) || defined(__x86_64__)

save_writer_text_value_t&
save_writer_text_value_t::operator << (dir_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (resource_type_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}

save_writer_text_value_t&
save_writer_text_value_t::operator << (const std::string &val) {
  if (!value.empty()) {
    value += ",";
  }

  value += val;

  return *this;
}
