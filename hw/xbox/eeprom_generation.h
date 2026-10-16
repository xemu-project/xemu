/*
 * QEMU Xbox EEPROM Generation (MCPX Version 1.0)
 *
 * Copyright (c) 2020 Mike Davis
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_EEPROM_GENERATION_H
#define HW_EEPROM_GENERATION_H

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "crypto/random.h"
#include "include/qemu/bswap.h"

#pragma pack(push,1)
typedef struct XboxEEPROM {
    uint8_t hash[20];
    uint8_t confounder[8];
    uint8_t hdd_key[16];
    uint32_t region;
    uint32_t checksum;
    uint8_t serial[12];
    uint8_t mac[6];
    uint16_t padding;
    uint8_t online_key[16];
    uint32_t video_standard;
    uint32_t padding2;
    uint32_t user_checksum;
    uint8_t user_section[156];
} XboxEEPROM;
#pragma pack(pop)

typedef enum {
    // debug kernels
    XBOX_EEPROM_VERSION_D,
    // retail v1.0 kernels
    XBOX_EEPROM_VERSION_R1,
    // retail v1.1-1.4 kernels
    XBOX_EEPROM_VERSION_R2,
    // retail v1.6 kernels
    XBOX_EEPROM_VERSION_R3
} XboxEEPROMVersion;

bool xbox_eeprom_generate(const char *file, XboxEEPROMVersion ver);

#endif

