/*
 * QEMU System Emulator block driver
 *
 * Copyright (c) 2003 Fabrice Bellard
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
#ifndef BLOCK_IO_H
#define BLOCK_IO_H

#include "block-common.h"

/*
 * I/O API functions. These functions are thread-safe, and therefore
 * can run in any thread as long as the thread has called
 * aio_context_acquire/release().
 *
 * These functions can only call functions from I/O and Common categories,
 * but can be invoked by GS, "I/O or GS" and I/O APIs.
 *
 * All functions in this category must use the macro
 * IO_CODE();
 * to catch when they are accidentally called by the wrong API.
 */

int generated_co_wrapper bdrv_pwrite_zeroes(BdrvChild *child, int64_t offset,
                                            int64_t bytes,
                                            BdrvRequestFlags flags);
int bdrv_make_zero(BdrvChild *child, BdrvRequestFlags flags);
int generated_co_wrapper bdrv_pread(BdrvChild *child, int64_t offset,
                                    int64_t bytes, void *buf,
                                    BdrvRequestFlags flags);
int generated_co_wrapper bdrv_pwrite(BdrvChild *child, int64_t offset,
                                     int64_t bytes, const void *buf,
                                     BdrvRequestFlags flags);
int generated_co_wrapper bdrv_pwrite_sync(BdrvChild *child, int64_t offset,
                                          int64_t bytes, const void *buf,
                                          BdrvRequestFlags flags);
int coroutine_fn bdrv_co_pwrite_sync(BdrvChild *child, int64_t offset,
                                     int64_t bytes, const void *buf,
                                     BdrvRequestFlags flags);
/*
 * Efficiently zero a region of the disk image.  Note that this is a regular
 * I/O request like read or write and should have a reasonable size.  This
 * function is not suitable for zeroing the entire image in a single request
 * because it may allocate memory for the entire region.
 */
int coroutine_fn bdrv_co_pwrite_zeroes(BdrvChild *child, int64_t offset,
                                       int64_t bytes, BdrvRequestFlags flags);

int coroutine_fn bdrv_co_truncate(BdrvChild *child, int64_t offset, bool exact,
                                  PreallocMode prealloc, BdrvRequestFlags flags,
                                  Error **errp);

int64_t bdrv_nb_sectors(BlockDriverState *bs);
int64_t bdrv_getlength(BlockDriverState *bs);
int64_t bdrv_get_allocated_file_size(BlockDriverState *bs);
BlockMeasureInfo *bdrv_measure(BlockDriver *drv, QemuOpts *opts,
                               BlockDriverState *in_bs, Error **errp);
void bdrv_get_geometry(BlockDriverState *bs, uint64_t *nb_sectors_ptr);
int coroutine_fn bdrv_co_delete_file(BlockDriverState *bs, Error **errp);
void coroutine_fn bdrv_co_delete_file_noerr(BlockDriverState *bs);


/* async block I/O */
void bdrv_aio_cancel(BlockAIOCB *acb);
void bdrv_aio_cancel_async(BlockAIOCB *acb);

/* sg packet commands */
int coroutine_fn bdrv_co_ioctl(BlockDriverState *bs, int req, void *buf);

/* Ensure contents are flushed to disk.  */
int coroutine_fn bdrv_co_flush(BlockDriverState *bs);

int coroutine_fn bdrv_co_pdiscard(BdrvChild *child, int64_t offset,
                                  int64_t bytes);
bool bdrv_can_write_zeroes_with_unmap(BlockDriverState *bs);
int bdrv_block_status(BlockDriverState *bs, int64_t offset,
                      int64_t bytes, int64_t *pnum, int64_t *map,
                      BlockDriverState **file);
int bdrv_block_status_above(BlockDriverState *bs, BlockDriverState *base,
                            int64_t offset, int64_t bytes, int64_t *pnum,
                            int64_t *map, BlockDriverState **file);
int bdrv_is_allocated(BlockDriverState *bs, int64_t offset, int64_t bytes,
                      int64_t *pnum);
int bdrv_is_allocated_above(BlockDriverState *top, BlockDriverState *base,
                            bool include_base, int64_t offset, int64_t bytes,
                            int64_t *pnum);
int coroutine_fn bdrv_co_is_zero_fast(BlockDriverState *bs, int64_t offset,
                                      int64_t bytes);

int bdrv_can_set_read_only(BlockDriverState *bs, bool read_only,
                           bool ignore_allow_rdw, Error **errp);
int bdrv_apply_auto_read_only(BlockDriverState *bs, const char *errmsg,
                              Error **errp);
bool bdrv_is_read_only(BlockDriverState *bs);
bool bdrv_is_writable(BlockDriverState *bs);
bool bdrv_is_sg(BlockDriverState *bs);
int bdrv_get_flags(BlockDriverState *bs);
bool bdrv_is_inserted(BlockDriverState *bs);
void bdrv_lock_medium(BlockDriverState *bs, bool locked);
void bdrv_eject(BlockDriverState *bs, bool eject_flag);
const char *bdrv_get_format_name(BlockDriverState *bs);

