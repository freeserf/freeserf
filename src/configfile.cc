/*
 * configfile.cc - Configuration file read/write
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/configfile.h"

#include <sstream>
#include <vector>
#include <map>
#include <fstream>
#include <functional>
#include <memory>
#include <algorithm>
#include <cctype>

#include "src/log.h"

ConfigFile::ConfigFile() {
}

ConfigFile::~ConfigFile() {
}

std::list<std::string>
ConfigFile::get_sections() const {
  std::list<std::string> keys;
  for (auto pair : data) {
    keys.push_back(pair.first);
  }
  return keys;
}

std::list<std::string>
ConfigFile::get_values(const std::string &section) const {
  std::list<std::string> keys;
  Sections::const_iterator sect = data.find(section);
  if (sect == data.end()) {
    return keys;
  }
  for (auto pair : *((*sect).second)) {
    keys.push_back(pair.first);
  }
  return keys;
}

bool
ConfigFile::load(const std::string &path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    Log::Error["config"] << "Failed to open config file '" << path << "'";
    return false;
  }

  return read(&file);
}

bool
ConfigFile::save(const std::string &path) {
  std::ofstream file(path, std::ios_base::trunc);
  if (!file.is_open()) {
    Log::Error["config"] << "Failed to open config file '" << path << "'";
    return false;
  }

  return write(&file);
}

std::string
ConfigFile::trim(std::string str) {
  str.erase(str.begin(),
            std::find_if(str.begin(), str.end(),
                         std::not1(std::ptr_fun<int, int>(std::isspace))));
  str.erase(std::find_if(str.rbegin(), str.rend(),
                        std::not1(std::ptr_fun<int, int>(std::isspace))).base(),
            str.end());

  return str;
}

bool
ConfigFile::read(std::istream *is) {
  PValues section = std::make_shared<Values>();
  data["global"] = section;
  size_t line_number = 0;

  while (!is->eof()) {
    std::string line;
    getline(*is, line);
    line_number++;
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line[0] == '[') {
      size_t end = line.rfind(']');
      if (end == std::string::npos) {
        Log::Error["config"] << "Failed to parse config file ("
                             << line_number << ")";
        return false;
      }
      std::string name = line.substr(1, end-1);
      if (name.empty()) {
        Log::Error["config"] << "Failed to parse config file ("
                             << line_number << ")";
        return false;
      }
      std::transform(name.begin(), name.end(), name.begin(), ::tolower);
      section = std::make_shared<Values>();
      data[name] = section;
    } else if (line[0] == ';' || line[0] == '#') {
      // it's a comment line. drop it for now.
    } else {
      if (line.length() != 0) {
        size_t pos = line.find('=');
        std::string name = line.substr(0, pos);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        std::string val = line.substr(pos+1, std::string::npos);
        std::transform(val.begin(), val.end(), val.begin(), ::tolower);
        (*section)[trim(name)] = trim(val);
      }
    }
  }

  return true;
}

bool
ConfigFile::write(std::ostream *os) {
  for (auto &section : data) {
    *os << "[" << section.first << "]\n";
    for (auto values : *(section.second)) {
      *os << "  " << values.first << " = " << values.second << "\n";
    }
  }

  return true;
}
