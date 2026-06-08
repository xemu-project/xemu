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
#include "qapi/error.h"
#include "qemu/bswap.h"
#include "sha1.h"

static uint32_t sha1_rotate(uint32_t bits, uint32_t val)
{
    return (val << bits) | (val >> (32 - bits));
}

void sha1_fill(SHA1Context *ctx, uint32_t a, uint32_t b, uint32_t c,
               uint32_t d, uint32_t e)
{
    ctx->intermediate[0] = a;
    ctx->intermediate[1] = b;
    ctx->intermediate[2] = c;
    ctx->intermediate[3] = d;
    ctx->intermediate[4] = e;
}

static void sha1_process(SHA1Context *ctx)
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
        w[i] = sha1_rotate(1, w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]);
    }

    for (int i = 0; i < 80; i++) {
        uint32_t temp = sha1_rotate(5, a) + w[i] + e;
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
        c = sha1_rotate(30, b);
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

static void sha1_pad(SHA1Context *ctx)
{
    ctx->msg_blk[ctx->msg_blk_index++] = 0x80;
    if (ctx->msg_blk_index > 56) {
        while (ctx->msg_blk_index < 64) {
            ctx->msg_blk[ctx->msg_blk_index++] = 0;
        }
        sha1_process(ctx);
    }
    while (ctx->msg_blk_index < 56) {
        ctx->msg_blk[ctx->msg_blk_index++] = 0;
    }
    *(uint32_t *)&ctx->msg_blk[56] = 0;
    *(uint32_t *)&ctx->msg_blk[60] = cpu_to_be32(ctx->length);
    sha1_process(ctx);
}

void sha1_input(SHA1Context *ctx, uint8_t *data, size_t len)
{
    ctx->length += len << 3;
    for (int i = 0; i < len; i++) {
        ctx->msg_blk[ctx->msg_blk_index++] = data[i];
        if (ctx->msg_blk_index == 64) {
            sha1_process(ctx);
        }
    }
}

void sha1_result(SHA1Context *ctx, uint8_t *data)
{
    if (!ctx->computed) {
        sha1_pad(ctx);
        ctx->length = 0;
        ctx->computed = true;
    }
    for (int i = 0; i < 20; ++i) {
        data[i] = ctx->intermediate[i >> 2] >> 8 * (3 - (i & 3));
    }
}

void sha1_reset(SHA1Context *ctx)
{
    sha1_fill(ctx, 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0);

    ctx->msg_blk_index = 0;
    ctx->computed = false;
    ctx->length = 0;
}
