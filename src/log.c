/* log.c */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "log.h"

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
