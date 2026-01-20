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

#include <stddef.h>
#include <stdint.h>

typedef struct SHA1Context {
    uint32_t intermediate[5];
    uint8_t msg_blk[64];
    uint32_t msg_blk_index;
    uint32_t length;
    bool computed;
} SHA1Context;

void sha1_fill(SHA1Context *ctx, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);
void sha1_input(SHA1Context *ctx, uint8_t *data, size_t len);
void sha1_result(SHA1Context *ctx, uint8_t *data);
void sha1_reset(SHA1Context *ctx);
