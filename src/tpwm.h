/*
 * tpwm.h - Uncomprassing TPWM'ed content
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

#ifndef SRC_TPWM_H_
#define SRC_TPWM_H_

#include <cstdlib>

bool tpwm_is_compressed(void *src_data, size_t src_size);
bool tpwm_uncompress(void *src_data, size_t src_size,
                     void **res_data, size_t *res_size,
                     const char **error);

#endif  // SRC_TPWM_H_
