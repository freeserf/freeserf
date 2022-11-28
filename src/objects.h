/*
 * objects.h - Game objects collection template
 *
 * Copyright (C) 2015-2019  Wicked_Digger <wicked_digger@mail.ru>
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

#include <vector>
#include <algorithm>
#include <list>
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

template<class T, size_t growth>
class Collection {
 protected:
  typedef std::vector<T*> Objects;
  typedef std::list<unsigned int> FObjects;

  Objects objects;
  unsigned int last_object_index;
  FObjects free_object_indexes;
  Game *game;

 public:
  Collection() {
    game = NULL;
    last_object_index = 0;
  }

  Collection(const Collection& other) {
    objects = other.objects;
    last_object_index = other.last_object_index;
    free_object_indexes = other.free_object_indexes;
    game = other.game;
  }

  explicit Collection(Game *_game) {
    game = _game;
    last_object_index = 0;
  }

  virtual ~Collection() {
  }

  void clear() {
    for (T *&obj : objects) {
      if (obj != nullptr) {
        delete obj;
      }
    }
    objects.clear();
  }

  T*
  allocate() {
    // I assume this means that Inventory and other index #s are never re-used?
    //  when an Inventory is removed, its number will not be re-used as far as I can
    //  see, the removed inventory is .erased, and a new one is created at the next
    //  value (inventories.size + 1)
    // I think I recall reading somewhere that there is a limitation of 255 inventories in the game?
    //  and this likely means that it isn't a limit of 255 Inventories concurrently, but 255 Inventories
    //  EVER, even if some have been removed
    unsigned int new_index = 0;
    T *new_object = nullptr;

    if (!free_object_indexes.empty()) {
      new_index = free_object_indexes.front();
      free_object_indexes.pop_front();
      new_object = new T(game, new_index);
      objects[new_index] = new_object;
    } else {
      new_index = static_cast<unsigned int>(objects.size());
      if (objects.size() == objects.capacity()) {
        objects.reserve(objects.capacity() + growth);
      }
      new_object = new T(game, new_index);
      objects.push_back(new_object);
    }

    return new_object;
  }

  bool
  exists(unsigned int index) const {
    if (index >= objects.size()) {
      return false;
    }
    return (objects[index] != nullptr);
  }

  T*
  get_or_insert(unsigned int index) {
    T *object = nullptr;
    if (index < objects.size()) {
      object = objects[index];
      if (object == nullptr) {
        FObjects::iterator i = std::find(free_object_indexes.begin(),
                                         free_object_indexes.end(), index);
        free_object_indexes.erase(i);
        object = new T(game, index);
        objects[index] = object;
      }
    } else {
      if (objects.size() == objects.capacity()) {
        objects.reserve(objects.capacity() + growth);
      }
      for (size_t i = objects.size(); i < index; ++i) {
        free_object_indexes.push_back(static_cast<unsigned int>(i));
        objects.push_back(nullptr);
      }
      object = new T(game, index);
      objects.push_back(object);
    }

    return object;
  }

  T* operator[] (unsigned int index) {
    if (index >= objects.size()) {
      return nullptr;
    }
    return objects[index];
  }

  const T* operator[] (unsigned int index) const {
    if (index >= objects.size()) {
      return nullptr;
    }
    return objects[index];
  }

  class Iterator {
   protected:
    typename Objects::iterator internal_iterator;
    Objects *objects;

   public:
    Iterator(typename Objects::iterator it, Objects *objs) {
      internal_iterator = it;
      objects = objs;
    }

    Iterator&
    operator++() {
      while (++internal_iterator != objects->end()) {
        if (*internal_iterator != nullptr) {
          return (*this);
        }
      }
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
      return *internal_iterator;
    }
  };

  class ConstIterator {
   protected:
    typename Objects::const_iterator internal_iterator;
    const Objects *objects;

   public:
    ConstIterator(typename Objects::const_iterator it, const Objects *objs) {
      internal_iterator = it;
      objects = objs;
    }

    ConstIterator& operator++() {
      while (++internal_iterator != objects->end()) {
        if (*internal_iterator != nullptr) {
          return (*this);
        }
      }
      return (*this);
    }

    bool operator == (const ConstIterator& right) const {
     return internal_iterator == right.internal_iterator;
    }

    bool operator != (const ConstIterator& right) const {
     return !(*this == right);
    }

    const T* operator*() const {
     return *internal_iterator;
    }
  };

  Iterator begin() { return Iterator(objects.begin(), &objects); }
  Iterator end() { return Iterator(objects.end(), &objects); }

  ConstIterator begin() const {
    return ConstIterator(objects.begin(), &objects);
  }

  ConstIterator end() const {
    return ConstIterator(objects.end(), &objects);
  }

  void
  erase(unsigned int index) {
    if ((index <= objects.size()) && (objects[index] != nullptr)) {
      T *object = objects[index];
      if (index + 1 == objects.size()) {
        objects.pop_back();
      } else {
        free_object_indexes.push_back(index);
        objects[index] = nullptr;
      }
      delete object;
    }
  }

  size_t
  size() const { return objects.size() - free_object_indexes.size(); }
};

#endif  // SRC_OBJECTS_H_
