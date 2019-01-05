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
#include <memory>
#include <limits>
#include <utility>

class Game;

class GameObject {
 protected:
  unsigned int index;
  Game *game;

 public:
  GameObject() = delete;
  GameObject(Game *game, unsigned int index) : index(index), game(game) {}
  GameObject(const GameObject& that) = delete;  // Copying prohibited
  GameObject(GameObject&& that) = delete;  // Moving prohibited
  virtual ~GameObject() {}

  GameObject& operator = (const GameObject& that) = delete;
  GameObject& operator = (GameObject&& that) = delete;

  Game *get_game() const { return game; }
  unsigned int get_index() const { return index; }
};

template<class T>
class Collection {
 protected:
  typedef std::map<unsigned int, T*> Objects;

  Objects objects;
  unsigned int last_object_index;
  std::set<unsigned int> free_object_indexes;
  Game *game;

 public:
  Collection() {
    game = NULL;
    last_object_index = 0;
  }

  explicit Collection(Game *_game) {
    game = _game;
    last_object_index = 0;
  }

  virtual ~Collection() {
    clear();
  }

  void clear() {
    for (std::pair<const unsigned int, T*> &kv : objects) {
      delete kv.second;
    }
    objects.clear();
  }

  T*
  allocate() {
    unsigned int new_index = 0;

    if (!free_object_indexes.empty()) {
      new_index = *free_object_indexes.begin();
      free_object_indexes.erase(free_object_indexes.begin());
    } else {
      if (last_object_index ==
          std::numeric_limits<decltype(last_object_index)>::max()) {
        return nullptr;
      }

      new_index = last_object_index;
      last_object_index++;
    }

    T *new_object = new T(game, new_index);
    objects[new_index] = new_object;

    return new_object;
  }

  bool
  exists(unsigned int index) const {
    return (objects.end() != objects.find(index));
  }

  T*
  get_or_insert(unsigned int index) {
    if (!exists(index)) {
      T *new_object = new T(game, index);
      objects[index] = new_object;

      std::set<unsigned int>::iterator i = free_object_indexes.find(index);
      if (i != free_object_indexes.end()) {
        free_object_indexes.erase(i);
      }
    }

    if (last_object_index <= index) {
      for (unsigned int i = last_object_index; i < index; i++) {
        free_object_indexes.insert(i);
      }
      last_object_index = index + 1;
    }

    return objects.at(index);
  }

  T* operator[] (unsigned int index) {
    if (!exists(index)) return nullptr;
    return objects.at(index);
  }

  const T* operator[] (unsigned int index) const {
    if (!exists(index)) return nullptr;
    return objects.at(index);
  }

  class Iterator {
   protected:
    typename Objects::iterator internal_iterator;

   public:
    explicit Iterator(typename Objects::iterator internal_iterator) {
      this->internal_iterator = internal_iterator;
    }

    Iterator&
    operator++() {
      internal_iterator++;
      return (*this);
    }

    bool
    operator==(const Iterator& right) const {
      return (internal_iterator == right.internal_iterator);
    }

    bool
    operator!=(const Iterator& right) const {
      return (!(*this == right));
    }

    T* operator*() const {
      return internal_iterator->second;
    }
  };

  class ConstIterator {
   protected:
    typename Objects::const_iterator internal_iterator;

   public:
    explicit ConstIterator(typename Objects::const_iterator it) {
     this->internal_iterator = it;
    }

    ConstIterator& operator++() {
     internal_iterator++;
     return (*this);
    }

    bool operator == (const ConstIterator& right) const {
     return internal_iterator == right.internal_iterator;
    }

    bool operator != (const ConstIterator& right) const {
     return !(*this == right);
    }

    const T* operator*() const {
     return internal_iterator->second;
    }
  };

  Iterator begin() { return Iterator(objects.begin()); }
  Iterator end() { return Iterator(objects.end()); }

  ConstIterator begin() const { return ConstIterator(objects.begin()); }
  ConstIterator end() const { return ConstIterator(objects.end()); }

  void
  erase(unsigned int index) {
    /* Decrement max_flag_index as much as possible. */
    typename Objects::iterator i = objects.find(index);
    if (i == objects.end()) {
      return;
    }
    T *object_for_deleting = objects[index];
    objects.erase(i);
    delete object_for_deleting;

    if (index == last_object_index) {
      last_object_index--;
      // ToDo: remove all empty indexes if needed
    } else {
      free_object_indexes.insert(index);
    }
  }

  size_t
  size() const { return objects.size(); }
};

#endif  // SRC_OBJECTS_H_
