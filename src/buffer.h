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

#include "src/debug.h"

class Buffer;
typedef std::shared_ptr<Buffer> PBuffer;

class Buffer : public std::enable_shared_from_this<Buffer> {
 protected:
  void *data;
  size_t size;
  bool owned;
  PBuffer parent;
  uint8_t *read;

 public:
  Buffer();
  Buffer(void *data, size_t size);
  Buffer(PBuffer parent, size_t start, size_t length);
  explicit Buffer(const std::string &path);
  virtual ~Buffer();

  size_t get_size() const { return size; }
  void *get_data() const { return data; }
  void *unfix();

  bool readable();
  uint8_t pop();
  PBuffer pop(size_t size);
  PBuffer pop_tail();
  uint16_t pop16be();
  uint16_t pop16le();
  uint32_t pop32be();
  uint32_t pop32le();

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
  MutableBuffer();
  explicit MutableBuffer(size_t size);
  virtual ~MutableBuffer();

  size_t get_size() const { return size; }
  void *get_data() const { return data; }
  void *unfix();

  void push(PBuffer buffer);
  void push(const void *data, size_t size);
  void push(void *data, size_t size) { push((const void*)data, size); }
  void push(const std::string &str);
  void push32be(uint32_t value, size_t count = 1);
  void push32le(uint32_t value, size_t count = 1);
  void push16be(uint16_t value, size_t count = 1);
  void push16le(uint16_t value, size_t count = 1);
  void push(uint8_t value, size_t count = 1);

 protected:
  void check_size(size_t size);
};

typedef std::shared_ptr<MutableBuffer> PMutableBuffer;

#endif  // SRC_BUFFER_H_
