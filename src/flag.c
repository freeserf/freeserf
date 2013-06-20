/*
 * flag.c - Flag related functions.
 *
 * Copyright (C) 2013  Jon Lund Steffensen <jonlst@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "flag.h"
#include "building.h"
#include "player.h"
#include "game.h"
#include "list.h"
#include "misc.h"

#define SEARCH_MAX_DEPTH  0x10000


typedef struct {
	list_elm_t elm;
	flag_t *flag;
} flag_proxy_t;


static flag_proxy_t *
flag_proxy_alloc(flag_t *flag)
{
	flag_proxy_t *proxy = malloc(sizeof(flag_proxy_t));
	if (proxy == NULL) abort();

	proxy->flag = flag;
	return proxy;
}

static int
next_search_id()
{
	game.flag_search_counter += 1;

	/* If we're back at zero the counter has overflown,
	   everything needs a reset to be safe. */
	if (game.flag_search_counter == 0) {
		game.flag_search_counter += 1;
		for (int i = 1; i < game.max_flag_index; i++) {
			if (FLAG_ALLOCATED(i)) {
				game_get_flag(i)->search_num = 0;
			}
		}
	}

	game.flag_queue_select = 0;
	return game.flag_search_counter;
}

void
flag_search_init(flag_search_t *search)
{
	list_init(&search->queue);
	search->id = next_search_id();
}

void
flag_search_add_source(flag_search_t *search, flag_t *flag)
{
	flag_proxy_t *flag_proxy = flag_proxy_alloc(flag);
	list_append(&search->queue, (list_elm_t *)flag_proxy);
	flag->search_num = search->id;
}

int
flag_search_execute(flag_search_t *search, flag_search_func *callback, int land, int transporter, void *data)
{
	for (int i = 0; i < SEARCH_MAX_DEPTH && !list_is_empty(&search->queue); i++) {
		flag_proxy_t *proxy = (flag_proxy_t *)list_remove_head(&search->queue);
		flag_t *flag = proxy->flag;
		free(proxy);

		int r = callback(flag, data);
		if (r) {
			/* Clean up */
			while (!list_is_empty(&search->queue)) {
				free(list_remove_head(&search->queue));
			}
			return 0;
		}

		for (int i = 0; i < 6; i++) {
			if ((!land || !FLAG_IS_WATER_PATH(flag, 5-i)) &&
			    (!transporter || FLAG_HAS_TRANSPORTER(flag, 5-i)) &&
			    flag->other_endpoint.f[5-i]->search_num != search->id) {
				flag->other_endpoint.f[5-i]->search_num = search->id;
				flag->other_endpoint.f[5-i]->search_dir = flag->search_dir;
				flag_proxy_t *other_flag_proxy = flag_proxy_alloc(flag->other_endpoint.f[5-i]);
				list_append(&search->queue, (list_elm_t *)other_flag_proxy);
			}
		}
	}

	/* Clean up */
	while (!list_is_empty(&search->queue)) {
		free(list_remove_head(&search->queue));
	}

	return -1;
}

int
flag_search_single(flag_t *src, flag_search_func *callback,
		   int land, int transporter, void *data)
{
	flag_search_t search;
	flag_search_init(&search);
	flag_search_add_source(&search, src);
	return flag_search_execute(&search, callback, land, transporter, data);
}

void
flag_prioritize_pickup(flag_t *flag, dir_t dir, const int flag_prio[])
{
	int res_next = -1;
	int res_prio = -1;

	for (int i = 0; i < FLAG_MAX_RES_COUNT; i++) {
		/* Use flag_prio to prioritize resource pickup. */
		dir_t res_dir = flag->slot[i].dir;
		resource_type_t res_type = flag->slot[i].type;
		if (res_dir == dir && flag_prio[res_type] > res_prio) {
			res_next = i;
			res_prio = flag_prio[res_type];
		}
	}

	flag->other_end_dir[dir] &= 0x78;
	if (res_next > -1) flag->other_end_dir[dir] |= BIT(7) | res_next;
}

/* Cancel transport of resources to building at flag. */
void
flag_cancel_transported_stock(flag_t *flag, resource_type_t res)
{
	assert(FLAG_HAS_BUILDING(flag));

	if (1/*FLAG_INDEX(flag) != ..*/) {
		building_t *building = flag->other_endpoint.b[DIR_UP_LEFT];

		if (res == RESOURCE_FISH ||
		    res == RESOURCE_MEAT ||
		    res == RESOURCE_BREAD) {
			res = RESOURCE_GROUP_FOOD;
		}

		for (int i = 0; i < 2; i++) {
			if (building->stock[i].type == res) {
				building->stock[i].requested -= 1;
				assert(building->stock[i].requested >= 0);
				break;
			}
		}
	}
}
