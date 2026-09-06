/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef HW_XBOX_NV2A_PGRAPH_UNIFORM_DIRTY_H
#define HW_XBOX_NV2A_PGRAPH_UNIFORM_DIRTY_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static inline void pgraph_uniform_u32_row_update(uint32_t (*rows)[4],
                                                 bool *dirty_rows,
                                                 unsigned int row,
                                                 unsigned int slot,
                                                 uint32_t value)
{
    assert(slot < 4);
    if (rows[row][slot] != value) {
        rows[row][slot] = value;
        dirty_rows[row] = true;
    }
}

static inline void pgraph_uniform_dirty_rows_invalidate(bool *dirty_rows,
                                                        unsigned int row_count)
{
    for (unsigned int row = 0; row < row_count; row++) {
        dirty_rows[row] = true;
    }
}

#endif
