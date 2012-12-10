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

#ifndef _FREESERF_LOG_H
#define _FREESERF_LOG_H

#include <stdio.h>

/* Log levels */
typedef enum {
	LOG_LEVEL_VERBOSE = 0,
	LOG_LEVEL_DEBUG,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARN,
	LOG_LEVEL_ERROR,

	LOG_LEVEL_MAX
} log_level_t;


#ifdef __ANDROID__ /* Bypass normal logging on Android. */
#include <android/log.h>

# define LOG_TAG "freeserf"

# define LOGV(system, ...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
# define LOGD(system, ...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG ,__VA_ARGS__)
# define LOGI(system, ...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
# define LOGW(system, ...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
# define LOGE(system, ...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#else

# define LOGV(system, ...)  log_msg(LOG_LEVEL_VERBOSE, system, __VA_ARGS__)
# define LOGD(system, ...)  log_msg(LOG_LEVEL_DEBUG, system, __VA_ARGS__)
# define LOGI(system, ...)  log_msg(LOG_LEVEL_INFO, system, __VA_ARGS__)
# define LOGW(system, ...)  log_msg(LOG_LEVEL_WARN, system, __VA_ARGS__)
# define LOGE(system, ...)  log_msg(LOG_LEVEL_ERROR, system, __VA_ARGS__)

#endif

void log_set_file(FILE *file);
void log_set_level(log_level_t level);
void log_msg(log_level_t level, const char *system, const char *format, ...);

#endif /* !_FREESERF_LOG_H */
