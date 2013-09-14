/*
* languageh multi language functions and defines
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

#if _MSC_VER
  #pragma warning(disable:4996)

  #define snprintf _snprintf
#endif


#define _S(str_id, string) get_language_text(str_id, string)
#define LANGUAGE_FILE(lang_ident) "lng_" lang_ident ".xml"

typedef enum {
	LNG_ENGLISH,
	LNG_GERMAN
} enum_lng_t;


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
	STR_SOUND_EFCT,
	STR_VOLUME,
	STR_FULLSCR,
	STR_VIDEO,
	STR_MESSAGES,
	STR_ALL,
	STR_MOST,
	STR_FEW,
	STR_NONE,

	STR_MINE_OUT,
	STR_BUILD_ORDER,
	STR_DEFENDERS,
	STR_STATE,
	STR_TRANS_INFO,
	STR_INDEX,
	STR_STOCK_OF_BUILD,

	STR_DOMOLISH_SURE,

	STR_NO_PRESENT,
	STR_MINIMAL,
	STR_VERY_FEW,
	STR_BELOW_AVG,
	STR_AVERAGE,
	STR_ABOVE_AVG,
	STR_MUCH,
	STR_VERY_MUCH,
	STR_PERFECT,
	STR_GND_ANALYSIS,
	
	STR_MINIMUM,
	STR_WEAK,
	STR_MEDIUM,
	STR_GOOD,
	STR_FULL,

	STR_QUIT,

	STR_NOSAVE,

	//- notification.cpp
	STR_UNDERATK,
	STR_KNIGHTLOST,
	STR_KNIGHTVIC,
	STR_MINEEMPTY,
	STR_CALLYOU,
	STR_KNIGHTOCCUP,
	STR_STOCKBUILD,
	STR_LANDLOST,
	STR_BUILDLOST,

	STR_YESNO,
	STR_EMERG_PROG_STOP,
	STR_EMERG_PROG_START, 

	STR_GEO_GOLD,
	STR_GEO_IRON,
	STR_GEO_COAL,
	STR_GEO_STONE,
	STR_CALLYOU_MENU,
	STR_30M_SAVE,
	STR_1H_SAVE,
	STR_CALLYOU_STOCK,

	_STR_ENUM_LIST_END
} enum_str_t;

//- has to be the same order as [enum_str_t]!
#define LANGUAGE_TOKENS "START_NEW_GAME", "MAP_SIZE", "START_MISSION", "MISSION", "START_TRAINING", \
	"LEVEL" , "MUSIC", "SOUND_EFCT", "VOLUME", "FULLSCR", "VIDEO", "MESSAGES", "ALL", "MOST", "FEW",\
	"NONE", "MINE_OUT", "BUILD_ORDER", "DEFENDERS", "STATE", "TRANS_INFO", "INDEX", \
	"STOCK_OF_BUILD", "DOMOLISH_SURE", "NO_PRESENT", "MINIMAL", "VERY_FEW", "BELOW_AVG", "AVERAGE",\
	"ABOVE_AVG", "MUCH", "VERY_MUCH", "PERFECT", "GND_ANALYSIS", "MINIMUM", "WEAK", "MEDIUM", "GOOD",\
	"FULL", "QUIT", "NOSAVE", "UNDERATK", "KNIGHTLOST", "KNIGHTVIC", "MINEEMPTY", "CALLYOU",\
	"KNIGHTOCCUP", "STOCKBUILD", "LANDLOST", "BUILDLOST", "YESNO", "EMERG_PROG_STOP", "EMERG_PROG_START",\
	"GEO_GOLD", "GEO_IRON", "GEO_COAL", "GEO_STONE", "CALLYOU_MENU", "30M_SAVE", "1H_SAVE", "CALLYOU_STOCK"


#define LANGUAGE_TOKENS_COUNT _STR_ENUM_LIST_END - _STR_ENUM_LIST_BEGIN



const char * get_language_text(enum_str_t str_id, const char * defaultStr);
void init_language_data(enum_lng_t lng_id);
void language_cleanup();


const char * lagEnum_to_str(enum_lng_t str_id);
enum_lng_t str_to_lagEnum(char * languageStr);

enum_str_t get_strEnum_of_token(const char * token);



#endif