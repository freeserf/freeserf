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

#include "src/misc.h"
BEGIN_EXT_C
  #include "src/log.h"
END_EXT_C
#include "src/data-source-dos.h"

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

#define MAX_DATA_PATH      1024

/* Load data file from path is non-NULL, otherwise search in
 various standard paths. */
bool
data_t::load(const std::string &path) {
  const char *default_data_file[] = {
    "SPAE.PA", /* English */
    "SPAF.PA", /* French */
    "SPAD.PA", /* German */
    "SPAU.PA", /* Engish (US) */
    NULL
  };

  data_source = new data_source_t();

  /* Use specified path. If something was specified
   but not found, this function should fail without
   looking anywhere else. */
  if (!path.empty()) {
    LOGI("main", "Looking for game data in `%s'...", path.c_str());
    return data_source->load(path);
  }

  /* If a path is not specified (path is NULL) then
   the configuration file is searched for in the directories
   specified by the XDG Base Directory Specification
   <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.

   On windows platforms the %localappdata% is used in place of XDG_CONFIG_HOME.
   */

  char cp[MAX_DATA_PATH];
  char *env;

  /* Look in home */
  if ((env = getenv("XDG_DATA_HOME")) != NULL &&
      env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      if (data_source->load(cp)) {
        return true;
      }
    }
  } else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      if (data_source->load(cp)) {
        return true;
      }
    }
  }

#ifdef _WIN32
  if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "%s/.local/share/freeserf/%s", env, *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      if (data_source->load(cp)) {
        return true;
      }
    }
  }
#endif

  if ((env = getenv("XDG_DATA_DIRS")) != NULL && env[0] != '\0') {
    char *begin = env;
    while (1) {
      char *end = strchr(begin, ':');
      if (end == NULL) end = strchr(begin, '\0');

      int len = static_cast<int>(end - begin);
      if (len > 0) {
        for (const char **df = default_data_file; *df != NULL; df++) {
          snprintf(cp, sizeof(cp), "%.*s/freeserf/%s", len, begin, *df);
          LOGI("main", "Looking for game data in `%s'...", cp);
          if (data_source->load(cp)) {
            return true;
          }
        }
      }

      if (end[0] == '\0') break;
      begin = end + 1;
    }
  } else {
    /* Look in /usr/local/share and /usr/share per XDG spec. */
    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "/usr/local/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      if (data_source->load(cp)) {
        return true;
      }
    }

    for (const char **df = default_data_file; *df != NULL; df++) {
      snprintf(cp, sizeof(cp), "/usr/share/freeserf/%s", *df);
      LOGI("main", "Looking for game data in `%s'...", cp);
      if (data_source->load(cp)) {
        return true;
      }
    }
  }

  /* Look in current directory */
  for (const char **df = default_data_file; *df != NULL; df++) {
    LOGI("main", "Looking for game data in `%s'...", *df);
    if (data_source->load(cp)) {
      return true;
    }
  }

  delete data_source;
  data_source = NULL;

  return false;
}
