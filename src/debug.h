/*
 * debug.h - Definitions to ease debugging.
 *
 * Copyright (C) 2012  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef SRC_DEBUG_H_
#define SRC_DEBUG_H_

#include <string>

#include "src/log.h"

class ExceptionFreeserf : public std::exception {
 protected:
  std::string description;

 public:
  explicit ExceptionFreeserf(const std::string &description) throw();
  virtual ~ExceptionFreeserf() throw();

  virtual const char* what() const throw();
  virtual std::string get_description() const;
  virtual std::string get_system() const { return "Unspecified"; }
};

#ifndef NDEBUG
# define NOT_REACHED()  do { LOGE("debug", "NOT_REACHED at line %i of %s.", \
                                  __LINE__, __FILE__); abort(); } while (0)
#else
# define NOT_REACHED()  do { } while (0)
#endif

#endif  // SRC_DEBUG_H_
