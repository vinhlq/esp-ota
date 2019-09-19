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

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif
#include "mbedtls/sha256.h"

#include "esp_libc.h"

#include "esp_system.h"
#include "esp_log.h"

#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"

#include "jsmn/jsmn.h"
#include "json_parser.h"
#include "json_jsmn.h"
#include "jsondoc/jsondoc.h"

#include "esp_ota_nvs.h"
#include "esp_ota_desc.h"
#include "esp_ota_http.h"

#ifdef ESP_OTA_DEBUG_ENABLED
#ifndef debugPrintln
#define debugPrintln(fmt,args...)	\
	printf("esp-ota-http: " fmt "%s", ## args, "\r\n")
#endif
#else
#define debugPrintln(...)
#endif

#ifndef ESP_OTA_HTTP_UPGRADE_BUF_SIZE
#define ESP_OTA_HTTP_UPGRADE_BUF_SIZE (130)
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

//	debugPrintln("free heap size is %d", system_get_free_heap_size());
	p = (void *)ESP_OTA_REALLOC(ptrmem, size);
	if (!p)
	{
		ESP_OTA_FREE (ptrmem);
	}
	return p;
}

static esp_err_t esp_ota_http_client_open
(
	const esp_http_client_config_t *config,
	esp_http_client_handle_t *out
)
{
	esp_err_t err;
	esp_http_client_handle_t client;

	if (!config)
	{
		debugPrintln("esp_http_client config not found");
		return ESP_ERR_INVALID_ARG;
	}

	if (!config->cert_pem)
	{
		debugPrintln("Server certificate not found in esp_http_client config");
		return ESP_FAIL;
	}

	client = esp_http_client_init(config);
	if (client == NULL)
	{
		debugPrintln("Failed to initialize HTTP connection");
		return ESP_FAIL;
	}

	if (esp_http_client_get_transport_type(client) != HTTP_TRANSPORT_OVER_SSL)
	{
		debugPrintln("Transport is not over HTTPS");
		return ESP_FAIL;
	}
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK)
	{
		esp_http_client_cleanup(client);
		debugPrintln("Failed to open HTTP connection: %d", err);
		return err;
	}
	*out = client;
	return ESP_OK;
}

