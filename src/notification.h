/*
 * notification.h - Notification GUI component
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

#ifndef _NOTIFICATION_H
#define _NOTIFICATION_H

#include "gui.h"

typedef struct {
	gui_object_t obj;
	struct interface *interface;

	int type;
	int param;
} notification_box_t;

void notification_box_init(notification_box_t *box,
			   struct interface *interface);

#endif /* !_NOTIFICATION_H */
