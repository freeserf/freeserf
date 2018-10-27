/*
 * test_map.cc - test for classic mission map generator
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

#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>

#include "src/map.h"
#include "src/map-generator.h"
#include "src/map-geometry.h"
#include "src/random.h"


TEST(Map, ClassicMissionMapGenerator) {
  int map_size = 3;

  // Read map data from memory dump
  const char *src_dir = std::getenv("srcdir");
  if (src_dir == NULL) src_dir = ".";

  std::string path = src_dir;
  path += "/tests/data/map-memdump-1";
  std::ifstream is(path.c_str(), std::ifstream::binary);
  ASSERT_TRUE(is.good()) << "Error opening file! " << path;

  std::istreambuf_iterator<char> is_start(is), is_end;
  std::vector<char> data(is_start, is_end);

  is.close();

  // Generate corresponding map
  const MapGeometry geom(map_size);
  Map map(geom);

  Random random = Random("8667715887436237");

  ClassicMissionMapGenerator generator(map, random);
  generator.init();
  generator.generate();

  // Check that data is matching
  int expected_size = map.get_rows() * map.get_cols() * 8;
  EXPECT_EQ(expected_size, data.size()) << "Unexpected data size";

  // Find offset for map dump indices
  int index_offset = 0;
  for (unsigned int v = map.get_cols(); v != 0; v >>= 1) {
    index_offset += 1;
  }

  for (unsigned int y = 0; y < map.get_rows(); y++) {
    for (unsigned int x = 0; x < map.get_cols(); x++) {
      MapPos pos = map.pos(x, y);
      int index_1 = ((y << index_offset) | x) << 2;
      int index_2 = ((y << index_offset) | (1 << (index_offset-1)) | x) << 2;

      // Height
      int height = data[index_1+1] & 0x1f;
      EXPECT_EQ(height, generator.get_height(pos)) <<
        "Invalid height at " << x << "," << y << " is " <<
          generator.get_height(pos) << " should have been " << height;

      // Type up
      int type_up = (data[index_1+2] & 0xf0) >> 4;
      EXPECT_EQ(type_up, generator.get_type_up(pos)) <<
        "Invalid type up at " << x << "," << y << " is " <<
          generator.get_type_up(pos) << " should have been " <<
          type_up;

      // Type down
      int type_down = data[index_1+2] & 0xf;
      EXPECT_EQ(type_down, generator.get_type_down(pos)) <<
        "Invalid type down at " << x << "," << y << " is " <<
          generator.get_type_down(pos) << " should have been " <<
          type_down;

      // Object
      int object = data[index_1+3] & 0x7f;
      EXPECT_EQ(object, generator.get_obj(pos)) <<
        "Invalid object at " << x << "," << y << " is " <<
          generator.get_obj(pos) << " should have been " << object;

      if (type_up <= Map::TerrainWater3) {
        // Fish resource
        int fish_amount = data[index_2];
        EXPECT_EQ(fish_amount, generator.get_resource_amount(pos)) <<
          "Invalid fish amount at " << x << "," << y <<
            " is " << generator.get_resource_amount(pos) <<
            " should have been " << fish_amount;
      } else {
        // Resource type
        int res_type = (data[index_2] & 0xe0) >> 5;
        EXPECT_EQ(res_type, generator.get_resource_type(pos)) <<
          "Invalid resource type at " << x << "," << y <<
            " is " << generator.get_resource_type(pos) <<
            " should have been " << res_type;

        // Resource amount
        int res_amount = data[index_2] & 0x1f;
        EXPECT_EQ(res_amount, generator.get_resource_amount(pos)) <<
          "Invalid resource amount at " << x << "," << y <<
            " is " << generator.get_resource_amount(pos) <<
            " should have been " << res_amount;
      }
    }
  }
}
