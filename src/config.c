#include <string.h>
#include "config.h"
#include "ezxml.h"
#include "log.h"


char * configData;
char * config[_CONFIG_ENUM_LIST_END];
char configIsAllocated[_CONFIG_ENUM_LIST_END];

const char * configTokens[CONFIG_TOKENS_COUNT] = { CONFIG_TOKENS };
int configHasChanged = 0;


const char * get_config_string(enum_config_t config_id, const char * defaultStr)
{
	if (configTokens == NULL) return defaultStr;
	if ((config_id <= _CONFIG_ENUM_LIST_BEGIN) && (config_id >= _CONFIG_ENUM_LIST_END)) return defaultStr;

	char * str = config[config_id];
	if (str != NULL) return str;

	set_config_string(config_id, defaultStr);

	return defaultStr;
}



int get_config_bool(enum_config_t config_id, int defaultBool)
{
	if (configTokens == NULL) return defaultBool;
	if ((config_id <= _CONFIG_ENUM_LIST_BEGIN) && (config_id >= _CONFIG_ENUM_LIST_END)) return defaultBool;

	char * str = config[config_id];
	if (str != NULL) {
		if (stricmp(str, "true") == 0) return TRUE;
		if (stricmp(str, "false") == 0) return FALSE;

		if (*str = '0') return FALSE;
		if (*str = '1') return TRUE;

		if (stricmp(str, "no") == 0) return FALSE;
		if (stricmp(str, "yes") == 0) return TRUE;
	}

	set_config_bool(config_id, defaultBool);

	return defaultBool;
}


int get_config_int(enum_config_t config_id, int defaultInt)
{
	if (configTokens == NULL) return defaultInt;
	if ((config_id <= _CONFIG_ENUM_LIST_BEGIN) && (config_id >= _CONFIG_ENUM_LIST_END)) return defaultInt;

	char * str = config[config_id];
	if (str != NULL) return atoi(str);

	set_config_int(config_id, defaultInt);

	return defaultInt;
}



void set_config_string(enum_config_t config_id, const char * value)
{
	if (configTokens == NULL) return;
	if ((config_id <= _CONFIG_ENUM_LIST_BEGIN) && (config_id >= _CONFIG_ENUM_LIST_END)) return;

	if (configIsAllocated[config_id] == TRUE) {
		if (config[config_id] != NULL) free(config[config_id]);
		configIsAllocated[config_id] = FALSE;
	}

	config[config_id] = (char *) value;
	configHasChanged = TRUE;
}


void set_config_int(enum_config_t config_id, int value)
{
	if (configTokens == NULL) return;
	if ((config_id <= _CONFIG_ENUM_LIST_BEGIN) && (config_id >= _CONFIG_ENUM_LIST_END)) return;


	if (configIsAllocated[config_id] == TRUE) {
		if (config[config_id]!=NULL) free(config[config_id]);
		configIsAllocated[config_id] = FALSE;
	}

	//- save value
	char * tmpBuf = (char *) calloc(12, sizeof(char) );
	sprintf(tmpBuf, "%d", value);

	config[config_id] = tmpBuf;
	configIsAllocated[config_id] = TRUE;
	configHasChanged = TRUE;
}


void set_config_bool(enum_config_t config_id, int boolValue)
{
	if (boolValue == 1) {
		set_config_string(config_id, "true");
	}
	else {
		set_config_string(config_id, "false");
	}
}



enum_config_t get_configEnum_of_token(const char * token)
{
	if (token == 0) return _CONFIG_ENUM_LIST_BEGIN;

	for (int i = 0; i < _CONFIG_ENUM_LIST_END - _CONFIG_ENUM_LIST_BEGIN; i++) {
		if (configTokens[i] != NULL) {
			if (strcmp(token, configTokens[i]) == 0) return (enum_config_t) (_CONFIG_ENUM_LIST_BEGIN + i + 1);
		}
	}

	LOGW("config", "unknown config file token: %s", token);

	return _CONFIG_ENUM_LIST_BEGIN;
}



void init_config_data()
{
	//- delete old language Data
	if (configData != NULL) {
		free(configData);
		configData = NULL;

		//- reset pointer string-array
		for (int i = _CONFIG_ENUM_LIST_BEGIN + 1; i < _CONFIG_ENUM_LIST_END; i++) {
			if (configIsAllocated[i] == 1)
				if (config[i]!=NULL) free(config[i]);
		}
	}

	//- reset pointer string-array
	for (int i = _CONFIG_ENUM_LIST_BEGIN + 1; i < _CONFIG_ENUM_LIST_END; i++) {
		configIsAllocated[i] = FALSE;
		config[i] = NULL;
	}


	int textLen = 0;
	int i = 0;

	ezxml_t rootTag = ezxml_parse_file(CONFIG_FILE_NAME);
	ezxml_t strTag;

	if (rootTag == NULL) {
		LOGW("config", "unable to open config file '%s'", CONFIG_FILE_NAME);
		return;
	}

	//- calc sting-data len
	for (strTag = ezxml_child(rootTag, "cfg"); strTag; strTag = strTag->next)
	{
		const char * val = ezxml_attr(strTag, "value");
		if (val != NULL) textLen += strlen(val) + 1;
	}


	configData = (char *) calloc(textLen, sizeof(char));
		
	char * configDataPos = configData;

	//- read all strings
	for (strTag = ezxml_child(rootTag, "cfg"); strTag; strTag = strTag->next)
	{
		const char * str_token = ezxml_attr(strTag, "key");
		enum_config_t str_id = get_configEnum_of_token(str_token);

		const char * value = ezxml_attr(strTag, "value");
		if (value == NULL)
		{
			LOGW("config", "config file error: missing 'val' attribute for key='%s'", str_token);
		}
		else if ((str_id > _CONFIG_ENUM_LIST_BEGIN) && (str_id < _CONFIG_ENUM_LIST_END))
		{
			int len = strlen(value)+1;

			//- copy string to buffer
			strncpy(configDataPos, value, len);

			//- save buffer pos of string
			config[str_id] = configDataPos;

			configDataPos += len;
		}
		else
		{
			LOGW("config", "unknown KEY in config file: key='%s'", str_token);
		}
	}

	ezxml_free(rootTag);

}


void config_cleanup()
{
	if (configHasChanged) {
		FILE *f = fopen(CONFIG_FILE_NAME, "w");
		if (f != NULL)
		{
			fprintf(f, "<config>\n");

			for (int i = _CONFIG_ENUM_LIST_BEGIN + 1; i < _CONFIG_ENUM_LIST_END; i++){
				if (config[i] != 0) {
					fprintf(f, "  <cfg key=\"%s\" value=\"%s\" />\n", configTokens[i - _CONFIG_ENUM_LIST_BEGIN - 1], config[i]);
				}
			}

			fprintf(f, "</config>\n");

			fclose(f);
		}
	}


	//- delete temp config values
	for (int i = _CONFIG_ENUM_LIST_BEGIN + 1; i < _CONFIG_ENUM_LIST_END; i++) {
		if (configIsAllocated[i] == TRUE) free(config[i]);
	}

	//- delete/save old config Data
	if (configData != NULL)
	{
		free(configData);
		configData = NULL;
	}
}

