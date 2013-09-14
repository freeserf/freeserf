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

#ifndef _CONFIG_H
#define _CONFIG_H

#include "ezxml.h"

#define TRUE 1
#define FALSE 0


typedef enum {
	_CONFIG_ENUM_LIST_BEGIN,

	CONFIG_LANGUAGE,
	CONFIG_SCREEN_WIDTH,
	CONFIG_SCREEN_HEIGHT,
	CONFIG_FULLSCREEN,
	CONFIG_LOGGLEVEL, 
	CONFIG_MUSIC,
	CONFIG_SFX,
	CONFIG_VOLUME,

	_CONFIG_ENUM_LIST_END
} enum_config_t;

//- has to be the same order as [enum_str_t]!
#define CONFIG_TOKENS "language", "screenWidth", "screenHeight", "fullscreen", "logLevel", "music", "sfx", "volume"


#define CONFIG_TOKENS_COUNT _CONFIG_ENUM_LIST_END - _CONFIG_ENUM_LIST_BEGIN
#define CONFIG_FILE_NAME "config.xml"

const char * get_config_string(enum_config_t config_id, const char * defaultStr);
int get_config_bool(enum_config_t config_id, int defaultBool);
int get_config_int(enum_config_t config_id, int defaultInt);
void set_config_string(enum_config_t config_id, const char * value);
void set_config_bool(enum_config_t config_id, int boolValue);
void set_config_int(enum_config_t config_id, int value);
enum_config_t get_configEnum_of_token(const char * token);
void init_config_data();
void config_cleanup();




#endif