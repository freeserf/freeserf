/*
 * buffer.cc - Memory buffer implementation
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

#include "src/buffer.h"

#include <fstream>
#include <string>
#include <cstddef>
#include <cstring>

#include "src/freeserf_endian.h"

Buffer::Buffer()
  : data(nullptr)
  , size(0)
  , owned(true)
  , read(nullptr) {
}

Buffer::Buffer(void *_data, size_t _size)
  : data(_data)
  , size(_size)
  , owned(false)
  , read(reinterpret_cast<uint8_t*>(data)) {
}

Buffer::Buffer(PBuffer _parent, size_t start, size_t length)
  : data(reinterpret_cast<uint8_t*>(_parent->get_data()) + start)
  , size(length)
  , owned(false)
  , parent(_parent)
  , read(reinterpret_cast<uint8_t*>(data)) {
}

Buffer::Buffer(const std::string &path) : owned(true) {
  std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
  if (!file.good()) {
    throw ExceptionFreeserf("Failed to open file '" + path + "'");
  }

  size = (size_t)file.tellg();

  file.seekg(0, file.beg);

  data = ::malloc(size);
  if (data == nullptr) {
    throw ExceptionFreeserf("Failed to allocate memory");
  }

  file.read(reinterpret_cast<char*>(data), size);
  file.close();

  read = reinterpret_cast<uint8_t*>(data);
}

Buffer::~Buffer() {
  if (owned && (data != nullptr)) {
    ::free(data);
  }
}

void *
Buffer::unfix() {
  void *result = data;
  data = nullptr;
  size = 0;
  return result;
}

bool
Buffer::readable() {
  return (read - reinterpret_cast<uint8_t*>(data) != (ptrdiff_t)size);
}

uint8_t
Buffer::pop() {
  uint8_t value = *read;
  read++;
  return value;
}

PBuffer
Buffer::pop(size_t _size) {
  PBuffer value = std::make_shared<Buffer>(shared_from_this(),
                                        read - reinterpret_cast<uint8_t*>(data),
                                           _size);
  read += _size;
  return value;
}

PBuffer
Buffer::pop_tail() {
  size_t _size = size - (read - reinterpret_cast<uint8_t*>(data));
  return pop(_size);
}

uint16_t
Buffer::pop16be() {
  uint16_t value = *reinterpret_cast<uint16_t*>(read);
  read += 2;
  return be16toh(value);
}

uint16_t
Buffer::pop16le() {
  uint16_t value = *reinterpret_cast<uint16_t*>(read);
  read += 2;
  return le16toh(value);
}

uint32_t
Buffer::pop32be() {
  uint32_t value = *reinterpret_cast<uint32_t*>(read);
  read += 4;
  return be32toh(value);
}

uint32_t
Buffer::pop32le() {
  uint32_t value = *reinterpret_cast<uint32_t*>(read);
  read += 4;
  return le32toh(value);
}

PBuffer
Buffer::get_subbuffer(size_t offset, size_t _size) {
  return std::make_shared<Buffer>(shared_from_this(), offset, _size);
}

PBuffer
Buffer::get_tail(size_t offset) {
  return get_subbuffer(offset, size - offset);
}

bool
Buffer::write(const std::string &path) {
  std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
  if (!file.good()) {
    return false;
  }

  file.write(reinterpret_cast<char*>(data), size);
  file.close();

  return true;
}

// MutableBuffer

MutableBuffer::MutableBuffer()
  : Buffer()
  , reserved(0)
  , growth(1000) {
  owned = true;
}

MutableBuffer::MutableBuffer(size_t _size)
  : reserved(_size)
  , growth(1000) {
  data = ::malloc(_size);
  size = 0;
  owned = true;
  read = reinterpret_cast<uint8_t*>(data);
}

MutableBuffer::~MutableBuffer() {
}

void *
MutableBuffer::unfix() {
  void *result = data;
  data = nullptr;
  size = 0;
  reserved = 0;
  return result;
}

void
MutableBuffer::check_size(size_t _size) {
  if (data == nullptr) {
    data = ::malloc(growth);
    size = 0;
    reserved = growth;
    owned = true;
    return;
  }
  while (_size > reserved) {
    data = ::realloc(data, reserved + growth);
    reserved += growth;
  }
}

void
MutableBuffer::push(PBuffer buffer) {
  size_t _size = buffer->get_size();
  check_size(size + _size);
  ::memcpy(reinterpret_cast<uint8_t*>(data) + size, buffer->get_data(), _size);
  size += _size;
}

void
MutableBuffer::push(const void *_data, size_t _size) {
  check_size(size + _size);
  ::memcpy(reinterpret_cast<uint8_t*>(data) + size, _data, _size);
  size += _size;
}

void
MutableBuffer::push(const std::string &str) {
  push((const void*)str.c_str(), str.size());
}

void
MutableBuffer::push32be(uint32_t value, size_t count) {
  check_size(size + (4 * count));
  for (size_t i = 0; i < count; i++) {
    *(reinterpret_cast<uint32_t*>(offset(size))) = htobe32(value);
    size += 4;
  }
}

void
MutableBuffer::push32le(uint32_t value, size_t count) {
  check_size(size + (4 * count));
  for (size_t i = 0; i < count; i++) {
    *(reinterpret_cast<uint32_t*>(offset(size))) = htole32(value);
    size += 4;
  }
}

void
MutableBuffer::push16be(uint16_t value, size_t count) {
  check_size(size + (2 * count));
  for (size_t i = 0; i < count; i++) {
    *(reinterpret_cast<uint16_t*>(offset(size))) = htobe16(value);
    size += 2;
  }
}

void
MutableBuffer::push16le(uint16_t value, size_t count) {
  check_size(size + (2 * count));
  for (size_t i = 0; i < count; i++) {
    *(reinterpret_cast<uint16_t*>(offset(size))) = htole16(value);
    size += 2;
  }
}

void
MutableBuffer::push(uint8_t value, size_t count) {
  check_size(size + count);
  for (size_t i = 0; i < count; i++) {
    *(reinterpret_cast<uint8_t*>(offset(size))) = value;
    size++;
  }
}

