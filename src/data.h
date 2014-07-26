/*
 * data.h - Definitions for data file access.
 *
 * Copyright (C) 2012-2014  Jon Lund Steffensen <jonlst@gmail.com>
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

#ifndef _DATA_H
#define _DATA_H

#include <sys/types.h>

#include "misc.h"

/* Index 0 is undefined (entry 0 in the data file
   contains a header with the size and total
   number of entries in the file). */

/* This sprite uses DATA_PALETTE_INTRO. */
#define DATA_ART_LANDSCAPE  1

#define DATA_SERF_ANIMATION_TABLE  2
#define DATA_PALETTE_GAME  3

#define DATA_SERF_SHADOW  4

#define DATA_DOTTED_LINES_BASE   5
#define DATA_DOTTED_LINES_COUNT  7

/* undefined: 12-14 */

/* These use DATA_PALETTE_INTRO. */
#define DATA_ART_FLAG_BASE   15
#define DATA_ART_FLAG_COUNT  7

/* undefined: 22-24 */

#define DATA_ART_BOX_BASE   25
#define DATA_ART_BOX_COUNT  14

/* undefined: 39 */

#define DATA_CREDITS_BG  40
#define DATA_BLUE_BYTE   41

#define DATA_SYMBOL_BASE   42
#define DATA_SYMBOL_COUNT  16

/* undefined: 58-59 */

/* Map masks: illegal ones in these
   ranges are undefined. */
#define DATA_MAP_MASK_UP_BASE     60
#define DATA_MAP_MASK_UP_COUNT    81
#define DATA_MAP_MASK_DOWN_BASE   141
#define DATA_MAP_MASK_DOWN_COUNT  81

/* undefined: 222-229 */

#define DATA_PATH_MASK_BASE   230
#define DATA_PATH_MASK_COUNT  26

/* undefined: 257-259 */

#define DATA_MAP_GROUND_BASE   260
#define DATA_MAP_GROUND_COUNT  33

/* undefined: 293-299 */

#define DATA_PATH_GROUND_BASE   300
#define DATA_PATH_GROUND_COUNT  10

/* undefined: 310-320 */

#define DATA_GAME_OBJECT_BASE  320

/* undefined: 347-351,366,373-447 */
/* undefined: 522-599 */

#define DATA_FRAME_TOP_BASE   600
#define DATA_FRAME_TOP_COUNT  4

/* undefined: 604-609 */

#define DATA_MAP_BORDER_BASE   610
#define DATA_MAP_BORDER_COUNT  10

/* undefined: 620-629 */

#define DATA_MAP_WAVES_BASE   630
#define DATA_MAP_WAVES_COUNT  16

/* undefined: 646-659 */

#define DATA_FRAME_POPUP_BASE  660

/* undefined: 664-669 */
/* undefined: 678-749 */

/* Fonts. */
#define DATA_FONT_BASE         750
#define DATA_FONT_COUNT        44

/* undefined: 794-809 */

#define DATA_FONT_SHADOW_BASE   810
#define DATA_FONT_SHADOW_COUNT  44

/* undefined: 854-869 */

/* Icons. */
#define DATA_ICON_BASE              870
#define DATA_ICON_COUNT             308
#define DATA_ICON_MAP_BUTTON_BASE   (DATA_ICON_BASE+0)

#define DATA_ICON_SERF_BASE         (DATA_ICON_BASE+9)
#define DATA_ICON_SERF_COUNT        25
#define DATA_ICON_SERF_TRANSPORTER  (DATA_ICON_SERF_BASE+0)
#define DATA_ICON_SERF_SAILOR       (DATA_ICON_SERF_BASE+1)
#define DATA_ICON_SERF_DIGGER       (DATA_ICON_SERF_BASE+2)
#define DATA_ICON_SERF_BUILDER      (DATA_ICON_SERF_BASE+3)
#define DATA_ICON_SERF_LUMBERJACK   (DATA_ICON_SERF_BASE+4)
#define DATA_ICON_SERF_SAWMILLER    (DATA_ICON_SERF_BASE+5)
#define DATA_ICON_SERF_STONECUTTER  (DATA_ICON_SERF_BASE+6)
#define DATA_ICON_SERF_FORESTER     (DATA_ICON_SERF_BASE+7)
/* ... */

#define DATA_ICON_RESOURCE_BASE     (DATA_ICON_BASE+34)
#define DATA_ICON_RESOURCE_COUNT    26

