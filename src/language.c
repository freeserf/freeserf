#include <string.h>
#include "language.h"
#include "ezxml.h"
#include "log.h"

char * languageData;
char * language[_STR_ENUM_LIST_END];

const char * languageTokens[LANGUAGE_TOKENS_COUNT] = { LANGUAGE_TOKENS };



const char * get_language_text(enum_str_t str_id, const char * defaultStr)
{
	if (languageData == NULL) return defaultStr;
	if ((str_id <= _STR_ENUM_LIST_BEGIN) && (str_id >= _STR_ENUM_LIST_END)) return defaultStr;

	char * str = language[str_id];
	if (str != NULL) return str;

	return defaultStr;
}

enum_str_t get_strEnum_of_token(const char * token)
{
	if (token == 0) return _STR_ENUM_LIST_BEGIN;

	for (int i = 0; i < _STR_ENUM_LIST_END - _STR_ENUM_LIST_BEGIN ; i++)
	{
		if (languageTokens[i] != NULL)
		{
			if (strcmp(token, languageTokens[i]) == 0) return (enum_str_t) (_STR_ENUM_LIST_BEGIN + i + 1);
		}
	}

	LOGW("language", "unknown language file token: %s", token);

	return _STR_ENUM_LIST_BEGIN;
}


void init_language_data(enum_lng_t lng_id)
{
	char * filename = NULL;

	//- delete old language Data
	if (languageData != NULL)
	{
		free(languageData);
		languageData = NULL;
	}
	
	//- reset pointer string-array
	for (int i = _STR_ENUM_LIST_BEGIN + 1; i < _STR_ENUM_LIST_END; i++)
	{
		language[i] = NULL;
	}

	//- selekt file name
	switch (lng_id)
	{
		//- no English case cause English is returned as default
		case LNG_GERMAN:
			filename = LANGUAGE_FILE("de");
			break;
	}



	if (filename)
	{
		int textLen = 0;
		int i = 0;

		ezxml_t rootTag = ezxml_parse_file(filename);
		ezxml_t strTag;

		if (rootTag == NULL)
		{
			LOGW("language", "unable to open language file '%s'", filename);
			return;
		}

		//- calc sting-data len
		for (strTag = ezxml_child(rootTag, "str"); strTag; strTag = strTag->next)
		{
			const char * val = ezxml_attr(strTag, "text");
			if (val != NULL) textLen += strlen(val) + 1;
		}


		languageData = (char *) calloc(textLen, sizeof(char));
		
		char * languageDataPos = languageData;

		//- read all strings
		for (strTag = ezxml_child(rootTag, "str"); strTag; strTag = strTag->next)
		{
			const char * str_token = ezxml_attr(strTag, "key");
			enum_str_t str_id = get_strEnum_of_token(str_token);

			const char * value = ezxml_attr(strTag, "text");
			if (value == NULL)
			{
				LOGW("language", "language file error: missing 'val' attribute for key='%s'", str_token);
			}
			else if ((str_id > _STR_ENUM_LIST_BEGIN) && (str_id < _STR_ENUM_LIST_END))
			{
				int len = strlen(value)+1;

				//- copy string to buffer
				strncpy(languageDataPos, value, len);

				//- save buffer pos of string
				language[str_id] = languageDataPos;

				languageDataPos += len;
			}
			else
			{
				LOGW("language", "unknown KEY in language file: key='%s'", str_token);
			}
		}

		ezxml_free(rootTag);
	}
}


void language_cleanup()
{
	//- delete old language Data
	if (languageData != NULL)
	{
		free(languageData);
		languageData = NULL;
	}
}