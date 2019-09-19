/*****************************************************************************
* File Name: esp_ota_http.h
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

#ifndef ESP_OTA_DESC_H
#define ESP_OTA_DESC_H

typedef struct
{
	union
	{
		struct
		{
			uint8_t minor;
			uint8_t major;
		};
		uint16_t u16;
	}version;
	uint8_t sha256[32];
}esp_ota_desc_t;

/** @brief esp_ota_nvs_set
 *
 *
 * @param index  Ver.: always
 */
int esp_ota_desc_parse_json(const char *js, unsigned int jslen, esp_ota_desc_t *info);

#endif
