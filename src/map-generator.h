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

// if I actually end up leaving all these values at 1
//  then get rid of the defaults entirely
  const double default_desert_frequency = 1.00;

  const double default_trees_both1 = 1.00;
  const double default_trees_deciduous = 1.00;
  const double default_trees_pine = 1.00;
  const double default_trees_both2 = 1.00;
  const double default_stonepile_dense = 1.00;
  const double default_stonepile_sparse = 1.00;
  const double default_junk_grass_trees_dead = 1.00;
  const double default_junk_grass_sandstone = 1.00;
  const double default_junk_water_trees = 1.00;
  const double default_junk_grass_stub_trees = 1.00;
  const double default_junk_grass_small_boulders = 1.00;
  const double default_junk_desert_cadavers = 1.00;
  const double default_junk_desert_cacti = 1.00;
  const double default_junk_water_boulders = 1.00;
  const double default_junk_desert_palm_trees = 1.00;

  double desert_frequency;

  double trees_both1;
  double trees_deciduous;
  double trees_pine;
  double trees_both2;
  double stonepile_dense;
  double stonepile_sparse;
  double junk_grass_trees_dead;
  double junk_grass_sandstone;
  double junk_water_trees;
  double junk_grass_stub_trees;
  double junk_grass_small_boulders;
  double junk_desert_cadavers;
  double junk_desert_cacti;
  double junk_water_boulders;
  double junk_desert_palm_trees;
  
  CustomMapGenerator(const Map& map, const Random& random);

  void init(HeightGenerator height_generator, bool preserve_bugs,
            int max_lake_area = default_max_lake_area,
            int water_level = default_water_level,
            int terrain_spikyness = default_terrain_spikyness){

    desert_frequency = default_desert_frequency;

    trees_both1 = default_trees_both1;
    trees_deciduous = default_trees_deciduous;
    trees_pine = default_trees_pine;
    trees_both2 = default_trees_both2;
    stonepile_dense = default_stonepile_dense;
    stonepile_sparse = default_stonepile_sparse;
    junk_grass_trees_dead = default_junk_grass_trees_dead;
    junk_grass_sandstone = default_junk_grass_sandstone;
    junk_water_trees = default_junk_water_trees;
    junk_grass_stub_trees = default_junk_grass_stub_trees;
    junk_grass_small_boulders = default_junk_grass_small_boulders;
    junk_desert_cadavers = default_junk_desert_cadavers;
    junk_desert_cacti = default_junk_desert_cacti;
    junk_water_boulders = default_junk_water_boulders;
    junk_desert_palm_trees = default_junk_desert_palm_trees;

  }

  

  /*
  // Add either tree or pine.
  // Add only trees.
  // Add only pines.
  // Add either tree or pine.
  // Create dense clusters of stone.
  // Create sparse clusters.
  // Create dead trees.
  // Create sandstone boulders.
  // Create trees submerged in water.
  // Create tree stubs.
  // Create small boulders.
  // Create animal cadavers in desert.
  // Create cacti in desert.
  // Create boulders submerged in water.
  // Create palm trees in desert.
*/
  void set_trees_both1(uint16_t val){ trees_both1 = slider_uint16_to_double(val); }
  void set_trees_deciduous(uint16_t val){ trees_deciduous = slider_uint16_to_double(val); }
  void set_trees_pine(uint16_t val){ trees_pine = slider_uint16_to_double(val); }
  void set_trees_both2(uint16_t val){ trees_both2 = slider_uint16_to_double(val); }
  void set_stonepile_dense(uint16_t val){ stonepile_dense = slider_uint16_to_double(val); }
  void set_stonepile_sparse(uint16_t val){ stonepile_sparse = slider_uint16_to_double(val); }
  void set_junk_grass_trees_dead(uint16_t val){ junk_grass_trees_dead = slider_uint16_to_double(val); }
  void set_junk_grass_sandstone(uint16_t val){ junk_grass_sandstone = slider_uint16_to_double(val); }
  void set_junk_water_trees(uint16_t val){ junk_water_trees = slider_uint16_to_double(val); }
  void set_junk_grass_stub_trees(uint16_t val){ junk_grass_stub_trees = slider_uint16_to_double(val); }
  void set_junk_grass_small_boulders(uint16_t val){ junk_grass_small_boulders = slider_uint16_to_double(val); }
  void set_junk_desert_cadavers(uint16_t val){ junk_desert_cadavers = slider_uint16_to_double(val); }
  void set_junk_desert_cacti(uint16_t val){ junk_desert_cacti = slider_uint16_to_double(val); }
  void set_junk_water_boulders(uint16_t val){ junk_water_boulders = slider_uint16_to_double(val); }
  void set_junk_desert_palm_trees(uint16_t val){ junk_desert_palm_trees = slider_uint16_to_double(val); }

  double get_trees_both1(){ return slider_double_to_uint16(trees_both1); }
  double get_trees_deciduous(){ return slider_double_to_uint16(trees_deciduous); }
  double get_trees_pine(){ return slider_double_to_uint16(trees_pine); }
  double get_trees_both2(){ return slider_double_to_uint16(trees_both2); }
  double get_stonepile_dense(){ return slider_double_to_uint16(stonepile_dense); }
  double get_stonepile_sparse(){ return slider_double_to_uint16(stonepile_sparse); }
  double get_junk_grass_trees_dead(){ return slider_double_to_uint16(junk_grass_trees_dead); }
  double get_junk_grass_sandstone(){ return slider_double_to_uint16(junk_grass_sandstone); }
  double get_junk_water_trees(){ return slider_double_to_uint16(junk_water_trees); }
  double get_junk_grass_stub_trees(){ return slider_double_to_uint16(junk_grass_stub_trees); }
  double get_junk_grass_small_boulders(){ return slider_double_to_uint16(junk_grass_small_boulders); }
  double get_junk_desert_cadavers(){ return slider_double_to_uint16(junk_desert_cadavers); }
  double get_junk_desert_cacti(){ return slider_double_to_uint16(junk_desert_cacti); }
  double get_junk_water_boulders(){ return slider_double_to_uint16(junk_water_boulders); }
  double get_junk_desert_palm_trees(){ return slider_double_to_uint16(junk_desert_palm_trees); }

 protected:
  void create_deserts();  // modified to allow changing of desert frequency
  void create_objects();  // modified to allow changing of all object frequency / quantity
  void create_mineral_deposits();  // modified to allow changing of all mineral deposit frequency / quantity

  // THESE FUNCTIONS BELOW ARE ALSO DEFINED IN popup.h !!!!
  // 65535 / 2 = 32767.5
  double slider_uint16_to_double(uint16_t val){ return double(32767 / val); }
  uint16_t slider_double_to_uint16(uint16_t val){ return uint16_t(val * 2); }
  // 65535 / 17 = 3855
  double slider_mineral_uint16_to_int_to_double(uint16_t val){ return double(int(3855 / val)); }  // convert to int midway so there are no fractional values
  uint16_t slider_mineral_double_to_uint16(uint16_t val){ return uint16_t(val * 17); }


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
