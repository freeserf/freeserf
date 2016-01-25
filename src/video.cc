/*
 * video.cc - Base for graphics rendering
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

#include "src/video.h"

#include <strstream>

Video_Exception::Video_Exception(const std::string &description) throw() {
  this->description = description;
}

Video_Exception::~Video_Exception() throw() {
}

const char*
Video_Exception::what() const throw() {
  std::strstream str;
  str << "[" << get_platform() << "] " << get_description();
  return str.str();
}

const char*
Video_Exception::get_description() const {
  return description.c_str();
}

video_t *video_t::instance = NULL;

video_t::video_t() throw(Video_Exception) {
  if (instance != NULL) {
    throw Video_Exception("Unable to create second instance.");
  }

  instance = this;
}

video_t::~video_t() {
  instance = NULL;
}
