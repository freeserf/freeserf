/*
 * ai_roadbuilder.h - class to store the state of a single pathfinding attempt
 *   Copyright 2019-2021 tlongstretch
 */

#ifndef SRC_AI_ROADBUILDER_H_
#define SRC_AI_ROADBUILDER_H_

#include <map>
#include <tuple>
#include <utility>      // to satisfy cpplinter
#include <vector>      // to satisfy cpplinter

#include "src/map.h"
#include "src/lookup.h"

typedef std::tuple<MapPos, Direction, MapPos, Direction> RoadEnds;

class RoadBuilderRoad {

 public:
  //

 protected:
  Road road;
  MapPos end1;
  MapPos end2;
  Direction dir1;
  Direction dir2;
  bool contains_castle_flag;

 public:
  RoadBuilderRoad();
  //RoadBuilderRoad(MapPos end1, Direction dir1, MapPos end2, Direction dir2);
  RoadBuilderRoad(RoadEnds ends, Road r);
  Road get_road() { return road; }
  void set_road(Road r) { road = r; }
  MapPos get_end1() { return end1; }
  void set_end1(MapPos e1) { end1 = e1; }
  MapPos get_end2() { return end2; }
  void set_end2(MapPos e2) { end2 = e2; }
  Direction get_dir1() { return dir1; }
  void set_dir1(Direction d1) { dir1 = d1; }
  Direction get_dir2() { return dir1; }
  void set_dir2(Direction d2) { dir2 = d2; }
  bool get_contains_castle_flag() { return contains_castle_flag; }
  void set_contains_castle_flag() { contains_castle_flag = true; }

 protected:
  //
};

class FlagScore {

 public:
  //
 protected:
  unsigned int flag_dist;
  unsigned int tile_dist;
  bool contains_castle_flag = false;

 public:
  FlagScore();
  FlagScore(unsigned int fd, unsigned int td);
  unsigned int get_flag_dist() { return flag_dist; }
  void set_flag_dist(unsigned int x) { flag_dist = x; }
  unsigned int get_tile_dist() { return tile_dist; }
  void set_tile_dist(unsigned int y) { tile_dist = y; }
  bool get_contains_castle_flag() { return contains_castle_flag; }
  void set_contains_castle_flag() { contains_castle_flag = true; }

 protected:
  //
};



class RoadBuilder {

 public:
  typedef std::map<RoadEnds, RoadBuilderRoad*> RoadBuilderRoads;
  typedef std::map<MapPos, FlagScore> FlagScores;

 protected:
  RoadBuilderRoads eroads;
  RoadBuilderRoads proads;  // proad is same object type as eroad, but start_pos/end1 is always same/known/fixed because it is set by roadbuilder
  FlagScores scores;
  MapPos start_pos; // this determines end1 for proad
  MapPos target_pos; // this is not included in any proad/eroad unless it happens to be a direct road solution
  //unsigned int roads_built = 0;

 public:
  RoadBuilder();
  RoadBuilder(MapPos start_pos, MapPos target_pos);
  MapPos get_start_pos() { return start_pos; }
  MapPos get_target_pos() { return target_pos; }
  void set_start_pos(MapPos pos) { start_pos = pos; }
  void set_target_pos(MapPos pos) { target_pos = pos; }
  //void set_score(MapPos pos, unsigned int flag_dist, unsigned int tile_dist) {
  void set_score(MapPos pos, unsigned int flag_dist, unsigned int tile_dist, bool contains_castle_flag) {
    Log::Debug["ai_roadbuilder"] << "inside RoadBuilder::set_score(" << pos << ", " << flag_dist << ", " << tile_dist << ", " << contains_castle_flag << ")";
    FlagScore score;
    if (scores.count(pos)) {
      Log::Debug["ai_roadbuilder"] << "inside RoadBuilder::set_score, existing score of flag_dist " << scores.at(pos).get_flag_dist() << ", tile_dist "
          << scores.at(pos).get_tile_dist() << " found for pos " << pos << ", removing it...";
      scores.erase(pos);
    }
    score.set_flag_dist(flag_dist);
    score.set_tile_dist(tile_dist);
    if (contains_castle_flag) { score.set_contains_castle_flag(); }
    scores.insert(std::make_pair(pos, score));
    Log::Debug["ai_roadbuilder"] << "inside RoadBuilder::set_score, done set_score for pos " << pos;
  }
  bool has_score(MapPos pos) {
    if (scores.count(pos) == 0) {
      Log::Debug["ai_roadbuilder"] << "inside RoadBuilder has_score(" << pos << "), returning false";
      return false;
    }
    else {
      Log::Debug["ai_roadbuilder"] << "inside RoadBuilder has_score(" << pos << "), returning true";
      return true;
    }
  }
  FlagScore get_score(MapPos pos) {
    Log::Debug["ai_roadbuilder"] << "called RoadBuilder::get_score() for MapPos " << pos;
    if (scores.count(pos) == 0) {
      Log::Debug["ai_roadbuilder"] << "ERROR!  scores.at(" << pos << ") is nullptr!  this is a crash bug";
      Log::Debug["ai_roadbuilder"] << "ERROR!  unable to find score for pos " << pos << ".  It should be scored already.  FIND OUT WHY";
      //std::this_thread::sleep_for(std::chrono::milliseconds(10000));
      Log::Debug["ai_roadbuilder"] << "returning bogus super-high score to avoid the crash this used to cause";
      //std::this_thread::sleep_for(std::chrono::milliseconds(2000));
      FlagScore bogus_score;
      // I was originally setting -1 here, but because other lengths/penalties are ADDED to this score, it wraps back around
      //     to be a very-low/good score again!   Instead, setting to HALF of -1 (4294967295) ... which is 2147483647.5 or 2147483648
      bogus_score.set_flag_dist(bad_score);
      bogus_score.set_tile_dist(bad_score);
      return bogus_score;
    }
    return scores.at(pos);
  }
  //unsigned int get_roads_built() { return roads_built; }
  bool has_eroad(MapPos end1, Direction dir1, MapPos end2, Direction dir2) {
    if (eroads.count(RoadEnds(end1, dir1, end2, dir2)) || eroads.count(RoadEnds(end1, dir1, end2, dir2))) { return true; }
    return false;
  }
  /*
  bool has_proad(MapPos end1, Direction dir1, MapPos end2, Direction dir2) {
    if (proads.count(RoadEnds(end1, dir1, end2, dir2)) || proads.count(RoadEnds(end1, dir1, end2, dir2))) { return true; }
    return false;
  }
  */
  bool has_proad(MapPos end_pos, Direction start_dir, Direction end_dir) {
    if (proads.count(RoadEnds(start_pos, start_dir, end_pos, end_dir))) {
      return true;
    }
    return false;
  }

