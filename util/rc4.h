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

#include <stddef.h>
#include <stdint.h>

typedef struct RC4Context {
    uint8_t s[256];
} RC4Context;

void rc4_init(RC4Context *ctx, uint8_t *data, size_t len);
void rc4_crypt(RC4Context *ctx, uint8_t *data, size_t len);
