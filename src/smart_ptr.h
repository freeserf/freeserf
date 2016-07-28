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

template <typename T> class SmartPtr {
 private:
  class RefCounter {
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

  RefCounter *ref_counter;
  T *pointer;

 public:
  SmartPtr() : pointer(NULL), ref_counter(NULL) {
    ref_counter = new RefCounter();
    ref_counter->retain();
  }

  explicit SmartPtr(T* pValue) : pointer(pValue), ref_counter(NULL) {
    ref_counter = new RefCounter();
    ref_counter->retain();
  }

  SmartPtr(const SmartPtr<T> &smart_ptr)
    : pointer(smart_ptr.pointer), ref_counter(smart_ptr.ref_counter) {
    ref_counter->retain();
  }

  virtual ~SmartPtr() {
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

  SmartPtr<T>& operator = (const SmartPtr<T>& smart_ptr) {
    if (this != &smart_ptr) {
      T *old_pointer = NULL;
      RefCounter *old_ref_counter = NULL;

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

  friend bool operator==(const SmartPtr<T> &smart_ptr, const T *pointer) {
    return (smart_ptr.pointer == pointer);
  }

  friend bool operator!=(const SmartPtr<T> &smart_ptr, const T *pointer) {
    return !(smart_ptr.pointer == pointer);
  }
};

#endif  // SRC_SMART_PTR_H_