bool bdrv_supports_compressed_writes(BlockDriverState *bs);
const char *bdrv_get_node_name(const BlockDriverState *bs);
const char *bdrv_get_device_name(const BlockDriverState *bs);
const char *bdrv_get_device_or_node_name(const BlockDriverState *bs);
int bdrv_get_info(BlockDriverState *bs, BlockDriverInfo *bdi);
ImageInfoSpecific *bdrv_get_specific_info(BlockDriverState *bs,
                                          Error **errp);
BlockStatsSpecific *bdrv_get_specific_stats(BlockDriverState *bs);
void bdrv_round_to_clusters(BlockDriverState *bs,
                            int64_t offset, int64_t bytes,
                            int64_t *cluster_offset,
                            int64_t *cluster_bytes);

void bdrv_get_backing_filename(BlockDriverState *bs,
                               char *filename, int filename_size);

int bdrv_save_vmstate(BlockDriverState *bs, const uint8_t *buf,
                      int64_t pos, int size);

int bdrv_load_vmstate(BlockDriverState *bs, uint8_t *buf,
                      int64_t pos, int size);

/*
 * Returns the alignment in bytes that is required so that no bounce buffer
 * is required throughout the stack
 */
size_t bdrv_min_mem_align(BlockDriverState *bs);
/* Returns optimal alignment in bytes for bounce buffer */
size_t bdrv_opt_mem_align(BlockDriverState *bs);
void *qemu_blockalign(BlockDriverState *bs, size_t size);
void *qemu_blockalign0(BlockDriverState *bs, size_t size);
void *qemu_try_blockalign(BlockDriverState *bs, size_t size);
void *qemu_try_blockalign0(BlockDriverState *bs, size_t size);

void bdrv_enable_copy_on_read(BlockDriverState *bs);
void bdrv_disable_copy_on_read(BlockDriverState *bs);

void bdrv_debug_event(BlockDriverState *bs, BlkdebugEvent event);

#define BLKDBG_EVENT(child, evt) \
    do { \
        if (child) { \
            bdrv_debug_event(child->bs, evt); \
        } \
    } while (0)

/**
 * bdrv_get_aio_context:
 *
 * Returns: the currently bound #AioContext
 */
AioContext *bdrv_get_aio_context(BlockDriverState *bs);

AioContext *bdrv_child_get_parent_aio_context(BdrvChild *c);

/**
 * Move the current coroutine to the AioContext of @bs and return the old
 * AioContext of the coroutine. Increase bs->in_flight so that draining @bs
 * will wait for the operation to proceed until the corresponding
 * bdrv_co_leave().
 *
 * Consequently, you can't call drain inside a bdrv_co_enter/leave() section as
 * this will deadlock.
 */
AioContext *coroutine_fn bdrv_co_enter(BlockDriverState *bs);

/**
 * Ends a section started by bdrv_co_enter(). Move the current coroutine back
 * to old_ctx and decrease bs->in_flight again.
 */
void coroutine_fn bdrv_co_leave(BlockDriverState *bs, AioContext *old_ctx);

/**
 * Transfer control to @co in the aio context of @bs
 */
void bdrv_coroutine_enter(BlockDriverState *bs, Coroutine *co);

AioContext *child_of_bds_get_parent_aio_context(BdrvChild *c);

void bdrv_io_plug(BlockDriverState *bs);
void bdrv_io_unplug(BlockDriverState *bs);

bool bdrv_can_store_new_dirty_bitmap(BlockDriverState *bs, const char *name,
                                     uint32_t granularity, Error **errp);

/**
 *
 * bdrv_co_copy_range:
 *
 * Do offloaded copy between two children. If the operation is not implemented
 * by the driver, or if the backend storage doesn't support it, a negative
 * error code will be returned.
 *
 * Note: block layer doesn't emulate or fallback to a bounce buffer approach
 * because usually the caller shouldn't attempt offloaded copy any more (e.g.
 * calling copy_file_range(2)) after the first error, thus it should fall back
 * to a read+write path in the caller level.
 *
 * @src: Source child to copy data from
 * @src_offset: offset in @src image to read data
 * @dst: Destination child to copy data to
 * @dst_offset: offset in @dst image to write data
 * @bytes: number of bytes to copy
 * @flags: request flags. Supported flags:
 *         BDRV_REQ_ZERO_WRITE - treat the @src range as zero data and do zero
 *                               write on @dst as if bdrv_co_pwrite_zeroes is
 *                               called. Used to simplify caller code, or
 *                               during BlockDriver.bdrv_co_copy_range_from()
 *                               recursion.
 *         BDRV_REQ_NO_SERIALISING - do not serialize with other overlapping
 *                                   requests currently in flight.
 *
 * Returns: 0 if succeeded; negative error code if failed.
 **/
int coroutine_fn bdrv_co_copy_range(BdrvChild *src, int64_t src_offset,
                                    BdrvChild *dst, int64_t dst_offset,
                                    int64_t bytes, BdrvRequestFlags read_flags,
                                    BdrvRequestFlags write_flags);

