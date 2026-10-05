/*
 * data.cc - Game resources file functions
 *
 * Copyright (C) 2014-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include <vector>
#include <memory>
#include <utility>
#include <string>
#include <functional>

#include "src/log.h"
#include "src/data-source-dos.h"
#include "src/data-source-amiga.h"
#include "src/data-source-custom.h"
#include "src/data-source-mixed.h"

#ifdef _WIN32
// need for GetModuleFileName
#include <Windows.h>
#endif

typedef struct DataResource {
  Data::Resource resource;
  Data::Type type;
  unsigned int count;
  const char *name;
} DataResource;

DataResource data_resources[] = {
  { Data::AssetNone,         Data::TypeUnknown,   0,   "error"         },
  { Data::AssetArtLandscape, Data::TypeSprite,    1,   "art_landscape" },
  { Data::AssetAnimation,    Data::TypeAnimation, 200, "animation"     },
  { Data::AssetSerfShadow,   Data::TypeSprite,    1,   "serf_shadow"   },
  { Data::AssetDottedLines,  Data::TypeSprite,    7,   "dotted_lines"  },
  { Data::AssetArtFlag,      Data::TypeSprite,    7,   "art_flag"      },
  { Data::AssetArtBox,       Data::TypeSprite,    14,  "art_box"       },
  { Data::AssetCreditsBg,    Data::TypeSprite,    1,   "credits_bg"    },
  { Data::AssetLogo,         Data::TypeSprite,    1,   "logo"          },
  { Data::AssetSymbol,       Data::TypeSprite,    16,  "symbol"        },
  { Data::AssetMapMaskUp,    Data::TypeSprite,    81,  "map_mask_up"   },
  { Data::AssetMapMaskDown,  Data::TypeSprite,    81,  "map_mask_down" },
  { Data::AssetPathMask,     Data::TypeSprite,    27,  "path_mask"     },
  { Data::AssetMapGround,    Data::TypeSprite,    33,  "map_ground"    },
  { Data::AssetPathGround,   Data::TypeSprite,    10,  "path_ground"   },
  { Data::AssetGameObject,   Data::TypeSprite,    279, "game_object"   },
  { Data::AssetFrameTop,     Data::TypeSprite,    4,   "frame_top"     },
  { Data::AssetMapBorder,    Data::TypeSprite,    10,  "map_border"    },
  { Data::AssetMapWaves,     Data::TypeSprite,    16,  "map_waves"     },
  { Data::AssetFramePopup,   Data::TypeSprite,    4,   "frame_popup"   },
  { Data::AssetIndicator,    Data::TypeSprite,    8,   "indicator"     },
  { Data::AssetFont,         Data::TypeSprite,    44,  "font"          },
  { Data::AssetFontShadow,   Data::TypeSprite,    44,  "font_shadow"   },
  { Data::AssetIcon,         Data::TypeSprite,    318, "icon"          },
  { Data::AssetMapObject,    Data::TypeSprite,    194, "map_object"    },
  { Data::AssetMapShadow,    Data::TypeSprite,    194, "map_shadow"    },
  { Data::AssetPanelButton,  Data::TypeSprite,    25,  "panel_button"  },
  { Data::AssetFrameBottom,  Data::TypeSprite,    26,  "frame_bottom"  },
  { Data::AssetSerfTorso,    Data::TypeSprite,    541, "serf_torso"    },
  { Data::AssetSerfHead,     Data::TypeSprite,    630, "serf_head"     },
  { Data::AssetFrameSplit,   Data::TypeSprite,    3,   "frame_split"   },
  { Data::AssetSound,        Data::TypeSound,     90,  "sound"         },
  { Data::AssetMusic,        Data::TypeMusic,     7,   "music"         },
  { Data::AssetCursor,       Data::TypeSprite,    1,   "cursor"        },
};

Data::Data() : data_source(nullptr) {}

Data &
Data::get_instance() {
  static Data instance;
  return instance;
}

Data::~Data() {
}

// Try to load the game data from the given paths or the standard paths.
//
// Return true if any data is found. Standard paths are searched only if no
// path is given. Each path is searched with its subfolders dos, amiga and
// custom, so that the data of several versions can be installed together.
bool
Data::load(const std::list<std::string> &paths) {
  typedef std::function<Data::PSource(const std::string &)> SourceFactory;
  struct Factory {
    DataSourceMixed::Kind kind;
    SourceFactory create;
  };
  const Factory factories[] = {
    { DataSourceMixed::KindCustom, [](const std::string &path) {
        return Data::PSource(std::make_shared<DataSourceCustom>(path)); } },
    { DataSourceMixed::KindDOS, [](const std::string &path) {
        return Data::PSource(std::make_shared<DataSourceDOS>(path)); } },
    { DataSourceMixed::KindAmiga, [](const std::string &path) {
        return Data::PSource(std::make_shared<DataSourceAmiga>(path)); } },
  };

  std::list<std::string> search_paths;
  for (const std::string &path : paths.empty() ? get_standard_search_paths()
                                               : paths) {
    search_paths.push_back(path);
    for (const char *sub : { "dos", "amiga", "custom" }) {
      search_paths.push_back(path + "/" + sub);
    }
  }

  mixed = std::make_shared<DataSourceMixed>();
  for (const Factory &factory : factories) {
    for (const std::string &path : search_paths) {
      Data::PSource source = factory.create(path);
      if (source->check()) {
        Log::Info["data"] << "Game data found in '" << source->get_path()
                          << "'...";
        if (source->load()) {
          mixed->add_source(factory.kind, source);
          break;
        }
      }
    }
  }

  if (mixed->is_empty()) {
    mixed = nullptr;
    data_source = nullptr;
    return false;
  }
  data_source = mixed;
  return true;
}

bool
Data::load(const std::string &path) {
  std::list<std::string> paths;
  if (!path.empty()) paths.push_back(path);
  return load(paths);
}

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
  add_env_path(std::getenv("HOME"), ".local/share/freeserf/custom");

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

  std::string dirs = (env == nullptr) ? std::string() : env;
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

Data::Type
Data::get_resource_type(Resource resource) {
  return data_resources[resource].type;
}

unsigned int
Data::get_resource_count(Resource resource) {
  return data_resources[resource].count;
}

const std::string
Data::get_resource_name(Resource resource) {
  return data_resources[resource].name;
}
