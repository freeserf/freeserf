/*
 * misc.h - Various definitions of general usefulness
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

#ifndef _MISC_H
#define _MISC_H

#define BIT(n)            (1 << (n))
#define BIT_TEST(x, n)    ((x) & BIT(n))
#define BIT_INVERT(x, n)  ((x) ^= 1 << (n))

#define max(x,y)      (((x) > (y)) ? (x) : (y))
#define min(x,y)      (((x) < (y)) ? (x) : (y))
#define clamp(l,x,h)  (max((l),min((x),(h))))

typedef unsigned int uint;


#if _MSC_VER
#pragma warning( disable : 4996 )
#define snprintf _snprintf
#endif


#endif /* ! _MISC_H */
