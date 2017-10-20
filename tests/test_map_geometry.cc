/*
 * test_map_geometry.cc - test for map geometry
 *
 * Copyright (C) 2016-2017  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/map-geometry.h"


TEST(MapGeometry, StandardDirectionCycle) {
  // Test standard direction cycle
  std::vector<Direction> dirs;
  for (Direction d : cycle_directions_cw()) {
    dirs.push_back(d);
  }

  std::vector<Direction> expected{
    DirectionRight, DirectionDownRight, DirectionDown,
    DirectionLeft, DirectionUpLeft, DirectionUp
  };

  EXPECT_EQ(expected, dirs);
}

TEST(MapGeometry, StandardCCWDirectionCycle) {
  // Test standard counter-clockwise direction cycle
  std::vector<Direction> dirs;
  for (Direction d : cycle_directions_ccw()) {
    dirs.push_back(d);
  }

  std::vector<Direction> expected{
    DirectionUp, DirectionUpLeft, DirectionLeft,
    DirectionDown, DirectionDownRight, DirectionRight
  };

  EXPECT_EQ(expected, dirs);
}

TEST(MapGeometry, ShorterCWDirectionCycle) {
  // Test shorter clockwise cycle
  std::vector<Direction> dirs;
  for (Direction d : cycle_directions_cw(DirectionLeft, 4)) {
    dirs.push_back(d);
  }

  std::vector<Direction> expected{
    DirectionLeft, DirectionUpLeft, DirectionUp, DirectionRight
  };

  EXPECT_EQ(expected, dirs);
}

TEST(MapGeometry, LongerCCWDirectionCycle) {
  // Test longer counter-clockwise cycle
  std::vector<Direction> dirs;
  for (Direction d : cycle_directions_ccw(DirectionLeft, 10)) {
    dirs.push_back(d);
  }

  std::vector<Direction> expected{
    DirectionLeft, DirectionDown, DirectionDownRight, DirectionRight,
    DirectionUp, DirectionUpLeft, DirectionLeft, DirectionDown,
    DirectionDownRight, DirectionRight
  };

  EXPECT_EQ(expected, dirs);
}