#define DATA_ICON_EXIT       (DATA_ICON_BASE+60)
#define DATA_ICON_FLIP       (DATA_ICON_BASE+61)
#define DATA_ICON_HOUSE      (DATA_ICON_BASE+62)
#define DATA_ICON_SWORD      (DATA_ICON_BASE+63)
#define DATA_ICON_AREA       (DATA_ICON_BASE+64)

#define DATA_ICON_TIME_30M   (DATA_ICON_BASE+65)
#define DATA_ICON_TIME_2H    (DATA_ICON_BASE+66)
#define DATA_ICON_TIME_10H   (DATA_ICON_BASE+67)
#define DATA_ICON_TIME_50H   (DATA_ICON_BASE+68)

#define DATA_ICON_ALL        (DATA_ICON_BASE+69)

#define DATA_ICON_MENU_BASE   (DATA_ICON_BASE+70)
#define DATA_ICON_MENU_COUNT  8

#define DATA_ICON_DIGIT_BASE   (DATA_ICON_BASE+78)
#define DATA_ICON_DIGIT_COUNT  10

#define DATA_ICON_DISK  (DATA_ICON_BASE+93)

#define DATA_ICON_MINUS   (DATA_ICON_BASE+220)
#define DATA_ICON_PLUS    (DATA_ICON_BASE+221)
#define DATA_ICON_ATTACK  (DATA_ICON_BASE+222)
#define DATA_ICON_NEW     (DATA_ICON_BASE+223)
#define DATA_ICON_SAVE    (DATA_ICON_BASE+224)
#define DATA_ICON_LOAD    (DATA_ICON_BASE+225)

#define DATA_ICON_UP      (DATA_ICON_BASE+237)
#define DATA_ICON_DOWN    (DATA_ICON_BASE+240)

#define DATA_ICON_CHECK  (DATA_ICON_BASE+288)

/* undefined: 1188-1249 */

#define DATA_MAP_OBJECT_BASE          1250
#define DATA_MAP_OBJECT_FLAG          (DATA_MAP_OBJECT_BASE+128)
#define DATA_MAP_OBJECT_CROSS         (DATA_MAP_OBJECT_BASE+144)
#define DATA_MAP_OBJECT_CORNER_STONE  (DATA_MAP_OBJECT_BASE+145)

/* undefined: 1444-1499 */

#define DATA_MAP_SHADOW_BASE  1500

/* undefined: 1694-1749 */

#define DATA_FRAME_BUTTON_BASE  1750

/* undefined: 1775-1779 */

#define DATA_FRAME_BOTTOM_BASE       1780
#define DATA_FRAME_BOTTOM_TIMERS     (DATA_FRAME_BOTTOM_BASE+1)
#define DATA_FRAME_BOTTOM_NOTIFY     (DATA_FRAME_BOTTOM_BASE+2)
#define DATA_FRAME_BOTTOM_NO_NOTIFY  (DATA_FRAME_BOTTOM_BASE+3)
#define DATA_FRAME_BOTTOM_ARROW      (DATA_FRAME_BOTTOM_BASE+4)
#define DATA_FRAME_BOTTOM_NO_ARROW   (DATA_FRAME_BOTTOM_BASE+5)
#define DATA_FRAME_BOTTOM_SHIELD     (DATA_FRAME_BOTTOM_BASE+6)

/* undefined: 1806-1849 */

#define DATA_SERF_ARMS_BASE   1850

/* undefined: 2391-2499 */

#define DATA_SERF_TORSO_BASE  2500

/* undefined: 3041-3149 */

#define DATA_SERF_HEAD_BASE   3150

/* undefined: 3780-3879 */

#define DATA_FRAME_SPLIT_SVGA_BASE   3880
#define DATA_FRAME_SPLIT_SVGA_COUNT  3

/* undefined: 3883-3900 */

/* Sound effects (index 0 is undefined). */
#define DATA_SFX_BASE    3900

/* XMI music (similar to MIDI). */
#define DATA_MUSIC_GAME    3990
#define DATA_MUSIC_ENDING  3992

#define DATA_PALETTE_ENDING  3997
#define DATA_PALETTE_INTRO   3998

#define DATA_CURSOR  3999

int data_load(const char *path);
void data_unload();

void *data_get_object(uint index, size_t *size);

void data_unpack_transparent_sprite(void *dest, const void *src, size_t destlen, int offset);
void data_unpack_overlay_sprite(void *dest, const void *src, size_t destlen);
void data_unpack_mask_sprite(void *dest, const void *src, size_t destlen);

#endif /* ! _DATA_H */