static void esp_ota_http_client_cleanup(esp_http_client_handle_t client)
{
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

static esp_err_t esp_ota_http_get_desc_internal
	(
		esp_http_client_handle_t client,
		char *upgrade_data_buf,
		unsigned int *buffer_length
	)
{
	esp_err_t err;
	int total_length, read_length, length;

	if((*buffer_length) < 2)
	{
		return ESP_ERR_NO_MEM;
	}

	err = esp_http_client_fetch_headers(client);
#if 1
	if(err == 0)
	{
		debugPrintln("http header is chunked");
		return ESP_FAIL;
	}
	else if(err > 0)
	{
		debugPrintln("http fetch header length: %d", err);
	}
	else
	{
		debugPrintln("http fetch header failed: %d", err);
		return err;
	}
#else
	debugPrintln("http fetch header: %s(%d)", err > 0 ? "success":"fail", err);
#endif

	for (
			total_length=0, length=*buffer_length;
			length > 1;
		)
	{
		read_length = esp_http_client_read
				(
					client,
					&upgrade_data_buf[total_length],
					length
				);
		if (read_length == 0)
		{
			debugPrintln("Connection closed, all data received: %u(bytes)", total_length);
			debugPrintln
				(
					"content[%u]: %.*s",
					total_length,
					total_length,
					upgrade_data_buf
				);
			*buffer_length = total_length;
			return ESP_OK;
		}
		else if (read_length < 0)
		{
			debugPrintln("Error: SSL data read error");
			return read_length;
		}
		else if (read_length > 0)
		{
			if((total_length + read_length) < length)
			{
				total_length += read_length;
				length -= read_length;
				upgrade_data_buf[total_length] = '\0';
				debugPrintln("http data total length: %d", read_length);
			}
			else
			{
				// truncated
				total_length += length;
				length = 0;
				upgrade_data_buf[total_length] = '\0';
				debugPrintln("http data is truncated: %u", total_length);
				debugPrintln("truncated content: %.*s", total_length, upgrade_data_buf);
				break;
			}
		}
	}
	return ESP_ERR_NO_MEM;
}

esp_err_t esp_ota_http_get_desc(const esp_http_client_config_t *config, esp_ota_desc_t *desc)
{
	esp_http_client_handle_t client;
	esp_err_t err;
	char *upgrade_data_buf;
	unsigned int buffer_size, allocated_size;

	err = esp_ota_http_client_open(config, &client);
	if(ESP_OK != err)
	{
		return err;
	}

	buffer_size = allocated_size = ESP_OTA_HTTP_UPGRADE_BUF_SIZE;
	upgrade_data_buf = (char *)ESP_OTA_MALLOC(buffer_size);
	assert(upgrade_data_buf);

	while(1)
	{
		buffer_size = allocated_size;
		err = esp_ota_http_get_desc_internal
		(
			client,
			upgrade_data_buf,
			&buffer_size
		);
		esp_ota_http_client_cleanup(client);

		if(ESP_OK  == err)
		{
			if(esp_ota_desc_parse_json
				(
					upgrade_data_buf,
					buffer_size,
					desc
				))
			{
				err = ESP_FAIL;
				break;
			}

			debugPrintln("ota version: %u.%u", desc->version.major, desc->version.minor);
			break;
		}
		else if(ESP_ERR_NO_MEM == err)
		{
			err = esp_ota_http_client_open(config, &client);
			if(ESP_OK != err)
			{
				break;
			}

			allocated_size = (allocated_size * 4)/3;
			upgrade_data_buf = realloc_safe(upgrade_data_buf, sizeof(char) * allocated_size);
			assert(upgrade_data_buf);
		}
		else
		{
			break;
		}
	}
	ESP_OTA_FREE(upgrade_data_buf);
	return err;
}

static int binary2hex
	(
		unsigned char *buffer,
		int length,
		char *hex_buffer,
		int hex_buffer_length
	)
{
	int i;

	if(((length * 2) + 1) > hex_buffer_length)
		length = (hex_buffer_length - 1) / 2;
	hex_buffer_length = (length * 2) + 1;
	for (i = 0; length > 0; i++, length--)
	{
		uint8_t x = buffer[length-1] & 0x0F;

		if (x > 9) x += ('a'-'9'-1);
		hex_buffer[(length-1) * 2 + 1] = x + '0';
		hex_buffer_length--;

		x = buffer[length-1] >> 4;
		if (x > 9) x += ('a'-'9'-1);
		hex_buffer[(length-1) * 2] = x + '0';
		hex_buffer_length--;
	}
	hex_buffer[i*2] = '\0';
	return (i*2);
}

static esp_err_t esp_ota_http_upgrade_internal
	(
		esp_http_client_handle_t client,
		char *upgrade_data_buf,
		unsigned int *buffer_length,
		mbedtls_sha256_context *ctx,
		const esp_ota_desc_t *desc,
		esp_ota_http_callback_t callback
	)
{
	esp_err_t err, ota_write_err, ota_end_err;
	int ret, total_length, read_length, length, header_reported_length;
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = NULL;

	if((*buffer_length) < ((64*2)+2))
	{
		return ESP_ERR_NO_MEM;
	}
	length = (*buffer_length) - 32;

	err = esp_http_client_fetch_headers(client);
#if 1
	if(err == 0)
	{
		debugPrintln("%s header is chunked", "http");
		return ESP_FAIL;
	}
	else if(err > 0)
	{
		header_reported_length = err;
		debugPrintln("%s fetch header length: %d", "http", header_reported_length);
	}
	else
	{
		debugPrintln("%s fetch header failed: %d", "http", err);
		return err;
	}
#else
	debugPrintln("%s fetch header: %s(%d)", , "http", err > 0 ? "success":"fail", err);
#endif

	debugPrintln("Starting OTA...");
	update_partition = esp_ota_get_next_update_partition(NULL);
	if (update_partition == NULL)
	{
		debugPrintln("Passive OTA partition not found");
		return ESP_FAIL;
	}
	debugPrintln("Writing to partition subtype %d at offset 0x%x",
			 update_partition->subtype, update_partition->address);

	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK)
	{
		debugPrintln("esp_ota_begin failed, error=0x%x", err);
		return err;
	}

	for (
			total_length=0, ota_write_err = ESP_FAIL;;
		)
	{
		read_length = esp_http_client_read
				(
					client,
					upgrade_data_buf,
					length
				);
		if (read_length == 0)
		{
			debugPrintln("Connection closed, all data received: %u(bytes)", total_length);
			err = ESP_OK;
			break;
		}
		else if (read_length < 0)
		{
			debugPrintln("Error: SSL data read error err=0x%x", read_length);
			err = read_length;
			if(callback)
			{
				callback(err, total_length, header_reported_length);
			}
			break;
		}
		else if (read_length > 0)
		{
		    if( ( ret = mbedtls_sha256_update_ret( ctx, upgrade_data_buf, read_length ) ) != 0 )
		    {
		    	debugPrintln("sha256: update failed: %d", ret);
		    	ota_write_err = ESP_FAIL;
		    	break;
		    }

			ota_write_err = esp_ota_write
								(
									update_handle,
									(const void *)upgrade_data_buf,
									read_length
								);
			if (ota_write_err != ESP_OK)
			{
				break;
			}
			total_length += read_length;
			if(callback)
			{
				callback(0, total_length, header_reported_length);
			}
//			debugPrintln("Written image length %d", total_length);
		}
	}


	ota_end_err = esp_ota_end(update_handle);

    if( ( ret = mbedtls_sha256_finish_ret( ctx, &upgrade_data_buf[length] ) ) != 0 )
    {
		debugPrintln("sha256: finish failed: %d", ret);
		return ESP_FAIL;
    }

    binary2hex
		(
			(unsigned char *)desc->sha256,
			32,
			upgrade_data_buf,
			length + 32
		);
    debugPrintln("hash on description: %s", upgrade_data_buf);
    binary2hex
		(
			(unsigned char *)&upgrade_data_buf[length],
			32,
			upgrade_data_buf,
			length + 32
		);
	debugPrintln("hash calculated:     %s", upgrade_data_buf);

    if(memcmp(desc->sha256, &upgrade_data_buf[length], 32))
    {
    	debugPrintln("sha256: is not match");
    	return ESP_FAIL;
    }

	if(err != ESP_OK)
	{
		return err;
	}
	else if (ota_write_err != ESP_OK)
	{
		debugPrintln("Error: esp_ota_write failed! err=0x%x", err);
		return ota_write_err;
	}
	else if (ota_end_err != ESP_OK)
	{
		debugPrintln("Error: esp_ota_end failed! err=0x%x. Image is invalid", ota_end_err);
		return ota_end_err;
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK)
	{
		debugPrintln("esp_ota_set_boot_partition failed! err=0x%x", err);
		return err;
	}
	debugPrintln("esp_ota_set_boot_partition succeeded");

	*buffer_length = total_length;
	return ESP_OK;
}

