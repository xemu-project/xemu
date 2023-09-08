#include "qemu/osdep.h"
#include "block/aio.h"
#include "block/thread-pool.h"
#include "block/block.h"
#include "qapi/error.h"
#include "qemu/timer.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"

static AioContext *ctx;
static ThreadPool *pool;
static int active;

typedef struct {
    BlockAIOCB *aiocb;
    int n;
    int ret;
} WorkerTestData;

static int worker_cb(void *opaque)
{
    WorkerTestData *data = opaque;
    return qatomic_fetch_inc(&data->n);
}

static int long_cb(void *opaque)
{
    WorkerTestData *data = opaque;
    if (qatomic_cmpxchg(&data->n, 0, 1) == 0) {
        g_usleep(2000000);
        qatomic_or(&data->n, 2);
    }
    return 0;
}

static void done_cb(void *opaque, int ret)
{
    WorkerTestData *data = opaque;
    g_assert(data->ret == -EINPROGRESS || data->ret == -ECANCELED);
    data->ret = ret;
    data->aiocb = NULL;

    /* Callbacks are serialized, so no need to use atomic ops.  */
    active--;
}

static void test_submit(void)
{
    WorkerTestData data = { .n = 0 };
    thread_pool_submit(pool, worker_cb, &data);
    while (data.n == 0) {
        aio_poll(ctx, true);
    }
    g_assert_cmpint(data.n, ==, 1);
}

static void test_submit_aio(void)
{
    WorkerTestData data = { .n = 0, .ret = -EINPROGRESS };
    data.aiocb = thread_pool_submit_aio(pool, worker_cb, &data,
                                        done_cb, &data);

    /* The callbacks are not called until after the first wait.  */
    active = 1;
    g_assert_cmpint(data.ret, ==, -EINPROGRESS);
    while (data.ret == -EINPROGRESS) {
        aio_poll(ctx, true);
    }
    g_assert_cmpint(active, ==, 0);
    g_assert_cmpint(data.n, ==, 1);
    g_assert_cmpint(data.ret, ==, 0);
}

static void co_test_cb(void *opaque)
{
    WorkerTestData *data = opaque;

    active = 1;
    data->n = 0;
    data->ret = -EINPROGRESS;
    thread_pool_submit_co(pool, worker_cb, data);

    /* The test continues in test_submit_co, after qemu_coroutine_enter... */

    g_assert_cmpint(data->n, ==, 1);
    data->ret = 0;
    active--;

    /* The test continues in test_submit_co, after aio_poll... */
}

static void test_submit_co(void)
{
    WorkerTestData data;
    Coroutine *co = qemu_coroutine_create(co_test_cb, &data);

    qemu_coroutine_enter(co);

    /* Back here once the worker has started.  */

    g_assert_cmpint(active, ==, 1);
    g_assert_cmpint(data.ret, ==, -EINPROGRESS);

    /* aio_poll will execute the rest of the coroutine.  */

    while (data.ret == -EINPROGRESS) {
        aio_poll(ctx, true);
    }

    /* Back here after the coroutine has finished.  */

    g_assert_cmpint(active, ==, 0);
    g_assert_cmpint(data.ret, ==, 0);
}

static void test_submit_many(void)
{
    WorkerTestData data[100];
    int i;

    /* Start more work items than there will be threads.  */
    for (i = 0; i < 100; i++) {
        data[i].n = 0;
        data[i].ret = -EINPROGRESS;
        thread_pool_submit_aio(pool, worker_cb, &data[i], done_cb, &data[i]);
    }

    active = 100;
    while (active > 0) {
        aio_poll(ctx, true);
    }
    for (i = 0; i < 100; i++) {
        g_assert_cmpint(data[i].n, ==, 1);
        g_assert_cmpint(data[i].ret, ==, 0);
    }
}

static void do_test_cancel(bool sync)
{
    WorkerTestData data[100];
    int num_canceled;
    int i;

    /* Start more work items than there will be threads, to ensure
     * the pool is full.
     */
    test_submit_many();

    /* Start long running jobs, to ensure we can cancel some.  */
    for (i = 0; i < 100; i++) {
        data[i].n = 0;
        data[i].ret = -EINPROGRESS;
        data[i].aiocb = thread_pool_submit_aio(pool, long_cb, &data[i],
                                               done_cb, &data[i]);
    }

    /* Starting the threads may be left to a bottom half.  Let it
     * run, but do not waste too much time...
     */
    active = 100;
    aio_notify(ctx);
    aio_poll(ctx, false);

    /* Wait some time for the threads to start, with some sanity
     * testing on the behavior of the scheduler...
     */
    g_assert_cmpint(active, ==, 100);
    g_usleep(1000000);
    g_assert_cmpint(active, >, 50);

    /* Cancel the jobs that haven't been started yet.  */
    num_canceled = 0;
    for (i = 0; i < 100; i++) {
        if (qatomic_cmpxchg(&data[i].n, 0, 4) == 0) {
            data[i].ret = -ECANCELED;
            if (sync) {
                bdrv_aio_cancel(data[i].aiocb);
            } else {
                bdrv_aio_cancel_async(data[i].aiocb);
            }
            num_canceled++;
        }
    }
    g_assert_cmpint(active, >, 0);
    g_assert_cmpint(num_canceled, <, 100);

    for (i = 0; i < 100; i++) {
        if (data[i].aiocb && qatomic_read(&data[i].n) < 4) {
            if (sync) {
                /* Canceling the others will be a blocking operation.  */
                bdrv_aio_cancel(data[i].aiocb);
            } else {
                bdrv_aio_cancel_async(data[i].aiocb);
            }
        }
    }

    /* Finish execution and execute any remaining callbacks.  */
    while (active > 0) {
        aio_poll(ctx, true);
    }
    g_assert_cmpint(active, ==, 0);
    for (i = 0; i < 100; i++) {
        g_assert(data[i].aiocb == NULL);
        switch (data[i].n) {
        case 0:
            fprintf(stderr, "Callback not canceled but never started?\n");
            abort();
        case 3:
            /* Couldn't be canceled asynchronously, must have completed.  */
            g_assert_cmpint(data[i].ret, ==, 0);
            break;
        case 4:
            /* Could be canceled asynchronously, never started.  */
            g_assert_cmpint(data[i].ret, ==, -ECANCELED);
            break;
        default:
            fprintf(stderr, "Callback aborted while running?\n");
            abort();
        }
    }
}

static void test_cancel(void)
{
    do_test_cancel(true);
}

static void test_cancel_async(void)
{
    do_test_cancel(false);
}

int main(int argc, char **argv)
{
    qemu_init_main_loop(&error_abort);
    ctx = qemu_get_current_aio_context();
    pool = aio_get_thread_pool(ctx);

    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/thread-pool/submit", test_submit);
    g_test_add_func("/thread-pool/submit-aio", test_submit_aio);
    g_test_add_func("/thread-pool/submit-co", test_submit_co);
    g_test_add_func("/thread-pool/submit-many", test_submit_many);
    g_test_add_func("/thread-pool/cancel", test_cancel);
    g_test_add_func("/thread-pool/cancel-async", test_cancel_async);

    return g_test_run();
}
