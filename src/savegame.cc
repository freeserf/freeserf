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
#include <iostream>

#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"

/* Load a save game. */
bool
load_v0_state(std::istream *is) {
  std::istreambuf_iterator<char> is_start(*is), is_end;
  std::vector<char> data(is_start, is_end);

  SaveReaderBinary reader(&data[0], 8628);
  Game *game = new Game();
  reader >> *game;

  return true;
}


class SaveWriterTextSection : public SaveWriterText {
 protected:
  typedef std::map<std::string, SaveWriterTextValue> values_t;
  typedef std::vector<SaveWriterTextSection*> sections_t;

 protected:
  std::string name;
  unsigned int number;
  values_t values;
  sections_t sections;

 public:
  SaveWriterTextSection(std::string name, unsigned int number) {
    this->name = name;
    this->number = number;
  }

  virtual SaveWriterTextValue &value(const std::string &name) {
    values_t::iterator i = values.find(name);
    if (i != values.end()) {
      return i->second;
    }

    values[name] = SaveWriterTextValue();
    i = values.find(name);
    return i->second;
  }

  bool save(std::ostream *os) {
    *os << "[" << name << " " << number << "]\n";

    for (values_t::iterator i = values.begin(); i != values.end(); ++i) {
      *os << i->first << "=" << i->second.get_value() << "\n";
    }

    *os << "\n";

    for (sections_t::iterator i = sections.begin(); i != sections.end(); ++i) {
      (*i)->save(os);
    }

    return true;
  }

  SaveWriterText &add_section(const std::string &name, unsigned int number) {
    SaveWriterTextSection *new_section =
                                   new SaveWriterTextSection(name, number);

    sections.push_back(new_section);

    return *new_section;
  }
};

bool
save_text_state(std::ostream *os, Game *game) {
  SaveWriterTextSection writer("game", 0);

  writer << *game;

  return writer.save(os);
}

class SaveReaderTextSection : public SaveReaderText {
 protected:
  typedef std::map<std::string, std::string> values_t;
  std::string name;
  int number;
  values_t values;
  Readers readers_stub;

 public:
  explicit SaveReaderTextSection(const std::string &header) {
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

  virtual SaveReaderTextValue
  value(const std::string &name) const throw(ExceptionFreeserf) {
    values_t::const_iterator it = values.find(name);
    if (it == values.end()) {
      throw ExceptionFreeserf("failed to load value");
    }

    return SaveReaderTextValue(it->second);
  }

  virtual Readers get_sections(const std::string &name) {
    throw ExceptionFreeserf("Recursive sections are not allowed");
    return readers_stub;
  }

  void add_value(std::string line) {
    size_t pos = line.find('=');
    if (pos == std::string::npos) {
      throw ExceptionFreeserf("Wrong save file format");
    }
    std::string name = line.substr(0, pos);
    std::string value = line.substr(pos + 1, line.length() - pos - 1);
    values[name] = value;
  }
};

typedef std::list<SaveReaderTextSection*> reader_sections_t;

class SaveReaderTextFile : public SaveReaderText {
 protected:
  reader_sections_t sections;

 public:
  explicit SaveReaderTextFile(std::istream *file) {
    SaveReaderTextSection *section =
                                       new SaveReaderTextSection("[main]");
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
        section = new SaveReaderTextSection(line);
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

  virtual SaveReaderTextValue
  value(const std::string &name) const throw(ExceptionFreeserf) {
    reader_sections_t result;

    reader_sections_t::const_iterator it = sections.begin();
    for (; it != sections.end(); ++it) {
      if ((*it)->get_name() == "main") {
        return (*it)->value(name);
      }
    }

    throw ExceptionFreeserf("Value \"" + name + "\" not found");

    return SaveReaderTextValue("");
  }

  virtual Readers get_sections(const std::string &name) {
    Readers result;

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
load_text_state(std::istream *is, Game *game) {
  try {
    SaveReaderTextFile reader_text(is);
    reader_text >> *game;
  } catch (...) {
    return false;
  }

  return true;
}

bool
load_state(const std::string &path, Game *game) {
  std::ifstream file;
  file.open(path.c_str());

  if (!file.is_open()) {
    Log::Error["savegame"] << "Unable to open save game file: '" << path << "'";
    return false;
  }

  try {
    SaveReaderTextFile reader_text(&file);
    reader_text >> *game;
    file.close();
  } catch (...) {
    file.close();
    Log::Warn["savegame"] << "Unable to load save game, "
                          << "trying compatability mode...";
    std::ifstream input(path.c_str(), std::ios::binary);
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)),
                             (std::istreambuf_iterator<char>()));
    SaveReaderBinary reader(&buffer[0], buffer.size());
    try {
      reader >> *game;
    } catch (...) {
      Log::Error["savegame"] << "Failed to load save game.";
      return false;
    }
  }