esp_err_t esp_ota_http_upgrade
(
	const esp_http_client_config_t *config,
	const esp_ota_desc_t *desc,
	esp_ota_http_callback_t callback
)
{
	esp_http_client_handle_t client;
	esp_err_t err;
	char *upgrade_data_buf;
	unsigned int buffer_size;
	mbedtls_sha256_context ctx;
	int ret;

	err = esp_ota_http_client_open(config, &client);
	if(ESP_OK != err)
	{
		return err;
	}

	buffer_size = ESP_OTA_HTTP_UPGRADE_BUF_SIZE;
	upgrade_data_buf = (char *)ESP_OTA_MALLOC(buffer_size);
	assert(upgrade_data_buf);

	mbedtls_sha256_init( &ctx );

	if( ( ret = mbedtls_sha256_starts_ret( &ctx, 0 ) ) != 0 )
	{
		debugPrintln("sha256: start failed: %d", ret);
		err = ESP_FAIL;
		goto exit;
	}

	err = esp_ota_http_upgrade_internal
		(
			client,
			upgrade_data_buf,
			&buffer_size,
			&ctx,
			desc,
			callback
		);

exit:
	esp_ota_http_client_cleanup(client);
	mbedtls_sha256_free( &ctx );
	ESP_OTA_FREE(upgrade_data_buf);
	return err;
}

/*
 * EOF
 */

