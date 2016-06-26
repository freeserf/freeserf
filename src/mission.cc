/*
 * mission.cc - Predefined game mission maps
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

mission_t
mission_t::mission[12];

mission_t tutorials[] = {
  {
    "",
    random_state_t(0xD372, 0x5192, 0xF9C2),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "",
    random_state_t(0x0A28, 0x763C, 0x1BB5),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "",
    random_state_t(0x4E19, 0xD3CE, 0xE017),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "",
    random_state_t(0x271B, 0xD849, 0xF2BB),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "",
    random_state_t(0x074B, 0x505C, 0x2983),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "",
    random_state_t(0x1DD9, 0xA702, 0xFC8A),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  }
};

mission_t missions[] = {
  {
    "START",
    random_state_t("8667715887436237"),
    {
      {12, 40, 35, 30, {-1, -1}},
      { 1, 10,  5, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "STATION",
    random_state_t("2831713285431227"),
    {
      {12, 40, 30, 40, {-1, -1}},
      { 2, 12, 15, 30, {-1, -1}},
      { 3, 14, 15, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "UNITY",
    random_state_t("4632253338621228"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 2, 18, 10, 25, {-1, -1}},
      { 4, 18, 10, 25, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "WAVE",
    random_state_t("8447342476811762"),
    {
      {12, 40, 25, 40, {-1, -1}},
      { 2, 16, 20, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "EXPORT",
    random_state_t("4276472414845177"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 3, 16, 25, 20, {-1, -1}},
      { 4, 16, 25, 20, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "OPTION",
    random_state_t("2333577877517478"),
    {
      {12, 40, 30, 30, {-1, -1}},
      { 3, 20, 12, 14, {-1, -1}},
      { 5, 20, 12, 14, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "RECORD",
    random_state_t("1416541231242884"),
    {
      {12, 40, 30, 40, {-1, -1}},
      { 3, 22, 30, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SCALE",
    random_state_t("7845187715476348"),
    {
      {12, 40, 25, 30, {-1, -1}},
      { 4, 23, 25, 30, {-1, -1}},
      { 6, 24, 25, 30, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SIGN",
    random_state_t("5185768873118642"),
    {
      {12, 40, 25, 40, {-1, -1}},
      { 4, 26, 13, 30, {-1, -1}},
      { 5, 28, 13, 30, {-1, -1}},
      { 6, 30, 13, 30, {-1, -1}}
    }
  },
  {
    "ACORN",
    random_state_t("3183215728814883"),
    {
      {12, 40, 20, 16, {28, 14}},
      { 4, 30, 19, 20, { 5, 47}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CHOPPER",
    random_state_t("4376241846215474"),
    {
      {12, 40, 16, 20, {16, 42}},
      { 5, 33, 10, 20, {52, 25}},
      { 7, 34, 13, 20, {23, 12}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "GATE",
    random_state_t("6371557668231277"),
    {
      {12, 40, 23, 27, {53, 13}},
      { 5, 27, 17, 24, {27, 10}},
      { 6, 27, 13, 24, {29, 38}},
      { 7, 27, 13, 24, {15, 32}}
    }
  },
  {
    "ISLAND",
    random_state_t("8473352672411117"),
    {
      {12, 40, 24, 20, { 7, 26}},
      { 5, 20, 30, 20, { 2, 10}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "LEGION",
    random_state_t("1167854231884464"),
    {
      {12, 40, 20, 23, {19,  3}},
      { 6, 28, 16, 20, {55,  7}},
      { 8, 28, 16, 20, {55, 46}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PIECE",
    random_state_t("2571462671725414"),
    {
      {12, 40, 20, 17, {41,  5}},
      { 6, 40, 23, 20, {19, 49}},
      { 7, 37, 20, 20, {58, 52}},
      { 8, 40, 15, 15, {43, 31}}
    }
  },
  {
    "RIVAL",
    random_state_t("4563653871271587"),
    {
      {12, 40, 26, 23, {36, 63}},
      { 6, 28, 29, 40, {14, 15}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "SAVAGE",
    random_state_t("7212145428156114"),
    {
      {12, 40, 25, 12, {63, 59}},
      { 7, 29, 17, 10, {29, 24}},
      { 8, 29, 17, 10, {39, 26}},
      { 9, 32, 17, 10, {42, 49}}
    }
  },
  {
    "XAVER",
    random_state_t("4276472414435177"),
    {
      {12, 40, 25, 40, {15,  0}},
      { 7, 40, 30, 35, {34, 48}},
      { 9, 30, 30, 35, {58,  5}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "BLADE",
    random_state_t("7142748441424786"),
    {
      {12, 40, 30, 20, {13, 37}},
      { 7, 40, 20, 20, {32, 34}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "BEACON",
    random_state_t("6882188351133886"),
    {
      {12, 40,  9, 10, {14, 42}},
      { 8, 40, 16, 22, {62,  1}},
      { 9, 40, 16, 23, {32, 14}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PASTURE",
    random_state_t("7742136435163436"),
    {
      {12, 40, 20, 11, {38, 17}},
      { 8, 30, 22, 13, {32, 51}},
      { 9, 30, 23, 13, { 1, 50}},
      {10, 30, 21, 13, { 4,  9}}
    }
  },
  {
    "OMNUS",
    random_state_t("6764387728224725"),
    {
      {12, 40, 20, 40, {42, 20}},
      { 8, 36, 25, 40, {48, 47}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "TRIBUTE",
    random_state_t("5848744734731253"),
    {
      {12, 40,  5, 11, {53,  1}},
      { 9, 35, 30, 10, {20,  2}},
      {10, 37, 30, 10, {16, 55}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "FOUNTAIN",
    random_state_t("6183541838474434"),
    {
      {12, 40, 20, 12, { 3, 34}},
      { 9, 30, 25, 10, {47, 41}},
      {10, 30, 26, 10, {42, 52}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CHUDE",
    random_state_t("7633126817245833"),
    {
      {12, 40, 20, 40, {23, 38}},
      { 9, 40, 25, 40, {57, 52}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "TRAILER",
    random_state_t("5554144773646312"),
    {
      {12, 40, 20, 30, {29, 11}},
      {10, 38, 30, 35, {15, 40}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "CANYON",
    random_state_t("3122431112682557"),
    {
      {12, 40, 18, 28, {49, 53}},
      {10, 39, 25, 40, {14, 53}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "REPRESS",
    random_state_t("2568412624848266"),
    {
      {12, 40, 20, 40, {44, 39}},
      {10, 39, 25, 40, {44, 63}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "YOKI",
    random_state_t("3736685353284538"),
    {
      {12, 40,  5, 22, {53,  8}},
      {11, 40, 15, 20, {30, 22}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  },
  {
    "PASSIVE",
    random_state_t("5471458635555317"),
    {
      {12, 40,  5, 20, {25, 46}},
      {11, 40, 20, 20, {51, 42}},
      { 0,  0,  0,  0, {-1, -1}},
      { 0,  0,  0,  0, {-1, -1}}
    }
  }
};

mission_t *
mission_t::get_mission(int m) {
  if (m >= get_mission_count()) {
    return NULL;
  }
  return missions + m;
}

mission_t *
mission_t::get_tutorial(int m) {
  if (m >= get_tutorials_count()) {
    return NULL;
  }
  return tutorials + m;
}

int
mission_t::get_mission_count() {
  return sizeof(missions) / sizeof(missions[0]);
}

int
mission_t::get_tutorials_count() {
  return sizeof(tutorials) / sizeof(tutorials[0]);
}
