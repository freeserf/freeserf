/*
 * tpwm.c - Uncomprassing TPWM'ed content
 *
 * Copyright (C) 2015  Wicked_Digger  <wicked_digger@mail.ru>
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

#include "tpwm.h"
#include "freeserf_endian.h"

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif

static const uint8_t tpwm_sign[4] = {'T','P','W','M'};

int
tpwm_is_compressed(void *src_data, unsigned int src_size)
{
	if (src_size < 8) {
		return -1;
	}

	if (0 != memcmp(src_data, tpwm_sign, 4)) {
		return -1;
	}

	return 0;
}

int
tpwm_uncompress(void *src_data, unsigned int src_size,
                void **res_data, unsigned int *res_size, char **error)
{
	if ((res_data == NULL) || (res_size == NULL)) {
		*error = "TPWM: bad parameter";
		return -1;
	}

	if (tpwm_is_compressed(src_data, src_size) < 0) {
		*error = "TPWM: source buffer is not tpwm packed";
		return -1;
	}

	*res_data = NULL;
	*res_size = 0;

	uint32_t *header = (uint32_t*)src_data;
	*res_size = le32toh(header[1]);
	*res_data = malloc(*res_size);
	if (*res_data == NULL) {
		*error = "TPWM: unable to allocate target buffer";
		return -1;
	}

	uint8_t *src_pos = (uint8_t*)src_data + 8;
	uint8_t *src_end = src_pos+src_size - 8;
	uint8_t *res_pos = (uint8_t*)*res_data;
	uint8_t *res_end = res_pos + *res_size;
	int result = 0;

	while ((src_pos < src_end) && (res_pos < res_end)) {
		size_t flag = *src_pos++;
		if (src_pos >= src_end) { result = -1; break; }
		for( int i=0 ; i<8 ; i++) {
			flag <<= 1;
			if (flag & ~0xFF) {
				flag &= 0xFF;
				size_t temp = *src_pos++;
				if (src_pos >= src_end) { result = -1; break; }
				size_t repeater = (temp & 0x0F) + 3;
				size_t stamp_offset = *src_pos++ | ( (temp << 4) & 0x0F00 );
				uint8_t *stamp = res_pos - stamp_offset;
				while (repeater--) {
					if ((res_pos >= res_end) || (stamp >= res_end)) { result = -1; break; }
					*res_pos++ = *stamp++;
				}
			} else {
				*res_pos++ = *src_pos++;
			}
		}
	}

	if (result < 0) {
		*error = "TPWM: unable to unpack, source data corrupted";
		free(*res_data);
		*res_data = NULL;
		*res_size = 0;
	}

	return result;
}
