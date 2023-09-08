/*
 * Declarations for long-running block device operations
 *
 * Copyright (c) 2011 IBM Corp.
 * Copyright (c) 2012 Red Hat, Inc.
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

#ifndef BLOCKJOB_H
#define BLOCKJOB_H

#include "qemu/job.h"
#include "block/block.h"
#include "qemu/ratelimit.h"

#define BLOCK_JOB_SLICE_TIME 100000000ULL /* ns */

typedef struct BlockJobDriver BlockJobDriver;

/**
 * BlockJob:
 *
 * Long-running operation on a BlockDriverState.
 */
typedef struct BlockJob {
    /**
     * Data belonging to the generic Job infrastructure.
     * Protected by job mutex.
     */
    Job job;

    /**
     * Status that is published by the query-block-jobs QMP API.
     * Protected by job mutex.
     */
    BlockDeviceIoStatus iostatus;

    /**
     * Speed that was set with @block_job_set_speed.
     * Always modified and read under QEMU global mutex (GLOBAL_STATE_CODE).
     */
    int64_t speed;

    /**
     * Rate limiting data structure for implementing @speed.
     * RateLimit API is thread-safe.
     */
    RateLimit limit;

    /**
     * Block other operations when block job is running.
     * Always modified and read under QEMU global mutex (GLOBAL_STATE_CODE).
     */
    Error *blocker;

    /** All notifiers are set once in block_job_create() and never modified. */

    /** Called when a cancelled job is finalised. */
    Notifier finalize_cancelled_notifier;

    /** Called when a successfully completed job is finalised. */
    Notifier finalize_completed_notifier;

    /** Called when the job transitions to PENDING */
    Notifier pending_notifier;

    /** Called when the job transitions to READY */
    Notifier ready_notifier;

    /** Called when the job coroutine yields or terminates */
    Notifier idle_notifier;

    /**
     * BlockDriverStates that are involved in this block job.
     * Always modified and read under QEMU global mutex (GLOBAL_STATE_CODE).
     */
    GSList *nodes;
} BlockJob;

/*
 * Global state (GS) API. These functions run under the BQL.
 *
 * See include/block/block-global-state.h for more information about
 * the GS API.
 */

/**
 * block_job_next_locked:
 * @job: A block job, or %NULL.
 *
 * Get the next element from the list of block jobs after @job, or the
 * first one if @job is %NULL.
 *
 * Returns the requested job, or %NULL if there are no more jobs left.
 * Called with job lock held.
 */
BlockJob *block_job_next_locked(BlockJob *job);

/**
 * block_job_get:
 * @id: The id of the block job.
 *
 * Get the block job identified by @id (which must not be %NULL).
 *
 * Returns the requested job, or %NULL if it doesn't exist.
 * Called with job lock *not* held.
 */
BlockJob *block_job_get(const char *id);

/* Same as block_job_get(), but called with job lock held. */
BlockJob *block_job_get_locked(const char *id);

/**
 * block_job_add_bdrv:
 * @job: A block job
 * @name: The name to assign to the new BdrvChild
 * @bs: A BlockDriverState that is involved in @job
 * @perm, @shared_perm: Permissions to request on the node
 *
 * Add @bs to the list of BlockDriverState that are involved in
 * @job. This means that all operations will be blocked on @bs while
 * @job exists.
 */
int block_job_add_bdrv(BlockJob *job, const char *name, BlockDriverState *bs,
                       uint64_t perm, uint64_t shared_perm, Error **errp);

/**
 * block_job_remove_all_bdrv:
 * @job: The block job
 *
 * Remove all BlockDriverStates from the list of nodes that are involved in the
 * job. This removes the blockers added with block_job_add_bdrv().
 */
void block_job_remove_all_bdrv(BlockJob *job);

/**
 * block_job_has_bdrv:
 * @job: The block job
 *
 * Searches for @bs in the list of nodes that are involved in the
 * job.
 */
bool block_job_has_bdrv(BlockJob *job, BlockDriverState *bs);

/**
 * block_job_set_speed_locked:
 * @job: The job to set the speed for.
 * @speed: The new value
 * @errp: Error object.
 *
 * Set a rate-limiting parameter for the job; the actual meaning may
 * vary depending on the job type.
 *
 * Called with job lock held, but might release it temporarily.
 */
bool block_job_set_speed_locked(BlockJob *job, int64_t speed, Error **errp);

/**
 * block_job_query_locked:
 * @job: The job to get information about.
 *
 * Return information about a job.
 *
 * Called with job lock held.
 */
BlockJobInfo *block_job_query_locked(BlockJob *job, Error **errp);

/**
 * block_job_iostatus_reset_locked:
 * @job: The job whose I/O status should be reset.
 *
 * Reset I/O status on @job and on BlockDriverState objects it uses,
 * other than job->blk.
 *
 * Called with job lock held.
 */
void block_job_iostatus_reset_locked(BlockJob *job);

/*
 * block_job_get_aio_context:
 *
 * Returns aio context associated with a block job.
 */
AioContext *block_job_get_aio_context(BlockJob *job);


/*
 * Common functions that are neither I/O nor Global State.
 *
 * See include/block/block-common.h for more information about
 * the Common API.
 */

/**
 * block_job_is_internal:
 * @job: The job to determine if it is user-visible or not.
 *
 * Returns true if the job should not be visible to the management layer.
 */
bool block_job_is_internal(BlockJob *job);

/**
 * block_job_driver:
 *
 * Returns the driver associated with a block job.
 */
const BlockJobDriver *block_job_driver(BlockJob *job);

#endif
