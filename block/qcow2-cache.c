/*
 * L2/refcount table cache for the QCOW2 format
 *
 * Copyright (c) 2010 Kevin Wolf <kwolf@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/memalign.h"
#include "qcow2.h"
#include "trace.h"

typedef struct Qcow2CachedTable {
    int64_t  offset;
    uint64_t lru_counter;
    int      ref;
    bool     dirty;
} Qcow2CachedTable;

struct Qcow2Cache {
    Qcow2CachedTable       *entries;
    struct Qcow2Cache      *depends;
    int                     size;
    int                     table_size;
    bool                    depends_on_flush;
    void                   *table_array;
    uint64_t                lru_counter;
    uint64_t                cache_clean_lru_counter;
};

static inline void *qcow2_cache_get_table_addr(Qcow2Cache *c, int table)
{
    return (uint8_t *) c->table_array + (size_t) table * c->table_size;
}

static inline int qcow2_cache_get_table_idx(Qcow2Cache *c, void *table)
{
    ptrdiff_t table_offset = (uint8_t *) table - (uint8_t *) c->table_array;
    int idx = table_offset / c->table_size;
    assert(idx >= 0 && idx < c->size && table_offset % c->table_size == 0);
    return idx;
}

static inline const char *qcow2_cache_get_name(BDRVQcow2State *s, Qcow2Cache *c)
{
    if (c == s->refcount_block_cache) {
        return "refcount block";
    } else if (c == s->l2_table_cache) {
        return "L2 table";
    } else {
        /* Do not abort, because this is not critical */
        return "unknown";
    }
}

static void qcow2_cache_table_release(Qcow2Cache *c, int i, int num_tables)
{
/* Using MADV_DONTNEED to discard memory is a Linux-specific feature */
#ifdef CONFIG_LINUX
    void *t = qcow2_cache_get_table_addr(c, i);
    int align = qemu_real_host_page_size();
    size_t mem_size = (size_t) c->table_size * num_tables;
    size_t offset = QEMU_ALIGN_UP((uintptr_t) t, align) - (uintptr_t) t;
    size_t length = QEMU_ALIGN_DOWN(mem_size - offset, align);
    if (mem_size > offset && length > 0) {
        madvise((uint8_t *) t + offset, length, MADV_DONTNEED);
    }
#endif
}

static inline bool can_clean_entry(Qcow2Cache *c, int i)
{
    Qcow2CachedTable *t = &c->entries[i];
    return t->ref == 0 && !t->dirty && t->offset != 0 &&
        t->lru_counter <= c->cache_clean_lru_counter;
}

void qcow2_cache_clean_unused(Qcow2Cache *c)
{
    int i = 0;
    while (i < c->size) {
        int to_clean = 0;

        /* Skip the entries that we don't need to clean */
        while (i < c->size && !can_clean_entry(c, i)) {
            i++;
        }

        /* And count how many we can clean in a row */
        while (i < c->size && can_clean_entry(c, i)) {
            c->entries[i].offset = 0;
            c->entries[i].lru_counter = 0;
            i++;
            to_clean++;
        }

        if (to_clean > 0) {
            qcow2_cache_table_release(c, i - to_clean, to_clean);
        }
    }

    c->cache_clean_lru_counter = c->lru_counter;
}

Qcow2Cache *qcow2_cache_create(BlockDriverState *bs, int num_tables,
                               unsigned table_size)
{
    BDRVQcow2State *s = bs->opaque;
    Qcow2Cache *c;

    assert(num_tables > 0);
    assert(is_power_of_2(table_size));
    assert(table_size >= (1 << MIN_CLUSTER_BITS));
    assert(table_size <= s->cluster_size);

    c = g_new0(Qcow2Cache, 1);
    c->size = num_tables;
    c->table_size = table_size;
    c->entries = g_try_new0(Qcow2CachedTable, num_tables);
    c->table_array = qemu_try_blockalign(bs->file->bs,
                                         (size_t) num_tables * c->table_size);

    if (!c->entries || !c->table_array) {
        qemu_vfree(c->table_array);
        g_free(c->entries);
        g_free(c);
        c = NULL;
    }

    return c;
}

