/*
 * pathfinder.c - Path finder functions
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

#include "pathfinder.h"
#include "pqueue.h"
#include "list.h"
#include "game.h"

#include <stdlib.h>

typedef struct search_node search_node_t;

struct search_node {
	list_elm_t elm;
	map_pos_t pos;
	uint g_score;
	uint f_score;
	search_node_t *parent;
	dir_t dir;
};


static int
search_node_less(search_node_t *n1, search_node_t *n2)
{
	return n1->f_score < n2->f_score;
}

static const uint walk_cost[] = { 255, 319, 383, 447, 511 };

static uint
heuristic_cost(map_pos_t start, map_pos_t end)
{
	/* Calculate distance to target. */
	int dist_col = (MAP_POS_COL(start) -
			MAP_POS_COL(end)) & game.map.col_mask;
	if (dist_col >= (int)(game.map.cols/2)) dist_col -= game.map.cols;

	int dist_row = (MAP_POS_ROW(start) -
			MAP_POS_ROW(end)) & game.map.row_mask;
	if (dist_row >= (int)(game.map.rows/2)) dist_row -= game.map.rows;

	int h_diff = abs(MAP_HEIGHT(start) - MAP_HEIGHT(end));
	int dist = 0;

	if ((dist_col > 0 && dist_row > 0) ||
	    (dist_col < 0 && dist_row < 0)) {
		dist = max(abs(dist_col), abs(dist_row));
	} else {
		dist = abs(dist_col) + abs(dist_row);
	}

	return dist > 0 ? dist*walk_cost[h_diff/dist] : 0;
}

static uint
actual_cost(map_pos_t pos, dir_t dir)
{
	map_pos_t other_pos = MAP_MOVE(pos, dir);
	int h_diff = abs(MAP_HEIGHT(pos) - MAP_HEIGHT(other_pos));
	return walk_cost[h_diff];
}

/* Find the shortest path from start to end (using A*) considering that
   the walking time for a serf walking in any direction of the path
   should be minimized. Returns a malloc'ed array of directions and
   the size of this array in length. */
dir_t *
pathfinder_map(map_pos_t start, map_pos_t end, uint *length)
{
	list_t closed;
	list_init(&closed);

	pqueue_t open;
	pqueue_init(&open, 32,
		    (pqueue_less_func *)search_node_less);

	dir_t *solution = NULL;

	/* Create start node */
	search_node_t *node = (search_node_t*)malloc(sizeof(search_node_t));
	if (node == NULL) abort();

	node->pos = start;
	node->g_score = 0;
	node->f_score = heuristic_cost(start, end);
	node->parent = NULL;
	int r = pqueue_insert(&open, node);
	if (r < 0) abort();

	while (!pqueue_is_empty(&open)) {
		node = (search_node_t*)pqueue_pop(&open);
		if (node->pos == end) {
			/* Construct solution */
			*length = 0;
			search_node_t *n = node;
			while (n->parent != NULL) {
				*length += 1;
				n = n->parent;
			}

			if (*length == 0) break;

			solution = (dir_t*)malloc(*length*sizeof(dir_t));
			if (solution == NULL) abort();

			for (int i = *length-1; i >= 0; i--) {
				solution[i] = node->dir;
				node = node->parent;
			}
			break;
		}

		/* Put current node on closed list. */
		list_prepend(&closed, (list_elm_t *)node);

		for (int d = DIR_RIGHT; d <= DIR_UP; d++) {
			map_pos_t new_pos = MAP_MOVE(node->pos, d);
			uint cost = actual_cost(node->pos, (dir_t)d);

			/* Check if neighbour is valid. */
			if (!game_road_segment_valid(node->pos, (dir_t)d) ||
			    (MAP_OBJ(new_pos) == MAP_OBJ_FLAG && new_pos != end)) {
				continue;
			}

			/* Check if neighbour is in closed list. */
			int in_closed = 0;
			list_elm_t *elm;
			list_foreach(&closed, elm) {
				search_node_t *n = (search_node_t *)elm;
				if (n->pos == new_pos) {
					in_closed = 1;
					break;
				}
			}

			if (in_closed) continue;

			/* See if neighbour is already in open list. */
			int in_open = 0;
			for (uint i = 0; i < open.size; i++) {
				search_node_t *n = (search_node_t*)open.entries[i];
				if (n->pos == new_pos) {
					in_open = 1;
					if (n->g_score >= node->g_score + cost) {
						pqueue_remove(&open, i);
						n->g_score = node->g_score + cost;
						n->f_score = n->g_score + heuristic_cost(new_pos, end);
						n->parent = node;
						n->dir = (dir_t)d;
						int r = pqueue_insert(&open, n);
						if (r < 0) abort();
					}
					break;
				}
			}

			/* If not found in the open set, create a new node. */
			if (!in_open) {
				search_node_t *new_node = (search_node_t*)malloc(sizeof(search_node_t));
				if (new_node == NULL) abort();

				new_node->pos = new_pos;
				new_node->g_score = node->g_score + cost;
				new_node->f_score = new_node->g_score + heuristic_cost(new_pos, end);
				new_node->parent = node;
				new_node->dir = (dir_t)d;
				int r = pqueue_insert(&open, new_node);
				if (r < 0) abort();
			}
		}
	}

	/* Clean up open list */
	for (uint i = 0; i < open.size; i++) {
		free(open.entries[i]);
	}

	pqueue_deinit(&open);

	/* Clean up closed list */
	while (!list_is_empty(&closed)) {
		search_node_t *node =
			(search_node_t *)list_remove_head(&closed);
		free(node);
	}

	return solution;
}
