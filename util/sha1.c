/*
 * QEMU SHA1 Implementation
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

#include "qemu/osdep.h"
#include "crypto/random.h"
#include "qapi/error.h"
#include "qemu/bswap.h"
#include "sha1.h"

static uint32_t xbox_sha1_rotate(uint32_t bits, uint32_t val)
{
    return (val << bits) | (val >> (32 - bits));
}

void xbox_sha1_fill(SHA1Context *ctx, uint32_t a, uint32_t b, uint32_t c,
                    uint32_t d, uint32_t e)
{
    ctx->intermediate[0] = a;
    ctx->intermediate[1] = b;
    ctx->intermediate[2] = c;
    ctx->intermediate[3] = d;
    ctx->intermediate[4] = e;
}

static void xbox_sha1_process(SHA1Context *ctx)
{
    const uint32_t k[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
    uint32_t w[80];
    uint32_t a = ctx->intermediate[0];
    uint32_t b = ctx->intermediate[1];
    uint32_t c = ctx->intermediate[2];
    uint32_t d = ctx->intermediate[3];
    uint32_t e = ctx->intermediate[4];

    for (int i = 0; i < 16; i++) {
        *(uint32_t *)&w[i] = cpu_to_be32(((uint32_t *)ctx->msg_blk)[i]);
    }

    for (int i = 16; i < 80; i++) {
        w[i] = xbox_sha1_rotate(1, w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
    }

    for (int i = 0; i < 80; i++) {
        uint32_t temp = xbox_sha1_rotate(5, a) + w[i] + e;
        switch (i / 20) {
        case 0:
            temp += k[0] + ((b & c) | ((~b) & d));
            break;
        case 1:
            temp += k[1] + (b ^ c ^ d);
            break;
        case 2:
            temp += k[2] + ((b & c) | (b & d) | (c & d));
            break;
        case 3:
            temp += k[3] + (b ^ c ^ d);
            break;
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

static void xbox_sha1_pad(SHA1Context *ctx)
{
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
    *(uint32_t *)&ctx->msg_blk[56] = 0;
    *(uint32_t *)&ctx->msg_blk[60] = cpu_to_be32(ctx->length);
    xbox_sha1_process(ctx);
}

void xbox_sha1_input(SHA1Context *ctx, uint8_t *data, size_t len)
{
    ctx->length += len << 3;
    for (int i = 0; i < len; i++) {
        ctx->msg_blk[ctx->msg_blk_index++] = data[i];
        if (ctx->msg_blk_index == 64) {
            xbox_sha1_process(ctx);
        }
    }
}

void xbox_sha1_result(SHA1Context *ctx, uint8_t *data)
{
    if (!ctx->computed) {
        xbox_sha1_pad(ctx);
        ctx->length = 0;
        ctx->computed = true;
    }
    for (int i = 0; i < 20; ++i) {
        data[i] = ctx->intermediate[i >> 2] >> 8 * (3 - (i & 3));
    }
}

void xbox_sha1_reset(SHA1Context *ctx, XboxEEPROMVersion ver, bool first)
{
    ctx->msg_blk_index = 0;
    ctx->computed = false;
    ctx->length = 512;

    // https://web.archive.org/web/20040618164907/http://www.xbox-linux.org/down/The%20Middle%20Message-1a.pdf
    switch (ver) {
    case XBOX_EEPROM_VERSION_D:
        if (first) {
            xbox_sha1_fill(ctx, 0x85F9E51A, 0xE04613D2, 0x6D86A50C, 0x77C32E3C,
                           0x4BD717A4);
        } else {
            xbox_sha1_fill(ctx, 0x5D7A9C6B, 0xE1922BEB, 0xB82CCDBC, 0x3137AB34,
                           0x486B52B3);
        }
        break;
    case XBOX_EEPROM_VERSION_R2:
        if (first) {
            xbox_sha1_fill(ctx, 0x39B06E79, 0xC9BD25E8, 0xDBC6B498, 0x40B4389D,
                           0x86BBD7ED);
        } else {
            xbox_sha1_fill(ctx, 0x9B49BED3, 0x84B430FC, 0x6B8749CD, 0xEBFE5FE5,
                           0xD96E7393);
        }
        break;
    case XBOX_EEPROM_VERSION_R3:
        if (first) {
            xbox_sha1_fill(ctx, 0x8058763A, 0xF97D4E0E, 0x865A9762, 0x8A3D920D,
                           0x08995B2C);
        } else {
            xbox_sha1_fill(ctx, 0x01075307, 0xA2f1E037, 0x1186EEEA, 0x88DA9992,
                           0x168A5609);
        }
        break;
    default: // default to 1.0 version
        if (first) {
            xbox_sha1_fill(ctx, 0x72127625, 0x336472B9, 0xBE609BEA, 0xF55E226B,
                           0x99958DAC);
        } else {
            xbox_sha1_fill(ctx, 0x76441D41, 0x4DE82659, 0x2E8EF85E, 0xB256FACA,
                           0xC4FE2DE8);
        }
    }
}