int qcow2_cache_destroy(Qcow2Cache *c)
{
    int i;

    for (i = 0; i < c->size; i++) {
        assert(c->entries[i].ref == 0);
    }

    qemu_vfree(c->table_array);
    g_free(c->entries);
    g_free(c);

    return 0;
}

static int qcow2_cache_flush_dependency(BlockDriverState *bs, Qcow2Cache *c)
{
    int ret;

    ret = qcow2_cache_flush(bs, c->depends);
    if (ret < 0) {
        return ret;
    }

    c->depends = NULL;
    c->depends_on_flush = false;

    return 0;
}

static int qcow2_cache_entry_flush(BlockDriverState *bs, Qcow2Cache *c, int i)
{
    BDRVQcow2State *s = bs->opaque;
    int ret = 0;

    if (!c->entries[i].dirty || !c->entries[i].offset) {
        return 0;
    }

    trace_qcow2_cache_entry_flush(qemu_coroutine_self(),
                                  c == s->l2_table_cache, i);

    if (c->depends) {
        ret = qcow2_cache_flush_dependency(bs, c);
    } else if (c->depends_on_flush) {
        ret = bdrv_flush(bs->file->bs);
        if (ret >= 0) {
            c->depends_on_flush = false;
        }
    }

    if (ret < 0) {
        return ret;
    }

    if (c == s->refcount_block_cache) {
        ret = qcow2_pre_write_overlap_check(bs, QCOW2_OL_REFCOUNT_BLOCK,
                c->entries[i].offset, c->table_size, false);
    } else if (c == s->l2_table_cache) {
        ret = qcow2_pre_write_overlap_check(bs, QCOW2_OL_ACTIVE_L2,
                c->entries[i].offset, c->table_size, false);
    } else {
        ret = qcow2_pre_write_overlap_check(bs, 0,
                c->entries[i].offset, c->table_size, false);
    }

    if (ret < 0) {
        return ret;
    }

    if (c == s->refcount_block_cache) {
        BLKDBG_EVENT(bs->file, BLKDBG_REFBLOCK_UPDATE_PART);
    } else if (c == s->l2_table_cache) {
        BLKDBG_EVENT(bs->file, BLKDBG_L2_UPDATE);
    }

    ret = bdrv_pwrite(bs->file, c->entries[i].offset, c->table_size,
                      qcow2_cache_get_table_addr(c, i), 0);
    if (ret < 0) {
        return ret;
    }

    c->entries[i].dirty = false;

    return 0;
}

int qcow2_cache_write(BlockDriverState *bs, Qcow2Cache *c)
{
    BDRVQcow2State *s = bs->opaque;
    int result = 0;
    int ret;
    int i;

    trace_qcow2_cache_flush(qemu_coroutine_self(), c == s->l2_table_cache);

    for (i = 0; i < c->size; i++) {
        ret = qcow2_cache_entry_flush(bs, c, i);
        if (ret < 0 && result != -ENOSPC) {
            result = ret;
        }
    }

    return result;
}

int qcow2_cache_flush(BlockDriverState *bs, Qcow2Cache *c)
{
    int result = qcow2_cache_write(bs, c);

    if (result == 0) {
        int ret = bdrv_flush(bs->file->bs);
        if (ret < 0) {
            result = ret;
        }
    }

    return result;
}

int qcow2_cache_set_dependency(BlockDriverState *bs, Qcow2Cache *c,
    Qcow2Cache *dependency)
{
    int ret;

    if (dependency->depends) {
        ret = qcow2_cache_flush_dependency(bs, dependency);
        if (ret < 0) {
            return ret;
        }
    }

    if (c->depends && (c->depends != dependency)) {
        ret = qcow2_cache_flush_dependency(bs, c);
        if (ret < 0) {
            return ret;
        }
    }

    c->depends = dependency;
    return 0;
}

void qcow2_cache_depends_on_flush(Qcow2Cache *c)
{
    c->depends_on_flush = true;
}

int qcow2_cache_empty(BlockDriverState *bs, Qcow2Cache *c)
{
    int ret, i;

    ret = qcow2_cache_flush(bs, c);
    if (ret < 0) {
        return ret;
    }

    for (i = 0; i < c->size; i++) {
        assert(c->entries[i].ref == 0);
        c->entries[i].offset = 0;
        c->entries[i].lru_counter = 0;
    }

    qcow2_cache_table_release(c, 0, c->size);

    c->lru_counter = 0;

    return 0;
}

