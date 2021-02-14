/*
 * map-generator.h - Map generator header
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

#ifndef SRC_MAP_GENERATOR_H_
#define SRC_MAP_GENERATOR_H_

#include <memory>
#include <vector>

#include "src/map.h"
#include "src/random.h"
#include "src/lookup.h"  // for CustomMapGeneratorOptions

/* Interface for map generators. */
class MapGenerator {
 public:
  typedef enum HeightGenerator {
    HeightGeneratorMidpoints = 0,
    HeightGeneratorDiamondSquare
  } HeightGenerator;

 public:
  virtual ~MapGenerator() {}
  virtual void generate() = 0;

  virtual int get_height(MapPos pos) const = 0;
  virtual Map::Terrain get_type_up(MapPos pos) const = 0;
  virtual Map::Terrain get_type_down(MapPos pos) const = 0;
  virtual Map::Object get_obj(MapPos pos) const = 0;
  virtual Map::Minerals get_resource_type(MapPos pos) const = 0;
  virtual int get_resource_amount(MapPos pos) const = 0;
  virtual const std::vector<Map::LandscapeTile> &get_landscape() const = 0;
};

/* Classic map generator as in original game. */
class ClassicMapGenerator : public MapGenerator {
 public:
  static const int default_max_lake_area;
  static const int default_water_level;
  static const int default_terrain_spikyness;

  ClassicMapGenerator(const Map &map, const Random &random);
  void init(HeightGenerator height_generator, bool preserve_bugs,
            int max_lake_area = default_max_lake_area,
            int water_level = default_water_level,
            int terrain_spikyness = default_terrain_spikyness);
  void generate();

  int get_height(MapPos pos) const { return tiles[pos].height; }
  Map::Terrain get_type_up(MapPos pos) const {
    return tiles[pos].type_up; }
  Map::Terrain get_type_down(MapPos pos) const {
    return tiles[pos].type_down; }
  Map::Object get_obj(MapPos pos) const { return tiles[pos].obj; }
  Map::Minerals get_resource_type(MapPos pos) const {
    return tiles[pos].mineral; }
  int get_resource_amount(MapPos pos) const {
    return tiles[pos].resource_amount; }
  virtual const std::vector<Map::LandscapeTile> &get_landscape() const {
    return tiles; }

 protected:
  const Map &map;
  Random rnd;

  std::vector<Map::LandscapeTile> tiles;
  std::vector<int> tags;
  HeightGenerator height_generator;
  bool preserve_bugs;

  unsigned int water_level;
  unsigned int max_lake_area;
  int terrain_spikyness;

  uint16_t random_int();
  MapPos pos_add_spirally_random(MapPos pos, int mask);

  bool is_water_tile(MapPos pos) const;
  bool is_in_water(MapPos pos) const;

  void init_heights_squares();
  int calc_height_displacement(int avg, int base, int offset);
  void init_heights_midpoints();
  void init_heights_diamond_square();
  bool adjust_map_height(int h1, int h2, MapPos pos);
  void clamp_heights();

  bool expand_water_position(MapPos pos);
  void expand_water_body(MapPos pos);
  void create_water_bodies();

  void heights_rebase();
  void init_types();
  void clear_all_tags();
  void remove_islands();
  void heights_rescale();

  void seed_terrain_type(Map::Terrain old, Map::Terrain seed,
                         Map::Terrain new_);
  void change_shore_water_type();
  void change_shore_grass_type();

  bool check_desert_down_triangle(MapPos pos);
  bool check_desert_up_triangle(MapPos pos);
  void create_deserts();

  void create_crosses();
  void create_objects();

  bool hexagon_types_in_range(MapPos pos, Map::Terrain min, Map::Terrain max);
  void create_random_object_clusters(int num_clusters, int objs_in_cluster,
                                     int pos_mask, Map::Terrain type_min,
                                     Map::Terrain type_max, int obj_base,
                                     int obj_mask);

  void expand_mineral_cluster(int iters, MapPos pos, int *index,
                              int amount, Map::Minerals type);
  void create_random_mineral_clusters(int num_clusters, Map::Minerals type,
                                      Map::Terrain min, Map::Terrain max);
  void create_mineral_deposits();

  void clean_up();
};

