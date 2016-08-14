/*
 * test_map_geometry.cc - TAP test for map geometry
 *
 * Copyright (C) 2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <iostream>
#include <vector>

#include "src/map-geometry.h"

int
main(int argc, char *argv[]) {
  // Print number of tests for TAP
  std::cout << "1..4" << "\n";

  {
    // Test standard direction cycle
    std::vector<Direction> dirs;
    for (Direction d : cycle_directions_cw()) {
      dirs.push_back(d);
    }

    std::vector<Direction> expected{
      DirectionRight, DirectionDownRight, DirectionDown,
      DirectionLeft, DirectionUpLeft, DirectionUp
    };

    if (dirs == expected) {
      std::cout << "ok 1 - Correct directions produced\n";
    } else {
      std::cout << "not ok 1 - Incorrect directions produced\n";
    }
  }

  {
    // Test standard counter-clockwise direction cycle
    std::vector<Direction> dirs;
    for (Direction d : cycle_directions_ccw()) {
      dirs.push_back(d);
    }

    std::vector<Direction> expected{
      DirectionUp, DirectionUpLeft, DirectionLeft,
      DirectionDown, DirectionDownRight, DirectionRight
    };

    if (dirs == expected) {
      std::cout << "ok 2 - Correct directions produced\n";
    } else {
      std::cout << "not ok 2 - Incorrect directions produced\n";
    }
  }

  {
    // Test shorter clockwise cycle
    std::vector<Direction> dirs;
    for (Direction d : cycle_directions_cw(DirectionLeft, 4)) {
      dirs.push_back(d);
    }

    std::vector<Direction> expected{
      DirectionLeft, DirectionUpLeft, DirectionUp, DirectionRight
    };

    if (dirs == expected) {
      std::cout << "ok 3 - Correct directions produced\n";
    } else {
      std::cout << "not ok 3 - Incorrect directions produced\n";
    }
  }

  {
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

    if (dirs == expected) {
      std::cout << "ok 4 - Correct directions produced\n";
    } else {
      std::cout << "not ok 4 - Incorrect directions produced\n";
    }
  }
}