  return true;
}

bool
save_state(const std::string &path, Game *game) {
  std::ofstream os(path.c_str(), std::ofstream::binary);
  if (!os.is_open()) return false;

  bool r = save_text_state(&os, game);
  os.close();

  return r;
}

SaveReaderBinary::SaveReaderBinary(const SaveReaderBinary &reader) {
  start = reader.start;
  current = reader.current;
  end = reader.end;
}

SaveReaderBinary::SaveReaderBinary(void *data, size_t size) {
  start = current = reinterpret_cast<uint8_t*>(data);
  end = start + size;
}

SaveReaderBinary&
SaveReaderBinary::operator >> (uint8_t &val) {
  val = *current;
  current++;
  return *this;
}

SaveReaderBinary&
SaveReaderBinary::operator >> (uint16_t &val) {
  val = *reinterpret_cast<uint16_t*>(current);
  current += 2;
  return *this;
}

SaveReaderBinary&
SaveReaderBinary::operator >> (uint32_t &val) {
  val = *reinterpret_cast<uint32_t*>(current);
  current += 4;
  return *this;
}

SaveReaderBinary&
SaveReaderBinary::operator = (const SaveReaderBinary& other) {
  start = other.start;
  current = other.current;
  end = other.end;
  return *this;
}

SaveReaderBinary
SaveReaderBinary::extract(size_t size) {
  SaveReaderBinary new_reader(current, size);
  current += size;
  return new_reader;
}

uint8_t *
SaveReaderBinary::read(size_t size) {
  if (current + size > end) {
    return NULL;
  }
  uint8_t *data = current;
  current += size;
  return data;
}

SaveReaderTextValue::SaveReaderTextValue(std::string value) {
  this->value = value;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (unsigned int &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

#if defined(_M_AMD64) || defined(__x86_64__)
SaveReaderTextValue&
SaveReaderTextValue::operator >> (size_t &val) {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}
#endif  // defined(_M_AMD64) || defined(__x86_64__)

SaveReaderTextValue&
SaveReaderTextValue::operator >> (Direction &val) {
  int result = atoi(value.c_str());
  val = (Direction)result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (Resource::Type &val) {
  int result = atoi(value.c_str());
  val = (Resource::Type)result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (Building::Type &val) {
  int result = atoi(value.c_str());
  val = (Building::Type)result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (Serf::State &val) {
  int result = atoi(value.c_str());
  val = (Serf::State)result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (uint16_t &val) {
  int result = atoi(value.c_str());
  val = (uint16_t)result;

  return *this;
}

SaveReaderTextValue&
SaveReaderTextValue::operator >> (std::string &val) {
  val = value;
  return *this;
}

SaveReaderTextValue
SaveReaderTextValue::operator[] (size_t pos) {
  std::vector<std::string> parts;
  std::istringstream iss(value);
  std::string item;
  while (std::getline(iss, item, ',')) {
    parts.push_back(item);
  }

  if (pos >= parts.size()) {
    return SaveReaderTextValue("");
  }

  return SaveReaderTextValue(parts[pos]);
}

SaveWriterTextValue&
SaveWriterTextValue::operator << (int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

SaveWriterTextValue&
SaveWriterTextValue::operator << (unsigned int val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}

#if defined(_M_AMD64) || defined(__x86_64__)
SaveWriterTextValue&
SaveWriterTextValue::operator << (size_t val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << val;
  value += ss.str();

  return *this;
}
#endif  // defined(_M_AMD64) || defined(__x86_64__)

SaveWriterTextValue&
SaveWriterTextValue::operator << (Direction val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}

SaveWriterTextValue&
SaveWriterTextValue::operator << (Resource::Type val) {
  if (!value.empty()) {
    value += ",";
  }

  std::ostringstream ss;
  ss << static_cast<int>(val);
  value += ss.str();

  return *this;
}

SaveWriterTextValue&
SaveWriterTextValue::operator << (const std::string &val) {
  if (!value.empty()) {
    value += ",";
  }

  value += val;

  return *this;
}
