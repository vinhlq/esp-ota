/*****************************************************************************
* File Name: esp_ota.h
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

/*******************************************************************************
* Included headers
*******************************************************************************/

/*******************************************************************************
* User defined Macros
*******************************************************************************/

#ifndef ESP_OTA_NVS_H
#define ESP_OTA_NVS_H

typedef union
{
	struct
	{
		union{
			struct
			{
				uint8_t minor;
				uint8_t major;
			};
			uint16_t u16;
		}version;
		uint8_t flags;
		uint8_t crc;
	};
	uint8_t u8[4];
	uint32_t u32;
}esp_ota_nvs_t;

/** @brief esp_ota_nvs_set
 *
 *
 * @param index  Ver.: always
 */
esp_err_t esp_ota_nvs_set(esp_ota_nvs_t *ota);

/** @brief esp_ota_nvs_get
 *
 *
 * @param index  Ver.: always
 */
esp_err_t esp_ota_nvs_get(esp_ota_nvs_t *ota_read);

esp_err_t esp_ota_nvs_restart_counter_inc(void);

uint32_t esp_ota_nvs_restart_counter_get(void);

esp_err_t esp_ota_nvs_set_upgrade(bool direction);

bool esp_ota_nvs_need_upgrade(bool direction);

esp_err_t esp_ota_nvs_set_upgrade_complete(void);

esp_err_t esp_ota_nvs_factory(uint8_t version_major, uint8_t version_minor);

#endif
