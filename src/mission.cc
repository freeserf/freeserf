/*
 * mission.cc - Predefined game mission maps
 *
 * Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/mission.h"

CharacterPreset characters[] = {
  { 0, "ERROR", "ERROR"},
  { 1, "Lady Amalie",
    "An inoffensive lady, reserved, who goes about her work peacefully."},
  { 2, "Kumpy Onefinger",
    "A very hostile character, who loves gold above all else."},
  { 3, "Balduin, a former monk",
    "A very discrete character, who worries chiefly about the protection of his"
    " lands and his important buildings."},
  { 4, "Frollin",
    "His unpredictable behaviour will always take you by surprise. "
    "He will \"pilfer\" away lands that are not occupied."},
  { 5, "Kallina",
    "She is a fighter who attempts to block the enemy’s food supply by using "
    "strategic tactics."},
  { 6, "Rasparuk the druid",
    "His tactics consist in amassing large stocks of raw materials. "
    "But he attacks slyly."},
  { 7, "Count Aldaba",
    "Protect your warehouses well, because he is aggressive and knows exactly "
    "where he must attack."},
  { 8, "The King Rolph VII",
    "He is a prudent ruler, without any particular weakness. He will try to "
    "check the supply of construction materials of his adversaries."},
  { 9, "Homen Doublehorn",
    "He is the most aggressive enemy. Watch your buildings carefully, "
    "otherwise he might take you by surprise."},
  {10, "Sollok the Joker",
    "A sly and repugnant adversary, he will try to stop the supply of raw "
    "materials of his enemies right from the beginning of the game."},
  {11, "Enemy",
    "Last enemy."},
  {12, "You",
    "You."},
  {13, "Friend",
    "Your partner."}
};

MissionPreset tutorials[] = {
  {
    "Tutorial 1",
    Random("3762665523225478"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "Tutorial 2",
    Random("1616118277634871"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "Tutorial 3",
    Random("2418554743842118"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "Tutorial 4",
    Random("4454333314864658"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "Tutorial 5",
    Random("4264118313621432"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "Tutorial 6",
    Random("2487251185361288"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  }
};

MissionPreset missions[] = {
  {
    "START",
    Random("8667715887436237"),
    {
      {&characters[12], 40, 35, 30, 64, {-1, -1}},
      {&characters[ 1], 10,  5, 30, 72, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "STATION",
    Random("2831713285431227"),
    {
      {&characters[12], 40, 30, 40, 64, {-1, -1}},
      {&characters[ 2], 12, 15, 30, 72, {-1, -1}},
      {&characters[ 3], 14, 15, 30, 68, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "UNITY",
    Random("4632253338621228"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 2], 18, 10, 25, 72, {-1, -1}},
      {&characters[ 4], 18, 10, 25, 68, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "WAVE",
    Random("8447342476811762"),
    {
      {&characters[12], 40, 25, 40, 64, {-1, -1}},
      {&characters[ 2], 16, 20, 30, 72, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "EXPORT",
    Random("4276472414845177"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 3], 16, 25, 20, 72, {-1, -1}},
      {&characters[ 4], 16, 25, 20, 68, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "OPTION",
    Random("2333577877517478"),
    {
      {&characters[12], 40, 30, 30, 64, {-1, -1}},
      {&characters[ 3], 20, 12, 14, 72, {-1, -1}},
      {&characters[ 5], 20, 12, 14, 68, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "RECORD",
    Random("1416541231242884"),
    {
      {&characters[12], 40, 30, 40, 64, {-1, -1}},
      {&characters[ 3], 22, 30, 30, 72, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SCALE",
    Random("7845187715476348"),
    {
      {&characters[12], 40, 25, 30, 64, {-1, -1}},
      {&characters[ 4], 23, 25, 30, 72, {-1, -1}},
      {&characters[ 6], 24, 25, 30, 68, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SIGN",
    Random("5185768873118642"),
    {
      {&characters[12], 40, 25, 40, 64, {-1, -1}},
      {&characters[ 4], 26, 13, 30, 72, {-1, -1}},
      {&characters[ 5], 28, 13, 30, 68, {-1, -1}},
      {&characters[ 6], 30, 13, 30, 76, {-1, -1}}
    }
  },
  {
    "ACORN",
    Random("3183215728814883"),
    {
      {&characters[12], 40, 20, 16, 64, {28, 14}},
      {&characters[ 4], 30, 19, 20, 72, { 5, 47}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CHOPPER",
    Random("4376241846215474"),
    {
      {&characters[12], 40, 16, 20, 64, {16, 42}},
      {&characters[ 5], 33, 10, 20, 72, {52, 25}},
      {&characters[ 7], 34, 13, 20, 68, {23, 12}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "GATE",
    Random("6371557668231277"),
    {
      {&characters[12], 40, 23, 27, 64, {53, 13}},
      {&characters[ 5], 27, 17, 24, 72, {27, 10}},
      {&characters[ 6], 27, 13, 24, 68, {29, 38}},
      {&characters[ 7], 27, 13, 24, 76, {15, 32}}
    }
  },
  {
    "ISLAND",
    Random("8473352672411117"),
    {
      {&characters[12], 40, 24, 20, 64, { 7, 26}},
      {&characters[ 5], 20, 30, 20, 72, { 2, 10}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "LEGION",
    Random("1167854231884464"),
    {
      {&characters[12], 40, 20, 23, 64, {19,  3}},
      {&characters[ 6], 28, 16, 20, 72, {55,  7}},
      {&characters[ 8], 28, 16, 20, 68, {55, 46}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PIECE",
    Random("2571462671725414"),
    {
      {&characters[12], 40, 20, 17, 64, {41,  5}},
      {&characters[ 6], 40, 23, 20, 72, {19, 49}},
      {&characters[ 7], 37, 20, 20, 68, {58, 52}},
      {&characters[ 8], 40, 15, 15, 76, {43, 31}}
    }
  },
  {
    "RIVAL",
    Random("4563653871271587"),
    {
      {&characters[12], 40, 26, 23, 64, {36, 63}},
      {&characters[ 6], 28, 29, 40, 72, {14, 15}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SAVAGE",
    Random("7212145428156114"),
    {
      {&characters[12], 40, 25, 12, 64, {63, 59}},
      {&characters[ 7], 29, 17, 10, 72, {29, 24}},
      {&characters[ 8], 29, 17, 10, 68, {39, 26}},
      {&characters[ 9], 32, 17, 10, 76, {42, 49}}
    }
  },
  {
    "XAVER",
    Random("4276472414435177"),
    {
      {&characters[12], 40, 25, 40, 64, {15,  0}},
      {&characters[ 7], 40, 30, 35, 72, {34, 48}},
      {&characters[ 9], 30, 30, 35, 68, {58,  5}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "BLADE",
    Random("7142748441424786"),
    {
      {&characters[12], 40, 30, 20, 64, {13, 37}},
      {&characters[ 7], 40, 20, 20, 72, {32, 34}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "BEACON",
    Random("6882188351133886"),
    {
      {&characters[12], 40,  9, 10, 64, {14, 42}},
      {&characters[ 8], 40, 16, 22, 72, {62,  1}},
      {&characters[ 9], 40, 16, 23, 68, {32, 14}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PASTURE",
    Random("7742136435163436"),
    {
      {&characters[12], 40, 20, 11, 64, {38, 17}},
      {&characters[ 8], 30, 22, 13, 72, {32, 51}},
      {&characters[ 9], 30, 23, 13, 68, { 1, 50}},
      {&characters[10], 30, 21, 13, 76, { 4,  9}}
    }
  },
  {
    "OMNUS",
    Random("6764387728224725"),
    {
      {&characters[12], 40, 20, 40, 64, {42, 20}},
      {&characters[ 8], 36, 25, 40, 72, {48, 47}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "TRIBUTE",
    Random("5848744734731253"),
    {
      {&characters[12], 40,  5, 11, 64, {53,  1}},
      {&characters[ 9], 35, 30, 10, 72, {20,  2}},
      {&characters[10], 37, 30, 10, 68, {16, 55}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "FOUNTAIN",
    Random("6183541838474434"),
    {
      {&characters[12], 40, 20, 12, 64, { 3, 34}},
      {&characters[ 9], 30, 25, 10, 72, {47, 41}},
      {&characters[10], 30, 26, 10, 68, {42, 52}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CHUDE",
    Random("7633126817245833"),
    {
      {&characters[12], 40, 20, 40, 64, {23, 38}},
      {&characters[ 9], 40, 25, 40, 72, {57, 52}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "TRAILER",
    Random("5554144773646312"),
    {
      {&characters[12], 40, 20, 30, 64, {29, 11}},
      {&characters[10], 38, 30, 35, 72, {15, 40}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CANYON",
    Random("3122431112682557"),
    {
      {&characters[12], 40, 18, 28, 64, {49, 53}},
      {&characters[10], 39, 25, 40, 72, {14, 53}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "REPRESS",
    Random("2568412624848266"),
    {
      {&characters[12], 40, 20, 40, 64, {44, 39}},
      {&characters[10], 39, 25, 40, 72, {44, 63}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "YOKI",
    Random("3736685353284538"),
    {
      {&characters[12], 40,  5, 22, 64, {53,  8}},
      {&characters[11], 40, 15, 20, 72, {30, 22}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PASSIVE",
    Random("5471458635555317"),
    {
      {&characters[12], 40,  5, 20, 64, {25, 46}},
      {&characters[11], 40, 20, 20, 72, {51, 42}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}},
      {&characters[ 0],  0,  0,  0,  0, {-1, -1}}
    }
  }
};

GameInfo::GameInfo(const Random &_random_base) {
  Random random = _random_base;
  random_base = _random_base;
  map_size = 3;
  name = random;

  // Player 0
  players.push_back(std::make_shared<PlayerInfo>(&random));
  players[0]->set_character(12);
  players[0]->set_color(64);
  players[0]->set_intelligence(40);

  // Player 1
  players.push_back(std::make_shared<PlayerInfo>(&random));

  uint32_t val = random.random();
  if ((val & 7) != 0) {
    // Player 2
    players.push_back(std::make_shared<PlayerInfo>(&random));
    uint32_t val = random.random();
    if ((val & 3) == 0) {
      // Player 3
      players.push_back(std::make_shared<PlayerInfo>(&random));
    }
  }
}

GameInfo::GameInfo(const MissionPreset *mission_preset) {
  map_size = 3;
  name = mission_preset->name;
  random_base = mission_preset->rnd;
  for (size_t i = 0; i < 4; i++) {
    if (mission_preset->player[i].character->face == 0) continue;
    size_t character = mission_preset->player[i].character->face;
    PPlayerInfo player(new PlayerInfo(character,
                                      mission_preset->player[i].color,
                                      mission_preset->player[i].intelligence,
                                      mission_preset->player[i].supplies,
                                      mission_preset->player[i].reproduction));
    player->set_castle_pos(mission_preset->player[i].castle);
    add_player(player);
  }
}

void
GameInfo::add_player(const PPlayerInfo &player) {
  players.push_back(player);
}

void
GameInfo::add_player(size_t character, unsigned int _color,
                     unsigned int _intelligence, unsigned int _supplies,
                     unsigned int _reproduction) {
  PPlayerInfo player(new PlayerInfo(character, _color, _intelligence, _supplies,
                                    _reproduction));
  add_player(player);
}

void
GameInfo::remove_all_players() {
  players.clear();
}

PGameInfo
GameInfo::get_mission(size_t m) {
  if (m >= get_mission_count()) {
    return PGameInfo(NULL);
  }

  return PGameInfo(new GameInfo(missions + m));
}

size_t
GameInfo::get_mission_count() {
  return sizeof(missions) / sizeof(missions[0]);
}

CharacterPreset *
GameInfo::get_character(size_t character) {
  if (character >= get_character_count()) {
    return NULL;
  }

  return characters + character;
}

size_t
GameInfo::get_character_count() {
  return sizeof(characters) / sizeof(characters[0]);
}

unsigned int
GameInfo::get_color(size_t index) {
  const unsigned int default_player_colors[] = {
    64, 72, 68, 76
  };

  return default_player_colors[index];
}

PlayerInfo::PlayerInfo(Random *random_base) {
  size_t character = (((random_base->random() * 10) >> 16) + 1) & 0xFF;
  set_character(character);
  set_intelligence(((random_base->random() * 41) >> 16) & 0xFF);
  set_supplies(((random_base->random() * 41) >> 16) & 0xFF);
  set_reproduction(((random_base->random() * 41) >> 16) & 0xFF);
  set_castle_pos({-1, -1});
}

PlayerInfo::PlayerInfo(size_t character, unsigned int _color,
                       unsigned int _intelligence, unsigned int _supplies,
                       unsigned int _reproduction) {
  set_character(character);
  set_intelligence(_intelligence);
  set_supplies(_supplies);
  set_reproduction(_reproduction);
  set_color(_color);
  set_castle_pos({-1, -1});
}

void
PlayerInfo::set_castle_pos(PosPreset _castle_pos) {
  castle_pos = _castle_pos;
}

void
PlayerInfo::set_character(size_t character) {
  face = characters[character].face;
  name = characters[character].name;
  characterization = characters[character].characterization;
}

bool
PlayerInfo::has_castle() const {
  return (castle_pos.col >= 0);
}
