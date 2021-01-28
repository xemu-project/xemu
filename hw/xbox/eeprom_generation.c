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

#include "eeprom_generation.h"

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

static void xbox_rc4_swap(RC4Context *ctx, int first, int second) {
    uint8_t temp = ctx->s[first];
    ctx->s[first] = ctx->s[second];
    ctx->s[second] = temp;
}

static void xbox_rc4_init(RC4Context *ctx, uint8_t *data, size_t len) {
    for (int i = 0; i < 256; i++) {
        ctx->s[i] = i;
    }
    for (int i = 0, j = 0; i < 256; i++) {
        j = (j + ctx->s[i] + data[i % len]) % 256;
        xbox_rc4_swap(ctx, i, j);
    }
}

static void xbox_rc4_crypt(RC4Context *ctx, uint8_t *data, size_t len) {
    for (int i = 0, j = 0, k = 0; k < len; k++) {
        i = (i + 1) % 256;
        j = (j + ctx->s[i]) % 256;
        xbox_rc4_swap(ctx, i, j);
        data[k] ^= ctx->s[(ctx->s[i] + ctx->s[j]) % 256];
    }
}

static uint32_t xbox_sha1_rotate(uint32_t bits, uint32_t val) {
    return (val << bits) | (val >> (32 - bits));
}

static void xbox_sha1_fill(SHA1Context *ctx,
    uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) {
    ctx->intermediate[0] = a;
    ctx->intermediate[1] = b;
    ctx->intermediate[2] = c;
    ctx->intermediate[3] = d;
    ctx->intermediate[4] = e;
}

static void xbox_sha1_reset(SHA1Context *ctx, XboxEEPROMVersion ver, bool first) {
    ctx->msg_blk_index = 0;
    ctx->computed = false;
    ctx->length = 512;

    // https://web.archive.org/web/20040618164907/http://www.xbox-linux.org/down/The%20Middle%20Message-1a.pdf
    switch (ver) {
    case XBOX_EEPROM_VERSION_D:
        if (first) {
            xbox_sha1_fill(ctx, 0x85F9E51A, 0xE04613D2, 
                0x6D86A50C, 0x77C32E3C, 0x4BD717A4);
        } else {
            xbox_sha1_fill(ctx, 0x5D7A9C6B, 0xE1922BEB,
                0xB82CCDBC, 0x3137AB34, 0x486B52B3);
        }   
        break;
    case XBOX_EEPROM_VERSION_R2:
        if (first) {
            xbox_sha1_fill(ctx, 0x39B06E79, 0xC9BD25E8, 
                0xDBC6B498, 0x40B4389D, 0x86BBD7ED);
        } else {
            xbox_sha1_fill(ctx, 0x9B49BED3, 0x84B430FC,
                0x6B8749CD, 0xEBFE5FE5, 0xD96E7393);
        }   
        break;
    case XBOX_EEPROM_VERSION_R3:
        if (first) {
            xbox_sha1_fill(ctx, 0x8058763A, 0xF97D4E0E, 
                0x865A9762, 0x8A3D920D, 0x08995B2C);
        } else {
            xbox_sha1_fill(ctx, 0x01075307, 0xA2f1E037,
                0x1186EEEA, 0x88DA9992, 0x168A5609);
        }   
        break;
    default: // default to 1.0 version
        if (first) {
            xbox_sha1_fill(ctx, 0x72127625, 0x336472B9,
                0xBE609BEA, 0xF55E226B, 0x99958DAC);
        } else {
            xbox_sha1_fill(ctx, 0x76441D41, 0x4DE82659,
                0x2E8EF85E, 0xB256FACA, 0xC4FE2DE8);
        }    
    }
}

static void xbox_sha1_process(SHA1Context *ctx) {
    const uint32_t k[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    uint32_t w[80];
    uint32_t a = ctx->intermediate[0];
    uint32_t b = ctx->intermediate[1];
    uint32_t c = ctx->intermediate[2];
    uint32_t d = ctx->intermediate[3];
    uint32_t e = ctx->intermediate[4];

    for (int i = 0; i < 16; i++) {
        *(uint32_t*)&w[i] = cpu_to_be32(((uint32_t*)ctx->msg_blk)[i]);
    }

    for (int i = 16; i < 80; i++) {
        w[i] = xbox_sha1_rotate(1, w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
    }

    for (int i = 0; i < 80; i++) {
        uint32_t temp = xbox_sha1_rotate(5, a) + w[i] + e;
        switch (i / 20) {
        case 0: temp += k[0] + ((b & c) | ((~b) & d)); break;
        case 1: temp += k[1] + (b ^ c ^ d); break;
        case 2: temp += k[2] + ((b & c) | (b & d) | (c & d)); break;
        case 3: temp += k[3] + (b ^ c ^ d); break;
        }
        e = d;
        d = c;
        c = xbox_sha1_rotate(30, b);
        b = a;
        a = temp;
    }

    ctx->intermediate[0] += a;
    ctx->intermediate[1] += b;
    ctx->intermediate[2] += c;
    ctx->intermediate[3] += d;
    ctx->intermediate[4] += e;
    ctx->msg_blk_index = 0;
}

static void xbox_sha1_input(SHA1Context *ctx, uint8_t *data, size_t len) {
    ctx->length += len << 3;
    for (int i = 0; i < len; i++) {
        ctx->msg_blk[ctx->msg_blk_index++] = data[i];
        if (ctx->msg_blk_index == 64) {
            xbox_sha1_process(ctx);
        }
    }
}

static void xbox_sha1_pad(SHA1Context *ctx) {
    ctx->msg_blk[ctx->msg_blk_index++] = 0x80;
    if (ctx->msg_blk_index > 56) {
        while (ctx->msg_blk_index < 64) {
            ctx->msg_blk[ctx->msg_blk_index++] = 0;
        }
        xbox_sha1_process(ctx);
    }
    while (ctx->msg_blk_index < 56) {
        ctx->msg_blk[ctx->msg_blk_index++] = 0;
    }
    *(uint32_t*)&ctx->msg_blk[56] = 0;
    *(uint32_t*)&ctx->msg_blk[60] = cpu_to_be32(ctx->length);
    xbox_sha1_process(ctx);
}

static void xbox_sha1_result(SHA1Context *ctx, uint8_t *data) {
    if (!ctx->computed) {
        xbox_sha1_pad(ctx);
        ctx->length = 0;
        ctx->computed = true;
    }
    for (int i = 0; i < 20; ++i) {
        data[i] = ctx->intermediate[i >> 2] >> 8 * (3 - (i & 3));
    }
}

static void xbox_sha1_compute(SHA1Context *ctx, XboxEEPROMVersion ver,
    uint8_t *data, size_t len, uint8_t *hash) {
    xbox_sha1_reset(ctx, ver, true);
    xbox_sha1_input(ctx, data, len);
    xbox_sha1_result(ctx, ctx->msg_blk);
    xbox_sha1_reset(ctx, ver, false);
    xbox_sha1_input(ctx, ctx->msg_blk, 20);
    xbox_sha1_result(ctx, hash);
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
    xbox_rc4_init(&rctx, seed, sizeof(seed));
    xbox_rc4_crypt(&rctx, e.confounder, 0x1C);

    // save to file
    FILE *fd = fopen(file, "wb");
    if (fd == NULL) {
        return false;
    }

    bool success = fwrite(&e, sizeof(e), 1, fd) == 1;
    fclose(fd);
    return success;
}
