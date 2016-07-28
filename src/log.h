/*
 * log.h - Logging
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

/* Log levels determine the importance of a log message. Use the
   following guidelines when deciding which level to use.

   - ERROR: The _user_ must take action to continue usage.
   Must usually be followed by exit(..) or abort().
   - WARN: Warn the _user_ of an important problem that the
   user might be able to resolve.
   - INFO: Information that is of general interest to _users_.
   - DEBUG: Log a problem that a _developer_ should look
   into and fix.
   - VERBOSE: Log any other information that a _developer_ might
   be interested in. */

#ifndef SRC_LOG_H_
#define SRC_LOG_H_

#include <cstdio>

class Log {
 public:
  /* Log levels */
  typedef enum Level {
    LevelVerbose = 0,
    LevelDebug,
    LevelInfo,
    LevelWarn,
    LevelError,

    LevelMax
  } Level;

  static void set_file(FILE *file);
  static void set_level(Log::Level level);
  static void msg(Log::Level level, const char *system,
                  const char *format, ...);
};


#ifdef __ANDROID__ /* Bypass normal logging on Android. */
#include <android/log.h>

# define LOG_TAG "freeserf"

# define LOGV(system, ...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, \
                                                __VA_ARGS__)
# define LOGD(system, ...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, \
                                                __VA_ARGS__)
# define LOGI(system, ...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, \
                                                __VA_ARGS__)
# define LOGW(system, ...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, \
                                                __VA_ARGS__)
# define LOGE(system, ...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, \
                                                __VA_ARGS__)

#else

# define LOGV(system, ...)  Log::msg(Log::LevelVerbose, system, __VA_ARGS__)
# define LOGD(system, ...)  Log::msg(Log::LevelDebug, system, __VA_ARGS__)
# define LOGI(system, ...)  Log::msg(Log::LevelInfo, system, __VA_ARGS__)
# define LOGW(system, ...)  Log::msg(Log::LevelWarn, system, __VA_ARGS__)
# define LOGE(system, ...)  Log::msg(Log::LevelError, system, __VA_ARGS__)

#endif

#endif  // SRC_LOG_H_
