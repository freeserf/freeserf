/*
* smart_ptr.h - Simple shared pointer
*
* Copyright (C) 2016  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_SMART_PTR_H_
#define SRC_SMART_PTR_H_

template <typename T> class smart_ptr_t {
 private:
  class ref_counter_t {
   private:
    size_t count;

   public:
    void retain() {
      count++;
    }

    size_t release() {
      return --count;
    }
  };

  ref_counter_t *ref_counter;
  T *pointer;

 public:
  smart_ptr_t() : pointer(NULL), ref_counter(NULL) {
    ref_counter = new ref_counter_t();
    ref_counter->retain();
  }

  explicit smart_ptr_t(T* pValue) : pointer(pValue), ref_counter(NULL) {
    ref_counter = new ref_counter_t();
    ref_counter->retain();
  }

  smart_ptr_t(const smart_ptr_t<T> &smart_ptr)
    : pointer(smart_ptr.pointer), ref_counter(smart_ptr.ref_counter) {
    ref_counter->retain();
  }

  virtual ~smart_ptr_t() {
    if (ref_counter->release() == 0) {
      delete pointer;
      delete ref_counter;
    }
  }

  T &operator* () {
    return *pointer;
  }

  const T &operator* () const {
    return *pointer;
  }

  T *operator-> () {
    return pointer;
  }

  const T *operator-> () const {
    return pointer;
  }

  smart_ptr_t<T>& operator = (const smart_ptr_t<T>& smart_ptr) {
    if (this != &smart_ptr) {
      T *old_pointer = NULL;
      ref_counter_t *old_ref_counter = NULL;

      if (ref_counter->release() == 0) {
        old_pointer = pointer;
        old_ref_counter = ref_counter;
      }

      pointer = smart_ptr.pointer;
      ref_counter = smart_ptr.ref_counter;
      ref_counter->retain();

      if (old_pointer != NULL) {
        delete old_pointer;
      }
      if (old_ref_counter != NULL) {
        delete old_ref_counter;
      }
    }

    return *this;
  }

  friend bool operator==(const smart_ptr_t<T> &smart_ptr, const T *pointer) {
    return (smart_ptr.pointer == pointer);
  }

  friend bool operator!=(const smart_ptr_t<T> &smart_ptr, const T *pointer) {
    return !(smart_ptr.pointer == pointer);
  }
};

#endif  // SRC_SMART_PTR_H_
