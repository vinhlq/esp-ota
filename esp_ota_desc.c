/*****************************************************************************
* File Name: esp_ota.c
*
* Version 1.00
*
* Description:
*   This file contains the declarations of all the high-level APIs.
*
* Note:
*   N/A
*
* Owner:
*   vinhlq
*
* Related Document:
*
* Hardware Dependency:
*   N/A
*
* Code Tested With:
*
******************************************************************************
* Copyright (2019), vinhlq.
******************************************************************************
* This software is owned by vinhlq and is
* protected by and subject to worldwide patent protection (United States and
* foreign), United States copyright laws and international treaty provisions.
* (vinhlq) hereby grants to licensee a personal, non-exclusive, non-transferable
* license to copy, use, modify, create derivative works of, and compile the
* (vinhlq) Source Code and derivative works for the sole purpose of creating
* custom software in support of licensee product to be used only in conjunction
* with a (vinhlq) integrated circuit as specified in the applicable agreement.
* Any reproduction, modification, translation, compilation, or representation of
* this software except as specified above is prohibited without the express
* written permission of (vinhlq).
*
* Disclaimer: CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH
* REGARD TO THIS MATERIAL, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* (vinhlq) reserves the right to make changes without further notice to the
* materials described herein. (vinhlq) does not assume any liability arising out
* of the application or use of any product or circuit described herein. (vinhlq)
* does not authorize its products for use as critical components in life-support
* systems where a malfunction or failure may reasonably be expected to result in
* significant injury to the user. The inclusion of (vinhlq)' product in a life-
* support systems application implies that the manufacturer assumes all risk of
* such use and in doing so indemnifies (vinhlq) against all charges. Use may be
* limited by and subject to the applicable (vinhlq) software license agreement.
*****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "esp_libc.h"

#include "esp_system.h"
#include "esp_log.h"

#include "jsmn/jsmn.h"
#include "json_parser.h"
#include "json_jsmn.h"
#include "jsondoc/jsondoc.h"

#include "esp_ota_desc.h"

#ifdef ESP_OTA_DEBUG_ENABLED
#ifndef debugPrintln
#define debugPrintln(fmt,args...)	\
	printf("esp-ota-desc: " fmt "%s", ## args, "\r\n")
#endif
#else
#define debugPrintln(...)
#endif

#ifndef ESP_OTA_HTTP_UPGRADE_BUF_SIZE
#define ESP_OTA_HTTP_UPGRADE_BUF_SIZE (256)
#endif

#ifndef ESP_OTA_MALLOC
#define ESP_OTA_MALLOC	os_malloc
#endif

#ifndef ESP_OTA_FREE
#define ESP_OTA_FREE	os_free
#endif

#ifndef ESP_OTA_REALLOC
#define ESP_OTA_REALLOC	os_realloc
#endif

/* Function realloc_safe() is a wrapper function for standart realloc()
 * with one difference - it frees old memory pointer in case of realloc
 * failure. Thus, DO NOT use old data pointer in anyway after call to
 * realloc_safe(). If your code has some kind of fallback algorithm if
 * memory can't be re-allocated - use standart realloc() instead.
 */
static inline void *realloc_safe(void *ptrmem, size_t size)
{
	void *p;

//	ESP_LOGE(TAG, "free heap size is %d", system_get_free_heap_size());
	p = (void *)ESP_OTA_REALLOC(ptrmem, size);
	if (!p)
	{
		ESP_OTA_FREE (ptrmem);
	}
	return p;
}

static int json_alloc_and_parse
(
	const char *js, unsigned int jslen,
	const char **keys_filter_list,
	json_jsmntok_t *json_jsmntok, int json_jsmntok_count,
	jsmntok_t **out_tokens
)
{
#define ESP_OTA_HTTP_TOKEN_COUNT	(5)
	jsmntok_t *tokens;
	int rc, tokcount;

	tokcount = ESP_OTA_HTTP_TOKEN_COUNT;
	tokens = ESP_OTA_MALLOC(sizeof(jsmntok_t) * tokcount);
	assert(tokens);

	while(1)
	{
		rc = json_parse
				(	js, jslen,
					tokens, tokcount,
					keys_filter_list,
					json_jsmntok, json_jsmntok_count
				);

		if(rc > 0)
		{
			break;
		}
		else if (rc == JSMN_ERROR_NOMEM)
		{

			tokcount = (tokcount * 4)/3;
			tokens = realloc_safe(tokens, sizeof(jsmntok_t) * tokcount);
			assert(tokens);
		}
		else
		{
			return -1;
		}
	}
	*out_tokens = tokens;
	return rc;
}

static inline uint8_t hex_to_dec(uint8_t hex)
{
	if(hex >= '0' && hex <= '9')
		return (hex - '0');
	else if(hex >= 'a' && hex <= 'f')
		return (hex - 'a' + 10);
	else if(hex >= 'A' && hex <= 'F')
		return (hex - 'A' + 10);
	return 0;
}
static inline int hex_to_bytes(const char *string, uint8_t *bytes, int size)
{
	int i;

	for(i = 0; i < size; i++)
	{
		uint8_t c;

		c = string[i*2];
		if(!c)
		{
			break;
		}
		bytes[i] = hex_to_dec(c) << 4;

		c = string[(i*2)+1];
		if(!c)
		{
			break;
		}
		bytes[i] |= hex_to_dec(c);
	}
	return i;
}

int esp_ota_desc_parse_json(const char *js, unsigned int jslen, esp_ota_desc_t *info)
{
	static const char *filter_list[] = {"version", "sha256", NULL};
	int i, tokcount;
	jsmntok_t *tokens;
	json_jsmntok_t json_jsmntok[2];

	tokcount = json_alloc_and_parse
				(
					js, jslen,
					filter_list,
					json_jsmntok, 6,
					&tokens
				);
	if(tokcount < 0)
	{
		debugPrintln("jsmn_parse: error=%d", tokcount);
		return -1;
	}
	debugPrintln("jsmn_parse: count: %d", tokcount);
	info->version.u16 = 0xffff;
	memset(info->sha256, 0, 32);
	for(i = 0; i < tokcount; i++)
	{
		debugPrintln
			(
				"token[%u]: Name: %.*s, Value: %.*s, Type: %u"
				,
				i,
				jsmntok_get_size(json_jsmntok[i].t_key),
				js+jsmntok_get_offset(json_jsmntok[i].t_key),
				jsmntok_get_size(json_jsmntok[i].t_value),
				js+jsmntok_get_offset(json_jsmntok[i].t_value),
				json_jsmntok[i].t_value->type
			);

		if(
			0 == jsmntok_strcmp(js, json_jsmntok[i].t_key, "version") &&
			json_jsmntok[i].t_value_type == JSMN_PRIMITIVE)
		{
			info->version.u16 = strtol(js+jsmntok_get_offset(json_jsmntok[i].t_value), NULL, 10);
		}
		else if(
			0 == jsmntok_strcmp(js, json_jsmntok[i].t_key, "sha256") &&
			json_jsmntok[i].t_value_type == JSMN_STRING)
		{
			int n;

			n = hex_to_bytes
				(
					js+jsmntok_get_offset(json_jsmntok[i].t_value),
					info->sha256,
					32
				);
			if(n != 32)
			{
				memset(info->sha256, 0, 32);
			}
		}
	}

	// final validate
	if(info->version.u16 == 0xffff)
	{
		return -1;
	}
	for(i = 0; i < 32; i++)
	{
		if(info->sha256[i])
		{
			return 0;
		}
	}
	return -1;
}

/*
 * EOF
 */

