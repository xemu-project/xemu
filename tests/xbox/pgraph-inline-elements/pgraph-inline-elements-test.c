/*
 * Known-input/known-output tests for NV2A inline packet helpers.
 *
 * Copyright (c) 2026 xemu project contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "hw/xbox/nv2a/pgraph/inline-elements.h"

static void test_packet_modes(void)
{
    assert(pgraph_inline_packet_mode(false, false) ==
           PGRAPH_INLINE_PACKET_BULK);
    assert(pgraph_inline_packet_mode(true, false) ==
           PGRAPH_INLINE_PACKET_SCALAR_INCREMENTING);
    assert(pgraph_inline_packet_mode(false, true) ==
           PGRAPH_INLINE_PACKET_SCALAR_TRACE);
    assert(pgraph_inline_packet_mode(true, true) ==
           PGRAPH_INLINE_PACKET_SCALAR_INCREMENTING);
}

static void test_capacity(void)
{
    assert(pgraph_inline_packet_fits(0, 4, 2, 8));
    assert(pgraph_inline_packet_fits(2, 3, 2, 8));
    assert(!pgraph_inline_packet_fits(1, 4, 2, 8));
    assert(!pgraph_inline_packet_fits(9, 0, 1, 8));
    assert(!pgraph_inline_packet_fits(0, 1, 0, 8));
    assert(!pgraph_inline_packet_fits(7, SIZE_MAX, 2, 8));
}

static void test_element16_order(void)
{
    uint32_t output[2] = { UINT32_MAX, UINT32_MAX };

    pgraph_inline_element16_store(output, UINT32_C(0x89abcdef));
    assert(output[0] == UINT32_C(0xcdef));
    assert(output[1] == UINT32_C(0x89ab));
}

int main(void)
{
    test_packet_modes();
    test_capacity();
    test_element16_order();
    return 0;
}