/* Classic map generator that generates identical maps for missions. */
class ClassicMissionMapGenerator : public ClassicMapGenerator {
 public:
  ClassicMissionMapGenerator(const Map &map, const Random &random);
  void init();
};




// the usual ClassicMapGenerator but allows direct tuning via
//  sliders in the game init screen - EditMapGenerator popup
class CustomMapGenerator : public ClassicMapGenerator {
 public:

  static const int default_max_lake_area = 14;
  static const int default_water_level = 20;
  static const int default_terrain_spikyness = 0x9999;

  CustomMapGeneratorOptions custom_map_generator_options;

  CustomMapGenerator(const Map& map, const Random& random);

  void init(HeightGenerator height_generator, bool preserve_bugs,
            CustomMapGeneratorOptions _custom_map_generator_options,
            int max_lake_area = default_max_lake_area,
            int water_level = default_water_level,
            int terrain_spikyness = default_terrain_spikyness){

    Log::Info["map-generator.h"] << "inside CustomMapGenerator::init";

    for (int x = 0; x < 23; x++){
      Log::Info["map-generator.h"] << "inside CustomMapGenerator::init, copying values x " << x << "_.opt[x] = " << _custom_map_generator_options.opt[x] << " as string " << std::to_string(_custom_map_generator_options.opt[x]);
      custom_map_generator_options.opt[x] = _custom_map_generator_options.opt[x];
    }
/*
    for (int x = 0; x < 23; x++){
      Log::Info["map-generator.h"] << "inside CustomMapGenerator::init, BEFORE reset opt" << x << " = " << custom_map_generator_options.opt[x];
    }

    // wtf
    for (int x = 0; x < 23; x++){
      custom_map_generator_options.opt[x] = 1.00;
    }
    custom_map_generator_options.opt[CustomMapGeneratorOption::MountainGold] = 2.00;
    custom_map_generator_options.opt[CustomMapGeneratorOption::MountainIron] = 4.00;
    custom_map_generator_options.opt[CustomMapGeneratorOption::MountainCoal] = 9.00;
    custom_map_generator_options.opt[CustomMapGeneratorOption::MountainStone] = 2.00;

    for (int x = 0; x < 23; x++){
      Log::Info["map-generator.h"] << "inside CustomMapGenerator::init, AFTER reset opt" << x << " = " << custom_map_generator_options.opt[x];
    }
*/
  }

  void generate();

 protected:
  bool expand_water_position(MapPos pos);  // modified to allow changing of water_level
  void create_water_bodies();  // modified to to allow changing of water_level and to call the override of expand_water_body that allows changing of max_lake_area
  void expand_water_body(MapPos pos);  // modified to allow changing of max_lake_area
  void create_deserts();  // modified to allow changing of desert frequency
  void create_objects();  // modified to allow changing of all object frequency / quantity
  void create_mineral_deposits();  // modified to allow changing of all mineral deposit frequency / quantity

  // THESE FUNCTIONS BELOW ARE ALSO DEFINED IN map-generator.h and interface.h !!!!
  // 65500 (not 65535) / 2 = 32750
  double slider_uint16_to_double(uint16_t val){ return double(double(val) / double(32750)); }
  uint16_t slider_double_to_uint16(double val){ return uint16_t(val * 32750); }
  // 65500 (not 65535) / 17 = 3852.94  (trying 3852)
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(val / 3852)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(double val){ return uint16_t(val * 3853); }

};


/*
// Desert map generator.  
// from jonls Freeserf Pull request 'Alternative map generator (WIP) #263'
//   at  https://github.com/freeserf/freeserf/pull/263
class DesertMapGenerator : public ClassicMapGenerator {
 public:
  static const int default_max_lake_area;
  static const int default_water_level;

  DesertMapGenerator(const Map& map, const Random& random);
  void init(int max_lake_area = default_max_lake_area,
            int water_level = default_water_level,
            int terrain_spikyness = default_terrain_spikyness);
  void generate();

 protected:
  void init_heights_squares();

  void replace_all_types(Map::Terrain old, Map::Terrain new_);

  void change_shore_water_type();
  void change_shore_grass_type();

  void create_oases();

  void create_objects();

  void create_link_patches();
  void lower_desert_elevation();
};
*/ 

#endif  // SRC_MAP_GENERATOR_H_
