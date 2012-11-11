/*
 * list.h - List header
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

#ifndef _LIST_H
#define _LIST_H


#define LIST_INIT(list)  \
  { .head = (list_elm_t *)&((list).null), \
    .null = NULL, \
    .tail = (list_elm_t *)&((list).head) }

#define list_foreach(list,elm)  \
  for ((elm) = (list)->head; \
       (elm)->next != NULL; \
       (elm) = (elm)->next)

#define list_foreach_reverse(list,elm)  \
  for ((elm) = (list)->tail; \
       (elm)->prev != NULL; \
       (elm) = (elm)->prev)

#define list_foreach_safe(list,elm,tmpelm)  \
  for ((elm) = (list)->head, (tmpelm) = (elm)->next; \
       (tmpelm) != NULL; \
       (elm) = (tmpelm), (tmpelm) = (elm)->next)


typedef struct list list_t;
typedef struct list_elm list_elm_t;

typedef int list_less_func(const list_elm_t *e1, const list_elm_t *e2);
typedef int list_pred_func(const list_elm_t *elm);


/* List */
struct list {
	list_elm_t *head;
	list_elm_t *null;
	list_elm_t *tail;
};

struct list_elm {
	list_elm_t *next;
	list_elm_t *prev;
};

/* List functions */
void list_init(list_t *list);
list_elm_t *list_head(list_t *list);
list_elm_t *list_tail(list_t *list);
list_elm_t *list_find(list_t *list, list_pred_func *predicate); /* O(n) */
void list_append(list_t *list, list_elm_t *elm);
void list_prepend(list_t *list, list_elm_t *elm);
void list_insert_before(list_elm_t *elm, list_elm_t *newelm);
void list_insert_sorted(list_t *list, list_elm_t *newelm,
			list_less_func *compare); /* O(n) */
void list_elm_remove(list_elm_t *elm);
list_elm_t *list_remove_head(list_t *list);
list_elm_t *list_remove_tail(list_t *list);
int list_is_empty(list_t *list);


#endif /* ! _LIST_H */
