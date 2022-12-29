/*
 * configfile.h - Configuration file read/write
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

#ifndef SRC_CONFIGFILE_H_
#define SRC_CONFIGFILE_H_

#include <string>
#include <map>
#include <sstream>
#include <memory>
#include <list>

#include "src/log.h"  //for log ging debug

class ConfigFile {
 protected:
  typedef std::map<std::string, std::string> Values;
  typedef std::shared_ptr<Values> PValues;
  typedef std::map<std::string, PValues> Sections;
  Sections data;

 public:
  ConfigFile();
  virtual ~ConfigFile();

  bool load(const std::string &path);
  bool save(const std::string &path);

  bool read(std::istream *is);
  bool write(std::ostream *os);

  std::list<std::string> get_sections() const;
  std::list<std::string> get_values(const std::string &section) const;

  bool contains(const std::string &section, const std::string &name) const {
    Log::Debug["configfile.h"] << "inside ConfigFile::contains";
    if (data.find(section) == data.end()) {
      Log::Debug["configfile.h"] << "inside ConfigFile::contains, didn't find section " << section;
      return false;
    }
    PValues values = data.at(section);
    if (values->find(name) == values->end()) {
      Log::Debug["configfile.h"] << "inside ConfigFile::contains, didn't find value name " << name;
      return false;
    }
    Log::Debug["configfile.h"] << "inside ConfigFile::contains, found section & value";
    return true;
  }

  std::string value(const std::string &section, const std::string &name,
                    const std::string &def_val) const {
    Log::Debug["configfile.h"] << "inside ConfigFile::value";
    if (!contains(section, name)) {
      return def_val;
    }
    return data.at(section)->at(name);
  }

  template <typename T> T value(const std::string &section,
                                const std::string &name,
                                const T &def_val) const {
    if (!contains(section, name)) {
      return def_val;
    }
    std::stringstream s(data.at(section)->at(name));
    T val;
    s >> val;
    return val;
  }

  template <typename T> void set_value(const std::string &section,
                                       const std::string &name,
                                       const T &value) {
    if (data.find(section) == data.end()) {
      PValues new_section = std::make_shared<Values>();
      data[section] = new_section;
    }

    std::stringstream str;
    str << value;

    (*data[section])[name] = str.str();
  }

 protected:
  std::string trim(const std::string &str);
};

typedef std::shared_ptr<ConfigFile> PConfigFile;

#endif  // SRC_CONFIGFILE_H_
