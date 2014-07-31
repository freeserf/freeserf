/*
 * log.c - Logging
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

#include "log.h"

#include <stdlib.h>
#include <stdarg.h>

static log_level_t log_level = LOG_LEVEL_DEBUG;
static FILE *log_file = NULL;


void
log_set_file(FILE *file)
{
	log_file = file;
}

void
log_set_level(log_level_t level)
{
	log_level = level;
}

static void
log_msg_va(log_level_t level, const char *system,
	   const char *format, va_list ap)
{
	if (level >= log_level) {
		if (system != NULL) fprintf(log_file, "[%s] ", system);
		vfprintf(log_file, format, ap);
		fprintf(log_file, "\n");
	}
}

void
log_msg(log_level_t level, const char *system, const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	log_msg_va(level, system, format, ap);
	va_end(ap);
}
