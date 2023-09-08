/*
 * Blockjob tests
 *
 * Copyright Igalia, S.L. 2016
 *
 * Authors:
 *  Alberto Garcia   <berto@igalia.com>
 *
 * This work is licensed under the terms of the GNU LGPL, version 2 or later.
 * See the COPYING.LIB file in the top-level directory.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/main-loop.h"
#include "block/blockjob_int.h"
#include "sysemu/block-backend.h"
#include "qapi/qmp/qdict.h"
#include "iothread.h"

static const BlockJobDriver test_block_job_driver = {
    .job_driver = {
        .instance_size = sizeof(BlockJob),
        .free          = block_job_free,
        .user_resume   = block_job_user_resume,
    },
};

static void block_job_cb(void *opaque, int ret)
{
}

static BlockJob *mk_job(BlockBackend *blk, const char *id,
                        const BlockJobDriver *drv, bool should_succeed,
                        int flags)
{
    BlockJob *job;
    Error *err = NULL;

    job = block_job_create(id, drv, NULL, blk_bs(blk),
                           0, BLK_PERM_ALL, 0, flags, block_job_cb,
                           NULL, &err);
    if (should_succeed) {
        g_assert_null(err);
        g_assert_nonnull(job);
        if (id) {
            g_assert_cmpstr(job->job.id, ==, id);
        } else {
            g_assert_cmpstr(job->job.id, ==, blk_name(blk));
        }
    } else {
        error_free_or_abort(&err);
        g_assert_null(job);
    }

    return job;
}

static BlockJob *do_test_id(BlockBackend *blk, const char *id,
                            bool should_succeed)
{
    return mk_job(blk, id, &test_block_job_driver,
                  should_succeed, JOB_DEFAULT);
}

/* This creates a BlockBackend (optionally with a name) with a
 * BlockDriverState inserted. */
static BlockBackend *create_blk(const char *name)
{
    /* No I/O is performed on this device */
    BlockBackend *blk = blk_new(qemu_get_aio_context(), 0, BLK_PERM_ALL);
    BlockDriverState *bs;

    QDict *opt = qdict_new();
    qdict_put_str(opt, "file.read-zeroes", "on");
    bs = bdrv_open("null-co://", NULL, opt, 0, &error_abort);
    g_assert_nonnull(bs);

    blk_insert_bs(blk, bs, &error_abort);
    bdrv_unref(bs);

    if (name) {
        Error *err = NULL;
        monitor_add_blk(blk, name, &err);
        g_assert_null(err);
    }

    return blk;
}

/* This destroys the backend */
static void destroy_blk(BlockBackend *blk)
{
    if (blk_name(blk)[0] != '\0') {
        monitor_remove_blk(blk);
    }

    blk_remove_bs(blk);
    blk_unref(blk);
}

static void test_job_ids(void)
{
    BlockBackend *blk[3];
    BlockJob *job[3];

    blk[0] = create_blk(NULL);
    blk[1] = create_blk("drive1");
    blk[2] = create_blk("drive2");

    /* No job ID provided and the block backend has no name */
    job[0] = do_test_id(blk[0], NULL, false);

    /* These are all invalid job IDs */
    job[0] = do_test_id(blk[0], "0id", false);
    job[0] = do_test_id(blk[0], "",    false);
    job[0] = do_test_id(blk[0], "   ", false);
    job[0] = do_test_id(blk[0], "123", false);
    job[0] = do_test_id(blk[0], "_id", false);
    job[0] = do_test_id(blk[0], "-id", false);
    job[0] = do_test_id(blk[0], ".id", false);
    job[0] = do_test_id(blk[0], "#id", false);

    /* This one is valid */
    job[0] = do_test_id(blk[0], "id0", true);

    /* We can have two jobs in the same BDS */
    job[1] = do_test_id(blk[0], "id1", true);
    job_early_fail(&job[1]->job);

    /* Duplicate job IDs are not allowed */
    job[1] = do_test_id(blk[1], "id0", false);

    /* But once job[0] finishes we can reuse its ID */
    job_early_fail(&job[0]->job);
    job[1] = do_test_id(blk[1], "id0", true);

    /* No job ID specified, defaults to the backend name ('drive1') */
    job_early_fail(&job[1]->job);
    job[1] = do_test_id(blk[1], NULL, true);

    /* Duplicate job ID */
    job[2] = do_test_id(blk[2], "drive1", false);

    /* The ID of job[2] would default to 'drive2' but it is already in use */
    job[0] = do_test_id(blk[0], "drive2", true);
    job[2] = do_test_id(blk[2], NULL, false);

    /* This one is valid */
    job[2] = do_test_id(blk[2], "id_2", true);

    job_early_fail(&job[0]->job);
    job_early_fail(&job[1]->job);
    job_early_fail(&job[2]->job);

    destroy_blk(blk[0]);
    destroy_blk(blk[1]);
    destroy_blk(blk[2]);
}

typedef struct CancelJob {
    BlockJob common;
    BlockBackend *blk;
    bool should_converge;
    bool should_complete;
} CancelJob;

