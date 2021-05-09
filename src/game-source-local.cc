/*
 * game-source-local.cc - Saved games source
 *
 * Copyright (C) 2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include "src/game-source-local.h"

#include <fstream>
#include <array>
#include <sstream>

#ifdef _WIN32
# include <Windows.h>
# include <Knownfolders.h>
# include <Shlobj.h>
# include <direct.h>
#elif defined(__APPLE__)
# include <dirent.h>
# include <sys/stat.h>
#else
# include <dirent.h>
# include <sys/stat.h>
#endif

#include "src/player.h"
#include "src/characters.h"

PlayerInfo::Color def_color[4] = {
  {0x00, 0xe3, 0xe3},
  {0xcf, 0x63, 0x63},
  {0xdf, 0x7f, 0xef},
  {0xef, 0xef, 0x8f}
};

GameInfoLocal::GameInfoLocal(const std::string &path_) {
  path = path_;
}

GameInfoLocal::GameInfoLocal(const std::string &path_, const std::string &name_,
                             Type type) {
  name = name_;
  path = path_;
}

PlayerInfoLocal::PlayerInfoLocal() {
}

bool
PlayerInfoLocal::has_castle() const {
  return (castle_pos.col >= 0);
}

// GameSourceLocal

GameSourceLocal::GameSourceLocal() {
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
#elif defined(__APPLE__)
  folder_path = std::string(std::getenv("HOME"));
  folder_path += "/Library/Application Support";
#else
  folder_path = std::string(std::getenv("HOME"));
  folder_path += "/.local/share";
#endif

  folder_path += "/freeserf";
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

  update();
}

PGameInfo
GameSourceLocal::game_info_at(const size_t &pos) const {
  if (pos >= count()) {
    return nullptr;
  }

  return games[pos];
}

PGameInfo
GameSourceLocal::create_game(const std::string &path) const {
  return nullptr;
}

bool
GameSourceLocal::publicate(PGame game) {
  return update();
}

bool
GameSourceLocal::update() {
  games.clear();
  find_legacy();
  find_files(GameInfoLocal::Legacy);
  find_files(GameInfoLocal::Regular);
  return true;
}

std::vector<std::string>
dir_enum_files(const std::string &path, const std::string &ext) {
  std::vector<std::string> result;

#ifdef _WIN32
  std::string find_mask = path + "\\*." + ext;
  WIN32_FIND_DATAA ffd;
  HANDLE hFind = FindFirstFileA(find_mask.c_str(), &ffd);
  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
        result.push_back(ffd.cFileName);
      }
    } while (FindNextFileA(hFind, &ffd) != FALSE);
    FindClose(hFind);
  }
#else
  DIR *dir = opendir(path.c_str());
  if (dir != nullptr) {
    struct dirent *ent = nullptr;
    while ((ent = readdir(dir)) != nullptr) {
      std::string file_name(ent->d_name);
      size_t pos = file_name.find_last_of(".");
      if (pos == std::string::npos) {
        continue;
      }
      std::string e = file_name.substr(pos+1, file_name.size());
      if (e != ext) {
        continue;
      }
      std::string file_path = path + "/" + file_name;
      struct stat info;
      if (stat(file_path.c_str(), &info) == 0) {
        if ((info.st_mode & S_IFDIR) != S_IFDIR) {
          result.push_back(file_name);
        }
      }
    }
    closedir(dir);
  }
#endif  // _WIN32

  return result;
}

void
GameSourceLocal::find_legacy() {
  std::ifstream archiv(folder_path + "/ARCHIV.DS", std::ios::binary);
  for (int i = 0; i < 10; i++) {
    std::array<char, 15> name_buffer;
    archiv.read(&name_buffer[0], 15);
    char used = 0;
    archiv.read(&used, 1);
    if (used == 1) {
      std::string name;
      for (char &c : name_buffer) {
        if (c != 0x20 && c != '\xff') {
          name += c;
        }
      }
      std::stringstream ss(folder_path);
      ss << "/SAVE" << i << ".DS";
      std::string path = ss.str();
      PGameInfoLocal game_info;
      try {
        game_info = std::make_shared<GameInfoLocal>(ss.str(), name,
                                                    GameInfoLocal::Legacy);
      } catch(...) {
        Log::Error["GameSourceLocal"] << "Failed to load legacy game info.";
      }
      if (game_info) {
        games.push_back(game_info);
      }
    }
  }
}

bool
GameSourceLocal::create_folder(const std::string &path) const {
#ifdef _WIN32
  return (CreateDirectoryA(path.c_str(), nullptr) != FALSE);
#else
  return (mkdir(folder_path.c_str(),
                S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0);
#endif  // _WIN32
}

bool
GameSourceLocal::is_folder_exists(const std::string &path) const {
  struct stat info;

  if (stat(path.c_str(), &info) != 0) {
    return false;
  }

  return ((info.st_mode & S_IFDIR) == S_IFDIR);
}

std::string
GameSourceLocal::name_from_file(const std::string &file_name) const {
  size_t pos = file_name.find_last_of('.');
  if (pos == std::string::npos) {
    return file_name;
  }

  return file_name.substr(0, pos);
}

void
GameSourceLocal::find_files(GameInfoLocal::Type type) {
  std::vector<std::string> files = dir_enum_files(folder_path,
                               (type == GameInfoLocal::Legacy) ? "DS" : "save");
  for (std::string &file_name : files) {
    PGameInfoLocal game_info;
    try {
      game_info = std::make_shared<GameInfoLocal>(folder_path + "/" + file_name,
                                                  name_from_file(file_name),
                                                  type);
    } catch(...) {
      Log::Error["GameSourceLocal"] << "Failed to load game info from file '"
                                    << folder_path + "/" + file_name << "'";
    }
    if (game_info) {
      games.push_back(game_info);
    }
  }
}
