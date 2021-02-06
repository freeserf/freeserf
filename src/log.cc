/*
 * log.cc - Logging
 *
 * Copyright (C) 2012-2016  Jon Lund Steffensen <jonlst@gmail.com>
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

#include "src/log.h"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif  // WIN32

#ifndef NDEBUG
Log::Level Log::level = Log::LevelDebug;
#else
Log::Level Log::level = Log::LevelInfo;
#endif

std::ostream *Log::stream = &std::cout;

std::ostream Log::Logger::dummy(0);
Log::Logger Log::Verbose(Log::LevelVerbose, "Verbose");
Log::Logger Log::Debug(Log::LevelDebug, "Debug");
Log::Logger Log::Info(Log::LevelInfo, "Info");
Log::Logger Log::Warn(Log::LevelWarn, "Warning");
Log::Logger Log::Error(Log::LevelError, "Error");
//Log::Logger Log::Player0(Log::LevelVerbose, "Verbose");

Log::Log() {
#ifdef WIN32
  if (::AttachConsole(ATTACH_PARENT_PROCESS)) {
    FILE *f = freopen("CONOUT$", "w", stdout);
    f = freopen("CONOUT$", "w", stderr);
  }
#endif  // WIN32
}

Log::~Log() {
#ifdef WIN32
  ::FreeConsole();
#endif  // WIN32
}

void
Log::set_file(std::ostream *_stream) {
  stream = _stream;
}

void
Log::set_level(Log::Level _level) {
  level = _level;
  Verbose.apply_level();
  Debug.apply_level();
  Info.apply_level();
  Warn.apply_level();
  Error.apply_level();
}
