//
// map-geometry.h - Map geometry functions
//
// Copyright (C) 2013-2016  Jon Lund Steffensen <jonlst@gmail.com>
//
// This file is part of freeserf.
//
// freeserf is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// freeserf is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with freeserf.  If not, see <http://www.gnu.org/licenses/>.

#ifndef SRC_MAP_GEOMETRY_H_
#define SRC_MAP_GEOMETRY_H_

#include <limits>
#include <utility>

#include "src/debug.h"

// Map directions
//
//    A ______ B
//     /\    /
//    /  \  /
// C /____\/ D
//
// Six standard directions:
// RIGHT: A to B
// DOWN_RIGHT: A to D
// DOWN: A to C
// LEFT: D to C
// UP_LEFT: D to A
// UP: D to B
//
// Non-standard directions:
// UP_RIGHT: C to B
// DOWN_LEFT: B to C
typedef enum Direction {
  DirectionNone = -1,

  DirectionRight = 0,
  DirectionDownRight,
  DirectionDown,
  DirectionLeft,
  DirectionUpLeft,
  DirectionUp,
} Direction;

// Return the given direction turned clockwise a number of times.
//
// Return the resulting direction from turning the given direction
// clockwise in 60 degree increment the specified number of times.
// If times is a negative number the direction will be turned counter
// clockwise.
inline Direction turn_direction(Direction d, int times) {
  if (d == DirectionNone) {
    return DirectionNone;
  }
  int td = (static_cast<int>(d) + times) % 6;
  if (td < 0) td += 6;
  return static_cast<Direction>(td);
}

// Return the given direction reversed.
inline Direction reverse_direction(Direction d) {
  return turn_direction(d, 3);
}

// Cycle direction (clockwise or counter-clockwise)
enum class Cycle {
  CW, CCW
};

// Provides iterator for cycling through directions.
template <Cycle C>
class DirectionCycle {
 public:
  class Iterator {
    friend DirectionCycle;

   public:
    Iterator(const Iterator& it) : cycle(it.cycle), offset(it.offset) {}

    Iterator& operator=(const Iterator &that) {
      cycle = that.cycle;
      offset = that.offset;
      return *this;
    }

    Iterator& operator++() {
      if (offset < static_cast<int>(cycle.length)) ++offset;
      return *this;
    }

    bool operator==(const Iterator& rhs) const {
      return cycle == rhs.cycle && offset == rhs.offset; }
    bool operator!=(const Iterator& rhs) const {
      return !(*this == rhs); }

    Direction operator*() const;

   protected:
    Iterator(const DirectionCycle& cycle, int offset)
      : cycle(cycle), offset(offset) {}

    const DirectionCycle& cycle;
    int offset;
  };

  DirectionCycle(Direction start, unsigned int length)
    : start(start), length(length) {
    if (start == DirectionNone) {
      throw ExceptionFreeserf("Failed to init DirectionCycle with "
                              "uninitialised direction");
    }
  }
  DirectionCycle(const DirectionCycle& that)
    : start(that.start), length(that.length) {}

  DirectionCycle& operator=(const DirectionCycle &that) {
    start = that.start;
    length = that.length;
    return *this;
  }

  Iterator begin() const { return Iterator(*this, 0); }
  Iterator end() const { return Iterator(*this, length); }

  bool operator==(const DirectionCycle& rhs) const {
    return start == rhs.start && length == rhs.length; }
  bool operator!=(const DirectionCycle& rhs) const {
    return !(*this == rhs); }

 protected:
  Direction start;
  unsigned int length;
};

// Specialize for clockwise cycle
template<> inline
Direction DirectionCycle<Cycle::CW>::Iterator::operator*() const {
  return static_cast<Direction>((cycle.start + offset) % 6);
}

// Specialize for counter-clockwise cycle
template<> inline
Direction DirectionCycle<Cycle::CCW>::Iterator::operator*() const {
  return static_cast<Direction>(((cycle.start - offset) % 6 + 6) % 6);
}

// Convenience function to create clockwise direction cycle.
inline DirectionCycle<Cycle::CW>
cycle_directions_cw(Direction start = DirectionRight, unsigned int length = 6) {
  return DirectionCycle<Cycle::CW>(start, length);
}

// Convenience function to create counter-clockwise direction cycle.
inline DirectionCycle<Cycle::CCW>
cycle_directions_ccw(Direction start = DirectionUp, unsigned int length = 6) {
  return DirectionCycle<Cycle::CCW>(start, length);
}


// MapPos is a compact composition of col and row values that
// uniquely identifies a vertex in the map space. It is also used
// directly as index to map data arrays.
typedef unsigned int MapPos;
const MapPos bad_map_pos = std::numeric_limits<unsigned int>::max();


class MapGeometry {
 protected:
  unsigned int size_;

  // Derived members
  MapPos dirs[6];
  unsigned int col_size_, row_size_;

  unsigned int cols_, rows_;
  unsigned int col_mask_, row_mask_;
  unsigned int row_shift_;

 public:
  class Iterator {
    friend MapGeometry;

   public:
    Iterator(const Iterator& that)
      : geom(that.geom), pos(that.pos) {}

