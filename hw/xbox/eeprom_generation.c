/*
 * QEMU Xbox EEPROM Generation (MCPX Version 1.0)
 *
 * Copyright (c) 2020 Mike Davis
 * Copyright (c) 2024 Ryan Wendland
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

#include "eeprom_generation.h"
#include "util/sha1.h"
#include "util/rc4.h"

static uint32_t xbox_eeprom_crc(uint8_t *data, size_t len) {
	uint32_t high = 0;
    uint32_t low = 0;
	for (int i = 0; i < len / 4; i++) {
		uint32_t val = le32_to_cpu(((uint32_t*)data)[i]);
		uint64_t sum = ((uint64_t)high << 32) | low;

		high = (sum + val) >> 32;
		low += val;
	}
	return ~(high + low);
}

static void xbox_sha1_reset(SHA1Context *ctx, XboxEEPROMVersion ver, bool first) {
    ctx->msg_blk_index = 0;
    ctx->computed = false;
    ctx->length = 512;

    // https://web.archive.org/web/20040618164907/http://www.xbox-linux.org/down/The%20Middle%20Message-1a.pdf
    switch (ver) {
    case XBOX_EEPROM_VERSION_D:
        if (first) {
            sha1_fill(ctx, 0x85F9E51A, 0xE04613D2,
                0x6D86A50C, 0x77C32E3C, 0x4BD717A4);
        } else {
            sha1_fill(ctx, 0x5D7A9C6B, 0xE1922BEB,
                0xB82CCDBC, 0x3137AB34, 0x486B52B3);
        }   
        break;
    case XBOX_EEPROM_VERSION_R2:
        if (first) {
            sha1_fill(ctx, 0x39B06E79, 0xC9BD25E8,
                0xDBC6B498, 0x40B4389D, 0x86BBD7ED);
        } else {
            sha1_fill(ctx, 0x9B49BED3, 0x84B430FC,
                0x6B8749CD, 0xEBFE5FE5, 0xD96E7393);
        }   
        break;
    case XBOX_EEPROM_VERSION_R3:
        if (first) {
            sha1_fill(ctx, 0x8058763A, 0xF97D4E0E,
                0x865A9762, 0x8A3D920D, 0x08995B2C);
        } else {
            sha1_fill(ctx, 0x01075307, 0xA2f1E037,
                0x1186EEEA, 0x88DA9992, 0x168A5609);
        }   
        break;
    default: // default to 1.0 version
        if (first) {
            sha1_fill(ctx, 0x72127625, 0x336472B9,
                0xBE609BEA, 0xF55E226B, 0x99958DAC);
        } else {
            sha1_fill(ctx, 0x76441D41, 0x4DE82659,
                0x2E8EF85E, 0xB256FACA, 0xC4FE2DE8);
        }    
    }
}

static void xbox_sha1_compute(SHA1Context *ctx, XboxEEPROMVersion ver,
    uint8_t *data, size_t len, uint8_t *hash) {
    xbox_sha1_reset(ctx, ver, true);
    sha1_input(ctx, data, len);
    sha1_result(ctx, ctx->msg_blk);
    xbox_sha1_reset(ctx, ver, false);
    sha1_input(ctx, ctx->msg_blk, 20);
    sha1_result(ctx, hash);
}

bool xbox_eeprom_generate(const char *file, XboxEEPROMVersion ver) {
    XboxEEPROM e;
    memset(&e, 0, sizeof(e));

    // set default North American and NTSC-M region settings
    e.region = cpu_to_le32(1);
    e.video_standard = cpu_to_le32(0x00400100);

    // randomize hardware information
    qcrypto_random_bytes(e.confounder, sizeof(e.confounder), &error_fatal);
    qcrypto_random_bytes(e.hdd_key, sizeof(e.hdd_key), &error_fatal);
    qcrypto_random_bytes(e.online_key, sizeof(e.online_key), &error_fatal);
    memcpy(e.mac, "\x00\x50\xF2", 3);
    qcrypto_random_bytes(e.mac + 3, sizeof(e.mac) - 3, &error_fatal);
    qcrypto_random_bytes(e.serial, sizeof(e.serial), &error_fatal);
    for (int i = 0; i < sizeof(e.serial); i++) {
        e.serial[i] = '0' + (e.serial[i] % 10);
    }
    
    // FIXME: temporarily set default London (GMT+0) time zone and English language
    memcpy(e.user_section, "\x00\x00\x00\x00\x47\x4D\x54\x00\x42\x53\x54\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x0A\x05\x00\x02\x03\x05\x00\x01\x00\x00" \
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xC4\xFF\xFF\xFF", 44);
    *(uint32_t*)(e.user_section + 0x2C) = cpu_to_le32(1);

    // update checksums
    e.checksum = cpu_to_le32(xbox_eeprom_crc(e.serial, 0x2C));
    e.user_checksum = cpu_to_le32(xbox_eeprom_crc(e.user_section, 0x5C));

    // encrypt security section
    RC4Context rctx;
    SHA1Context sctx;
    uint8_t seed[20];
    xbox_sha1_compute(&sctx, ver, e.confounder, 0x1C, e.hash);
    xbox_sha1_compute(&sctx, ver, e.hash, sizeof(e.hash), seed);
    rc4_init(&rctx, seed, sizeof(seed));
    rc4_crypt(&rctx, e.confounder, 0x1C);

    // save to file
    FILE *fd = qemu_fopen(file, "wb");
    if (fd == NULL) {
        return false;
    }

    bool success = fwrite(&e, sizeof(e), 1, fd) == 1;
    fclose(fd);
    return success;
}
