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

#include "src/log.h"
#include "src/data-source-dos.h"

#ifdef _WIN32
// need for GetModuleFileName
#include <Windows.h>
#endif


data_t *data_t::instance = NULL;

data_t::data_t() {
  data_source = NULL;
  instance = this;
}

data_t::~data_t() {
  if (data_source != NULL) {
    delete data_source;
    data_source = NULL;
  }

  instance = NULL;
}

data_t *
data_t::get_instance() {
  if (instance == NULL) {
    instance = new data_t();
  }

  return instance;
}

void
data_t::add_to_search_paths(const char *path,
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

#define MAX_DATA_PATH      1024

bool
data_t::load(const std::string &path) {
  /* If it possible, prefer DOS game data. */
  data_source_t *data_sources[] = { new data_source_dos_t(),
                                    NULL, };

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

  for (int i = 0; data_sources[i] != NULL; i++) {
    std::list<std::string>::iterator it = search_paths.begin();
    for (; it != search_paths.end(); ++it) {
      std::string res_path;
      if (data_sources[i]->check(*it, &res_path)) {
        LOGI("data", "Game data found in '%s'...", res_path.c_str());
        if (data_sources[i]->load(res_path)) {
          data_source = data_sources[i];
          break;
        }
      }
    }
    if (data_source != NULL) {
      break;
    }
  }

  for (int i = 0; data_sources[i] != NULL; i++) {
    if (data_sources[i] != data_source) {
      delete data_sources[i];
    }
  }

  return (data_source != NULL);
}