    Iterator& operator=(const Iterator &that) {
      geom = that.geom;
      pos = that.pos;
      return *this;
    }

    Iterator& operator++() {
      if (pos < geom->tile_count()) {
        pos++;
      }
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp(*this);
      operator++();
      return tmp;
    }

    bool operator==(const Iterator& rhs) const {
      return (geom == rhs.geom) && (pos == rhs.pos); }
    bool operator!=(const Iterator& rhs) const { return !(*this == rhs); }

    MapPos operator*() const {
      return pos;
    }

   protected:
    Iterator(const MapGeometry &geom_, MapPos pos)
      : geom(&geom_), pos(pos) {}

    const MapGeometry *geom;
    MapPos pos;
  };

  explicit MapGeometry(unsigned int size)
    : size_(size)
    , dirs{6}
    , col_size_(0)
    , row_size_(0)
    , cols_(0)
    , rows_(0)
    , col_mask_(0)
    , row_mask_(0)
    , row_shift_(0) {
    init();
  }

  MapGeometry(const MapGeometry& that)
    : size_(that.size_)
    , dirs{6}
    , col_size_(0)
    , row_size_(0)
    , cols_(0)
    , rows_(0)
    , col_mask_(0)
    , row_mask_(0)
    , row_shift_(0) {
    init();
  }

  MapGeometry& operator=(const MapGeometry &that) {
    size_ = that.size_;
    init();
    return *this;
  }

  unsigned int size() const { return size_; }
  unsigned int cols() const { return cols_; }
  unsigned int rows() const { return rows_; }
  unsigned int col_mask() const { return col_mask_; }
  unsigned int row_mask() const { return row_mask_; }
  unsigned int row_shift() const { return row_shift_; }
  unsigned int tile_count() const { return cols_ * rows_; }

  /* Extract col and row from MapPos */
  int pos_col(int pos) const { return (pos & col_mask_); }
  int pos_row(int pos) const { return ((pos >> row_shift_) & row_mask_); }

  /* Translate col, row coordinate to MapPos value. */
  MapPos pos(int x, int y) const { return ((y << row_shift_) | x); }

  /* Addition of two map positions. */
  MapPos pos_add(MapPos pos_, int x, int y) const {
    return pos((pos_col(pos_) + x) & col_mask_,
               (pos_row(pos_) + y) & row_mask_); }
  MapPos pos_add(MapPos pos_, MapPos off) const {
    return pos((pos_col(pos_) + pos_col(off)) & col_mask_,
               (pos_row(pos_) + pos_row(off)) & row_mask_); }

  // Shortest signed distance between map positions.
  int dist_x(MapPos pos1, MapPos pos2) const {
    return cols_/2 - ((cols_/2 + pos_col(pos1) - pos_col(pos2)) & col_mask_);
  }
  int dist_y(MapPos pos1, MapPos pos2) const {
    return rows_/2 - ((rows_/2 + pos_row(pos1) - pos_row(pos2)) & row_mask_);
  }

  /* Movement of map position according to directions. */
  MapPos move(MapPos pos, Direction dir) const {
    return pos_add(pos, dirs[dir]); }

  MapPos move_right(MapPos pos) const { return move(pos, DirectionRight); }
  MapPos move_down_right(MapPos pos) const {
    return move(pos, DirectionDownRight); }
  MapPos move_down(MapPos pos) const { return move(pos, DirectionDown); }
  MapPos move_left(MapPos pos) const { return move(pos, DirectionLeft); }
  MapPos move_up_left(MapPos pos) const { return move(pos, DirectionUpLeft); }
  MapPos move_up(MapPos pos) const { return move(pos, DirectionUp); }

  MapPos move_right_n(MapPos pos, int n) const {
    return pos_add(pos, dirs[DirectionRight]*n); }
  MapPos move_down_n(MapPos pos, int n) const {
    return pos_add(pos, dirs[DirectionDown]*n); }

  Iterator begin() const { return Iterator(*this, 0); }
  Iterator end() const { return Iterator(*this, tile_count()); }

  bool operator == (const MapGeometry& rhs) const {
    return this->size_ == rhs.size_; }
  bool operator != (const MapGeometry& rhs) const { return !(*this == rhs); }

 protected:
  void init() {
    if (size_ > 20) {
      throw ExceptionFreeserf("Above size 20 the map positions can no longer "
                              "fit in a 32-bit integer.");
    }

    col_size_ = 5 + size_ / 2;
    row_size_ = 5 + (size_ - 1) / 2;
    cols_ = 1 << col_size_;
    rows_ = 1 << row_size_;

    col_mask_ = cols_ - 1;
    row_mask_ = rows_ - 1;
    row_shift_ = col_size_;

    // Setup direction offsets
    dirs[DirectionRight] = 1 & col_mask_;
    dirs[DirectionLeft] = -1 & col_mask_;
    dirs[DirectionDown] = (1 & row_mask_) << row_shift_;
    dirs[DirectionUp] = (-1 & row_mask_) << row_shift_;

    dirs[DirectionDownRight] = dirs[DirectionRight] | dirs[DirectionDown];
    dirs[DirectionUpLeft] = dirs[DirectionLeft] | dirs[DirectionUp];
  }
};

#endif  // SRC_MAP_GEOMETRY_H_