  // because the RoadEnds are never used directly (I think?), rather only xroad->get_ calls,
  //   I don't think this should return a std::pair including the RoadEnds, only the RoadBuilderRoad itself
  RoadBuilderRoads get_eroads() { return eroads; }
  RoadBuilderRoads get_proads() { return proads; }

  // search all ERoads for any with specified end1/dir1, OR end2/dir, and return the list as vector
  std::vector<RoadBuilderRoad*> get_eroads(MapPos end, Direction dir) {
    std::vector<RoadBuilderRoad*> found_eroads;
    for (std::pair<RoadEnds, RoadBuilderRoad *> eroad : eroads) {
      RoadEnds ends = eroad.first;
      MapPos end1 = std::get<0>(ends);
      Direction dir1 = std::get<1>(ends);
      MapPos end2 = std::get<2>(ends);
      Direction dir2 = std::get<3>(ends);
      if (eroads.count(RoadEnds(end, dir, end2, dir2))) { found_eroads.push_back(eroads.at(RoadEnds(end, dir, end2, dir2))); }
      if (eroads.count(RoadEnds(end2, dir2, end, dir))) { found_eroads.push_back(eroads.at(RoadEnds(end2, dir2, end, dir))); }
    }
    if (found_eroads.size() == 0) {
      Log::Debug["ai_roadbuilder"] << "ERROR could not find any eroad with one end:dir " << end << ":" << NameDirection[dir];
    }
    return found_eroads;
  }

  // return a specific known ERoad by individual identifiers  (convenience)
  RoadBuilderRoad* get_eroad(MapPos end1, Direction dir1, MapPos end2, Direction dir2) {
    if (eroads.count(RoadEnds(end1, dir1, end2, dir2))) { return eroads.at(RoadEnds(end1, dir1, end2, dir2)); }
    if (eroads.count(RoadEnds(end2, dir2, end1, dir1))) { return eroads.at(RoadEnds(end2, dir2, end1, dir1)); }
    Log::Debug["ai_roadbuilder"] << "could not find eroad with ends:dirs " << end1 << ":" << NameDirection[dir1] << ", " << end2 << ":" << NameDirection[dir2] << " or inverse";
    return nullptr;
  }

  // return a specific known ERoad by RoadEnds (convenience)
  RoadBuilderRoad* get_eroad(RoadEnds ends) {
    MapPos end1 = std::get<0>(ends);
    Direction dir1 = std::get<1>(ends);
    MapPos end2 = std::get<2>(ends);
    Direction dir2 = std::get<3>(ends);
    if (eroads.count(RoadEnds(end1, dir1, end2, dir2))) { return eroads.at(RoadEnds(end1, dir1, end2, dir2)); }
    if (eroads.count(RoadEnds(end2, dir2, end1, dir1))) { return eroads.at(RoadEnds(end2, dir2, end1, dir1)); }
    Log::Debug["ai_roadbuilder"] << "could not find eroad with ends:dirs " << end1 << ":" << NameDirection[dir1] << ", " << end2 << ":" << NameDirection[dir2] << " or inverse";
    return nullptr;
  }

