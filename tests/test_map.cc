
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>

#include "src/map.h"
#include "src/map-generator.h"
#include "src/random.h"


int
main(int argc, char *argv[]) {
  /* Print number of tests for TAP */
  std::cout << "1..1" << "\n";

  int map_size = 3;

  // Read map data from memory dump
  const char *src_dir = std::getenv("srcdir");
  if (src_dir == NULL) src_dir = ".";

  std::string path = src_dir;
  path += "/tests/data/map-memdump-1";
  std::ifstream is(path.c_str(), std::ifstream::binary);
  if (!is.good()) {
    std::cerr << "Error opening file!\n";
    return 1;
  }

  std::istreambuf_iterator<char> is_start(is), is_end;
  std::vector<char> data(is_start, is_end);

  is.close();

  // Generate corresponding map
  Map map;
  map.init(map_size);

  Random random = Random("8667715887436237");

  ClassicMissionMapGenerator generator(map, random);
  generator.init();
  generator.generate();

  // Check that data is matching
  int expected_size = map.get_rows() * map.get_cols() * 8;
  if (data.size() != expected_size) {
    std::cerr << "Unexpected data size: " << data.size() << "\n";
    return 1;
  }

  // Find offset for map dump indices
  int index_offset = 0;
  for (unsigned int v = map.get_cols(); v != 0; v >>= 1) {
    index_offset += 1;
  }

  int errors = 0;
  for (int y = 0; y < map.get_rows(); y++) {
    for (int x = 0; x < map.get_cols(); x++) {
      MapPos pos = map.pos(x, y);
      int index_1 = ((y << index_offset) | x) << 2;
      int index_2 = ((y << index_offset) | (1 << (index_offset-1)) | x) << 2;

      // Height
      int height = data[index_1+1] & 0x1f;
      if (height != generator.get_height(pos)) {
        std::cerr << "Invalid height at " << x << "," << y << " is " <<
          generator.get_height(pos) << " should have been " << height << "\n";
        errors += 1;
      }

      // Type up
      int type_up = (data[index_1+2] & 0xf0) >> 4;
      if (type_up != generator.get_type_up(pos)) {
        std::cerr << "Invalid type up at " << x << "," << y << " is " <<
          generator.get_type_up(pos) << " should have been " <<
          type_up << "\n";
        errors += 1;
      }

      // Type down
      int type_down = data[index_1+2] & 0xf;
      if (type_down != generator.get_type_down(pos)) {
        std::cerr << "Invalid type down at " << x << "," << y << " is " <<
          generator.get_type_down(pos) << " should have been " <<
          type_down << "\n";
        errors += 1;
      }

      // Object
      int object = data[index_1+3] & 0x7f;
      if (object != generator.get_obj(pos)) {
        std::cerr << "Invalid object at " << x << "," << y << " is " <<
          generator.get_obj(pos) << " should have been " << object << "\n";
        errors += 1;
      }

      if (type_up <= Map::TerrainWater3) {
        // Fish resource
        int fish_amount = data[index_2];
        if (fish_amount != generator.get_resource_amount(pos)) {
          std::cerr << "Invalid fish amount at " << x << "," << y <<
            " is " << generator.get_resource_amount(pos) <<
            " should have been " << fish_amount << "\n";
          errors += 1;
        }
      } else {
        // Resource type
        int res_type = (data[index_2] & 0xe0) >> 5;
        if (res_type != generator.get_resource_type(pos)) {
          std::cerr << "Invalid resource type at " << x << "," << y <<
            " is " << generator.get_resource_type(pos) <<
            " should have been " << res_type << "\n";
          errors += 1;
        }

        // Resource amount
        int res_amount = data[index_2] & 0x1f;
        if (res_amount != generator.get_resource_amount(pos)) {
          std::cerr << "Invalid resource amount at " << x << "," << y <<
            " is " << generator.get_resource_amount(pos) <<
            " should have been " << res_amount << "\n";
          errors += 1;
        }
      }
    }
  }

  if (errors > 0) {
    std::cout << "not ok 1 - Found " << errors << " map errors!\n";
  } else {
    std::cout << "ok 1 - Map is identical to memory dump.\n";
  }
}
