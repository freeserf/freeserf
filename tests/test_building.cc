/*
 * test_building.cc - Tests of the buildings
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

#include <gtest/gtest.h>

#include <vector>

#include "src/building.h"
#include "src/game.h"
#include "src/mission.h"
#include "src/serf.h"

/* The knights of a building, first one first. */
static std::vector<Serf *>
knights_of(Building *building) {
  std::vector<Serf *> knights;
  for (unsigned int index = building->get_first_knight(); index != 0;) {
    Serf *knight = building->get_game()->get_serf(index);
    knights.push_back(knight);
    index = knight->get_next();
  }
  return knights;
}

/* The knight chosen to attack leaves the building; the others stay in
   their order. */
TEST(Building, AttackerOut) {
  PGame game = GameInfo::get_mission(9)->instantiate();
  ASSERT_TRUE(game != nullptr);
  Building *castle = nullptr;
  for (int step = 0; step < 6000 && castle == nullptr; step++) {
    game->update();
    for (unsigned int i = 1; i < 100; i++) {
      Building *building = game->get_building(i);
      if (building != nullptr && building->is_done() &&
          building->get_type() == Building::TypeCastle &&
          knights_of(building).size() >= 3) {
        castle = building;
        break;
      }
    }
  }
  ASSERT_TRUE(castle != nullptr);

  std::vector<Serf *> before = knights_of(castle);
  Serf *middle = before[1];
  EXPECT_EQ(middle, castle->call_attacker_out(middle->get_index()));

  std::vector<Serf *> after = knights_of(castle);
  before.erase(before.begin() + 1);
  EXPECT_EQ(before, after);
  EXPECT_EQ(before.front(),
            castle->call_attacker_out(before.front()->get_index()));
  before.erase(before.begin());
  EXPECT_EQ(before, knights_of(castle));
  EXPECT_EQ(before.back(),
            castle->call_attacker_out(before.back()->get_index()));
  before.pop_back();
  EXPECT_EQ(before, knights_of(castle));
}
