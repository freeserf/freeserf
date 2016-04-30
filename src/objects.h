/*
 * objects.h - Game objects collection template
 *
 * Copyright (C) 2015  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_OBJECTS_H_
#define SRC_OBJECTS_H_

#include <map>
#include <algorithm>
#include <set>
#include <climits>

class game_t;

class game_object_t {
 protected:
  unsigned int index;
  game_t *game;

 public:
  game_object_t(game_t *game, unsigned int index) : index(index), game(game) {}
  virtual ~game_object_t() {}

  game_t *get_game() const { return game; }
  unsigned int get_index() const { return index; }
};

template<class object_t>
class collection_t {
 protected:
  typedef std::map<unsigned int, object_t*> objects_t;

  objects_t objects;
  unsigned int last_object_index;
  std::set<unsigned int> free_object_indexes;
  game_t *game;

 public:
  explicit collection_t(game_t *game) {
    this->game = game;
    last_object_index = 0;
  }

  object_t *
  allocate() {
    unsigned int new_index = 0;

    if (!free_object_indexes.empty()) {
      new_index = *free_object_indexes.begin();
      free_object_indexes.erase(free_object_indexes.begin());
    } else {
      if (last_object_index == UINT_MAX) {
        return NULL;
      }

      new_index = last_object_index;
      last_object_index++;
    }

    object_t *new_object = new object_t(game, new_index);
    objects[new_index] = new_object;

    return new_object;
  }

  bool
  exists(unsigned int index) {
    return (objects.end() != objects.find(index));
  }

  object_t*
  get_or_insert(unsigned int index) {
    object_t *object = NULL;

    if (exists(index)) {
      object = objects[index];
    } else {
      object = new object_t(game, index);
      objects[index] = object;

      std::set<unsigned int>::iterator i = free_object_indexes.find(index);
      if (i != free_object_indexes.end()) {
        free_object_indexes.erase(i);
      }
    }

    if (last_object_index <= index) {
      for (unsigned int i = last_object_index; i < index; i++) {
        free_object_indexes.insert(index);
      }
      last_object_index = index + 1;
    }

    return object;
  }

  object_t*
  operator[] (unsigned int index) {
    if (!exists(index)) {
      return NULL;
    }
    return objects[index];
  }

  class iterator {
   protected:
    typename objects_t::iterator internal_iterator;

   public:
    explicit iterator(typename objects_t::iterator internal_iterator) {
      this->internal_iterator = internal_iterator;
    }

    iterator&
    operator++() {
      internal_iterator++;
      return (*this);
    }

    bool
    operator==(const iterator& right) const {
      return (internal_iterator == right.internal_iterator);
    }

    bool
    operator!=(const iterator& right) const {
      return (!(*this == right));
    }

    object_t*
    operator*() const {
      return internal_iterator->second;
    }
  };

  iterator
  begin() {
    return iterator(objects.begin());
  }

  iterator
  end() {
    return iterator(objects.end());
  }

  void
  erase(unsigned int index) {
    /* Decrement max_flag_index as much as possible. */
    typename objects_t::iterator i = objects.find(index);
    if (i == objects.end()) {
      return;
    }
    object_t *object = i->second;
    objects.erase(i);
    delete object;

    if (index == last_object_index) {
      last_object_index--;
      // ToDo: remove all empty indexes if needed
    } else {
      free_object_indexes.insert(index);
    }
  }

  size_t
  size() { return objects.size(); }
};

#endif  // SRC_OBJECTS_H_
