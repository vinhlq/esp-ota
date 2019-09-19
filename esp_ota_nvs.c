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

#include "esp_system.h"
#include "esp_log.h"

#include "esp_partition.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_ota_nvs.h"

#ifdef ESP_OTA_DEBUG_ENABLED
#ifndef debugPrintln
#define debugPrintln(fmt,args...)	\
	printf("esp-ota-nvs: " fmt "%s", ## args, "\r\n")
#endif
#else
#define debugPrintln(...)
#endif

#ifndef ESP_OTA_NVS_STORAGE
#define ESP_OTA_NVS_STORAGE "nvs"
#endif

#ifndef ESP_OTA_NVS_UPGRADE_KEY
#define ESP_OTA_NVS_UPGRADE_KEY	"ota"
#endif

#ifndef ESP_OTA_NVS_RESTART_COUNTER_KEY
#define ESP_OTA_NVS_RESTART_COUNTER_KEY	"restart_conter"
#endif

#define ESP_OTA_FLAG_UPGRADE	(1<<0)
#define ESP_OTA_FLAG_DOWNGRADE	(1<<1)

static uint8_t crc8(uint8_t *data, size_t len)
{
    uint8_t crc = 0xff;
    size_t i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if ((crc & 0x80) != 0)
                crc = (uint8_t)((crc << 1) ^ 0x31);
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t esp_ota_nvs_set(esp_ota_nvs_t *ota)
{
	nvs_handle my_handle;
	esp_err_t err;
	esp_ota_nvs_t ota_read;

	// Open
	err = nvs_open(ESP_OTA_NVS_STORAGE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_open", err);
		return err;
	}

	err = nvs_get_u32(my_handle, ESP_OTA_NVS_UPGRADE_KEY, &ota_read.u32);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_get_u32", err);
		return err;
	}
	if(ota_read.u32 == ota->u32)
	{
		debugPrintln("%s: data is not changed: 0x%x", "nvs", ota->u32);
		return ESP_OK;
	}
	ota_read.crc = crc8(ota_read.u8, 3);

	err = nvs_set_u32(my_handle, ESP_OTA_NVS_UPGRADE_KEY, ota->u32);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_set_u32", err);
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_commit", err);
		return err;
	}

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t esp_ota_nvs_get(esp_ota_nvs_t *ota_read)
{
	nvs_handle my_handle;
	esp_err_t err;
	uint8_t crc;

	// Open
	err = nvs_open(ESP_OTA_NVS_STORAGE, NVS_READONLY, &my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_open", err);
		return ESP_FAIL;
	}

	err = nvs_get_u32(my_handle, ESP_OTA_NVS_UPGRADE_KEY, &ota_read->u32);

	// Close
	nvs_close(my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_get_u8", err);
		return ESP_FAIL;
	}
	crc = crc8(ota_read->u8, 3);
	if(ota_read->crc == crc)
	{
		debugPrintln("%s: crc8 is not match: 0x%x:0x%x", "nvs_get_u8", ota_read->crc, crc);
		return ESP_FAIL;
	}
	return ESP_OK;
}

esp_err_t esp_ota_nvs_set_upgrade(bool direction)
{
	esp_ota_nvs_t ota_rw;
	esp_err_t err;

	err = esp_ota_nvs_get(&ota_rw);
	if(ESP_OK != err)
	{
		return err;
	}
	if(direction)
	{
		ota_rw.flags |= ESP_OTA_FLAG_UPGRADE;
	}
	else
	{
		ota_rw.flags |= ESP_OTA_FLAG_DOWNGRADE;
	}
	return esp_ota_nvs_set(&ota_rw);
}

bool esp_ota_nvs_need_upgrade(bool direction)
{
	esp_ota_nvs_t ota_rw;
	esp_err_t err;

	err = esp_ota_nvs_get(&ota_rw);
	if(ESP_OK != err)
	{
		return false;
	}
	if(direction && ota_rw.flags & ESP_OTA_FLAG_UPGRADE)
	{
		return true;
	}
	else if(!direction && ota_rw.flags & ESP_OTA_FLAG_DOWNGRADE)
	{
		return true;
	}
	return false;
}

esp_err_t esp_ota_nvs_set_upgrade_complete(void)
{
	esp_ota_nvs_t ota_rw;
	esp_err_t err;

	err = esp_ota_nvs_get(&ota_rw);
	if(ESP_OK != err)
	{
		return err;
	}
	ota_rw.flags &= ~(ESP_OTA_FLAG_UPGRADE | ESP_OTA_FLAG_DOWNGRADE);
	return esp_ota_nvs_set(&ota_rw);
}

static esp_err_t esp_ota_reset_counter(void)
{
	nvs_handle my_handle;
	esp_err_t err;
	uint32_t restart_conter;

	// Open
	err = nvs_open(ESP_OTA_NVS_STORAGE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_open", err);
		return err;
	}

	err = nvs_get_u32(my_handle, ESP_OTA_NVS_RESTART_COUNTER_KEY, &restart_conter);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_get_u32", err);
		return err;
	}
	if(restart_conter == 0)
	{
		debugPrintln("%s: data is not changed: 0x%x", "nvs", restart_conter);
		return ESP_OK;
	}

	err = nvs_set_u32(my_handle, ESP_OTA_NVS_RESTART_COUNTER_KEY, 0);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_set_u32", err);
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_commit", err);
		return err;
	}

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

esp_err_t esp_ota_nvs_restart_counter_inc(void)
{
	nvs_handle my_handle;
	esp_err_t err;
	uint32_t restart_conter;

	// Open
	err = nvs_open(ESP_OTA_NVS_STORAGE, NVS_READWRITE, &my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_open", err);
		return err;
	}

	err = nvs_get_u32(my_handle, ESP_OTA_NVS_RESTART_COUNTER_KEY, &restart_conter);
	if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_get_u32", err);
		return err;
	}
	if(restart_conter < 0xffffffff)
	{
		restart_conter++;
	}

	err = nvs_set_u32(my_handle, ESP_OTA_NVS_RESTART_COUNTER_KEY, restart_conter);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_set_u32", err);
		return err;
	}

	err = nvs_commit(my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_commit", err);
		return err;
	}

	// Close
	nvs_close(my_handle);
	return ESP_OK;
}

uint32_t esp_ota_nvs_restart_counter_get(void)
{
	nvs_handle my_handle;
	esp_err_t err;
	uint32_t restart_conter;

	// Open
	err = nvs_open(ESP_OTA_NVS_STORAGE, NVS_READONLY, &my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_open", err);
		return (uint32_t)-1;
	}

	err = nvs_get_u32(my_handle, ESP_OTA_NVS_RESTART_COUNTER_KEY, &restart_conter);

	// Close
	nvs_close(my_handle);
	if (err != ESP_OK)
	{
		debugPrintln("%s: return error: 0x%x", "nvs_get_u8", err);
		return (uint32_t)-1;
	}
	return restart_conter;
}

esp_err_t esp_ota_nvs_factory(uint8_t version_major, uint8_t version_minor)
{
	esp_ota_nvs_t ota_write;

	ota_write.version.major = version_major;
	ota_write.version.minor = version_minor;
	ota_write.flags = 0;
	esp_ota_reset_counter();
	return esp_ota_nvs_set(&ota_write);
}

/*
 * EOF
 */

