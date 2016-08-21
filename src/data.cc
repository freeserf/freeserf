/*
 * data.cc - Game resources file functions
 *
 * Copyright (C) 2014  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/data.h"

#include <cstdlib>
#include <vector>
#include <memory>
#include <utility>

#include "src/log.h"
#include "src/data-source-dos.h"

#ifdef _WIN32
// need for GetModuleFileName
#include <Windows.h>
#endif


Data::Data() : data_source(nullptr) {}

Data *
Data::get_instance() {
  static Data instance;
  return &instance;
}

void
Data::add_to_search_paths(const char *path,
                            const char *suffix) {
  if (path == NULL) {
    return;
  }

  std::string res_path = path;
  if (suffix != NULL) {
    res_path += '/';
    res_path += suffix;
  }

  search_paths.push_back(res_path);
}

bool
Data::load(const std::string &path) {
  /* If it possible, prefer DOS game data. */
  std::vector<std::unique_ptr<DataSource>> data_sources;
  data_sources.emplace_back(new DataSourceDOS());

  /* Use specified path. If something was specified
     but not found, this function should fail without
     looking anywhere else. */
  add_to_search_paths(path.c_str(), NULL);

  /* If a path is not specified (path is NULL) then
     the configuration file is searched for in the directories
     specified by the XDG Base Directory Specification
     <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.

     On windows platforms the %localappdata% is used in place of XDG_CONFIG_HOME.
  */

  // Look in current directory
  add_to_search_paths(".", NULL);
  /* Look in home */
  add_to_search_paths(std::getenv("XDG_DATA_HOME"), "freeserf");
  add_to_search_paths(std::getenv("HOME"), ".local/share/freeserf");
#ifdef _WIN32
  // to find the data within the same directory as the freeserf.exe app.
  char app_path[MAX_PATH + 1];
  GetModuleFileNameA(NULL, app_path, MAX_PATH);
  add_to_search_paths(app_path, "/../");

  add_to_search_paths(std::getenv("userprofile"), ".local/share/freeserf");
  add_to_search_paths(std::getenv("LOCALAPPDATA"), "/freeserf");
#endif

  // search in preference base directories
#ifdef _WIN32
  char *env = std::getenv("PATH");
  #define PATH_SEPERATING_CHAR ';'
#else
  char *env = std::getenv("XDG_DATA_DIRS");
  #define PATH_SEPERATING_CHAR ':'
#endif

  std::string dirs = (env == NULL) ? std::string() : env;
  while (!dirs.empty()) {
    size_t pos = dirs.find(PATH_SEPERATING_CHAR);
    std::string dir = dirs.substr(0, pos);
    dirs.replace(0, (pos == std::string::npos) ? pos : pos + 1, std::string());
    add_to_search_paths(dir.c_str(), "freeserf");
  }

  add_to_search_paths("/usr/local/share", "freeserf");
  add_to_search_paths("/usr/share", "freeserf");

  for (auto& ds : data_sources) {
    std::list<std::string>::iterator it = search_paths.begin();
    for (; it != search_paths.end(); ++it) {
      std::string res_path;
      if (ds->check(*it, &res_path)) {
        Log::Info["data"] << "Game data found in '"
                          << res_path.c_str() << "'...";
        if (ds->load(res_path)) {
          data_source = std::move(ds);
          break;
        }
      }
    }
    if (data_source) {
      break;
    }
  }

  return data_source.get() != nullptr;
}

// Return standard game data search paths for current platform.
std::list<std::string>
Data::get_standard_search_paths() const {
  // Data files are searched for in some common directories, some of which are
  // specific to the platform we're running on.
  //
  // On platforms where the XDG specification applies, the data file is
  // searched for in the directories specified by the
  // XDG Base Directory Specification
  // <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.
  //
  // On Windows platforms the %localappdata% is used in place of
  // XDG_DATA_HOME.

  std::list<std::string> paths;

  // Add path where base is obtained from an environment variable and can
  // be nullptr or empty.
  auto add_env_path = [&](const char* base, const char* suffix) {
    if (base != nullptr) {
      std::string b(base);
      if (!b.empty()) paths.push_back(b + "/" + suffix);
    }
  };

  // Look in current directory
  paths.push_back(".");

  // Look in data directories under the home directory
  add_env_path(std::getenv("XDG_DATA_HOME"), "freeserf");
  add_env_path(std::getenv("HOME"), ".local/share/freeserf");

#ifdef _WIN32
  // Look in the same directory as the freeserf.exe app.
  char app_path[MAX_PATH + 1];
  GetModuleFileNameA(NULL, app_path, MAX_PATH);
  add_env_path(app_path, "../");

  // Look in windows XDG_DATA_HOME equivalents.
  add_env_path(std::getenv("userprofile"), ".local/share/freeserf");
  add_env_path(std::getenv("LOCALAPPDATA"), "freeserf");
#endif

  // Search in global directories.
#ifdef _WIN32
  const char *env = std::getenv("PATH");
  #define PATH_SEPERATING_CHAR ';'
#else
  const char *env = std::getenv("XDG_DATA_DIRS");
  #define PATH_SEPERATING_CHAR ':'
#endif

  std::string dirs = (env == NULL) ? std::string() : env;
  size_t next_path = 0;
  while (next_path != std::string::npos) {
    size_t pos = dirs.find(PATH_SEPERATING_CHAR, next_path);
    std::string dir = dirs.substr(next_path, pos);
    next_path = (pos != std::string::npos) ? pos + 1 : pos;
    add_env_path(dir.c_str(), "freeserf");
  }

#ifndef _WIN32
  paths.push_back("/usr/local/share/freeserf");
  paths.push_back("/usr/share/freeserf");
#endif

  return paths;
}
