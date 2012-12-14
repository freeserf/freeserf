/*
 * pqueue.h - Priority queue
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

#ifndef _PQUEUE_H
#define _PQUEUE_H

#include "misc.h"

/* Return non-zero if e1 comes before e2. */
typedef int pqueue_less_func(const void *e1, const void *e2);

/* Priority queue implemented as binary heap.
   This is a min-heap, but by inverting the less function
   it can be turned into a max-heap. */
typedef struct {
	uint size;
	uint capacity;
	void **entries;
	pqueue_less_func *less;
} pqueue_t;

int pqueue_init(pqueue_t *queue, uint capacity, pqueue_less_func *less);
void pqueue_deinit(pqueue_t *queue);

int pqueue_insert(pqueue_t *queue, void *elm);
void *pqueue_remove(pqueue_t *queue, uint index);
void *pqueue_pop(pqueue_t *queue);
int pqueue_is_empty(pqueue_t *pqueue);


#endif /* !_PQUEUE_H */