static void cancel_job_complete(Job *job, Error **errp)
{
    CancelJob *s = container_of(job, CancelJob, common.job);
    s->should_complete = true;
}

static int coroutine_fn cancel_job_run(Job *job, Error **errp)
{
    CancelJob *s = container_of(job, CancelJob, common.job);

    while (!s->should_complete) {
        if (job_is_cancelled(&s->common.job)) {
            return 0;
        }

        if (!job_is_ready(&s->common.job) && s->should_converge) {
            job_transition_to_ready(&s->common.job);
        }

        job_sleep_ns(&s->common.job, 100000);
    }

    return 0;
}

static const BlockJobDriver test_cancel_driver = {
    .job_driver = {
        .instance_size = sizeof(CancelJob),
        .free          = block_job_free,
        .user_resume   = block_job_user_resume,
        .run           = cancel_job_run,
        .complete      = cancel_job_complete,
    },
};

static CancelJob *create_common(Job **pjob)
{
    BlockBackend *blk;
    Job *job;
    BlockJob *bjob;
    CancelJob *s;

    blk = create_blk(NULL);
    bjob = mk_job(blk, "Steve", &test_cancel_driver, true,
                  JOB_MANUAL_FINALIZE | JOB_MANUAL_DISMISS);
    job = &bjob->job;
    WITH_JOB_LOCK_GUARD() {
        job_ref_locked(job);
        assert(job->status == JOB_STATUS_CREATED);
    }

    s = container_of(bjob, CancelJob, common);
    s->blk = blk;

    *pjob = job;
    return s;
}

static void cancel_common(CancelJob *s)
{
    BlockJob *job = &s->common;
    BlockBackend *blk = s->blk;
    JobStatus sts = job->job.status;
    AioContext *ctx = job->job.aio_context;

    job_cancel_sync(&job->job, true);
    WITH_JOB_LOCK_GUARD() {
        if (sts != JOB_STATUS_CREATED && sts != JOB_STATUS_CONCLUDED) {
            Job *dummy = &job->job;
            job_dismiss_locked(&dummy, &error_abort);
        }
        assert(job->job.status == JOB_STATUS_NULL);
        job_unref_locked(&job->job);
    }

    aio_context_acquire(ctx);
    destroy_blk(blk);
    aio_context_release(ctx);

}

static void test_cancel_created(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);
    cancel_common(s);
}

static void assert_job_status_is(Job *job, int status)
{
    WITH_JOB_LOCK_GUARD() {
        assert(job->status == status);
    }
}

static void test_cancel_running(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    assert_job_status_is(job, JOB_STATUS_RUNNING);

    cancel_common(s);
}

static void test_cancel_paused(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    WITH_JOB_LOCK_GUARD() {
        assert(job->status == JOB_STATUS_RUNNING);
        job_user_pause_locked(job, &error_abort);
    }
    job_enter(job);
    assert_job_status_is(job, JOB_STATUS_PAUSED);

    cancel_common(s);
}

static void test_cancel_ready(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    assert_job_status_is(job, JOB_STATUS_RUNNING);

    s->should_converge = true;
    job_enter(job);
    assert_job_status_is(job, JOB_STATUS_READY);

    cancel_common(s);
}

static void test_cancel_standby(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    assert_job_status_is(job, JOB_STATUS_RUNNING);

    s->should_converge = true;
    job_enter(job);
    WITH_JOB_LOCK_GUARD() {
        assert(job->status == JOB_STATUS_READY);
        job_user_pause_locked(job, &error_abort);
    }
    job_enter(job);
    assert_job_status_is(job, JOB_STATUS_STANDBY);

    cancel_common(s);
}

static void test_cancel_pending(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    assert_job_status_is(job, JOB_STATUS_RUNNING);

    s->should_converge = true;
    job_enter(job);
    WITH_JOB_LOCK_GUARD() {
        assert(job->status == JOB_STATUS_READY);
        job_complete_locked(job, &error_abort);
    }
    job_enter(job);
    while (!job->deferred_to_main_loop) {
        aio_poll(qemu_get_aio_context(), true);
    }
    assert_job_status_is(job, JOB_STATUS_READY);
    aio_poll(qemu_get_aio_context(), true);
    assert_job_status_is(job, JOB_STATUS_PENDING);

    cancel_common(s);
}

static void test_cancel_concluded(void)
{
    Job *job;
    CancelJob *s;

    s = create_common(&job);

    job_start(job);
    assert_job_status_is(job, JOB_STATUS_RUNNING);

    s->should_converge = true;
    job_enter(job);
    WITH_JOB_LOCK_GUARD() {
        assert(job->status == JOB_STATUS_READY);
        job_complete_locked(job, &error_abort);
    }
    job_enter(job);
    while (!job->deferred_to_main_loop) {
        aio_poll(qemu_get_aio_context(), true);
    }
    assert_job_status_is(job, JOB_STATUS_READY);
    aio_poll(qemu_get_aio_context(), true);
    assert_job_status_is(job, JOB_STATUS_PENDING);

    WITH_JOB_LOCK_GUARD() {
        job_finalize_locked(job, &error_abort);
        assert(job->status == JOB_STATUS_CONCLUDED);
    }

    cancel_common(s);
}

