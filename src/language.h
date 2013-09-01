/*
* languageh muli language functions and defines
*
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

#ifndef _LANGUAGE_H
#define _LANGUAGE_H

#include "ezxml.h"

#define _S(str_id, string) get_language_text(str_id, string)
#define LANGUAGE_FILE(lang_ident) "lng_" lang_ident ".xml"

typedef enum {
	LNG_ENGLISH,
	LNG_GERMAN
} enum_lng_t;

/*
typedef enum { 
	_STR_ENUM_LIST_BEGIN,
	STR_START_NEW_GAME
} enum_str_t;

char * TOKEN [] = { "STR_START_NEW_GAME", "STR_START_NEW_GAME" };


typedef enum {
	STRING_IDS
} enum_str_t;
*/

typedef enum {
	_STR_ENUM_LIST_BEGIN, 
	
	//-- game-init.cpp
	STR_START_NEW_GAME, 
	STR_MAP_SIZE, 
	STR_START_MISSION, 
	STR_MISSION, 
	STR_START_TRAINING, 
	STR_LEVEL, 
	
	//-- popup.cpp
	STR_MUSIC,
	STR_SOUND,
	STR_EFFECTS,
	STR_VOLUME,
	STR_FULLSCR,
	STR_VIDEO,
	STR_MESSAGES,
	STR_ALL,
	STR_MOST,
	STR_FEW,
	STR_NONE,

	STR_MINING,
	STR_OUTPUT,
	STR_ORDERED,
	STR_BUILD,
	STR_DEFENDERS,
	STR_STATE,
	STR_TRANS_INFO,
	STR_INDEX,
	STR_THIS_BULD,
	STR_STOCK_OF,

	STR_DEST_SURE1,
	STR_DEST_SURE2,
	STR_DEST_SURE3,
	STR_DEST_SURE4,

	STR_NO_PRESENT,
	STR_MINIMUM,
	STR_VERY_FEW,
	STR_BELOW_AVG,
	STR_AVERAGE,
	STR_ABOVE_AVG,
	STR_MUCH,
	STR_VERY_MUCH,
	STR_PERFECT,
	STR_GND_ANALYSIS,
	
	STR_WEAK,
	STR_MEDIUM,
	STR_GOOD,
	STR_FULL,

	STR_QUIT1,
	STR_QUIT2,
	STR_QUIT3,
	STR_QUIT4,

	STR_NOSAVE1,
	STR_NOSAVE2,
	STR_NOSAVE3,
	STR_NOSAVE4,
	STR_NOSAVE5,
	STR_NOSAVE6,

	_STR_ENUM_LIST_END
} enum_str_t;

#define LANGUAGE_TOKENS "START_NEW_GAME", "MAP_SIZE", "START_MISSION", "MISSION", "START_TRAINING", "LEVEL" , "MUSIC", "SOUND", \
	"EFFECTS", "VOLUME", "FULLSCR", "VIDEO", "MESSAGES", "ALL", "MOST", "FEW", "NONE", "MINING", "OUTPUT", "ORDERED", \
	"BUILD", "DEFENDERS", "STATE", "TRANS_INFO", "INDEX", "THIS_BULD", "STOCK_OF", "DEST_SURE1", "DEST_SURE2", "DEST_SURE3", \
	"DEST_SURE4", "NO_PRESENT", "MINIMUM", "VERY_FEW", "BELOW_AVG", "AVERAGE", "ABOVE_AVG", "MUCH", "VERY_MUCH", "PERFECT", \
	"GND_ANALYSIS", "WEAK", "MEDIUM", "GOOD", "FULL", "QUIT1", "QUIT2", "QUIT3", "QUIT4", "NOSAVE1", "NOSAVE2", "NOSAVE3", \
	"NOSAVE4", "NOSAVE5", "NOSAVE6"


#define LANGUAGE_TOKENS_COUNT _STR_ENUM_LIST_END - _STR_ENUM_LIST_BEGIN



const char * get_language_text(enum_str_t str_id, const char * defaultStr);
void init_language_data(enum_lng_t lng_id);
void language_cleanup();

enum_str_t get_strEnum_of_token(const char * token);


#endif