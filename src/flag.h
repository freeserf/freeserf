/*
 * flag.h - Flag related functions.
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

#ifndef _FLAG_H
#define _FLAG_H

#include "freeserf.h"
#include "list.h"
#include "map.h"

#define FLAG_INDEX(ptr)  ((int)((ptr) - globals.flgs))
#define FLAG_ALLOCATED(i)  BIT_TEST(globals.flg_bitmap[(i)>>3], 7-((i)&7))

/* Bitmap of all directions with outgoing paths. */
#define FLAG_PATHS(flag)  ((int)((flag)->path_con & 0x3f))
/* Whether a path exists in a given direction. */
#define FLAG_HAS_PATH(flag,dir)  ((int)((flag)->path_con & (1<<(dir))))
/* Owner of this flag. */
#define FLAG_PLAYER(flag)  ((int)(((flag)->path_con >> 6) & 3))

/* Bitmap showing whether the outgoing paths are land paths. */
#define FLAG_LAND_PATHS(flag)  ((int)((flag)->endpoint & 0x3f))
/* Whether the path in the given direction is a water path. */
#define FLAG_IS_WATER_PATH(flag,dir)  (!(int)((flag)->endpoint & (1<<(dir))))
/* Whether a building is connected to this flag. If so, the pointer to
   the other endpoint is a valid building pointer. (Always at UP LEFT direction). */
#define FLAG_HAS_BUILDING(flag)  ((int)(((flag)->endpoint >> 6) & 1))
/* Whether resources exist that are not yet scheduled. */
#define FLAG_HAS_RESOURCES(flag)  ((int)(((flag)->endpoint >> 7) & 1))

/* Bitmap showing whether the outgoing paths have transporters
   servicing them. */
#define FLAG_TRANSPORTERS(flag)  ((int)((flag)->transporter & 0x3f))
/* Whether the path in the given direction has a transporter
   serving it. */
#define FLAG_HAS_TRANSPORTER(flag,dir)  ((int)((flag)->transporter & (1<<(dir))))
/* Whether this flag has tried to request a transporter without success. */
#define FLAG_SERF_REQUEST_FAIL(flag)  ((int)(((flag)->transporter >> 7) & 1))

/* Current number of transporters on path. */
#define FLAG_TRANSPORTER_COUNT(flag,dir)  ((uint)((flag)->length[(dir)] & 0xf))
/* Length category of path determining max number of transporters. */
#define FLAG_LENGTH_CATEGORY(flag,dir)  ((uint)(((flag)->length[(dir)] >> 4) & 7))
/* Whether a transporter serf was successfully requested for this path. */
#define FLAG_SERF_REQUESTED(flag,dir)  ((int)(((flag)->length[(dir)] >> 7) & 1))

/* The slot that is scheduled for pickup by the given path. */
#define FLAG_SCHEDULED_SLOT(flag,dir)  ((uint)((flag)->other_end_dir[(dir)] & 7))
/* The direction from the other endpoint leading back to this flag. */
#define FLAG_OTHER_END_DIR(flag,dir)  ((dir_t)(((flag)->other_end_dir[(dir)] >> 3) & 7))
/* Whether the given direction has a resource pickup scheduled. */
#define FLAG_IS_SCHEDULED(flag,dir)  ((int)(((flag)->other_end_dir[(dir)] >> 7) & 1))

/* Whether this flag has an inventory building. */
#define FLAG_HAS_INVENTORY(flag)  ((int)(((flag)->bld_flags >> 6) & 1))
/* Whether this inventory accepts serfs. */
#define FLAG_ACCEPTS_SERFS(flag)  ((int)(((flag)->bld_flags >> 7) & 1))
/* Whether this inventory accepts resources. */
#define FLAG_ACCEPTS_RESOURCES(flag)  ((int)(((flag)->bld2_flags >> 7) & 1))


typedef struct flag flag_t;

struct flag {
	map_pos_t pos; /* ADDITION */
	int search_num;
	dir_t search_dir;
	int path_con;
	int endpoint;
	int transporter;
	int length[6];
	int res_waiting[8];
	int res_dest[8];
	union {
		building_t *b[6];
		flag_t *f[6];
		void *v[6];
	} other_endpoint;
	int other_end_dir[6];
	int bld_flags;
	int stock1_prio;
	int bld2_flags;
	int stock2_prio;
};

typedef int flag_search_func(flag_t *flag, void *data);

typedef struct {
	list_t queue;
	int id;
} flag_search_t;

void flag_search_init(flag_search_t *search);
void flag_search_add_source(flag_search_t *search, flag_t *flag);
int flag_search_execute(flag_search_t *search, flag_search_func *callback,
			int land, int transporter, void *data);
int flag_search_single(flag_t *src, flag_search_func *callback,
		       int land, int transporter, void *data);

void flag_prioritize_pickup(flag_t *flag, dir_t dir, const int flag_prio[]);
void flag_cancel_transported_stock(flag_t *flag, int res);

#endif /* ! _FLAG_H */
