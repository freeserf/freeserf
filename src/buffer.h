/*
 * buffer.h - Memory buffer declaration
 *
 * Copyright (C) 2017  Wicked_Digger <wicked_digger@mail.ru>
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

#ifndef SRC_BUFFER_H_
#define SRC_BUFFER_H_

#include <memory>
#include <string>
#include <algorithm>

#include "src/freeserf_endian.h"

class Buffer;
typedef std::shared_ptr<Buffer> PBuffer;

class Buffer : public std::enable_shared_from_this<Buffer> {
 public:
  typedef enum EndianessMode {
    EndianessBig,
    EndianessLittle
  } EndianessMode;

 protected:
  void *data;
  size_t size;
  bool owned;
  PBuffer parent;
  uint8_t *read;
  EndianessMode endianess;

 public:
  Buffer(EndianessMode endianess = is_big_endian() ? EndianessBig
                                                   : EndianessLittle);
  Buffer(void *data, size_t size,
         EndianessMode endianess = is_big_endian() ? EndianessBig
                                                   : EndianessLittle);
  Buffer(PBuffer parent, size_t start, size_t length);
  Buffer(PBuffer parent, size_t start, size_t length, EndianessMode endianess);
  explicit Buffer(const std::string &path,
                  EndianessMode endianess = is_big_endian() ? EndianessBig
                                                            : EndianessLittle);
  virtual ~Buffer();

  size_t get_size() const { return size; }
  void *get_data() const { return data; }

  // adding to support messing with weather/seasons/palette
  void reset_read() {
    read = reinterpret_cast<uint8_t*>(data);
  }

  void *unfix();
  void set_endianess(EndianessMode _endianess) { endianess = _endianess; }

  bool readable();
  PBuffer pop(size_t size);
  PBuffer pop_tail();
  template<typename T> T pop() {
    T value = *reinterpret_cast<T*>(read);
    read += sizeof(T);
    return (endianess == EndianessBig) ? betoh(value) : letoh(value);
  }

  PBuffer get_subbuffer(size_t offset, size_t size);
  PBuffer get_tail(size_t offset);

  operator std::string() const {
    char *str = reinterpret_cast<char*>(data);
    return std::string(str, str + size);
  }

  bool write(const std::string &path);

 protected:
  void *offset(size_t off) { return reinterpret_cast<char*>(data) + off; }
};

class MutableBuffer : public Buffer {
 protected:
  size_t reserved;
  size_t growth;

 public:
  explicit MutableBuffer(EndianessMode endianess);
  MutableBuffer(size_t size, EndianessMode endianess);
  virtual ~MutableBuffer();

  size_t get_size() const { return size; }
  void *get_data() const { return data; }
  void *unfix();

  void push(PBuffer buffer);
  void push(const void *data, size_t size);
  void push(void *buf, size_t len) { push((const void*)buf, len); }
  void push(const std::string &str);
  void push(const char *str) { push(std::string(str)); }
  template<typename T> void push(T value, size_t count = 1) {
    check_size(size + (sizeof(T) * count));
    for (size_t i = 0; i < count; i++) {
      *(reinterpret_cast<T*>(offset(size))) = (endianess == EndianessBig) ?
                                                htobe(value) :
                                                htole(value);
      size += sizeof(T);
    }
  }

 protected:
  void check_size(size_t size);
};

typedef std::shared_ptr<MutableBuffer> PMutableBuffer;

#endif  // SRC_BUFFER_H_