/**
 * bdrv_drained_end_no_poll:
 *
 * Same as bdrv_drained_end(), but do not poll for the subgraph to
 * actually become unquiesced.  Therefore, no graph changes will occur
 * with this function.
 *
 * *drained_end_counter is incremented for every background operation
 * that is scheduled, and will be decremented for every operation once
 * it settles.  The caller must poll until it reaches 0.  The counter
 * should be accessed using atomic operations only.
 */
void bdrv_drained_end_no_poll(BlockDriverState *bs, int *drained_end_counter);


/*
 * "I/O or GS" API functions. These functions can run without
 * the BQL, but only in one specific iothread/main loop.
 *
 * More specifically, these functions use BDRV_POLL_WHILE(bs), which
 * requires the caller to be either in the main thread and hold
 * the BlockdriverState (bs) AioContext lock, or directly in the
 * home thread that runs the bs AioContext. Calling them from
 * another thread in another AioContext would cause deadlocks.
 *
 * Therefore, these functions are not proper I/O, because they
 * can't run in *any* iothreads, but only in a specific one.
 *
 * These functions can call any function from I/O, Common and this
 * categories, but must be invoked only by other "I/O or GS" and GS APIs.
 *
 * All functions in this category must use the macro
 * IO_OR_GS_CODE();
 * to catch when they are accidentally called by the wrong API.
 */

#define BDRV_POLL_WHILE(bs, cond) ({                       \
    BlockDriverState *bs_ = (bs);                          \
    IO_OR_GS_CODE();                                       \
    AIO_WAIT_WHILE(bdrv_get_aio_context(bs_),              \
                   cond); })

void bdrv_drain(BlockDriverState *bs);

int generated_co_wrapper
bdrv_truncate(BdrvChild *child, int64_t offset, bool exact,
              PreallocMode prealloc, BdrvRequestFlags flags, Error **errp);

int generated_co_wrapper bdrv_check(BlockDriverState *bs, BdrvCheckResult *res,
                                    BdrvCheckMode fix);

/* Invalidate any cached metadata used by image formats */
int generated_co_wrapper bdrv_invalidate_cache(BlockDriverState *bs,
                                               Error **errp);
int generated_co_wrapper bdrv_flush(BlockDriverState *bs);
int generated_co_wrapper bdrv_pdiscard(BdrvChild *child, int64_t offset,
                                       int64_t bytes);
int generated_co_wrapper
bdrv_readv_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);
int generated_co_wrapper
bdrv_writev_vmstate(BlockDriverState *bs, QEMUIOVector *qiov, int64_t pos);

/**
 * bdrv_parent_drained_begin_single:
 *
 * Begin a quiesced section for the parent of @c. If @poll is true, wait for
 * any pending activity to cease.
 */
void bdrv_parent_drained_begin_single(BdrvChild *c, bool poll);

/**
 * bdrv_parent_drained_end_single:
 *
 * End a quiesced section for the parent of @c.
 *
 * This polls @bs's AioContext until all scheduled sub-drained_ends
 * have settled, which may result in graph changes.
 */
void bdrv_parent_drained_end_single(BdrvChild *c);

/**
 * bdrv_drain_poll:
 *
 * Poll for pending requests in @bs, its parents (except for @ignore_parent),
 * and if @recursive is true its children as well (used for subtree drain).
 *
 * If @ignore_bds_parents is true, parents that are BlockDriverStates must
 * ignore the drain request because they will be drained separately (used for
 * drain_all).
 *
 * This is part of bdrv_drained_begin.
 */
bool bdrv_drain_poll(BlockDriverState *bs, bool recursive,
                     BdrvChild *ignore_parent, bool ignore_bds_parents);

/**
 * bdrv_drained_begin:
 *
 * Begin a quiesced section for exclusive access to the BDS, by disabling
 * external request sources including NBD server, block jobs, and device model.
 *
 * This function can be recursive.
 */
void bdrv_drained_begin(BlockDriverState *bs);

/**
 * bdrv_do_drained_begin_quiesce:
 *
 * Quiesces a BDS like bdrv_drained_begin(), but does not wait for already
 * running requests to complete.
 */
void bdrv_do_drained_begin_quiesce(BlockDriverState *bs,
                                   BdrvChild *parent, bool ignore_bds_parents);

/**
 * Like bdrv_drained_begin, but recursively begins a quiesced section for
 * exclusive access to all child nodes as well.
 */
void bdrv_subtree_drained_begin(BlockDriverState *bs);

/**
 * bdrv_drained_end:
 *
 * End a quiescent section started by bdrv_drained_begin().
 *
 * This polls @bs's AioContext until all scheduled sub-drained_ends
 * have settled.  On one hand, that may result in graph changes.  On
 * the other, this requires that the caller either runs in the main
 * loop; or that all involved nodes (@bs and all of its parents) are
 * in the caller's AioContext.
 */
void bdrv_drained_end(BlockDriverState *bs);

/**
 * End a quiescent section started by bdrv_subtree_drained_begin().
 */
void bdrv_subtree_drained_end(BlockDriverState *bs);

#endif /* BLOCK_IO_H */