/* (See test_yielding_driver for the job description) */
typedef struct YieldingJob {
    BlockJob common;
    bool should_complete;
} YieldingJob;

static void yielding_job_complete(Job *job, Error **errp)
{
    YieldingJob *s = container_of(job, YieldingJob, common.job);
    s->should_complete = true;
    job_enter(job);
}

static int coroutine_fn yielding_job_run(Job *job, Error **errp)
{
    YieldingJob *s = container_of(job, YieldingJob, common.job);

    job_transition_to_ready(job);

    while (!s->should_complete) {
        job_yield(job);
    }

    return 0;
}

/*
 * This job transitions immediately to the READY state, and then
 * yields until it is to complete.
 */
static const BlockJobDriver test_yielding_driver = {
    .job_driver = {
        .instance_size  = sizeof(YieldingJob),
        .free           = block_job_free,
        .user_resume    = block_job_user_resume,
        .run            = yielding_job_run,
        .complete       = yielding_job_complete,
    },
};

/*
 * Test that job_complete_locked() works even on jobs that are in a paused
 * state (i.e., STANDBY).
 *
 * To do this, run YieldingJob in an IO thread, get it into the READY
 * state, then have a drained section.  Before ending the section,
 * acquire the context so the job will not be entered and will thus
 * remain on STANDBY.
 *
 * job_complete_locked() should still work without error.
 *
 * Note that on the QMP interface, it is impossible to lock an IO
 * thread before a drained section ends.  In practice, the
 * bdrv_drain_all_end() and the aio_context_acquire() will be
 * reversed.  However, that makes for worse reproducibility here:
 * Sometimes, the job would no longer be in STANDBY then but already
 * be started.  We cannot prevent that, because the IO thread runs
 * concurrently.  We can only prevent it by taking the lock before
 * ending the drained section, so we do that.
 *
 * (You can reverse the order of operations and most of the time the
 * test will pass, but sometimes the assert(status == STANDBY) will
 * fail.)
 */
static void test_complete_in_standby(void)
{
    BlockBackend *blk;
    IOThread *iothread;
    AioContext *ctx;
    Job *job;
    BlockJob *bjob;

    /* Create a test drive, move it to an IO thread */
    blk = create_blk(NULL);
    iothread = iothread_new();

    ctx = iothread_get_aio_context(iothread);
    blk_set_aio_context(blk, ctx, &error_abort);

    /* Create our test job */
    bjob = mk_job(blk, "job", &test_yielding_driver, true,
                  JOB_MANUAL_FINALIZE | JOB_MANUAL_DISMISS);
    job = &bjob->job;
    assert_job_status_is(job, JOB_STATUS_CREATED);

    /* Wait for the job to become READY */
    job_start(job);
    /*
     * Here we are waiting for the status to change, so don't bother
     * protecting the read every time.
     */
    AIO_WAIT_WHILE_UNLOCKED(ctx, job->status != JOB_STATUS_READY);

    /* Begin the drained section, pausing the job */
    bdrv_drain_all_begin();
    assert_job_status_is(job, JOB_STATUS_STANDBY);

    /* Lock the IO thread to prevent the job from being run */
    aio_context_acquire(ctx);
    /* This will schedule the job to resume it */
    bdrv_drain_all_end();
    aio_context_release(ctx);

    WITH_JOB_LOCK_GUARD() {
        /* But the job cannot run, so it will remain on standby */
        assert(job->status == JOB_STATUS_STANDBY);

        /* Even though the job is on standby, this should work */
        job_complete_locked(job, &error_abort);

        /* The test is done now, clean up. */
        job_finish_sync_locked(job, NULL, &error_abort);
        assert(job->status == JOB_STATUS_PENDING);

        job_finalize_locked(job, &error_abort);
        assert(job->status == JOB_STATUS_CONCLUDED);

        job_dismiss_locked(&job, &error_abort);
    }

    aio_context_acquire(ctx);
    destroy_blk(blk);
    aio_context_release(ctx);
    iothread_join(iothread);
}

int main(int argc, char **argv)
{
    qemu_init_main_loop(&error_abort);
    bdrv_init();

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/blockjob/ids", test_job_ids);
    g_test_add_func("/blockjob/cancel/created", test_cancel_created);
    g_test_add_func("/blockjob/cancel/running", test_cancel_running);
    g_test_add_func("/blockjob/cancel/paused", test_cancel_paused);
    g_test_add_func("/blockjob/cancel/ready", test_cancel_ready);
    g_test_add_func("/blockjob/cancel/standby", test_cancel_standby);
    g_test_add_func("/blockjob/cancel/pending", test_cancel_pending);
    g_test_add_func("/blockjob/cancel/concluded", test_cancel_concluded);
    g_test_add_func("/blockjob/complete_in_standby", test_complete_in_standby);
    return g_test_run();
}
