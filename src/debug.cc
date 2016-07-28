/*
 * debug.cc - Definitions to ease debugging.
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

#include "src/debug.h"

ExceptionFreeserf::ExceptionFreeserf(const std::string &description) throw() {
  this->description = description;
}

ExceptionFreeserf::~ExceptionFreeserf() throw() {
}

const char*
ExceptionFreeserf::what() const throw() {
  return get_description().c_str();
}

std::string
ExceptionFreeserf::get_description() const {
  return "[" + get_system() + "] " + description;
}