  // find and return a specific PRoad by end_pos
  RoadBuilderRoad* get_proad(MapPos end_pos) {
    for (std::pair<RoadEnds, RoadBuilderRoad *> proad_pair : proads) {
      RoadBuilderRoad *rb_road = proad_pair.second;
      if (end_pos == rb_road->get_end2()) {
        return rb_road;
      }
    }
    Log::Debug["ai_roadbuilder"] << "could not find proad with end2/end_pos :dirs " << end_pos;
    return nullptr;
  }

  /*
  RoadBuilderRoad* get_proad(MapPos end1, Direction dir1, MapPos end2, Direction dir2) {
    if (proads.count(RoadEnds(end1, dir1, end2, dir2))) { return proads.at(RoadEnds(end1, dir1, end2, dir2)); }
    if (proads.count(RoadEnds(end2, dir2, end1, dir1))) { return proads.at(RoadEnds(end2, dir2, end1, dir1)); }
    Log::Info["ai_util"] << "ERROR could not find proad with ends:dirs " << end1 << ":" << NameDirection[dir1] << ", " << end2 << ":" << NameDirection[dir2] << " or inverse";
    return nullptr;
  }
  */

  /* I don't think this is needed?
  std::pair<MapPos, Direction> get_rb_road_other_end(MapPos end, Direction dir) {
    // search all ERoads for one that has specified end1/dir1, OR end2/dir2 and return the other end_pos and end_dir
    for (std::pair<RoadEnds, RoadBuilderRoad *> record : eroads) {
      // the RoadBuilderRoad * is not used here, only the key/index ERoadEnds is used to uniquely identify an ERoad
      RoadEnds ends = record.first;
      if (std::get<0>(ends) == end && std::get<1>(ends) == dir) {
        Log::Debug["ai"] << "found RBRoad (forwards) with one end pos " << end << " and dir " << NameDirection[dir];
        return std::make_pair(std::get<2>(ends), std::get<3>(ends));
      }
      if (std::get<2>(ends) == end && std::get<3>(ends) == dir) {
        Log::Debug["ai"] << "found RBRoad (backward) with one end pos " << end << " and dir " << NameDirection[dir];
        return std::make_pair(std::get<0>(ends), std::get<1>(ends));
      }
    }
    Log::Warn["ai_util"] << "ERROR could not find eroad with ends:dirs " << end << ":" << NameDirection[dir];
    return std::make_pair(bad_map_pos, DirectionNone);
  }
  */

  /*
  void new_eroad(MapPos end1, Direction dir1, MapPos end2, Direction dir2, Road road) {
    Log::Info["ai_roadbuilder"] << " inside new_eroad with end1 " << end1 << ", end2 " << end2 << ", dir1 " << dir1 << ", dir2 " << dir2;
    //ERoadEnds ends = ERoadEnds(end1, dir1, end2, dir2);
    RoadEnds ends = RoadEnds(end1, dir1, end2, dir2);
    RoadBuilderRoad *rb_road = new RoadBuilderRoad(end1, dir1, end2, dir2);
    rb_road->set_road(road);
    eroads[ends] = rb_road;
  }
  */
  void new_eroad(RoadEnds ends, Road road) {
    Log::Debug["ai_roadbuilder"] << " inside new_eroad";
    MapPos end1 = std::get<0>(ends);
    Direction dir1 = std::get<1>(ends);
    MapPos end2 = std::get<2>(ends);
    Direction dir2 = std::get<3>(ends);
    Log::Debug["ai_roadbuilder"] << " inside new_eroad with end1 " << end1 << ", dir1 " << NameDirection[dir1] << ", end2 " << end2 << ", dir2 " << NameDirection[dir2];
    //RoadBuilderRoad *rb_road = new RoadBuilderRoad(end1, dir1, end2, dir2);
    RoadBuilderRoad *rb_road = new RoadBuilderRoad(ends, road);
    //rb_road->set_road(road);
    eroads[ends] = rb_road;
  }

  //void new_proad(MapPos end1, Direction dir1, MapPos end2, Direction dir2, Road road) {
  void new_proad(RoadEnds ends, Road road) {
    Log::Debug["ai_roadbuilder"] << " inside new_proad";
    MapPos start_pos = std::get<0>(ends);
    Direction start_dir = std::get<1>(ends);
    MapPos end_pos = std::get<2>(ends);
    Direction end_dir = std::get<3>(ends);
    Log::Debug["ai_roadbuilder"] << " inside new_proad with start_pos " << start_pos << ", start_dir " << NameDirection[start_dir] << ", end_pos " << end_pos << ", end_dir " << NameDirection[end_dir];
    //RoadBuilderRoad *rb_road = new RoadBuilderRoad(end1, dir1, end2, dir2);
    RoadBuilderRoad *rb_road = new RoadBuilderRoad(ends, road);
    //rb_road->set_road(road);
    proads[ends] = rb_road;
  }

 protected:
  //
}; // class Roadbuilder


#endif  // SRC_AI_ROADBUILDER_H_
