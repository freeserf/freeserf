/*
 * game-source-mission.cc - Predefined game mission maps
 *
 * Copyright (C) 2013-2019  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/game-source-mission.h"

#include "src/game.h"
#include "src/characters.h"

static PlayerInfo::Color def_color[4] = {
  {0x00, 0xe3, 0xe3},
  {0xcf, 0x63, 0x63},
  {0xdf, 0x7f, 0xef},
  {0xef, 0xef, 0x8f}
};

MissionPreset tutorials[] = {
  {
    "Tutorial 1",
    Random("3762665523225478"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  },
  {
    "Tutorial 2",
    Random("1616118277634871"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  },
  {
    "Tutorial 3",
    Random("2418554743842118"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  },
  {
    "Tutorial 4",
    Random("4454333314864658"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  },
  {
    "Tutorial 5",
    Random("4264118313621432"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  },
  {
    "Tutorial 6",
    Random("2487251185361288"),
    {
      {12, 40, 30, 30, {-1, -1}}
    }
  }
};

MissionPreset missions[] = {
  {
    "START",
    Random("8667715887436237"),
    {
      {12, 40, 35, 30, {-1, -1}},
      { 1, 10,  5, 30, {-1, -1}}
    }
  },
  {
    "STATION",
    Random("2831713285431227"),
    {
      {12, 40, 30, 40, {-1, -1}},
      { 2, 12, 15, 30, {-1, -1}},
      { 3, 14, 15, 30, {-1, -1}}
    }
  },
  {
    "UNITY",
    Random("4632253338621228"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 2, 18, 10, 25, {-1, -1}},
      { 4, 18, 10, 25, {-1, -1}}
    }
  },
  {
    "WAVE",
    Random("8447342476811762"),
    {
      {12, 40, 25, 40, {-1, -1}},
      { 2, 16, 20, 30, {-1, -1}}
    }
  },
  {
    "EXPORT",
    Random("4276472414845177"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 3, 16, 25, 20, {-1, -1}},
      { 4, 16, 25, 20, {-1, -1}}
    }
  },
  {
    "OPTION",
    Random("2333577877517478"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 3, 20, 12, 14, {-1, -1}},
      { 5, 20, 12, 14, {-1, -1}}
    }
  },
  {
    "RECORD",
    Random("1416541231242884"),
    {
      {12, 40, 30, 40, {-1, -1}},
      { 3, 22, 30, 30, {-1, -1}}
    }
  },
  {
    "SCALE",
    Random("7845187715476348"),
    {
      {12, 40, 25, 30, {-1, -1}},
      { 4, 23, 25, 30, {-1, -1}},
      { 6, 24, 25, 30, {-1, -1}}
    }
  },
  {
    "SIGN",
    Random("5185768873118642"),
    {
      {12, 40, 25, 40, {-1, -1}},
      { 4, 26, 13, 30, {-1, -1}},
      { 5, 28, 13, 30, {-1, -1}},
      { 6, 30, 13, 30, {-1, -1}}
    }
  },
  {
    "ACORN",
    Random("3183215728814883"),
    {
      {12, 40, 20, 16, {28, 14}},
      { 4, 30, 19, 20, { 5, 47}}
    }
  },
  {
    "CHOPPER",
    Random("4376241846215474"),
    {
      {12, 40, 16, 20, {16, 42}},
      { 5, 33, 10, 20, {52, 25}},
      { 7, 34, 13, 20, {23, 12}}
    }
  },
  {
    "GATE",
    Random("6371557668231277"),
    {
      {12, 40, 23, 27, {53, 13}},
      { 5, 27, 17, 24, {27, 10}},
      { 6, 27, 13, 24, {29, 38}},
      { 7, 27, 13, 24, {15, 32}}
    }
  },
  {
    "ISLAND",
    Random("8473352672411117"),
    {
      {12, 40, 24, 20, { 7, 26}},
      { 5, 20, 30, 20, { 2, 10}}
    }
  },
  {
    "LEGION",
    Random("1167854231884464"),
    {
      {12, 40, 20, 23, {19,  3}},
      { 6, 28, 16, 20, {55,  7}},
      { 8, 28, 16, 20, {55, 46}}
    }
  },
  {
    "PIECE",
    Random("2571462671725414"),
    {
      {12, 40, 20, 17, {41,  5}},
      { 6, 40, 23, 20, {19, 49}},
      { 7, 37, 20, 20, {58, 52}},
      { 8, 40, 15, 15, {43, 31}}
    }
  },
  {
    "RIVAL",
    Random("4563653871271587"),
    {
      {12, 40, 26, 23, {36, 63}},
      { 6, 28, 29, 40, {14, 15}}
    }
  },
  {
    "SAVAGE",
    Random("7212145428156114"),
    {
      {12, 40, 25, 12, {63, 59}},
      { 7, 29, 17, 10, {29, 24}},
      { 8, 29, 17, 10, {39, 26}},
      { 9, 32, 17, 10, {42, 49}}
    }
  },
  {
    "XAVER",
    Random("4276472414435177"),
    {
      {12, 40, 25, 40, {15,  0}},
      { 7, 40, 30, 35, {34, 48}},
      { 9, 30, 30, 35, {58,  5}}
    }
  },
  {
    "BLADE",
    Random("7142748441424786"),
    {
      {12, 40, 30, 20, {13, 37}},
      { 7, 40, 20, 20, {32, 34}}
    }
  },
  {
    "BEACON",
    Random("6882188351133886"),
    {
      {12, 40,  9, 10, {14, 42}},
      { 8, 40, 16, 22, {62,  1}},
      { 9, 40, 16, 23, {32, 14}}
    }
  },
  {
    "PASTURE",
    Random("7742136435163436"),
    {
      {12, 40, 20, 11, {38, 17}},
      { 8, 30, 22, 13, {32, 51}},
      { 9, 30, 23, 13, { 1, 50}},
      {10, 30, 21, 13, { 4,  9}}
    }
  },
  {
    "OMNUS",
    Random("6764387728224725"),
    {
      {12, 40, 20, 40, {42, 20}},
      { 8, 36, 25, 40, {48, 47}}
    }
  },
  {
    "TRIBUTE",
    Random("5848744734731253"),
    {
      {12, 40,  5, 11, {53,  1}},
      { 9, 35, 30, 10, {20,  2}},
      {10, 37, 30, 10, {16, 55}}
    }
  },
  {
    "FOUNTAIN",
    Random("6183541838474434"),
    {
      {12, 40, 20, 12, { 3, 34}},
      { 9, 30, 25, 10, {47, 41}},
      {10, 30, 26, 10, {42, 52}}
    }
  },
  {
    "CHUDE",
    Random("7633126817245833"),
    {
      {12, 40, 20, 40, {23, 38}},
      { 9, 40, 25, 40, {57, 52}}
    }
  },
  {
    "TRAILER",
    Random("5554144773646312"),
    {
      {12, 40, 20, 30, {29, 11}},
      {10, 38, 30, 35, {15, 40}}
    }
  },
  {
    "CANYON",
    Random("3122431112682557"),
    {
      {12, 40, 18, 28, {49, 53}},
      {10, 39, 25, 40, {14, 53}}
    }
  },
  {
    "REPRESS",
    Random("2568412624848266"),
    {
      {12, 40, 20, 40, {44, 39}},
      {10, 39, 25, 40, {44, 63}}
    }
  },
  {
    "YOKI",
    Random("3736685353284538"),
    {
      {12, 40,  5, 22, {53,  8}},
      {11, 40, 15, 20, {30, 22}}
    }
  },
  {
    "PASSIVE",
    Random("5471458635555317"),
    {
      {12, 40,  5, 20, {25, 46}},
      {11, 40, 20, 20, {51, 42}}
    }
  }
};

GameInfoMission::GameInfoMission(const MissionPreset *_mission_preset)
  : mission_preset(_mission_preset) {
}

std::string
GameInfoMission::get_name() const {
  return mission_preset->name;
}

Random
GameInfoMission::get_random_base() const {
  return mission_preset->rnd;
}

size_t
GameInfoMission::get_player_count() const {
  return mission_preset->player.size();
}

PPlayerInfo
GameInfoMission::get_player(size_t player) const {
  return std::make_shared<PlayerInfoMission>(&mission_preset->player[player],
                                             player);
}

// PlayerInfoMission

PlayerInfoMission::PlayerInfoMission(const PlayerPreset *_player_preset,
                                     size_t _id) {
  player_preset = _player_preset;
  id = _id;
}

std::string
PlayerInfoMission::get_name() const {
  size_t index = player_preset->character_index;
  Character &character = Characters::characters.get_character(index);
  return character.get_name();
}

unsigned int
PlayerInfoMission::get_face() const {
  size_t index = player_preset->character_index;
  Character &character = Characters::characters.get_character(index);
  return character.get_face();
}

std::string
PlayerInfoMission::get_characterization() const {
  size_t index = player_preset->character_index;
  Character &character = Characters::characters.get_character(index);
  return character.get_characterization();
}

PlayerInfo::Color
PlayerInfoMission::get_color() const {
  return def_color[id];
}

unsigned int
PlayerInfoMission::get_intelligence() const {
  return player_preset->intelligence;
}

unsigned int
PlayerInfoMission::get_supplies() const {
  return player_preset->supplies;
}

unsigned int
PlayerInfoMission::get_reproduction() const {
  return player_preset->reproduction;
}

PlayerInfo::BuildingPos
PlayerInfoMission::get_castle_pos() const {
  return player_preset->castle;
}

bool
PlayerInfoMission::has_castle() const {
  return (player_preset->castle.col >= 0);
}

// GameSourceMission

GameSourceMission::GameSourceMission() {
}

size_t
GameSourceMission::count() const {
  return sizeof(missions) / sizeof(missions[0]);
}

PGameInfo
GameSourceMission::game_info_at(const size_t &pos) const {
  if (pos >= count()) {
    return nullptr;
  }

  return std::make_shared<GameInfoMission>(missions + pos);
}

PGameInfo
GameSourceMission::create_game(const std::string &path) const {
  return nullptr;
}
