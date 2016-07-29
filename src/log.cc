/*
 * log.cc - Logging
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

#include "src/log.h"

#include <cstdarg>

static Log::Level log_level = Log::LevelDebug;
static FILE *log_file = NULL;

void
Log::set_file(FILE *file) {
  log_file = file;
}

void
Log::set_level(Log::Level level) {
  log_level = level;
}

static void
log_msg_va(Log::Level level, const char *system, const char *format,
           va_list ap) {
  if (level >= log_level) {
    if (system != NULL) fprintf(log_file, "[%s] ", system);
    vfprintf(log_file, format, ap);
    fprintf(log_file, "\n");
  }
}

void
Log::msg(Log::Level level, const char *system, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  log_msg_va(level, system, format, ap);
  va_end(ap);
}
