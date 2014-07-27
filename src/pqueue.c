/*
 * pqueue.c - Priority queue
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

#include "pqueue.h"

#include <stdlib.h>

/* Initialize priority queue. */
int
pqueue_init(pqueue_t *queue, uint capacity, pqueue_less_func *less)
{
	queue->size = 0;
	queue->capacity = capacity;
	queue->less = less;
	queue->entries = (void**)malloc(capacity*sizeof(void *));
	if (queue->entries == NULL) return -1;

	return 0;
}

/* Free priority queue. Elements on the queue should be freed first. */
void
pqueue_deinit(pqueue_t *queue)
{
	free(queue->entries);
}

/* Insert an element in the queue. */
int
pqueue_insert(pqueue_t *queue, void *elm)
{
	if (queue->size == queue->capacity) {
		/* Double queue capacity if full. */
		queue->capacity *= 2;
		queue->entries = (void**)realloc(queue->entries,
					 queue->capacity*sizeof(void *));
		if (queue->entries == NULL) return -1;
	}

	/* Add to end of binheap. */
	uint i = queue->size;
	queue->entries[i] = elm;
	queue->size += 1;

	/* Heapify-up */
	while (i > 0) {
		uint parent = (i-1)/2;
		if (queue->less(queue->entries[i], queue->entries[parent])) {
			/* Swap */
			void *temp = queue->entries[parent];
			queue->entries[parent] = queue->entries[i];
			queue->entries[i] = temp;
			i = parent;
		} else {
			break;
		}
	}

	return 0;
}

/* Remove random element from the queue. */
void *
pqueue_remove(pqueue_t *queue, uint index)
{
	if (queue->size <= index) return NULL;

	void *elm = queue->entries[index];

	/* Move last element to index */
	uint i = index;
	queue->entries[i] = queue->entries[queue->size-1];
	queue->size -= 1;

	if (queue->size == 0) return elm;

	/* Heapify-down */
	while (i < (queue->size-1)/2) {
		uint child_l = 2*i+1;
		uint child_r = 2*i+2;
		uint swap = 0;
		if (queue->less(queue->entries[child_l], queue->entries[i])) {
			if (child_r < queue->size &&
			    queue->less(queue->entries[child_r], queue->entries[child_l])) {
				swap = child_r;
			} else {
				swap = child_l;
			}
		} else if (child_r < queue->size &&
			   queue->less(queue->entries[child_r], queue->entries[i])) {
			swap = child_r;
		} else {
			break;
		}

		/* Swap */
		void *temp = queue->entries[i];
		queue->entries[i] = queue->entries[swap];
		queue->entries[swap] = temp;
		i = swap;
	}

	return elm;
}

/* Remove and return the next element in the queue. */
void *
pqueue_pop(pqueue_t *queue)
{
	return pqueue_remove(queue, 0);
}

int
pqueue_is_empty(pqueue_t *pqueue)
{
	return pqueue->size == 0;
}
