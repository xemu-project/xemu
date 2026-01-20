/*
 * QEMU RC4 Implementation
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

#include "rc4.h"

static void rc4_swap(RC4Context *ctx, int first, int second)
{
    uint8_t temp = ctx->s[first];
    ctx->s[first] = ctx->s[second];
    ctx->s[second] = temp;
}

void rc4_init(RC4Context *ctx, uint8_t *data, size_t len)
{
    for (int i = 0; i < 256; i++) {
        ctx->s[i] = i;
    }
    for (int i = 0, j = 0; i < 256; i++) {
        j = (j + ctx->s[i] + data[i % len]) % 256;
        rc4_swap(ctx, i, j);
    }
}

void rc4_crypt(RC4Context *ctx, uint8_t *data, size_t len)
{
    for (int i = 0, j = 0, k = 0; k < len; k++) {
        i = (i + 1) % 256;
        j = (j + ctx->s[i]) % 256;
        rc4_swap(ctx, i, j);
        data[k] ^= ctx->s[(ctx->s[i] + ctx->s[j]) % 256];
    }
}