static int qcow2_cache_do_get(BlockDriverState *bs, Qcow2Cache *c,
    uint64_t offset, void **table, bool read_from_disk)
{
    BDRVQcow2State *s = bs->opaque;
    int i;
    int ret;
    int lookup_index;
    uint64_t min_lru_counter = UINT64_MAX;
    int min_lru_index = -1;

    assert(offset != 0);

    trace_qcow2_cache_get(qemu_coroutine_self(), c == s->l2_table_cache,
                          offset, read_from_disk);

    if (!QEMU_IS_ALIGNED(offset, c->table_size)) {
        qcow2_signal_corruption(bs, true, -1, -1, "Cannot get entry from %s "
                                "cache: Offset %#" PRIx64 " is unaligned",
                                qcow2_cache_get_name(s, c), offset);
        return -EIO;
    }

    /* Check if the table is already cached */
    i = lookup_index = (offset / c->table_size * 4) % c->size;
    do {
        const Qcow2CachedTable *t = &c->entries[i];
        if (t->offset == offset) {
            goto found;
        }
        if (t->ref == 0 && t->lru_counter < min_lru_counter) {
            min_lru_counter = t->lru_counter;
            min_lru_index = i;
        }
        if (++i == c->size) {
            i = 0;
        }
    } while (i != lookup_index);

    if (min_lru_index == -1) {
        /* This can't happen in current synchronous code, but leave the check
         * here as a reminder for whoever starts using AIO with the cache */
        abort();
    }

    /* Cache miss: write a table back and replace it */
    i = min_lru_index;
    trace_qcow2_cache_get_replace_entry(qemu_coroutine_self(),
                                        c == s->l2_table_cache, i);

    ret = qcow2_cache_entry_flush(bs, c, i);
    if (ret < 0) {
        return ret;
    }

    trace_qcow2_cache_get_read(qemu_coroutine_self(),
                               c == s->l2_table_cache, i);
    c->entries[i].offset = 0;
    if (read_from_disk) {
        if (c == s->l2_table_cache) {
            BLKDBG_EVENT(bs->file, BLKDBG_L2_LOAD);
        }

        ret = bdrv_pread(bs->file, offset, c->table_size,
                         qcow2_cache_get_table_addr(c, i), 0);
        if (ret < 0) {
            return ret;
        }
    }

    c->entries[i].offset = offset;

    /* And return the right table */
found:
    c->entries[i].ref++;
    *table = qcow2_cache_get_table_addr(c, i);

    trace_qcow2_cache_get_done(qemu_coroutine_self(),
                               c == s->l2_table_cache, i);

    return 0;
}

int qcow2_cache_get(BlockDriverState *bs, Qcow2Cache *c, uint64_t offset,
    void **table)
{
    return qcow2_cache_do_get(bs, c, offset, table, true);
}

int qcow2_cache_get_empty(BlockDriverState *bs, Qcow2Cache *c, uint64_t offset,
    void **table)
{
    return qcow2_cache_do_get(bs, c, offset, table, false);
}

void qcow2_cache_put(Qcow2Cache *c, void **table)
{
    int i = qcow2_cache_get_table_idx(c, *table);

    c->entries[i].ref--;
    *table = NULL;

    if (c->entries[i].ref == 0) {
        c->entries[i].lru_counter = ++c->lru_counter;
    }

    assert(c->entries[i].ref >= 0);
}

void qcow2_cache_entry_mark_dirty(Qcow2Cache *c, void *table)
{
    int i = qcow2_cache_get_table_idx(c, table);
    assert(c->entries[i].offset != 0);
    c->entries[i].dirty = true;
}

void *qcow2_cache_is_table_offset(Qcow2Cache *c, uint64_t offset)
{
    int i;

    for (i = 0; i < c->size; i++) {
        if (c->entries[i].offset == offset) {
            return qcow2_cache_get_table_addr(c, i);
        }
    }
    return NULL;
}

void qcow2_cache_discard(Qcow2Cache *c, void *table)
{
    int i = qcow2_cache_get_table_idx(c, table);

    assert(c->entries[i].ref == 0);

    c->entries[i].offset = 0;
    c->entries[i].lru_counter = 0;
    c->entries[i].dirty = false;

    qcow2_cache_table_release(c, i, 1);
}
