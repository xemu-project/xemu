/*
 * Fsdev Throttle
 *
 * Copyright (C) 2016 Huawei Technologies Duesseldorf GmbH
 *
 * Author: Pradeep Jagadeesh <pradeep.jagadeesh@huawei.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * (at your option) any later version.
 *
 * See the COPYING file in the top-level directory for details.
 *
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qemu-fsdev-throttle.h"
#include "qemu/iov.h"
#include "qemu/main-loop.h"
#include "qemu/option.h"

static void fsdev_throttle_read_timer_cb(void *opaque)
{
    FsThrottle *fst = opaque;
    qemu_co_enter_next(&fst->throttled_reqs[false], NULL);
}

static void fsdev_throttle_write_timer_cb(void *opaque)
{
    FsThrottle *fst = opaque;
    qemu_co_enter_next(&fst->throttled_reqs[true], NULL);
}

int fsdev_throttle_parse_opts(QemuOpts *opts, FsThrottle *fst, Error **errp)
{
    throttle_config_init(&fst->cfg);
    fst->cfg.buckets[THROTTLE_BPS_TOTAL].avg =
        qemu_opt_get_number(opts, "throttling.bps-total", 0);
    fst->cfg.buckets[THROTTLE_BPS_READ].avg  =
        qemu_opt_get_number(opts, "throttling.bps-read", 0);
    fst->cfg.buckets[THROTTLE_BPS_WRITE].avg =
        qemu_opt_get_number(opts, "throttling.bps-write", 0);
    fst->cfg.buckets[THROTTLE_OPS_TOTAL].avg =
        qemu_opt_get_number(opts, "throttling.iops-total", 0);
    fst->cfg.buckets[THROTTLE_OPS_READ].avg =
        qemu_opt_get_number(opts, "throttling.iops-read", 0);
    fst->cfg.buckets[THROTTLE_OPS_WRITE].avg =
        qemu_opt_get_number(opts, "throttling.iops-write", 0);

    fst->cfg.buckets[THROTTLE_BPS_TOTAL].max =
        qemu_opt_get_number(opts, "throttling.bps-total-max", 0);
    fst->cfg.buckets[THROTTLE_BPS_READ].max  =
        qemu_opt_get_number(opts, "throttling.bps-read-max", 0);
    fst->cfg.buckets[THROTTLE_BPS_WRITE].max =
        qemu_opt_get_number(opts, "throttling.bps-write-max", 0);
    fst->cfg.buckets[THROTTLE_OPS_TOTAL].max =
        qemu_opt_get_number(opts, "throttling.iops-total-max", 0);
    fst->cfg.buckets[THROTTLE_OPS_READ].max =
        qemu_opt_get_number(opts, "throttling.iops-read-max", 0);
    fst->cfg.buckets[THROTTLE_OPS_WRITE].max =
        qemu_opt_get_number(opts, "throttling.iops-write-max", 0);

    fst->cfg.buckets[THROTTLE_BPS_TOTAL].burst_length =
        qemu_opt_get_number(opts, "throttling.bps-total-max-length", 1);
    fst->cfg.buckets[THROTTLE_BPS_READ].burst_length  =
        qemu_opt_get_number(opts, "throttling.bps-read-max-length", 1);
    fst->cfg.buckets[THROTTLE_BPS_WRITE].burst_length =
        qemu_opt_get_number(opts, "throttling.bps-write-max-length", 1);
    fst->cfg.buckets[THROTTLE_OPS_TOTAL].burst_length =
        qemu_opt_get_number(opts, "throttling.iops-total-max-length", 1);
    fst->cfg.buckets[THROTTLE_OPS_READ].burst_length =
        qemu_opt_get_number(opts, "throttling.iops-read-max-length", 1);
    fst->cfg.buckets[THROTTLE_OPS_WRITE].burst_length =
        qemu_opt_get_number(opts, "throttling.iops-write-max-length", 1);
    fst->cfg.op_size =
        qemu_opt_get_number(opts, "throttling.iops-size", 0);

    return throttle_is_valid(&fst->cfg, errp) ? 0 : -1;
}

void fsdev_throttle_init(FsThrottle *fst)
{
    if (throttle_enabled(&fst->cfg)) {
        throttle_init(&fst->ts);
        throttle_timers_init(&fst->tt,
                             qemu_get_aio_context(),
                             QEMU_CLOCK_REALTIME,
                             fsdev_throttle_read_timer_cb,
                             fsdev_throttle_write_timer_cb,
                             fst);
        throttle_config(&fst->ts, QEMU_CLOCK_REALTIME, &fst->cfg);
        qemu_co_queue_init(&fst->throttled_reqs[0]);
        qemu_co_queue_init(&fst->throttled_reqs[1]);
    }
}

void coroutine_fn fsdev_co_throttle_request(FsThrottle *fst,
                                            ThrottleDirection direction,
                                            struct iovec *iov, int iovcnt)
{
    assert(direction < THROTTLE_MAX);
    if (throttle_enabled(&fst->cfg)) {
        if (throttle_schedule_timer(&fst->ts, &fst->tt, direction) ||
            !qemu_co_queue_empty(&fst->throttled_reqs[direction])) {
            qemu_co_queue_wait(&fst->throttled_reqs[direction], NULL);
        }

        throttle_account(&fst->ts, direction, iov_size(iov, iovcnt));

        if (!qemu_co_queue_empty(&fst->throttled_reqs[direction]) &&
            !throttle_schedule_timer(&fst->ts, &fst->tt, direction)) {
            qemu_co_queue_next(&fst->throttled_reqs[direction]);
        }
    }
}

void fsdev_throttle_cleanup(FsThrottle *fst)
{
    if (throttle_enabled(&fst->cfg)) {
        throttle_timers_destroy(&fst->tt);
    }
}
