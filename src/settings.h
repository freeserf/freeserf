/*
 * settings.h - Settings kept between the runs of the game
 *
 * Copyright (C) 2026  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_SETTINGS_H_
#define SRC_SETTINGS_H_

#include <string>

#include "src/configfile.h"

/* Settings kept between the runs of the game: freeserf.ini in the user's
   folder for application data (SDL_GetPrefPath: Application Support on
   macOS, %APPDATA% on Windows, ~/.local/share on Linux). */
class Settings {
 protected:
  ConfigFile config;
  std::string path;

  Settings();

 public:
  static Settings &get_instance();

  /* Missing values keep their defaults; a broken file is ignored. */
  bool load();
  bool save();

  template <typename T> T get(const std::string &section,
                              const std::string &name,
                              const T &def_val) const {
    return config.value(section, name, def_val);
  }
  template <typename T> void set(const std::string &section,
                                 const std::string &name, const T &value) {
    config.set_value(section, name, value);
  }
};

#endif  // SRC_SETTINGS_H_
