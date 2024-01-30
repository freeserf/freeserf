/*
 * savegame.cc - Loading and saving of save games
 *
 * Copyright (C) 2013-2017  Jon Lund Steffensen <jonlst@gmail.com>
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
#include <array>
#include <ctime>
#include <utility>
#include <algorithm>

#include "src/game.h"
#include "src/log.h"
#include "src/debug.h"
#include "src/configfile.h"

#ifdef _WIN32
#include <Windows.h>
#include <Knownfolders.h>
#include <Shlobj.h>
#include <direct.h>
#elif defined(SDL_PLATFORM_APPLE)
#include <dirent.h>
#include <sys/stat.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

class SaveWriterTextSection : public SaveWriterText {
 protected:
  typedef std::map<std::string, SaveWriterTextValue> Values;
  typedef std::vector<SaveWriterTextSection*> Sections;

 protected:
  std::string name;
  unsigned int number;
  Values values;
  Sections sections;

 public:
  SaveWriterTextSection(std::string name_, unsigned int number_)
    : name(name_)
    , number(number_) {
  }

  virtual ~SaveWriterTextSection() {
    for (auto section : sections) {
      delete section;
    }
  }

  virtual SaveWriterTextValue &value(const std::string &val_name) {
    Values::iterator i = values.find(val_name);
    if (i != values.end()) {
      return i->second;
    }

    values[val_name] = SaveWriterTextValue();
    return values[val_name];
  }

  bool save(const std::string &path) {
    ConfigFile file;
    save(&file);
    return file.save(path);
  }

  bool write(std::ostream *os) {
    ConfigFile file;
    save(&file);
    return file.write(os);
  }


  SaveWriterText &add_section(const std::string &sub_name,
                              unsigned int sub_number) {
    SaveWriterTextSection *section = new SaveWriterTextSection(sub_name,
                                                               sub_number);
    sections.push_back(section);
    return *section;
  }

 protected:
  bool save(ConfigFile *file) {
    std::stringstream str;
    str << name << " " << number;

    for (auto value : values) {
      file->set_value(str.str(), value.first, value.second.get_value());
    }

    for (SaveWriterTextSection *writer : sections) {
      writer->save(file);
    }

    return true;
  }
};

typedef std::map<std::string, SaveReaderTextValue> Values;

class SaveReaderTextSection : public SaveReaderText {
 protected:
  std::string name;
  int number;
  Values values;
  Readers readers_stub;

 public:
  SaveReaderTextSection(ConfigFile *file, const std::string &_name)
    : name(_name)
    , number(0) {
    size_t pos = name.find(' ');
    if (pos != std::string::npos) {
      std::string value = name.substr(pos + 1, name.length() - pos - 1);
      name = name.substr(0, pos);
      std::stringstream ss;
      ss << value;
      ss >> number;
    }

    auto vals = file->get_values(_name);
    for (std::string vname : vals) {
      values.emplace(std::make_pair(vname,
                           SaveReaderTextValue(file->value(_name, vname, ""))));
    }
  }

  virtual std::string get_name() const {
    return name;
  }

  virtual unsigned int get_number() const {
    return number;
  }

  virtual const SaveReaderTextValue &
  value(const std::string &val_name) const {
    std::string v_name = val_name;
    std::transform(v_name.begin(), v_name.end(), v_name.begin(), ::tolower);
    Values::const_iterator it = values.find(v_name);
    if (it == values.end()) {
      std::ostringstream str;
      str << "Failed to load value: " << val_name;
      throw ExceptionFreeserf(str.str());
    }

    return it->second;
  }

  virtual Readers get_sections(const std::string &name) {
    throw ExceptionFreeserf("Recursive sections are not allowed");
    return readers_stub;
  }

  virtual bool has_value(const std::string &name) {
    std::string v_name = name;
    std::transform(v_name.begin(), v_name.end(), v_name.begin(), ::tolower);
    Values::const_iterator it = values.find(v_name);
    return (it != values.end());
  }
};

typedef std::list<SaveReaderTextSection*> ReaderSections;

class SaveReaderTextFile : public SaveReaderText {
 protected:
  ReaderSections sections;
  Values values;

 public:
  explicit SaveReaderTextFile(std::istream *is) {
    ConfigFile file;

    try {
      file.read(is);
    } catch (...) {
      throw ExceptionFreeserf("Wrong config file format.");
    }

    auto sects = file.get_sections();
    for (std::string name : sects) {
      SaveReaderTextSection *section = new SaveReaderTextSection(&file, name);
      sections.push_back(section);
    }

    auto vals = file.get_values("main");
    for (std::string vname : vals) {
      values.emplace(std::make_pair(vname,
                           SaveReaderTextValue(file.value("main", vname, ""))));
    }
  }

  virtual ~SaveReaderTextFile() {
    for (auto section : sections) {
      delete section;
    }
  }

  virtual std::string get_name() const {
    return std::string();
  }

  virtual unsigned int get_number() const {
    return 0;
  }

  virtual const SaveReaderTextValue &
  value(const std::string &name) const {
    std::string v_name = name;
    std::transform(v_name.begin(), v_name.end(), v_name.begin(), ::tolower);
    Values::const_iterator it = values.find(v_name);
    if (it == values.end()) {
      std::ostringstream str;
      str << "Failed to load value: " << name;
      throw ExceptionFreeserf(str.str());
    }

    return it->second;
  }

  virtual Readers get_sections(const std::string &name) {
    Readers result;

    for (SaveReaderTextSection *reader : sections) {
      if (reader->get_name() == name) {
        result.push_back(reader);
      }
    }

    return result;
  }

  virtual bool has_value(const std::string &name) {
    std::string v_name = name;
    std::transform(v_name.begin(), v_name.end(), v_name.begin(), ::tolower);
    Values::const_iterator it = values.find(v_name);
    return (it != values.end());
  }
};

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
  if (!has_data_left(1)) throw ExceptionFreeserf("Invalid read past end.");
  val = *current;
  current++;
  return *this;
}

SaveReaderBinary&
SaveReaderBinary::operator >> (uint16_t &val) {
  if (!has_data_left(2)) throw ExceptionFreeserf("Invalid read past end.");
  val = *reinterpret_cast<uint16_t*>(current);
  current += 2;
  return *this;
}

SaveReaderBinary&
SaveReaderBinary::operator >> (uint32_t &val) {
  if (!has_data_left(4)) throw ExceptionFreeserf("Invalid read past end.");
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
  if (!has_data_left(size)) {
    throw ExceptionFreeserf("Invalid extract past end.");
  }

  SaveReaderBinary new_reader(current, size);
  current += size;
  return new_reader;
}

uint8_t *
SaveReaderBinary::read(size_t size) {
  if (!has_data_left(size)) throw ExceptionFreeserf("Invalid read past end.");
  uint8_t *data = current;
  current += size;
  return data;
}

SaveReaderTextValue::SaveReaderTextValue(const std::string &_value)
  : value(_value) {
  if (value.find(',') != std::string::npos) {
    std::istringstream iss(value);
    std::string item;
    while (std::getline(iss, item, ',')) {
      parts.emplace_back(item);
    }
  }
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (int &val) const {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (unsigned int &val) const {
  int result = atoi(value.c_str());
  val = result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (Direction &val) const {
  int result = atoi(value.c_str());
  val = (Direction)result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (Resource::Type &val) const {
  int result = atoi(value.c_str());
  val = (Resource::Type)result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (Building::Type &val) const {
  int result = atoi(value.c_str());
  val = (Building::Type)result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (Serf::State &val) const {
  int result = atoi(value.c_str());
  val = (Serf::State)result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (uint16_t &val) const {
  int result = atoi(value.c_str());
  val = (uint16_t)result;

  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator >> (std::string &val) const {
  val = value;
  return *this;
}

const SaveReaderTextValue&
SaveReaderTextValue::operator[] (size_t pos) const {
  if (pos >= parts.size()) {
    throw ExceptionFreeserf("Failed to read value");
  }

  return parts[pos];
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

// SaveGame

GameStore::GameStore() {
  folder_path = ".";

#ifdef _WIN32
  PWSTR saved_games_path;
  HRESULT res = SHGetKnownFolderPath(FOLDERID_SavedGames,
                                     KF_FLAG_CREATE | KF_FLAG_INIT,
                                     nullptr, &saved_games_path);
  int len = static_cast<int>(wcslen(saved_games_path));
  folder_path.resize(len);
  ::WideCharToMultiByte(CP_ACP, 0, saved_games_path, len,
                        (LPSTR)folder_path.data(), len, nullptr, nullptr);
  ::CoTaskMemFree(saved_games_path);
  folder_path += '\\';
#elif defined(SDL_PLATFORM_APPLE)
  folder_path = std::string(std::getenv("HOME"));
  folder_path += "/Library/Application Support";
  folder_path += '/';
#else
  char *folder = std::getenv("XDG_DATA_HOME");
  if (folder == nullptr) {
    folder = std::getenv("HOME");
  }
  folder_path = std::string(folder);
  folder_path += '/';
#endif

  folder_path += "freeserf";
  if (!is_folder_exists(folder_path)) {
    if (!create_folder(folder_path)) {
      throw ExceptionFreeserf("Failed to create folder");
    }
  }

#ifndef _WIN32
  folder_path += "/saves";
  if (!is_folder_exists(folder_path)) {
    if (!create_folder(folder_path)) {
      throw ExceptionFreeserf("Failed to create folder");
    }
  }
#endif
}

GameStore::~GameStore() {
}

GameStore &
GameStore::get_instance() {
  static GameStore instance;
  return instance;
}

bool
GameStore::create_folder(const std::string &path) {
#ifdef _WIN32
  return (CreateDirectoryA(path.c_str(), nullptr) != FALSE);
#else
  int result = mkdir(path.c_str(),
                     S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (result != 0) {
    Log::Error["savegame"] << "Failed to create folder: errno = "
                           << errno << "";
  }
  return (result == 0);
#endif  // _WIN32
}

bool
GameStore::is_folder_exists(const std::string &path) {
  struct stat info;

  if (stat(path.c_str(), &info) != 0) {
    return false;
  }

  return ((info.st_mode & S_IFDIR) == S_IFDIR);
}

const std::vector<GameStore::SaveInfo> &
GameStore::get_saved_games() {
  saved_games.clear();
  update();
  return saved_games;
}

std::string
GameStore::name_from_file(const std::string &file_name) {
  size_t pos = file_name.find_last_of('.');
  if (pos == std::string::npos) {
    return file_name;
  }

  return file_name.substr(0, pos);
}

void
GameStore::update() {
  find_legacy();
#ifdef _WIN32
  std::string find_mask = folder_path + "\\*.save";
  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(find_mask.c_str(), &ffd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        SaveInfo info;
        info.name = name_from_file(ffd.cFileName);
        info.path = folder_path + "\\" + ffd.cFileName;
        info.type = SaveInfo::Regular;
        saved_games.push_back(info);
      }
    } while (FindNextFileA(hFind, &ffd) != FALSE);
  }

  FindClose(hFind);
#else
  DIR *dir = opendir(folder_path.c_str());
  if (dir == nullptr) {
    return;
  }

  struct dirent *ent = nullptr;
  while ((ent = readdir(dir)) != nullptr) {
    std::string file_name(ent->d_name);
    size_t pos = file_name.find_last_of(".");
    if (pos == std::string::npos) {
      continue;
    }
    std::string ext = file_name.substr(pos+1, file_name.size());
    if (ext != "save") {
      continue;
    }
    std::string file_path = folder_path + "/" + file_name;
    struct stat info;
    if (stat(file_path.c_str(), &info) == 0) {
      if ((info.st_mode & S_IFDIR) != S_IFDIR) {
        SaveInfo info;
        info.name = name_from_file(file_name);
        info.path = file_path;
        info.type = SaveInfo::Regular;
        saved_games.push_back(info);
      }
    }
  }

  closedir(dir);
#endif  // _WIN32
}

bool
GameStore::is_file_exists(const std::string &path) {
  struct stat info;
  if (stat(path.c_str(), &info) == 0) {
    if ((info.st_mode & S_IFDIR) != S_IFDIR) {
      return true;
    }
  }
  return false;
}

void
GameStore::add_info(SaveInfo info) {
  if (!is_file_exists(info.path)) {
    return;
  }
  bool exists = false;
  for (SaveInfo &i : saved_games) {
    if (i.path == info.path) {
      exists = true;
    }
  }
  if (!exists) {
    saved_games.push_back(info);
  }
}

void
GameStore::find_legacy() {
  std::ifstream archiv(folder_path + "/ARCHIV.DS", std::ios::binary);
  for (int i = 0; i < 10; i++) {
    std::array<char, 15> name;
    archiv.read(&name[0], 15);
    char used = 0;
    archiv.read(&used, 1);
    if (used == 1) {
      SaveInfo info;
      info.type = SaveInfo::Legacy;
      for (char &c : name) {
        if (c != 0x20 && c != '\xff') {
          info.name += c;
        }
      }
#ifdef _WIN32
      info.path = folder_path + '\\';
#else
      info.path = folder_path + '/';
#endif  // _WIN32
      info.path += "SAVE";
      info.path += static_cast<char>(i + '0');
      info.path += ".DS";
      add_info(info);
    }
  }

#ifdef _WIN32
  std::string find_mask = folder_path + "\\*.DS";
  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(find_mask.c_str(), &ffd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        if (std::string(ffd.cFileName) != "ARCHIV.DS") {
          SaveInfo info;
          info.name = name_from_file(ffd.cFileName);
          info.path = folder_path + "\\" + ffd.cFileName;
          info.type = SaveInfo::Legacy;
          add_info(info);
        }
      }
    } while (FindNextFileA(hFind, &ffd) != FALSE);
  }

  FindClose(hFind);
#else
  DIR *dir = opendir(folder_path.c_str());
  if (dir == nullptr) {
    return;
  }

  struct dirent *ent = nullptr;
  while ((ent = readdir(dir)) != nullptr) {
    std::string file_name(ent->d_name);
    size_t pos = file_name.find_last_of(".");
    if (pos == std::string::npos) {
      continue;
    }
    std::string ext = file_name.substr(pos+1, file_name.size());
    if (ext != "DS" || file_name == "ARCHIV.DS") {
      continue;
    }
    SaveInfo info;
    info.name = name_from_file(file_name);
    info.path = folder_path + "/" + file_name;
    info.type = SaveInfo::Legacy;
    add_info(info);
  }

  closedir(dir);
#endif  // _WIN32
}

void
GameStore::find_regular() {
}

bool
GameStore::load(const std::string &path, Game *game) {
  std::ifstream file;
  file.open(path.c_str());

  if (!file.is_open()) {
    Log::Error["savegame"] << "Unable to open save game file: '" << path << "'";
    return false;
  }

  try {
    SaveReaderTextFile reader_text(&file);
    reader_text >> *game;
  } catch (ExceptionFreeserf& e) {
    file.close();
    Log::Warn["savegame"] << "Unable to load save game: " << e.what();
    Log::Warn["savegame"] << "Trying compatability mode...";
    std::ifstream input(path.c_str(), std::ios::binary);
    std::vector<char> buffer((std::istreambuf_iterator<char>(input)),
                             (std::istreambuf_iterator<char>()));
    SaveReaderBinary reader(&buffer[0], buffer.size());
    try {
      reader >> *game;
    } catch (ExceptionFreeserf& e) {
      Log::Error["savegame"] << "Failed to load save game: " << e.what();
      return false;
    }
  }

  return true;
}

bool
GameStore::quick_save(const std::string &prefix, Game *game) {
  /* Build filename including time stamp. */
  std::time_t t = time(NULL);
  struct tm *tm = std::localtime(&t);
  if (tm == nullptr) {
    return false;
  }

  char name[128];
  size_t r = strftime(name, sizeof(name), "%Y-%m-%d_%H-%M-%S", tm);
  if (r == 0) {
    return false;
  }

  GameStore save_game;

  std::string path = save_game.get_folder_path();
  path += "/" + prefix + "-" + name + ".save";

  return save(path, game);
}

// In target, replace any character from needle with replacement character.
static std::string
strreplace(const std::string &src, const std::string &needle, char replace) {
  std::string target;

  for (char c : src) {
    for (char r : needle) {
      if (r == c) {
        c = replace;
        break;
      }
    }
    target += c;
  }

  return target;
}

bool
GameStore::save(const std::string &path, Game *game) {
  /* Substitute problematic characters. These are problematic
   particularly on windows platforms, but also in general on FAT
   filesystems through any platform. */
  /* TODO Possibly use PathCleanupSpec() when building for windows platform. */
  std::string file_path = strreplace(path, "*?\"<>|", '_');

  SaveWriterTextSection writer("game", 0);
  writer << *game;
  return writer.save(file_path);
}

bool
GameStore::read(std::istream *is, Game *game) {
  try {
    SaveReaderTextFile reader_text(is);
    reader_text >> *game;
  } catch (...) {
    return false;
  }
  return true;
}

bool
GameStore::write(std::ostream *os, Game *game) {
  SaveWriterTextSection writer("game", 0);
  writer << *game;
  return writer.write(os);
}

