/*
 * data.c - Basic game resources file functions.
 *
 * Copyright (C) 2014  Wicked_Digger <wicked_digger@mail.ru>
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

#include "data.h"
#include "log.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#else
#ifdef WIN32
#include <io.h>
#endif
#endif

#define MAX_DATA_PATH      1024

/* Load data file from path is non-NULL, otherwise search in
 various standard paths. */
int
data_init(const char *path)
{
	char *load_path = NULL;

	do {
		/* Use specified path. If something was specified
		 but not found, this function should fail without
		 looking anywhere else. */
		if ((path != NULL) && (data_check_file(path) >= 0)) {
			load_path = strdup(path);
			break;
		}

		/* If a path is not specified (path is NULL) then
		   the configuration file is searched for in the directories
		   specified by the XDG Base Directory Specification
		   <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>.

		   On windows platforms the %localappdata% is used in place of XDG_CONFIG_HOME.
		*/

		char cp[MAX_DATA_PATH];
		char *env;

		/* Look in home */
		if ((env = getenv("XDG_DATA_HOME")) != NULL &&
		     env[0] != '\0') {
			snprintf(cp, sizeof(cp), "%s/freeserf", env);
			if (data_check(cp, &load_path) >= 0) {
				break;
			}
		} else if ((env = getenv("HOME")) != NULL && env[0] != '\0') {
			snprintf(cp, sizeof(cp),
					 "%s/.local/share/freeserf", env);
			if (data_check(cp, &load_path) >= 0) {
				break;
			}
		}
#ifdef _WIN32
		if ((env = getenv("userprofile")) != NULL && env[0] != '\0') {
			snprintf(cp, sizeof(cp),
					 "%s/.local/share/freeserf", env);
			if (data_check(cp, &load_path) >= 0) {
				break;
			}
		}
#endif

		if ((env = getenv("XDG_DATA_DIRS")) != NULL && env[0] != '\0') {
			char *begin = env;
			while (1) {
				char *end = strchr(begin, ':');
				if (end == NULL) end = strchr(begin, '\0');

				int len = (int)(end - begin);
				if (len > 0) {
					snprintf(cp, sizeof(cp),
							 "%.*s/freeserf", len, begin);
					if (data_check(cp, &load_path) >= 0) {
						break;
					}
				}

				if (end[0] == '\0') break;
				begin = end + 1;
			}
			if (load_path != NULL) {
				break;
			}
		} else {
			/* Look in /usr/local/share and /usr/share per XDG spec. */
			if (data_check("/usr/local/share/freeserf", &load_path) >= 0) {
				break;
			}

			if (data_check("/usr/share/freeserf", &load_path) >= 0) {
				break;
			}
		}

		/* Look in current directory */
		if (data_check("", &load_path) >= 0) {
			break;
		}
	} while (0);

	int r = -1;

	if (NULL != load_path) {
		r = data_load(load_path);
		free(load_path);
	}

	return r;
}

#ifndef R_OK
#define R_OK (4)
#endif

int
data_check_file(const char *path)
{
	return access(path, R_OK);
}

void*
data_file_read(const char *path, uint *size)
{
	void *data = NULL;
	FILE *file = NULL;

	do {
		file = fopen(path, "rb");
		if (file == NULL) {
			break;
		}

		int r = fseek(file, 0, SEEK_END);
		if (r < 0) {
			break;
		}

		long result = ftell(file);

		if (result == -1) {
			break;
		}

		*size = (uint)result;

		r = fseek(file, 0, SEEK_SET);
		if (r < 0) {
			break;
		}

		data = malloc(*size);
		if (data == NULL) {
			break;
		}

		size_t rd = fread(data, *size, 1, file);
		if (rd < 1) {
			free(data);
			data = NULL;
			*size = 0;
		}
	} while (0);

	if (NULL != file) {
		fclose(file);
	}

	return data;
}

void
data_sprite_free(sprite_t *sprite)
{
	if (NULL != sprite->data) {
		free(sprite->data);
		sprite->data = NULL;
	}

	free(sprite);
}

sprite_t *
data_apply_mask(sprite_t *sprite, sprite_t *mask)
{
	sprite_t *masked = (sprite_t*)malloc(sizeof(sprite_t));
	masked->width = mask->width;
	masked->height = mask->height;
	masked->offset_x = mask->offset_x;
	masked->offset_y = mask->offset_y;
	masked->delta_x = 0;
	masked->delta_y = 0;
	masked->data = malloc(mask->width * mask->height * 4);

	uint32_t *pos = (uint32_t*)masked->data;
	uint32_t *end = pos + mask->width * mask->height;
	uint32_t *s_pos = (uint32_t*)sprite->data;
	uint32_t *s_end = s_pos + sprite->width * sprite->height;
	uint32_t *m_pos = (uint32_t*)mask->data;
	while (pos < end) {
		if (s_pos > s_end) {
			s_pos = (uint32_t*)sprite->data;
		}
		*(pos++) = *(s_pos++) & *(m_pos++);
	}

	return masked;
}
