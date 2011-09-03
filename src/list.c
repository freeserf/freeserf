/*
 * list.c - List functions
 *
 * Copyright (C) 2007  Jon Lund Steffensen <jonlst@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include "list.h"


void
list_init(list_t *list)
{
	list->head = (list_elm_t *)&list->null;
	list->null = NULL;
	list->tail = (list_elm_t *)&list->head;
}

list_elm_t *
list_head(list_t *list)
{
	return list->head;
}

list_elm_t *
list_tail(list_t *list)
{
	return list->tail;
}

list_elm_t *
list_find(list_t *list, list_pred_func *predicate)
{
	list_elm_t *elm;
	list_foreach(list, elm) {
		if (predicate(elm)) return elm;
	}

	return NULL;
}

void
list_insert_before(list_elm_t *elm, list_elm_t *newelm)
{
	newelm->next = elm;
	newelm->prev = elm->prev;
	elm->prev->next = newelm;
	elm->prev = newelm;
}

void
list_insert_sorted(list_t *list, list_elm_t *newelm, list_less_func *less)
{
	list_elm_t *elm;
	list_foreach(list, elm) {
		if (less(newelm, elm)) {
			list_insert_before(elm, newelm);
			return;
		}
	}

	list_append(list, newelm);
}

void
list_append(list_t *list, list_elm_t *elm)
{
	list->tail->next = elm;
	elm->next = (list_elm_t *)&list->null;
	elm->prev = list->tail;
	list->tail = elm;
}

void
list_prepend(list_t *list, list_elm_t *elm)
{
	list->head->prev = elm;
	elm->prev = (list_elm_t *)&list->head;
	elm->next = list->head;
	list->head = elm;
}

void
list_elm_remove(list_elm_t *elm)
{
	elm->next->prev = elm->prev;
	elm->prev->next = elm->next;
	elm->next = NULL;
	elm->prev = NULL;
}

list_elm_t *
list_remove_head(list_t *list)
{
	if (list_is_empty(list)) return NULL;
	list_elm_t *elm = list_head(list);
	list_elm_remove(elm);
	return elm;
}

list_elm_t *
list_remove_tail(list_t *list)
{
	if (list_is_empty(list)) return NULL;
	list_elm_t *elm = list_tail(list);
	list_elm_remove(elm);
	return elm;
}

int
list_is_empty(list_t *list)
{
	return (list->head->next == NULL);
}
