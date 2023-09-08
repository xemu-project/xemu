/*
 * QEMU host block devices
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
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
#include "sysemu/block-backend.h"
#include "sysemu/blockdev.h"
#include "hw/block/block.h"
#include "block/blockjob.h"
#include "block/qdict.h"
#include "block/throttle-groups.h"
#include "monitor/monitor.h"
#include "qemu/error-report.h"
#include "qemu/option.h"
#include "qemu/qemu-print.h"
#include "qemu/config-file.h"
#include "qapi/qapi-commands-block.h"
#include "qapi/qapi-commands-transaction.h"
#include "qapi/qapi-visit-block-core.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qnum.h"
#include "qapi/qmp/qstring.h"
#include "qapi/error.h"
#include "qapi/qmp/qerror.h"
#include "qapi/qmp/qlist.h"
#include "qapi/qobject-output-visitor.h"
#include "sysemu/sysemu.h"
#include "sysemu/iothread.h"
#include "block/block_int.h"
#include "block/trace.h"
#include "sysemu/runstate.h"
#include "sysemu/replay.h"
#include "qemu/cutils.h"
#include "qemu/help_option.h"
#include "qemu/main-loop.h"
#include "qemu/throttle-options.h"

/* Protected by BQL */
QTAILQ_HEAD(, BlockDriverState) monitor_bdrv_states =
    QTAILQ_HEAD_INITIALIZER(monitor_bdrv_states);

void bdrv_set_monitor_owned(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    QTAILQ_INSERT_TAIL(&monitor_bdrv_states, bs, monitor_list);
}

static const char *const if_name[IF_COUNT] = {
    [IF_NONE] = "none",
    [IF_IDE] = "ide",
    [IF_SCSI] = "scsi",
    [IF_FLOPPY] = "floppy",
    [IF_PFLASH] = "pflash",
    [IF_MTD] = "mtd",
    [IF_SD] = "sd",
    [IF_VIRTIO] = "virtio",
    [IF_XEN] = "xen",
};

static int if_max_devs[IF_COUNT] = {
    /*
     * Do not change these numbers!  They govern how drive option
     * index maps to unit and bus.  That mapping is ABI.
     *
     * All controllers used to implement if=T drives need to support
     * if_max_devs[T] units, for any T with if_max_devs[T] != 0.
     * Otherwise, some index values map to "impossible" bus, unit
     * values.
     *
     * For instance, if you change [IF_SCSI] to 255, -drive
     * if=scsi,index=12 no longer means bus=1,unit=5, but
     * bus=0,unit=12.  With an lsi53c895a controller (7 units max),
     * the drive can't be set up.  Regression.
     */
    [IF_IDE] = 2,
    [IF_SCSI] = 7,
};

/**
 * Boards may call this to offer board-by-board overrides
 * of the default, global values.
 */
void override_max_devs(BlockInterfaceType type, int max_devs)
{
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    if (max_devs <= 0) {
        return;
    }

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo->type == type) {
            fprintf(stderr, "Cannot override units-per-bus property of"
                    " the %s interface, because a drive of that type has"
                    " already been added.\n", if_name[type]);
            g_assert_not_reached();
        }
    }

    if_max_devs[type] = max_devs;
}

/*
 * We automatically delete the drive when a device using it gets
 * unplugged.  Questionable feature, but we can't just drop it.
 * Device models call blockdev_mark_auto_del() to schedule the
 * automatic deletion, and generic qdev code calls blockdev_auto_del()
 * when deletion is actually safe.
 */
void blockdev_mark_auto_del(BlockBackend *blk)
{
    DriveInfo *dinfo = blk_legacy_dinfo(blk);
    BlockJob *job;

    GLOBAL_STATE_CODE();

    if (!dinfo) {
        return;
    }

    JOB_LOCK_GUARD();

    do {
        job = block_job_next_locked(NULL);
        while (job && (job->job.cancelled ||
                       job->job.deferred_to_main_loop ||
                       !block_job_has_bdrv(job, blk_bs(blk))))
        {
            job = block_job_next_locked(job);
        }
        if (job) {
            /*
             * This drops the job lock temporarily and polls, so we need to
             * restart processing the list from the start after this.
             */
            job_cancel_locked(&job->job, false);
        }
    } while (job);

    dinfo->auto_del = 1;
}

void blockdev_auto_del(BlockBackend *blk)
{
    DriveInfo *dinfo = blk_legacy_dinfo(blk);
    GLOBAL_STATE_CODE();

    if (dinfo && dinfo->auto_del) {
        monitor_remove_blk(blk);
        blk_unref(blk);
    }
}

static int drive_index_to_bus_id(BlockInterfaceType type, int index)
{
    int max_devs = if_max_devs[type];
    return max_devs ? index / max_devs : 0;
}

static int drive_index_to_unit_id(BlockInterfaceType type, int index)
{
    int max_devs = if_max_devs[type];
    return max_devs ? index % max_devs : index;
}

QemuOpts *drive_add(BlockInterfaceType type, int index, const char *file,
                    const char *optstr)
{
    QemuOpts *opts;

    GLOBAL_STATE_CODE();

    opts = qemu_opts_parse_noisily(qemu_find_opts("drive"), optstr, false);
    if (!opts) {
        return NULL;
    }
    if (type != IF_DEFAULT) {
        qemu_opt_set(opts, "if", if_name[type], &error_abort);
    }
    if (index >= 0) {
        qemu_opt_set_number(opts, "index", index, &error_abort);
    }
    if (file)
        qemu_opt_set(opts, "file", file, &error_abort);
    return opts;
}

DriveInfo *drive_get(BlockInterfaceType type, int bus, int unit)
{
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo && dinfo->type == type
            && dinfo->bus == bus && dinfo->unit == unit) {
            return dinfo;
        }
    }

    return NULL;
}

/*
 * Check board claimed all -drive that are meant to be claimed.
 * Fatal error if any remain unclaimed.
 */
void drive_check_orphaned(void)
{
    BlockBackend *blk;
    DriveInfo *dinfo;
    Location loc;
    bool orphans = false;

    GLOBAL_STATE_CODE();

    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        /*
         * Ignore default drives, because we create certain default
         * drives unconditionally, then leave them unclaimed.  Not the
         * users fault.
         * Ignore IF_VIRTIO, because it gets desugared into -device,
         * so we can leave failing to -device.
         * Ignore IF_NONE, because leaving unclaimed IF_NONE remains
         * available for device_add is a feature.
         */
        if (dinfo->is_default || dinfo->type == IF_VIRTIO
            || dinfo->type == IF_NONE) {
            continue;
        }
        if (!blk_get_attached_dev(blk)) {
            loc_push_none(&loc);
            qemu_opts_loc_restore(dinfo->opts);
            error_report("machine type does not support"
                         " if=%s,bus=%d,unit=%d",
                         if_name[dinfo->type], dinfo->bus, dinfo->unit);
            loc_pop(&loc);
            orphans = true;
        }
    }

    if (orphans) {
        exit(1);
    }
}

DriveInfo *drive_get_by_index(BlockInterfaceType type, int index)
{
    GLOBAL_STATE_CODE();
    return drive_get(type,
                     drive_index_to_bus_id(type, index),
                     drive_index_to_unit_id(type, index));
}

int drive_get_max_bus(BlockInterfaceType type)
{
    int max_bus;
    BlockBackend *blk;
    DriveInfo *dinfo;

    GLOBAL_STATE_CODE();

    max_bus = -1;
    for (blk = blk_next(NULL); blk; blk = blk_next(blk)) {
        dinfo = blk_legacy_dinfo(blk);
        if (dinfo && dinfo->type == type && dinfo->bus > max_bus) {
            max_bus = dinfo->bus;
        }
    }
    return max_bus;
}

static void bdrv_format_print(void *opaque, const char *name)
{
    qemu_printf(" %s", name);
}

typedef struct {
    QEMUBH *bh;
    BlockDriverState *bs;
} BDRVPutRefBH;

static int parse_block_error_action(const char *buf, bool is_read, Error **errp)
{
    if (!strcmp(buf, "ignore")) {
        return BLOCKDEV_ON_ERROR_IGNORE;
    } else if (!is_read && !strcmp(buf, "enospc")) {
        return BLOCKDEV_ON_ERROR_ENOSPC;
    } else if (!strcmp(buf, "stop")) {
        return BLOCKDEV_ON_ERROR_STOP;
    } else if (!strcmp(buf, "report")) {
        return BLOCKDEV_ON_ERROR_REPORT;
    } else {
        error_setg(errp, "'%s' invalid %s error action",
                   buf, is_read ? "read" : "write");
        return -1;
    }
}

static bool parse_stats_intervals(BlockAcctStats *stats, QList *intervals,
                                  Error **errp)
{
    const QListEntry *entry;
    for (entry = qlist_first(intervals); entry; entry = qlist_next(entry)) {
        switch (qobject_type(entry->value)) {

        case QTYPE_QSTRING: {
            unsigned long long length;
            const char *str = qstring_get_str(qobject_to(QString,
                                                         entry->value));
            if (parse_uint_full(str, &length, 10) == 0 &&
                length > 0 && length <= UINT_MAX) {
                block_acct_add_interval(stats, (unsigned) length);
            } else {
                error_setg(errp, "Invalid interval length: %s", str);
                return false;
            }
            break;
        }

        case QTYPE_QNUM: {
            int64_t length = qnum_get_int(qobject_to(QNum, entry->value));

            if (length > 0 && length <= UINT_MAX) {
                block_acct_add_interval(stats, (unsigned) length);
            } else {
                error_setg(errp, "Invalid interval length: %" PRId64, length);
                return false;
            }
            break;
        }

        default:
            error_setg(errp, "The specification of stats-intervals is invalid");
            return false;
        }
    }
    return true;
}

typedef enum { MEDIA_DISK, MEDIA_CDROM } DriveMediaType;

/* All parameters but @opts are optional and may be set to NULL. */
static void extract_common_blockdev_options(QemuOpts *opts, int *bdrv_flags,
    const char **throttling_group, ThrottleConfig *throttle_cfg,
    BlockdevDetectZeroesOptions *detect_zeroes, Error **errp)
{
    Error *local_error = NULL;
    const char *aio;

    if (bdrv_flags) {
        if (qemu_opt_get_bool(opts, "copy-on-read", false)) {
            *bdrv_flags |= BDRV_O_COPY_ON_READ;
        }

        if ((aio = qemu_opt_get(opts, "aio")) != NULL) {
            if (bdrv_parse_aio(aio, bdrv_flags) < 0) {
                error_setg(errp, "invalid aio option");
                return;
            }
        }
    }

    /* disk I/O throttling */
    if (throttling_group) {
        *throttling_group = qemu_opt_get(opts, "throttling.group");
    }

    if (throttle_cfg) {
        throttle_config_init(throttle_cfg);
        throttle_cfg->buckets[THROTTLE_BPS_TOTAL].avg =
            qemu_opt_get_number(opts, "throttling.bps-total", 0);
        throttle_cfg->buckets[THROTTLE_BPS_READ].avg  =
            qemu_opt_get_number(opts, "throttling.bps-read", 0);
        throttle_cfg->buckets[THROTTLE_BPS_WRITE].avg =
            qemu_opt_get_number(opts, "throttling.bps-write", 0);
        throttle_cfg->buckets[THROTTLE_OPS_TOTAL].avg =
            qemu_opt_get_number(opts, "throttling.iops-total", 0);
        throttle_cfg->buckets[THROTTLE_OPS_READ].avg =
            qemu_opt_get_number(opts, "throttling.iops-read", 0);
        throttle_cfg->buckets[THROTTLE_OPS_WRITE].avg =
            qemu_opt_get_number(opts, "throttling.iops-write", 0);

        throttle_cfg->buckets[THROTTLE_BPS_TOTAL].max =
            qemu_opt_get_number(opts, "throttling.bps-total-max", 0);
        throttle_cfg->buckets[THROTTLE_BPS_READ].max  =
            qemu_opt_get_number(opts, "throttling.bps-read-max", 0);
        throttle_cfg->buckets[THROTTLE_BPS_WRITE].max =
            qemu_opt_get_number(opts, "throttling.bps-write-max", 0);
        throttle_cfg->buckets[THROTTLE_OPS_TOTAL].max =
            qemu_opt_get_number(opts, "throttling.iops-total-max", 0);
        throttle_cfg->buckets[THROTTLE_OPS_READ].max =
            qemu_opt_get_number(opts, "throttling.iops-read-max", 0);
        throttle_cfg->buckets[THROTTLE_OPS_WRITE].max =
            qemu_opt_get_number(opts, "throttling.iops-write-max", 0);

        throttle_cfg->buckets[THROTTLE_BPS_TOTAL].burst_length =
            qemu_opt_get_number(opts, "throttling.bps-total-max-length", 1);
        throttle_cfg->buckets[THROTTLE_BPS_READ].burst_length  =
            qemu_opt_get_number(opts, "throttling.bps-read-max-length", 1);
        throttle_cfg->buckets[THROTTLE_BPS_WRITE].burst_length =
            qemu_opt_get_number(opts, "throttling.bps-write-max-length", 1);
        throttle_cfg->buckets[THROTTLE_OPS_TOTAL].burst_length =
            qemu_opt_get_number(opts, "throttling.iops-total-max-length", 1);
        throttle_cfg->buckets[THROTTLE_OPS_READ].burst_length =
            qemu_opt_get_number(opts, "throttling.iops-read-max-length", 1);
        throttle_cfg->buckets[THROTTLE_OPS_WRITE].burst_length =
            qemu_opt_get_number(opts, "throttling.iops-write-max-length", 1);

        throttle_cfg->op_size =
            qemu_opt_get_number(opts, "throttling.iops-size", 0);

        if (!throttle_is_valid(throttle_cfg, errp)) {
            return;
        }
    }

    if (detect_zeroes) {
        *detect_zeroes =
            qapi_enum_parse(&BlockdevDetectZeroesOptions_lookup,
                            qemu_opt_get(opts, "detect-zeroes"),
                            BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF,
                            &local_error);
        if (local_error) {
            error_propagate(errp, local_error);
            return;
        }
    }
}

static OnOffAuto account_get_opt(QemuOpts *opts, const char *name)
{
    if (!qemu_opt_find(opts, name)) {
        return ON_OFF_AUTO_AUTO;
    }
    if (qemu_opt_get_bool(opts, name, true)) {
        return ON_OFF_AUTO_ON;
    }
    return ON_OFF_AUTO_OFF;
}

/* Takes the ownership of bs_opts */
static BlockBackend *blockdev_init(const char *file, QDict *bs_opts,
                                   Error **errp)
{
    const char *buf;
    int bdrv_flags = 0;
    int on_read_error, on_write_error;
    OnOffAuto account_invalid, account_failed;
    bool writethrough, read_only;
    BlockBackend *blk;
    BlockDriverState *bs;
    ThrottleConfig cfg;
    int snapshot = 0;
    Error *error = NULL;
    QemuOpts *opts;
    QDict *interval_dict = NULL;
    QList *interval_list = NULL;
    const char *id;
    BlockdevDetectZeroesOptions detect_zeroes =
        BLOCKDEV_DETECT_ZEROES_OPTIONS_OFF;
    const char *throttling_group = NULL;

    /* Check common options by copying from bs_opts to opts, all other options
     * stay in bs_opts for processing by bdrv_open(). */
    id = qdict_get_try_str(bs_opts, "id");
    opts = qemu_opts_create(&qemu_common_drive_opts, id, 1, errp);
    if (!opts) {
        goto err_no_opts;
    }

    if (!qemu_opts_absorb_qdict(opts, bs_opts, errp)) {
        goto early_err;
    }

    if (id) {
        qdict_del(bs_opts, "id");
    }

    /* extract parameters */
    snapshot = qemu_opt_get_bool(opts, "snapshot", 0);

    account_invalid = account_get_opt(opts, "stats-account-invalid");
    account_failed = account_get_opt(opts, "stats-account-failed");

    writethrough = !qemu_opt_get_bool(opts, BDRV_OPT_CACHE_WB, true);

    id = qemu_opts_id(opts);

    qdict_extract_subqdict(bs_opts, &interval_dict, "stats-intervals.");
    qdict_array_split(interval_dict, &interval_list);

    if (qdict_size(interval_dict) != 0) {
        error_setg(errp, "Invalid option stats-intervals.%s",
                   qdict_first(interval_dict)->key);
        goto early_err;
    }

    extract_common_blockdev_options(opts, &bdrv_flags, &throttling_group, &cfg,
                                    &detect_zeroes, &error);
    if (error) {
        error_propagate(errp, error);
        goto early_err;
    }

    if ((buf = qemu_opt_get(opts, "format")) != NULL) {
        if (is_help_option(buf)) {
            qemu_printf("Supported formats:");
            bdrv_iterate_format(bdrv_format_print, NULL, false);
            qemu_printf("\nSupported formats (read-only):");
            bdrv_iterate_format(bdrv_format_print, NULL, true);
            qemu_printf("\n");
            goto early_err;
        }

        if (qdict_haskey(bs_opts, "driver")) {
            error_setg(errp, "Cannot specify both 'driver' and 'format'");
            goto early_err;
        }
        qdict_put_str(bs_opts, "driver", buf);
    }

    on_write_error = BLOCKDEV_ON_ERROR_ENOSPC;
    if ((buf = qemu_opt_get(opts, "werror")) != NULL) {
        on_write_error = parse_block_error_action(buf, 0, &error);
        if (error) {
            error_propagate(errp, error);
            goto early_err;
        }
    }

    on_read_error = BLOCKDEV_ON_ERROR_REPORT;
    if ((buf = qemu_opt_get(opts, "rerror")) != NULL) {
        on_read_error = parse_block_error_action(buf, 1, &error);
        if (error) {
            error_propagate(errp, error);
            goto early_err;
        }
    }

    if (snapshot) {
        bdrv_flags |= BDRV_O_SNAPSHOT;
    }

    read_only = qemu_opt_get_bool(opts, BDRV_OPT_READ_ONLY, false);

    /* init */
    if ((!file || !*file) && !qdict_size(bs_opts)) {
        BlockBackendRootState *blk_rs;

        blk = blk_new(qemu_get_aio_context(), 0, BLK_PERM_ALL);
        blk_rs = blk_get_root_state(blk);
        blk_rs->open_flags    = bdrv_flags | (read_only ? 0 : BDRV_O_RDWR);
        blk_rs->detect_zeroes = detect_zeroes;

        qobject_unref(bs_opts);
    } else {
        if (file && !*file) {
            file = NULL;
        }

        /* bdrv_open() defaults to the values in bdrv_flags (for compatibility
         * with other callers) rather than what we want as the real defaults.
         * Apply the defaults here instead. */
        qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_DIRECT, "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_NO_FLUSH, "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_READ_ONLY,
                              read_only ? "on" : "off");
        qdict_set_default_str(bs_opts, BDRV_OPT_AUTO_READ_ONLY, "on");
        assert((bdrv_flags & BDRV_O_CACHE_MASK) == 0);

        if (runstate_check(RUN_STATE_INMIGRATE)) {
            bdrv_flags |= BDRV_O_INACTIVE;
        }

        blk = blk_new_open(file, NULL, bs_opts, bdrv_flags, errp);
        if (!blk) {
            goto err_no_bs_opts;
        }
        bs = blk_bs(blk);

        bs->detect_zeroes = detect_zeroes;

        block_acct_setup(blk_get_stats(blk), account_invalid, account_failed);

        if (!parse_stats_intervals(blk_get_stats(blk), interval_list, errp)) {
            blk_unref(blk);
            blk = NULL;
            goto err_no_bs_opts;
        }
    }

    /* disk I/O throttling */
    if (throttle_enabled(&cfg)) {
        if (!throttling_group) {
            throttling_group = id;
        }
        blk_io_limits_enable(blk, throttling_group);
        blk_set_io_limits(blk, &cfg);
    }

    blk_set_enable_write_cache(blk, !writethrough);
    blk_set_on_error(blk, on_read_error, on_write_error);

    if (!monitor_add_blk(blk, id, errp)) {
        blk_unref(blk);
        blk = NULL;
        goto err_no_bs_opts;
    }

err_no_bs_opts:
    qemu_opts_del(opts);
    qobject_unref(interval_dict);
    qobject_unref(interval_list);
    return blk;

early_err:
    qemu_opts_del(opts);
    qobject_unref(interval_dict);
    qobject_unref(interval_list);
err_no_opts:
    qobject_unref(bs_opts);
    return NULL;
}

/* Takes the ownership of bs_opts */
BlockDriverState *bds_tree_init(QDict *bs_opts, Error **errp)
{
    int bdrv_flags = 0;

    GLOBAL_STATE_CODE();
    /* bdrv_open() defaults to the values in bdrv_flags (for compatibility
     * with other callers) rather than what we want as the real defaults.
     * Apply the defaults here instead. */
    qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_DIRECT, "off");
    qdict_set_default_str(bs_opts, BDRV_OPT_CACHE_NO_FLUSH, "off");
    qdict_set_default_str(bs_opts, BDRV_OPT_READ_ONLY, "off");

    if (runstate_check(RUN_STATE_INMIGRATE)) {
        bdrv_flags |= BDRV_O_INACTIVE;
    }

    return bdrv_open(NULL, NULL, bs_opts, bdrv_flags, errp);
}

void blockdev_close_all_bdrv_states(void)
{
    BlockDriverState *bs, *next_bs;

    GLOBAL_STATE_CODE();
    QTAILQ_FOREACH_SAFE(bs, &monitor_bdrv_states, monitor_list, next_bs) {
        AioContext *ctx = bdrv_get_aio_context(bs);

        aio_context_acquire(ctx);
        bdrv_unref(bs);
        aio_context_release(ctx);
    }
}

/* Iterates over the list of monitor-owned BlockDriverStates */
BlockDriverState *bdrv_next_monitor_owned(BlockDriverState *bs)
{
    GLOBAL_STATE_CODE();
    return bs ? QTAILQ_NEXT(bs, monitor_list)
              : QTAILQ_FIRST(&monitor_bdrv_states);
}

static bool qemu_opt_rename(QemuOpts *opts, const char *from, const char *to,
                            Error **errp)
{
    const char *value;

    value = qemu_opt_get(opts, from);
    if (value) {
        if (qemu_opt_find(opts, to)) {
            error_setg(errp, "'%s' and its alias '%s' can't be used at the "
                       "same time", to, from);
            return false;
        }
    }

    /* rename all items in opts */
    while ((value = qemu_opt_get(opts, from))) {
        qemu_opt_set(opts, to, value, &error_abort);
        qemu_opt_unset(opts, from);
    }
    return true;
}

QemuOptsList qemu_legacy_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_legacy_drive_opts.head),
    .desc = {
        {
            .name = "bus",
            .type = QEMU_OPT_NUMBER,
            .help = "bus number",
        },{
            .name = "unit",
            .type = QEMU_OPT_NUMBER,
            .help = "unit number (i.e. lun for scsi)",
        },{
            .name = "index",
            .type = QEMU_OPT_NUMBER,
            .help = "index number",
        },{
            .name = "media",
            .type = QEMU_OPT_STRING,
            .help = "media type (disk, cdrom)",
        },{
            .name = "if",
            .type = QEMU_OPT_STRING,
            .help = "interface (ide, scsi, sd, mtd, floppy, pflash, virtio)",
        },{
            .name = "file",
            .type = QEMU_OPT_STRING,
            .help = "file name",
        },

        /* Options that are passed on, but have special semantics with -drive */
        {
            .name = BDRV_OPT_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "open drive file as read-only",
        },{
            .name = "rerror",
            .type = QEMU_OPT_STRING,
            .help = "read error action",
        },{
            .name = "werror",
            .type = QEMU_OPT_STRING,
            .help = "write error action",
        },{
            .name = "copy-on-read",
            .type = QEMU_OPT_BOOL,
            .help = "copy read data from backing file into image file",
        },{
            .name = "locked",
            .type = QEMU_OPT_BOOL,
            .help = "emulate a security locked drive",
        },

        { /* end of list */ }
    },
};

DriveInfo *drive_new(QemuOpts *all_opts, BlockInterfaceType block_default_type,
                     Error **errp)
{
    const char *value;
    BlockBackend *blk;
    DriveInfo *dinfo = NULL;
    QDict *bs_opts;
    QemuOpts *legacy_opts;
    DriveMediaType media = MEDIA_DISK;
    BlockInterfaceType type;
    int max_devs, bus_id, unit_id, index;
    const char *werror, *rerror;
    bool read_only = false;
    bool copy_on_read;
    const char *filename;
    int i;
    bool locked;

    GLOBAL_STATE_CODE();

    /* Change legacy command line options into QMP ones */
    static const struct {
        const char *from;
        const char *to;
    } opt_renames[] = {
        { "iops",           "throttling.iops-total" },
        { "iops_rd",        "throttling.iops-read" },
        { "iops_wr",        "throttling.iops-write" },

        { "bps",            "throttling.bps-total" },
        { "bps_rd",         "throttling.bps-read" },
        { "bps_wr",         "throttling.bps-write" },

        { "iops_max",       "throttling.iops-total-max" },
        { "iops_rd_max",    "throttling.iops-read-max" },
        { "iops_wr_max",    "throttling.iops-write-max" },

        { "bps_max",        "throttling.bps-total-max" },
        { "bps_rd_max",     "throttling.bps-read-max" },
        { "bps_wr_max",     "throttling.bps-write-max" },

        { "iops_size",      "throttling.iops-size" },

        { "group",          "throttling.group" },

        { "readonly",       BDRV_OPT_READ_ONLY },
    };

    for (i = 0; i < ARRAY_SIZE(opt_renames); i++) {
        if (!qemu_opt_rename(all_opts, opt_renames[i].from,
                             opt_renames[i].to, errp)) {
            return NULL;
        }
    }

    value = qemu_opt_get(all_opts, "cache");
    if (value) {
        int flags = 0;
        bool writethrough;

        if (bdrv_parse_cache_mode(value, &flags, &writethrough) != 0) {
            error_setg(errp, "invalid cache option");
            return NULL;
        }

        /* Specific options take precedence */
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_WB)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_WB,
                              !writethrough, &error_abort);
        }
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_DIRECT)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_DIRECT,
                              !!(flags & BDRV_O_NOCACHE), &error_abort);
        }
        if (!qemu_opt_get(all_opts, BDRV_OPT_CACHE_NO_FLUSH)) {
            qemu_opt_set_bool(all_opts, BDRV_OPT_CACHE_NO_FLUSH,
                              !!(flags & BDRV_O_NO_FLUSH), &error_abort);
        }
        qemu_opt_unset(all_opts, "cache");
    }

    /* Get a QDict for processing the options */
    bs_opts = qdict_new();
    qemu_opts_to_qdict(all_opts, bs_opts);

    legacy_opts = qemu_opts_create(&qemu_legacy_drive_opts, NULL, 0,
                                   &error_abort);
    if (!qemu_opts_absorb_qdict(legacy_opts, bs_opts, errp)) {
        goto fail;
    }

    /* Media type */
    value = qemu_opt_get(legacy_opts, "media");
    if (value) {
        if (!strcmp(value, "disk")) {
            media = MEDIA_DISK;
        } else if (!strcmp(value, "cdrom")) {
            media = MEDIA_CDROM;
            read_only = true;
        } else {
            error_setg(errp, "'%s' invalid media", value);
            goto fail;
        }
    }

    /* copy-on-read is disabled with a warning for read-only devices */
    read_only |= qemu_opt_get_bool(legacy_opts, BDRV_OPT_READ_ONLY, false);
    copy_on_read = qemu_opt_get_bool(legacy_opts, "copy-on-read", false);

    if (read_only && copy_on_read) {
        warn_report("disabling copy-on-read on read-only drive");
        copy_on_read = false;
    }

    qdict_put_str(bs_opts, BDRV_OPT_READ_ONLY, read_only ? "on" : "off");
    qdict_put_str(bs_opts, "copy-on-read", copy_on_read ? "on" : "off");

    /* Controller type */
    value = qemu_opt_get(legacy_opts, "if");
    if (value) {
        for (type = 0;
             type < IF_COUNT && strcmp(value, if_name[type]);
             type++) {
        }
        if (type == IF_COUNT) {
            error_setg(errp, "unsupported bus type '%s'", value);
            goto fail;
        }
    } else {
        type = block_default_type;
    }

    /* Device address specified by bus/unit or index.
     * If none was specified, try to find the first free one. */
    bus_id  = qemu_opt_get_number(legacy_opts, "bus", 0);
    unit_id = qemu_opt_get_number(legacy_opts, "unit", -1);
    index   = qemu_opt_get_number(legacy_opts, "index", -1);

    max_devs = if_max_devs[type];

    if (index != -1) {
        if (bus_id != 0 || unit_id != -1) {
            error_setg(errp, "index cannot be used with bus and unit");
            goto fail;
        }
        bus_id = drive_index_to_bus_id(type, index);
        unit_id = drive_index_to_unit_id(type, index);
    }

    if (unit_id == -1) {
       unit_id = 0;
       while (drive_get(type, bus_id, unit_id) != NULL) {
           unit_id++;
           if (max_devs && unit_id >= max_devs) {
               unit_id -= max_devs;
               bus_id++;
           }
       }
    }

    if (max_devs && unit_id >= max_devs) {
        error_setg(errp, "unit %d too big (max is %d)", unit_id, max_devs - 1);
        goto fail;
    }

    if (drive_get(type, bus_id, unit_id) != NULL) {
        error_setg(errp, "drive with bus=%d, unit=%d (index=%d) exists",
                   bus_id, unit_id, index);
        goto fail;
    }

    /* Locked */
    locked = qemu_opt_get_bool(legacy_opts, "locked", false);

    /* no id supplied -> create one */
    if (qemu_opts_id(all_opts) == NULL) {
        char *new_id;
        const char *mediastr = "";
        if (type == IF_IDE || type == IF_SCSI) {
            mediastr = (media == MEDIA_CDROM) ? "-cd" : "-hd";
        }
        if (max_devs) {
            new_id = g_strdup_printf("%s%i%s%i", if_name[type], bus_id,
                                     mediastr, unit_id);
        } else {
            new_id = g_strdup_printf("%s%s%i", if_name[type],
                                     mediastr, unit_id);
        }
        qdict_put_str(bs_opts, "id", new_id);
        g_free(new_id);
    }

    /* Add virtio block device */
    if (type == IF_VIRTIO) {
        QemuOpts *devopts;
        devopts = qemu_opts_create(qemu_find_opts("device"), NULL, 0,
                                   &error_abort);
        qemu_opt_set(devopts, "driver", "virtio-blk", &error_abort);
        qemu_opt_set(devopts, "drive", qdict_get_str(bs_opts, "id"),
                     &error_abort);
    }

    filename = qemu_opt_get(legacy_opts, "file");

    /* Check werror/rerror compatibility with if=... */
    werror = qemu_opt_get(legacy_opts, "werror");
    if (werror != NULL) {
        if (type != IF_IDE && type != IF_SCSI && type != IF_VIRTIO &&
            type != IF_NONE) {
            error_setg(errp, "werror is not supported by this bus type");
            goto fail;
        }
        qdict_put_str(bs_opts, "werror", werror);
    }

    rerror = qemu_opt_get(legacy_opts, "rerror");
    if (rerror != NULL) {
        if (type != IF_IDE && type != IF_VIRTIO && type != IF_SCSI &&
            type != IF_NONE) {
            error_setg(errp, "rerror is not supported by this bus type");
            goto fail;
        }
        qdict_put_str(bs_opts, "rerror", rerror);
    }

    /* Actual block device init: Functionality shared with blockdev-add */
    blk = blockdev_init(filename, bs_opts, errp);
    bs_opts = NULL;
    if (!blk) {
        goto fail;
    }

    /* Create legacy DriveInfo */
    dinfo = g_malloc0(sizeof(*dinfo));
    dinfo->opts = all_opts;

    dinfo->type = type;
    dinfo->bus = bus_id;
    dinfo->unit = unit_id;
    dinfo->locked = locked;

    blk_set_legacy_dinfo(blk, dinfo);

    switch(type) {
    case IF_IDE:
    case IF_SCSI:
    case IF_XEN:
    case IF_NONE:
        dinfo->media_cd = media == MEDIA_CDROM;
        break;
    default:
        break;
    }

fail:
    qemu_opts_del(legacy_opts);
    qobject_unref(bs_opts);
    return dinfo;
}

static BlockDriverState *qmp_get_root_bs(const char *name, Error **errp)
{
    BlockDriverState *bs;

    bs = bdrv_lookup_bs(name, name, errp);
    if (bs == NULL) {
        return NULL;
    }

    if (!bdrv_is_root_node(bs)) {
        error_setg(errp, "Need a root block node");
        return NULL;
    }

    if (!bdrv_is_inserted(bs)) {
        error_setg(errp, "Device has no medium");
        return NULL;
    }

    return bs;
}

static void blockdev_do_action(TransactionAction *action, Error **errp)
{
    TransactionActionList list;

    list.value = action;
    list.next = NULL;
    qmp_transaction(&list, false, NULL, errp);
}

void qmp_blockdev_snapshot_sync(bool has_device, const char *device,
                                bool has_node_name, const char *node_name,
                                const char *snapshot_file,
                                bool has_snapshot_node_name,
                                const char *snapshot_node_name,
                                bool has_format, const char *format,
                                bool has_mode, NewImageMode mode, Error **errp)
{
    BlockdevSnapshotSync snapshot = {
        .has_device = has_device,
        .device = (char *) device,
        .has_node_name = has_node_name,
        .node_name = (char *) node_name,
        .snapshot_file = (char *) snapshot_file,
        .has_snapshot_node_name = has_snapshot_node_name,
        .snapshot_node_name = (char *) snapshot_node_name,
        .has_format = has_format,
        .format = (char *) format,
        .has_mode = has_mode,
        .mode = mode,
    };
    TransactionAction action = {
        .type = TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_SYNC,
        .u.blockdev_snapshot_sync.data = &snapshot,
    };
    blockdev_do_action(&action, errp);
}

void qmp_blockdev_snapshot(const char *node, const char *overlay,
                           Error **errp)
{
    BlockdevSnapshot snapshot_data = {
        .node = (char *) node,
        .overlay = (char *) overlay
    };
    TransactionAction action = {
        .type = TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT,
        .u.blockdev_snapshot.data = &snapshot_data,
    };
    blockdev_do_action(&action, errp);
}

void qmp_blockdev_snapshot_internal_sync(const char *device,
                                         const char *name,
                                         Error **errp)
{
    BlockdevSnapshotInternal snapshot = {
        .device = (char *) device,
        .name = (char *) name
    };
    TransactionAction action = {
        .type = TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_INTERNAL_SYNC,
        .u.blockdev_snapshot_internal_sync.data = &snapshot,
    };
    blockdev_do_action(&action, errp);
}

SnapshotInfo *qmp_blockdev_snapshot_delete_internal_sync(const char *device,
                                                         bool has_id,
                                                         const char *id,
                                                         bool has_name,
                                                         const char *name,
                                                         Error **errp)
{
    BlockDriverState *bs;
    AioContext *aio_context;
    QEMUSnapshotInfo sn;
    Error *local_err = NULL;
    SnapshotInfo *info = NULL;
    int ret;

    bs = qmp_get_root_bs(device, errp);
    if (!bs) {
        return NULL;
    }
    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    if (!has_id) {
        id = NULL;
    }

    if (!has_name) {
        name = NULL;
    }

    if (!id && !name) {
        error_setg(errp, "Name or id must be provided");
        goto out_aio_context;
    }

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_INTERNAL_SNAPSHOT_DELETE, errp)) {
        goto out_aio_context;
    }

    ret = bdrv_snapshot_find_by_id_and_name(bs, id, name, &sn, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto out_aio_context;
    }
    if (!ret) {
        error_setg(errp,
                   "Snapshot with id '%s' and name '%s' does not exist on "
                   "device '%s'",
                   STR_OR_NULL(id), STR_OR_NULL(name), device);
        goto out_aio_context;
    }

    bdrv_snapshot_delete(bs, id, name, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto out_aio_context;
    }

    aio_context_release(aio_context);

    info = g_new0(SnapshotInfo, 1);
    info->id = g_strdup(sn.id_str);
    info->name = g_strdup(sn.name);
    info->date_nsec = sn.date_nsec;
    info->date_sec = sn.date_sec;
    info->vm_state_size = sn.vm_state_size;
    info->vm_clock_nsec = sn.vm_clock_nsec % 1000000000;
    info->vm_clock_sec = sn.vm_clock_nsec / 1000000000;
    if (sn.icount != -1ULL) {
        info->icount = sn.icount;
        info->has_icount = true;
    }

    return info;

out_aio_context:
    aio_context_release(aio_context);
    return NULL;
}

/* New and old BlockDriverState structs for atomic group operations */

typedef struct BlkActionState BlkActionState;

/**
 * BlkActionOps:
 * Table of operations that define an Action.
 *
 * @instance_size: Size of state struct, in bytes.
 * @prepare: Prepare the work, must NOT be NULL.
 * @commit: Commit the changes, can be NULL.
 * @abort: Abort the changes on fail, can be NULL.
 * @clean: Clean up resources after all transaction actions have called
 *         commit() or abort(). Can be NULL.
 *
 * Only prepare() may fail. In a single transaction, only one of commit() or
 * abort() will be called. clean() will always be called if it is present.
 *
 * Always run under BQL.
 */
typedef struct BlkActionOps {
    size_t instance_size;
    void (*prepare)(BlkActionState *common, Error **errp);
    void (*commit)(BlkActionState *common);
    void (*abort)(BlkActionState *common);
    void (*clean)(BlkActionState *common);
} BlkActionOps;

/**
 * BlkActionState:
 * Describes one Action's state within a Transaction.
 *
 * @action: QAPI-defined enum identifying which Action to perform.
 * @ops: Table of ActionOps this Action can perform.
 * @block_job_txn: Transaction which this action belongs to.
 * @entry: List membership for all Actions in this Transaction.
 *
 * This structure must be arranged as first member in a subclassed type,
 * assuming that the compiler will also arrange it to the same offsets as the
 * base class.
 */
struct BlkActionState {
    TransactionAction *action;
    const BlkActionOps *ops;
    JobTxn *block_job_txn;
    TransactionProperties *txn_props;
    QTAILQ_ENTRY(BlkActionState) entry;
};

/* internal snapshot private data */
typedef struct InternalSnapshotState {
    BlkActionState common;
    BlockDriverState *bs;
    QEMUSnapshotInfo sn;
    bool created;
} InternalSnapshotState;


static int action_check_completion_mode(BlkActionState *s, Error **errp)
{
    if (s->txn_props->completion_mode != ACTION_COMPLETION_MODE_INDIVIDUAL) {
        error_setg(errp,
                   "Action '%s' does not support Transaction property "
                   "completion-mode = %s",
                   TransactionActionKind_str(s->action->type),
                   ActionCompletionMode_str(s->txn_props->completion_mode));
        return -1;
    }
    return 0;
}

static void internal_snapshot_prepare(BlkActionState *common,
                                      Error **errp)
{
    Error *local_err = NULL;
    const char *device;
    const char *name;
    BlockDriverState *bs;
    QEMUSnapshotInfo old_sn, *sn;
    bool ret;
    int64_t rt;
    BlockdevSnapshotInternal *internal;
    InternalSnapshotState *state;
    AioContext *aio_context;
    int ret1;

    g_assert(common->action->type ==
             TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_INTERNAL_SYNC);
    internal = common->action->u.blockdev_snapshot_internal_sync.data;
    state = DO_UPCAST(InternalSnapshotState, common, common);

    /* 1. parse input */
    device = internal->device;
    name = internal->name;

    /* 2. check for validation */
    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    bs = qmp_get_root_bs(device, errp);
    if (!bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    state->bs = bs;

    /* Paired with .clean() */
    bdrv_drained_begin(bs);

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_INTERNAL_SNAPSHOT, errp)) {
        goto out;
    }

    if (bdrv_is_read_only(bs)) {
        error_setg(errp, "Device '%s' is read only", device);
        goto out;
    }

    if (!bdrv_can_snapshot(bs)) {
        error_setg(errp, "Block format '%s' used by device '%s' "
                   "does not support internal snapshots",
                   bs->drv->format_name, device);
        goto out;
    }

    if (!strlen(name)) {
        error_setg(errp, "Name is empty");
        goto out;
    }

    /* check whether a snapshot with name exist */
    ret = bdrv_snapshot_find_by_id_and_name(bs, NULL, name, &old_sn,
                                            &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto out;
    } else if (ret) {
        error_setg(errp,
                   "Snapshot with name '%s' already exists on device '%s'",
                   name, device);
        goto out;
    }

    /* 3. take the snapshot */
    sn = &state->sn;
    pstrcpy(sn->name, sizeof(sn->name), name);
    rt = g_get_real_time();
    sn->date_sec = rt / G_USEC_PER_SEC;
    sn->date_nsec = (rt % G_USEC_PER_SEC) * 1000;
    sn->vm_clock_nsec = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    if (replay_mode != REPLAY_MODE_NONE) {
        sn->icount = replay_get_current_icount();
    } else {
        sn->icount = -1ULL;
    }

    ret1 = bdrv_snapshot_create(bs, sn);
    if (ret1 < 0) {
        error_setg_errno(errp, -ret1,
                         "Failed to create snapshot '%s' on device '%s'",
                         name, device);
        goto out;
    }

    /* 4. succeed, mark a snapshot is created */
    state->created = true;

out:
    aio_context_release(aio_context);
}

static void internal_snapshot_abort(BlkActionState *common)
{
    InternalSnapshotState *state =
                             DO_UPCAST(InternalSnapshotState, common, common);
    BlockDriverState *bs = state->bs;
    QEMUSnapshotInfo *sn = &state->sn;
    AioContext *aio_context;
    Error *local_error = NULL;

    if (!state->created) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    if (bdrv_snapshot_delete(bs, sn->id_str, sn->name, &local_error) < 0) {
        error_reportf_err(local_error,
                          "Failed to delete snapshot with id '%s' and "
                          "name '%s' on device '%s' in abort: ",
                          sn->id_str, sn->name,
                          bdrv_get_device_name(bs));
    }

    aio_context_release(aio_context);
}

static void internal_snapshot_clean(BlkActionState *common)
{
    InternalSnapshotState *state = DO_UPCAST(InternalSnapshotState,
                                             common, common);
    AioContext *aio_context;

    if (!state->bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    bdrv_drained_end(state->bs);

    aio_context_release(aio_context);
}

/* external snapshot private data */
typedef struct ExternalSnapshotState {
    BlkActionState common;
    BlockDriverState *old_bs;
    BlockDriverState *new_bs;
    bool overlay_appended;
} ExternalSnapshotState;

static void external_snapshot_prepare(BlkActionState *common,
                                      Error **errp)
{
    int ret;
    int flags = 0;
    QDict *options = NULL;
    Error *local_err = NULL;
    /* Device and node name of the image to generate the snapshot from */
    const char *device;
    const char *node_name;
    /* Reference to the new image (for 'blockdev-snapshot') */
    const char *snapshot_ref;
    /* File name of the new image (for 'blockdev-snapshot-sync') */
    const char *new_image_file;
    ExternalSnapshotState *state =
                             DO_UPCAST(ExternalSnapshotState, common, common);
    TransactionAction *action = common->action;
    AioContext *aio_context;
    uint64_t perm, shared;

    /* 'blockdev-snapshot' and 'blockdev-snapshot-sync' have similar
     * purpose but a different set of parameters */
    switch (action->type) {
    case TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT:
        {
            BlockdevSnapshot *s = action->u.blockdev_snapshot.data;
            device = s->node;
            node_name = s->node;
            new_image_file = NULL;
            snapshot_ref = s->overlay;
        }
        break;
    case TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_SYNC:
        {
            BlockdevSnapshotSync *s = action->u.blockdev_snapshot_sync.data;
            device = s->has_device ? s->device : NULL;
            node_name = s->has_node_name ? s->node_name : NULL;
            new_image_file = s->snapshot_file;
            snapshot_ref = NULL;
        }
        break;
    default:
        g_assert_not_reached();
    }

    /* start processing */
    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    state->old_bs = bdrv_lookup_bs(device, node_name, errp);
    if (!state->old_bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->old_bs);
    aio_context_acquire(aio_context);

    /* Paired with .clean() */
    bdrv_drained_begin(state->old_bs);

    if (!bdrv_is_inserted(state->old_bs)) {
        error_setg(errp, QERR_DEVICE_HAS_NO_MEDIUM, device);
        goto out;
    }

    if (bdrv_op_is_blocked(state->old_bs,
                           BLOCK_OP_TYPE_EXTERNAL_SNAPSHOT, errp)) {
        goto out;
    }

    if (!bdrv_is_read_only(state->old_bs)) {
        if (bdrv_flush(state->old_bs)) {
            error_setg(errp, QERR_IO_ERROR);
            goto out;
        }
    }

    if (action->type == TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_SYNC) {
        BlockdevSnapshotSync *s = action->u.blockdev_snapshot_sync.data;
        const char *format = s->has_format ? s->format : "qcow2";
        enum NewImageMode mode;
        const char *snapshot_node_name =
            s->has_snapshot_node_name ? s->snapshot_node_name : NULL;

        if (node_name && !snapshot_node_name) {
            error_setg(errp, "New overlay node-name missing");
            goto out;
        }

        if (snapshot_node_name &&
            bdrv_lookup_bs(snapshot_node_name, snapshot_node_name, NULL)) {
            error_setg(errp, "New overlay node-name already in use");
            goto out;
        }

        flags = state->old_bs->open_flags;
        flags &= ~(BDRV_O_SNAPSHOT | BDRV_O_COPY_ON_READ);
        flags |= BDRV_O_NO_BACKING;

        /* create new image w/backing file */
        mode = s->has_mode ? s->mode : NEW_IMAGE_MODE_ABSOLUTE_PATHS;
        if (mode != NEW_IMAGE_MODE_EXISTING) {
            int64_t size = bdrv_getlength(state->old_bs);
            if (size < 0) {
                error_setg_errno(errp, -size, "bdrv_getlength failed");
                goto out;
            }
            bdrv_refresh_filename(state->old_bs);
            bdrv_img_create(new_image_file, format,
                            state->old_bs->filename,
                            state->old_bs->drv->format_name,
                            NULL, size, flags, false, &local_err);
            if (local_err) {
                error_propagate(errp, local_err);
                goto out;
            }
        }

        options = qdict_new();
        if (snapshot_node_name) {
            qdict_put_str(options, "node-name", snapshot_node_name);
        }
        qdict_put_str(options, "driver", format);
    }

    state->new_bs = bdrv_open(new_image_file, snapshot_ref, options, flags,
                              errp);
    /* We will manually add the backing_hd field to the bs later */
    if (!state->new_bs) {
        goto out;
    }

    /*
     * Allow attaching a backing file to an overlay that's already in use only
     * if the parents don't assume that they are already seeing a valid image.
     * (Specifically, allow it as a mirror target, which is write-only access.)
     */
    bdrv_get_cumulative_perm(state->new_bs, &perm, &shared);
    if (perm & BLK_PERM_CONSISTENT_READ) {
        error_setg(errp, "The overlay is already in use");
        goto out;
    }

    if (state->new_bs->drv->is_filter) {
        error_setg(errp, "Filters cannot be used as overlays");
        goto out;
    }

    if (bdrv_cow_child(state->new_bs)) {
        error_setg(errp, "The overlay already has a backing image");
        goto out;
    }

    if (!state->new_bs->drv->supports_backing) {
        error_setg(errp, "The overlay does not support backing images");
        goto out;
    }

    ret = bdrv_append(state->new_bs, state->old_bs, errp);
    if (ret < 0) {
        goto out;
    }
    state->overlay_appended = true;

out:
    aio_context_release(aio_context);
}

static void external_snapshot_commit(BlkActionState *common)
{
    ExternalSnapshotState *state =
                             DO_UPCAST(ExternalSnapshotState, common, common);
    AioContext *aio_context;

    aio_context = bdrv_get_aio_context(state->old_bs);
    aio_context_acquire(aio_context);

    /* We don't need (or want) to use the transactional
     * bdrv_reopen_multiple() across all the entries at once, because we
     * don't want to abort all of them if one of them fails the reopen */
    if (!qatomic_read(&state->old_bs->copy_on_read)) {
        bdrv_reopen_set_read_only(state->old_bs, true, NULL);
    }

    aio_context_release(aio_context);
}

static void external_snapshot_abort(BlkActionState *common)
{
    ExternalSnapshotState *state =
                             DO_UPCAST(ExternalSnapshotState, common, common);
    if (state->new_bs) {
        if (state->overlay_appended) {
            AioContext *aio_context;
            AioContext *tmp_context;
            int ret;

            aio_context = bdrv_get_aio_context(state->old_bs);
            aio_context_acquire(aio_context);

            bdrv_ref(state->old_bs);   /* we can't let bdrv_set_backind_hd()
                                          close state->old_bs; we need it */
            bdrv_set_backing_hd(state->new_bs, NULL, &error_abort);

            /*
             * The call to bdrv_set_backing_hd() above returns state->old_bs to
             * the main AioContext. As we're still going to be using it, return
             * it to the AioContext it was before.
             */
            tmp_context = bdrv_get_aio_context(state->old_bs);
            if (aio_context != tmp_context) {
                aio_context_release(aio_context);
                aio_context_acquire(tmp_context);

                ret = bdrv_try_change_aio_context(state->old_bs,
                                                  aio_context, NULL, NULL);
                assert(ret == 0);

                aio_context_release(tmp_context);
                aio_context_acquire(aio_context);
            }

            bdrv_replace_node(state->new_bs, state->old_bs, &error_abort);
            bdrv_unref(state->old_bs); /* bdrv_replace_node() ref'ed old_bs */

            aio_context_release(aio_context);
        }
    }
}

static void external_snapshot_clean(BlkActionState *common)
{
    ExternalSnapshotState *state =
                             DO_UPCAST(ExternalSnapshotState, common, common);
    AioContext *aio_context;

    if (!state->old_bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->old_bs);
    aio_context_acquire(aio_context);

    bdrv_drained_end(state->old_bs);
    bdrv_unref(state->new_bs);

    aio_context_release(aio_context);
}

typedef struct DriveBackupState {
    BlkActionState common;
    BlockDriverState *bs;
    BlockJob *job;
} DriveBackupState;

static BlockJob *do_backup_common(BackupCommon *backup,
                                  BlockDriverState *bs,
                                  BlockDriverState *target_bs,
                                  AioContext *aio_context,
                                  JobTxn *txn, Error **errp);

static void drive_backup_prepare(BlkActionState *common, Error **errp)
{
    DriveBackupState *state = DO_UPCAST(DriveBackupState, common, common);
    DriveBackup *backup;
    BlockDriverState *bs;
    BlockDriverState *target_bs;
    BlockDriverState *source = NULL;
    AioContext *aio_context;
    AioContext *old_context;
    QDict *options;
    Error *local_err = NULL;
    int flags;
    int64_t size;
    bool set_backing_hd = false;
    int ret;

    assert(common->action->type == TRANSACTION_ACTION_KIND_DRIVE_BACKUP);
    backup = common->action->u.drive_backup.data;

    if (!backup->has_mode) {
        backup->mode = NEW_IMAGE_MODE_ABSOLUTE_PATHS;
    }

    bs = bdrv_lookup_bs(backup->device, backup->device, errp);
    if (!bs) {
        return;
    }

    if (!bs->drv) {
        error_setg(errp, "Device has no medium");
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    state->bs = bs;
    /* Paired with .clean() */
    bdrv_drained_begin(bs);

    if (!backup->has_format) {
        backup->format = backup->mode == NEW_IMAGE_MODE_EXISTING ?
                         NULL : (char *) bs->drv->format_name;
    }

    /* Early check to avoid creating target */
    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_BACKUP_SOURCE, errp)) {
        goto out;
    }

    flags = bs->open_flags | BDRV_O_RDWR;

    /*
     * See if we have a backing HD we can use to create our new image
     * on top of.
     */
    if (backup->sync == MIRROR_SYNC_MODE_TOP) {
        /*
         * Backup will not replace the source by the target, so none
         * of the filters skipped here will be removed (in contrast to
         * mirror).  Therefore, we can skip all of them when looking
         * for the first COW relationship.
         */
        source = bdrv_cow_bs(bdrv_skip_filters(bs));
        if (!source) {
            backup->sync = MIRROR_SYNC_MODE_FULL;
        }
    }
    if (backup->sync == MIRROR_SYNC_MODE_NONE) {
        source = bs;
        flags |= BDRV_O_NO_BACKING;
        set_backing_hd = true;
    }

    size = bdrv_getlength(bs);
    if (size < 0) {
        error_setg_errno(errp, -size, "bdrv_getlength failed");
        goto out;
    }

    if (backup->mode != NEW_IMAGE_MODE_EXISTING) {
        assert(backup->format);
        if (source) {
            /* Implicit filters should not appear in the filename */
            BlockDriverState *explicit_backing =
                bdrv_skip_implicit_filters(source);

            bdrv_refresh_filename(explicit_backing);
            bdrv_img_create(backup->target, backup->format,
                            explicit_backing->filename,
                            explicit_backing->drv->format_name, NULL,
                            size, flags, false, &local_err);
        } else {
            bdrv_img_create(backup->target, backup->format, NULL, NULL, NULL,
                            size, flags, false, &local_err);
        }
    }

    if (local_err) {
        error_propagate(errp, local_err);
        goto out;
    }

    options = qdict_new();
    qdict_put_str(options, "discard", "unmap");
    qdict_put_str(options, "detect-zeroes", "unmap");
    if (backup->format) {
        qdict_put_str(options, "driver", backup->format);
    }

    target_bs = bdrv_open(backup->target, NULL, options, flags, errp);
    if (!target_bs) {
        goto out;
    }

    /* Honor bdrv_try_change_aio_context() context acquisition requirements. */
    old_context = bdrv_get_aio_context(target_bs);
    aio_context_release(aio_context);
    aio_context_acquire(old_context);

    ret = bdrv_try_change_aio_context(target_bs, aio_context, NULL, errp);
    if (ret < 0) {
        bdrv_unref(target_bs);
        aio_context_release(old_context);
        return;
    }

    aio_context_release(old_context);
    aio_context_acquire(aio_context);

    if (set_backing_hd) {
        if (bdrv_set_backing_hd(target_bs, source, errp) < 0) {
            goto unref;
        }
    }

    state->job = do_backup_common(qapi_DriveBackup_base(backup),
                                  bs, target_bs, aio_context,
                                  common->block_job_txn, errp);

unref:
    bdrv_unref(target_bs);
out:
    aio_context_release(aio_context);
}

static void drive_backup_commit(BlkActionState *common)
{
    DriveBackupState *state = DO_UPCAST(DriveBackupState, common, common);
    AioContext *aio_context;

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    assert(state->job);
    job_start(&state->job->job);

    aio_context_release(aio_context);
}

static void drive_backup_abort(BlkActionState *common)
{
    DriveBackupState *state = DO_UPCAST(DriveBackupState, common, common);

    if (state->job) {
        job_cancel_sync(&state->job->job, true);
    }
}

static void drive_backup_clean(BlkActionState *common)
{
    DriveBackupState *state = DO_UPCAST(DriveBackupState, common, common);
    AioContext *aio_context;

    if (!state->bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    bdrv_drained_end(state->bs);

    aio_context_release(aio_context);
}

typedef struct BlockdevBackupState {
    BlkActionState common;
    BlockDriverState *bs;
    BlockJob *job;
} BlockdevBackupState;

static void blockdev_backup_prepare(BlkActionState *common, Error **errp)
{
    BlockdevBackupState *state = DO_UPCAST(BlockdevBackupState, common, common);
    BlockdevBackup *backup;
    BlockDriverState *bs;
    BlockDriverState *target_bs;
    AioContext *aio_context;
    AioContext *old_context;
    int ret;

    assert(common->action->type == TRANSACTION_ACTION_KIND_BLOCKDEV_BACKUP);
    backup = common->action->u.blockdev_backup.data;

    bs = bdrv_lookup_bs(backup->device, backup->device, errp);
    if (!bs) {
        return;
    }

    target_bs = bdrv_lookup_bs(backup->target, backup->target, errp);
    if (!target_bs) {
        return;
    }

    /* Honor bdrv_try_change_aio_context() context acquisition requirements. */
    aio_context = bdrv_get_aio_context(bs);
    old_context = bdrv_get_aio_context(target_bs);
    aio_context_acquire(old_context);

    ret = bdrv_try_change_aio_context(target_bs, aio_context, NULL, errp);
    if (ret < 0) {
        aio_context_release(old_context);
        return;
    }

    aio_context_release(old_context);
    aio_context_acquire(aio_context);
    state->bs = bs;

    /* Paired with .clean() */
    bdrv_drained_begin(state->bs);

    state->job = do_backup_common(qapi_BlockdevBackup_base(backup),
                                  bs, target_bs, aio_context,
                                  common->block_job_txn, errp);

    aio_context_release(aio_context);
}

static void blockdev_backup_commit(BlkActionState *common)
{
    BlockdevBackupState *state = DO_UPCAST(BlockdevBackupState, common, common);
    AioContext *aio_context;

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    assert(state->job);
    job_start(&state->job->job);

    aio_context_release(aio_context);
}

static void blockdev_backup_abort(BlkActionState *common)
{
    BlockdevBackupState *state = DO_UPCAST(BlockdevBackupState, common, common);

    if (state->job) {
        job_cancel_sync(&state->job->job, true);
    }
}

static void blockdev_backup_clean(BlkActionState *common)
{
    BlockdevBackupState *state = DO_UPCAST(BlockdevBackupState, common, common);
    AioContext *aio_context;

    if (!state->bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(state->bs);
    aio_context_acquire(aio_context);

    bdrv_drained_end(state->bs);

    aio_context_release(aio_context);
}

typedef struct BlockDirtyBitmapState {
    BlkActionState common;
    BdrvDirtyBitmap *bitmap;
    BlockDriverState *bs;
    HBitmap *backup;
    bool prepared;
    bool was_enabled;
} BlockDirtyBitmapState;

static void block_dirty_bitmap_add_prepare(BlkActionState *common,
                                           Error **errp)
{
    Error *local_err = NULL;
    BlockDirtyBitmapAdd *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_add.data;
    /* AIO context taken and released within qmp_block_dirty_bitmap_add */
    qmp_block_dirty_bitmap_add(action->node, action->name,
                               action->has_granularity, action->granularity,
                               action->has_persistent, action->persistent,
                               action->has_disabled, action->disabled,
                               &local_err);

    if (!local_err) {
        state->prepared = true;
    } else {
        error_propagate(errp, local_err);
    }
}

static void block_dirty_bitmap_add_abort(BlkActionState *common)
{
    BlockDirtyBitmapAdd *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    action = common->action->u.block_dirty_bitmap_add.data;
    /* Should not be able to fail: IF the bitmap was added via .prepare(),
     * then the node reference and bitmap name must have been valid.
     */
    if (state->prepared) {
        qmp_block_dirty_bitmap_remove(action->node, action->name, &error_abort);
    }
}

static void block_dirty_bitmap_clear_prepare(BlkActionState *common,
                                             Error **errp)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);
    BlockDirtyBitmap *action;

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_clear.data;
    state->bitmap = block_dirty_bitmap_lookup(action->node,
                                              action->name,
                                              &state->bs,
                                              errp);
    if (!state->bitmap) {
        return;
    }

    if (bdrv_dirty_bitmap_check(state->bitmap, BDRV_BITMAP_DEFAULT, errp)) {
        return;
    }

    bdrv_clear_dirty_bitmap(state->bitmap, &state->backup);
}

static void block_dirty_bitmap_restore(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (state->backup) {
        bdrv_restore_dirty_bitmap(state->bitmap, state->backup);
    }
}

static void block_dirty_bitmap_free_backup(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    hbitmap_free(state->backup);
}

static void block_dirty_bitmap_enable_prepare(BlkActionState *common,
                                              Error **errp)
{
    BlockDirtyBitmap *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_enable.data;
    state->bitmap = block_dirty_bitmap_lookup(action->node,
                                              action->name,
                                              NULL,
                                              errp);
    if (!state->bitmap) {
        return;
    }

    if (bdrv_dirty_bitmap_check(state->bitmap, BDRV_BITMAP_ALLOW_RO, errp)) {
        return;
    }

    state->was_enabled = bdrv_dirty_bitmap_enabled(state->bitmap);
    bdrv_enable_dirty_bitmap(state->bitmap);
}

static void block_dirty_bitmap_enable_abort(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (!state->was_enabled) {
        bdrv_disable_dirty_bitmap(state->bitmap);
    }
}

static void block_dirty_bitmap_disable_prepare(BlkActionState *common,
                                               Error **errp)
{
    BlockDirtyBitmap *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_disable.data;
    state->bitmap = block_dirty_bitmap_lookup(action->node,
                                              action->name,
                                              NULL,
                                              errp);
    if (!state->bitmap) {
        return;
    }

    if (bdrv_dirty_bitmap_check(state->bitmap, BDRV_BITMAP_ALLOW_RO, errp)) {
        return;
    }

    state->was_enabled = bdrv_dirty_bitmap_enabled(state->bitmap);
    bdrv_disable_dirty_bitmap(state->bitmap);
}

static void block_dirty_bitmap_disable_abort(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (state->was_enabled) {
        bdrv_enable_dirty_bitmap(state->bitmap);
    }
}

static void block_dirty_bitmap_merge_prepare(BlkActionState *common,
                                             Error **errp)
{
    BlockDirtyBitmapMerge *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_merge.data;

    state->bitmap = block_dirty_bitmap_merge(action->node, action->target,
                                             action->bitmaps, &state->backup,
                                             errp);
}

static void block_dirty_bitmap_remove_prepare(BlkActionState *common,
                                              Error **errp)
{
    BlockDirtyBitmap *action;
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (action_check_completion_mode(common, errp) < 0) {
        return;
    }

    action = common->action->u.block_dirty_bitmap_remove.data;

    state->bitmap = block_dirty_bitmap_remove(action->node, action->name,
                                              false, &state->bs, errp);
    if (state->bitmap) {
        bdrv_dirty_bitmap_skip_store(state->bitmap, true);
        bdrv_dirty_bitmap_set_busy(state->bitmap, true);
    }
}

static void block_dirty_bitmap_remove_abort(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    if (state->bitmap) {
        bdrv_dirty_bitmap_skip_store(state->bitmap, false);
        bdrv_dirty_bitmap_set_busy(state->bitmap, false);
    }
}

static void block_dirty_bitmap_remove_commit(BlkActionState *common)
{
    BlockDirtyBitmapState *state = DO_UPCAST(BlockDirtyBitmapState,
                                             common, common);

    bdrv_dirty_bitmap_set_busy(state->bitmap, false);
    bdrv_release_dirty_bitmap(state->bitmap);
}

static void abort_prepare(BlkActionState *common, Error **errp)
{
    error_setg(errp, "Transaction aborted using Abort action");
}

static void abort_commit(BlkActionState *common)
{
    g_assert_not_reached(); /* this action never succeeds */
}

static const BlkActionOps actions[] = {
    [TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT] = {
        .instance_size = sizeof(ExternalSnapshotState),
        .prepare  = external_snapshot_prepare,
        .commit   = external_snapshot_commit,
        .abort = external_snapshot_abort,
        .clean = external_snapshot_clean,
    },
    [TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_SYNC] = {
        .instance_size = sizeof(ExternalSnapshotState),
        .prepare  = external_snapshot_prepare,
        .commit   = external_snapshot_commit,
        .abort = external_snapshot_abort,
        .clean = external_snapshot_clean,
    },
    [TRANSACTION_ACTION_KIND_DRIVE_BACKUP] = {
        .instance_size = sizeof(DriveBackupState),
        .prepare = drive_backup_prepare,
        .commit = drive_backup_commit,
        .abort = drive_backup_abort,
        .clean = drive_backup_clean,
    },
    [TRANSACTION_ACTION_KIND_BLOCKDEV_BACKUP] = {
        .instance_size = sizeof(BlockdevBackupState),
        .prepare = blockdev_backup_prepare,
        .commit = blockdev_backup_commit,
        .abort = blockdev_backup_abort,
        .clean = blockdev_backup_clean,
    },
    [TRANSACTION_ACTION_KIND_ABORT] = {
        .instance_size = sizeof(BlkActionState),
        .prepare = abort_prepare,
        .commit = abort_commit,
    },
    [TRANSACTION_ACTION_KIND_BLOCKDEV_SNAPSHOT_INTERNAL_SYNC] = {
        .instance_size = sizeof(InternalSnapshotState),
        .prepare  = internal_snapshot_prepare,
        .abort = internal_snapshot_abort,
        .clean = internal_snapshot_clean,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_ADD] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_add_prepare,
        .abort = block_dirty_bitmap_add_abort,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_CLEAR] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_clear_prepare,
        .commit = block_dirty_bitmap_free_backup,
        .abort = block_dirty_bitmap_restore,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_ENABLE] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_enable_prepare,
        .abort = block_dirty_bitmap_enable_abort,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_DISABLE] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_disable_prepare,
        .abort = block_dirty_bitmap_disable_abort,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_MERGE] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_merge_prepare,
        .commit = block_dirty_bitmap_free_backup,
        .abort = block_dirty_bitmap_restore,
    },
    [TRANSACTION_ACTION_KIND_BLOCK_DIRTY_BITMAP_REMOVE] = {
        .instance_size = sizeof(BlockDirtyBitmapState),
        .prepare = block_dirty_bitmap_remove_prepare,
        .commit = block_dirty_bitmap_remove_commit,
        .abort = block_dirty_bitmap_remove_abort,
    },
    /* Where are transactions for MIRROR, COMMIT and STREAM?
     * Although these blockjobs use transaction callbacks like the backup job,
     * these jobs do not necessarily adhere to transaction semantics.
     * These jobs may not fully undo all of their actions on abort, nor do they
     * necessarily work in transactions with more than one job in them.
     */
};

/**
 * Allocate a TransactionProperties structure if necessary, and fill
 * that structure with desired defaults if they are unset.
 */
static TransactionProperties *get_transaction_properties(
    TransactionProperties *props)
{
    if (!props) {
        props = g_new0(TransactionProperties, 1);
    }

    if (!props->has_completion_mode) {
        props->has_completion_mode = true;
        props->completion_mode = ACTION_COMPLETION_MODE_INDIVIDUAL;
    }

    return props;
}

/*
 * 'Atomic' group operations.  The operations are performed as a set, and if
 * any fail then we roll back all operations in the group.
 *
 * Always run under BQL.
 */
void qmp_transaction(TransactionActionList *dev_list,
                     bool has_props,
                     struct TransactionProperties *props,
                     Error **errp)
{
    TransactionActionList *dev_entry = dev_list;
    JobTxn *block_job_txn = NULL;
    BlkActionState *state, *next;
    Error *local_err = NULL;

    GLOBAL_STATE_CODE();

    QTAILQ_HEAD(, BlkActionState) snap_bdrv_states;
    QTAILQ_INIT(&snap_bdrv_states);

    /* Does this transaction get canceled as a group on failure?
     * If not, we don't really need to make a JobTxn.
     */
    props = get_transaction_properties(props);
    if (props->completion_mode != ACTION_COMPLETION_MODE_INDIVIDUAL) {
        block_job_txn = job_txn_new();
    }

    /* drain all i/o before any operations */
    bdrv_drain_all();

    /* We don't do anything in this loop that commits us to the operations */
    while (NULL != dev_entry) {
        TransactionAction *dev_info = NULL;
        const BlkActionOps *ops;

        dev_info = dev_entry->value;
        dev_entry = dev_entry->next;

        assert(dev_info->type < ARRAY_SIZE(actions));

        ops = &actions[dev_info->type];
        assert(ops->instance_size > 0);

        state = g_malloc0(ops->instance_size);
        state->ops = ops;
        state->action = dev_info;
        state->block_job_txn = block_job_txn;
        state->txn_props = props;
        QTAILQ_INSERT_TAIL(&snap_bdrv_states, state, entry);

        state->ops->prepare(state, &local_err);
        if (local_err) {
            error_propagate(errp, local_err);
            goto delete_and_fail;
        }
    }

    QTAILQ_FOREACH(state, &snap_bdrv_states, entry) {
        if (state->ops->commit) {
            state->ops->commit(state);
        }
    }

    /* success */
    goto exit;

delete_and_fail:
    /* failure, and it is all-or-none; roll back all operations */
    QTAILQ_FOREACH_REVERSE(state, &snap_bdrv_states, entry) {
        if (state->ops->abort) {
            state->ops->abort(state);
        }
    }
exit:
    QTAILQ_FOREACH_SAFE(state, &snap_bdrv_states, entry, next) {
        if (state->ops->clean) {
            state->ops->clean(state);
        }
        g_free(state);
    }
    if (!has_props) {
        qapi_free_TransactionProperties(props);
    }
    job_txn_unref(block_job_txn);
}

BlockDirtyBitmapSha256 *qmp_x_debug_block_dirty_bitmap_sha256(const char *node,
                                                              const char *name,
                                                              Error **errp)
{
    BdrvDirtyBitmap *bitmap;
    BlockDriverState *bs;
    BlockDirtyBitmapSha256 *ret = NULL;
    char *sha256;

    bitmap = block_dirty_bitmap_lookup(node, name, &bs, errp);
    if (!bitmap || !bs) {
        return NULL;
    }

    sha256 = bdrv_dirty_bitmap_sha256(bitmap, errp);
    if (sha256 == NULL) {
        return NULL;
    }

    ret = g_new(BlockDirtyBitmapSha256, 1);
    ret->sha256 = sha256;

    return ret;
}

void coroutine_fn qmp_block_resize(bool has_device, const char *device,
                                   bool has_node_name, const char *node_name,
                                   int64_t size, Error **errp)
{
    Error *local_err = NULL;
    BlockBackend *blk;
    BlockDriverState *bs;
    AioContext *old_ctx;

    bs = bdrv_lookup_bs(has_device ? device : NULL,
                        has_node_name ? node_name : NULL,
                        &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        return;
    }

    if (size < 0) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "size", "a >0 size");
        return;
    }

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_RESIZE, NULL)) {
        error_setg(errp, QERR_DEVICE_IN_USE, device);
        return;
    }

    blk = blk_new_with_bs(bs, BLK_PERM_RESIZE, BLK_PERM_ALL, errp);
    if (!blk) {
        return;
    }

    bdrv_co_lock(bs);
    bdrv_drained_begin(bs);
    bdrv_co_unlock(bs);

    old_ctx = bdrv_co_enter(bs);
    blk_co_truncate(blk, size, false, PREALLOC_MODE_OFF, 0, errp);
    bdrv_co_leave(bs, old_ctx);

    bdrv_co_lock(bs);
    bdrv_drained_end(bs);
    blk_unref(blk);
    bdrv_co_unlock(bs);
}

void qmp_block_stream(bool has_job_id, const char *job_id, const char *device,
                      bool has_base, const char *base,
                      bool has_base_node, const char *base_node,
                      bool has_backing_file, const char *backing_file,
                      bool has_bottom, const char *bottom,
                      bool has_speed, int64_t speed,
                      bool has_on_error, BlockdevOnError on_error,
                      bool has_filter_node_name, const char *filter_node_name,
                      bool has_auto_finalize, bool auto_finalize,
                      bool has_auto_dismiss, bool auto_dismiss,
                      Error **errp)
{
    BlockDriverState *bs, *iter, *iter_end;
    BlockDriverState *base_bs = NULL;
    BlockDriverState *bottom_bs = NULL;
    AioContext *aio_context;
    Error *local_err = NULL;
    int job_flags = JOB_DEFAULT;

    if (has_base && has_base_node) {
        error_setg(errp, "'base' and 'base-node' cannot be specified "
                   "at the same time");
        return;
    }

    if (has_base && has_bottom) {
        error_setg(errp, "'base' and 'bottom' cannot be specified "
                   "at the same time");
        return;
    }

    if (has_bottom && has_base_node) {
        error_setg(errp, "'bottom' and 'base-node' cannot be specified "
                   "at the same time");
        return;
    }

    if (!has_on_error) {
        on_error = BLOCKDEV_ON_ERROR_REPORT;
    }

    bs = bdrv_lookup_bs(device, device, errp);
    if (!bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    if (has_base) {
        base_bs = bdrv_find_backing_image(bs, base);
        if (base_bs == NULL) {
            error_setg(errp, "Can't find '%s' in the backing chain", base);
            goto out;
        }
        assert(bdrv_get_aio_context(base_bs) == aio_context);
    }

    if (has_base_node) {
        base_bs = bdrv_lookup_bs(NULL, base_node, errp);
        if (!base_bs) {
            goto out;
        }
        if (bs == base_bs || !bdrv_chain_contains(bs, base_bs)) {
            error_setg(errp, "Node '%s' is not a backing image of '%s'",
                       base_node, device);
            goto out;
        }
        assert(bdrv_get_aio_context(base_bs) == aio_context);
        bdrv_refresh_filename(base_bs);
    }

    if (has_bottom) {
        bottom_bs = bdrv_lookup_bs(NULL, bottom, errp);
        if (!bottom_bs) {
            goto out;
        }
        if (!bottom_bs->drv) {
            error_setg(errp, "Node '%s' is not open", bottom);
            goto out;
        }
        if (bottom_bs->drv->is_filter) {
            error_setg(errp, "Node '%s' is a filter, use a non-filter node "
                       "as 'bottom'", bottom);
            goto out;
        }
        if (!bdrv_chain_contains(bs, bottom_bs)) {
            error_setg(errp, "Node '%s' is not in a chain starting from '%s'",
                       bottom, device);
            goto out;
        }
        assert(bdrv_get_aio_context(bottom_bs) == aio_context);
    }

    /*
     * Check for op blockers in the whole chain between bs and base (or bottom)
     */
    iter_end = has_bottom ? bdrv_filter_or_cow_bs(bottom_bs) : base_bs;
    for (iter = bs; iter && iter != iter_end;
         iter = bdrv_filter_or_cow_bs(iter))
    {
        if (bdrv_op_is_blocked(iter, BLOCK_OP_TYPE_STREAM, errp)) {
            goto out;
        }
    }

    /* if we are streaming the entire chain, the result will have no backing
     * file, and specifying one is therefore an error */
    if (base_bs == NULL && has_backing_file) {
        error_setg(errp, "backing file specified, but streaming the "
                         "entire chain");
        goto out;
    }

    if (has_auto_finalize && !auto_finalize) {
        job_flags |= JOB_MANUAL_FINALIZE;
    }
    if (has_auto_dismiss && !auto_dismiss) {
        job_flags |= JOB_MANUAL_DISMISS;
    }

    stream_start(has_job_id ? job_id : NULL, bs, base_bs, backing_file,
                 bottom_bs, job_flags, has_speed ? speed : 0, on_error,
                 filter_node_name, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto out;
    }

    trace_qmp_block_stream(bs);

out:
    aio_context_release(aio_context);
}

void qmp_block_commit(bool has_job_id, const char *job_id, const char *device,
                      bool has_base_node, const char *base_node,
                      bool has_base, const char *base,
                      bool has_top_node, const char *top_node,
                      bool has_top, const char *top,
                      bool has_backing_file, const char *backing_file,
                      bool has_speed, int64_t speed,
                      bool has_on_error, BlockdevOnError on_error,
                      bool has_filter_node_name, const char *filter_node_name,
                      bool has_auto_finalize, bool auto_finalize,
                      bool has_auto_dismiss, bool auto_dismiss,
                      Error **errp)
{
    BlockDriverState *bs;
    BlockDriverState *iter;
    BlockDriverState *base_bs, *top_bs;
    AioContext *aio_context;
    Error *local_err = NULL;
    int job_flags = JOB_DEFAULT;
    uint64_t top_perm, top_shared;

    if (!has_speed) {
        speed = 0;
    }
    if (!has_on_error) {
        on_error = BLOCKDEV_ON_ERROR_REPORT;
    }
    if (!has_filter_node_name) {
        filter_node_name = NULL;
    }
    if (has_auto_finalize && !auto_finalize) {
        job_flags |= JOB_MANUAL_FINALIZE;
    }
    if (has_auto_dismiss && !auto_dismiss) {
        job_flags |= JOB_MANUAL_DISMISS;
    }

    /* Important Note:
     *  libvirt relies on the DeviceNotFound error class in order to probe for
     *  live commit feature versions; for this to work, we must make sure to
     *  perform the device lookup before any generic errors that may occur in a
     *  scenario in which all optional arguments are omitted. */
    bs = qmp_get_root_bs(device, &local_err);
    if (!bs) {
        bs = bdrv_lookup_bs(device, device, NULL);
        if (!bs) {
            error_free(local_err);
            error_set(errp, ERROR_CLASS_DEVICE_NOT_FOUND,
                      "Device '%s' not found", device);
        } else {
            error_propagate(errp, local_err);
        }
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_COMMIT_SOURCE, errp)) {
        goto out;
    }

    /* default top_bs is the active layer */
    top_bs = bs;

    if (has_top_node && has_top) {
        error_setg(errp, "'top-node' and 'top' are mutually exclusive");
        goto out;
    } else if (has_top_node) {
        top_bs = bdrv_lookup_bs(NULL, top_node, errp);
        if (top_bs == NULL) {
            goto out;
        }
        if (!bdrv_chain_contains(bs, top_bs)) {
            error_setg(errp, "'%s' is not in this backing file chain",
                       top_node);
            goto out;
        }
    } else if (has_top && top) {
        /* This strcmp() is just a shortcut, there is no need to
         * refresh @bs's filename.  If it mismatches,
         * bdrv_find_backing_image() will do the refresh and may still
         * return @bs. */
        if (strcmp(bs->filename, top) != 0) {
            top_bs = bdrv_find_backing_image(bs, top);
        }
    }

    if (top_bs == NULL) {
        error_setg(errp, "Top image file %s not found", top ? top : "NULL");
        goto out;
    }

    assert(bdrv_get_aio_context(top_bs) == aio_context);

    if (has_base_node && has_base) {
        error_setg(errp, "'base-node' and 'base' are mutually exclusive");
        goto out;
    } else if (has_base_node) {
        base_bs = bdrv_lookup_bs(NULL, base_node, errp);
        if (base_bs == NULL) {
            goto out;
        }
        if (!bdrv_chain_contains(top_bs, base_bs)) {
            error_setg(errp, "'%s' is not in this backing file chain",
                       base_node);
            goto out;
        }
    } else if (has_base && base) {
        base_bs = bdrv_find_backing_image(top_bs, base);
        if (base_bs == NULL) {
            error_setg(errp, "Can't find '%s' in the backing chain", base);
            goto out;
        }
    } else {
        base_bs = bdrv_find_base(top_bs);
        if (base_bs == NULL) {
            error_setg(errp, "There is no backimg image");
            goto out;
        }
    }

    assert(bdrv_get_aio_context(base_bs) == aio_context);

    for (iter = top_bs; iter != bdrv_filter_or_cow_bs(base_bs);
         iter = bdrv_filter_or_cow_bs(iter))
    {
        if (bdrv_op_is_blocked(iter, BLOCK_OP_TYPE_COMMIT_TARGET, errp)) {
            goto out;
        }
    }

    /* Do not allow attempts to commit an image into itself */
    if (top_bs == base_bs) {
        error_setg(errp, "cannot commit an image into itself");
        goto out;
    }

    /*
     * Active commit is required if and only if someone has taken a
     * WRITE permission on the top node.  Historically, we have always
     * used active commit for top nodes, so continue that practice
     * lest we possibly break clients that rely on this behavior, e.g.
     * to later attach this node to a writing parent.
     * (Active commit is never really wrong.)
     */
    bdrv_get_cumulative_perm(top_bs, &top_perm, &top_shared);
    if (top_perm & BLK_PERM_WRITE ||
        bdrv_skip_filters(top_bs) == bdrv_skip_filters(bs))
    {
        if (has_backing_file) {
            if (bdrv_skip_filters(top_bs) == bdrv_skip_filters(bs)) {
                error_setg(errp, "'backing-file' specified,"
                                 " but 'top' is the active layer");
            } else {
                error_setg(errp, "'backing-file' specified, but 'top' has a "
                                 "writer on it");
            }
            goto out;
        }
        if (!has_job_id) {
            /*
             * Emulate here what block_job_create() does, because it
             * is possible that @bs != @top_bs (the block job should
             * be named after @bs, even if @top_bs is the actual
             * source)
             */
            job_id = bdrv_get_device_name(bs);
        }
        commit_active_start(job_id, top_bs, base_bs, job_flags, speed, on_error,
                            filter_node_name, NULL, NULL, false, &local_err);
    } else {
        BlockDriverState *overlay_bs = bdrv_find_overlay(bs, top_bs);
        if (bdrv_op_is_blocked(overlay_bs, BLOCK_OP_TYPE_COMMIT_TARGET, errp)) {
            goto out;
        }
        commit_start(has_job_id ? job_id : NULL, bs, base_bs, top_bs, job_flags,
                     speed, on_error, has_backing_file ? backing_file : NULL,
                     filter_node_name, &local_err);
    }
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        goto out;
    }

out:
    aio_context_release(aio_context);
}

/* Common QMP interface for drive-backup and blockdev-backup */
static BlockJob *do_backup_common(BackupCommon *backup,
                                  BlockDriverState *bs,
                                  BlockDriverState *target_bs,
                                  AioContext *aio_context,
                                  JobTxn *txn, Error **errp)
{
    BlockJob *job = NULL;
    BdrvDirtyBitmap *bmap = NULL;
    BackupPerf perf = { .max_workers = 64 };
    int job_flags = JOB_DEFAULT;

    if (!backup->has_speed) {
        backup->speed = 0;
    }
    if (!backup->has_on_source_error) {
        backup->on_source_error = BLOCKDEV_ON_ERROR_REPORT;
    }
    if (!backup->has_on_target_error) {
        backup->on_target_error = BLOCKDEV_ON_ERROR_REPORT;
    }
    if (!backup->has_job_id) {
        backup->job_id = NULL;
    }
    if (!backup->has_auto_finalize) {
        backup->auto_finalize = true;
    }
    if (!backup->has_auto_dismiss) {
        backup->auto_dismiss = true;
    }
    if (!backup->has_compress) {
        backup->compress = false;
    }

    if (backup->x_perf) {
        if (backup->x_perf->has_use_copy_range) {
            perf.use_copy_range = backup->x_perf->use_copy_range;
        }
        if (backup->x_perf->has_max_workers) {
            perf.max_workers = backup->x_perf->max_workers;
        }
        if (backup->x_perf->has_max_chunk) {
            perf.max_chunk = backup->x_perf->max_chunk;
        }
    }

    if ((backup->sync == MIRROR_SYNC_MODE_BITMAP) ||
        (backup->sync == MIRROR_SYNC_MODE_INCREMENTAL)) {
        /* done before desugaring 'incremental' to print the right message */
        if (!backup->has_bitmap) {
            error_setg(errp, "must provide a valid bitmap name for "
                       "'%s' sync mode", MirrorSyncMode_str(backup->sync));
            return NULL;
        }
    }

    if (backup->sync == MIRROR_SYNC_MODE_INCREMENTAL) {
        if (backup->has_bitmap_mode &&
            backup->bitmap_mode != BITMAP_SYNC_MODE_ON_SUCCESS) {
            error_setg(errp, "Bitmap sync mode must be '%s' "
                       "when using sync mode '%s'",
                       BitmapSyncMode_str(BITMAP_SYNC_MODE_ON_SUCCESS),
                       MirrorSyncMode_str(backup->sync));
            return NULL;
        }
        backup->has_bitmap_mode = true;
        backup->sync = MIRROR_SYNC_MODE_BITMAP;
        backup->bitmap_mode = BITMAP_SYNC_MODE_ON_SUCCESS;
    }

    if (backup->has_bitmap) {
        bmap = bdrv_find_dirty_bitmap(bs, backup->bitmap);
        if (!bmap) {
            error_setg(errp, "Bitmap '%s' could not be found", backup->bitmap);
            return NULL;
        }
        if (!backup->has_bitmap_mode) {
            error_setg(errp, "Bitmap sync mode must be given "
                       "when providing a bitmap");
            return NULL;
        }
        if (bdrv_dirty_bitmap_check(bmap, BDRV_BITMAP_ALLOW_RO, errp)) {
            return NULL;
        }

        /* This does not produce a useful bitmap artifact: */
        if (backup->sync == MIRROR_SYNC_MODE_NONE) {
            error_setg(errp, "sync mode '%s' does not produce meaningful bitmap"
                       " outputs", MirrorSyncMode_str(backup->sync));
            return NULL;
        }

        /* If the bitmap isn't used for input or output, this is useless: */
        if (backup->bitmap_mode == BITMAP_SYNC_MODE_NEVER &&
            backup->sync != MIRROR_SYNC_MODE_BITMAP) {
            error_setg(errp, "Bitmap sync mode '%s' has no meaningful effect"
                       " when combined with sync mode '%s'",
                       BitmapSyncMode_str(backup->bitmap_mode),
                       MirrorSyncMode_str(backup->sync));
            return NULL;
        }
    }

    if (!backup->has_bitmap && backup->has_bitmap_mode) {
        error_setg(errp, "Cannot specify bitmap sync mode without a bitmap");
        return NULL;
    }

    if (!backup->auto_finalize) {
        job_flags |= JOB_MANUAL_FINALIZE;
    }
    if (!backup->auto_dismiss) {
        job_flags |= JOB_MANUAL_DISMISS;
    }

    job = backup_job_create(backup->job_id, bs, target_bs, backup->speed,
                            backup->sync, bmap, backup->bitmap_mode,
                            backup->compress,
                            backup->filter_node_name,
                            &perf,
                            backup->on_source_error,
                            backup->on_target_error,
                            job_flags, NULL, NULL, txn, errp);
    return job;
}

void qmp_drive_backup(DriveBackup *backup, Error **errp)
{
    TransactionAction action = {
        .type = TRANSACTION_ACTION_KIND_DRIVE_BACKUP,
        .u.drive_backup.data = backup,
    };
    blockdev_do_action(&action, errp);
}

BlockDeviceInfoList *qmp_query_named_block_nodes(bool has_flat,
                                                 bool flat,
                                                 Error **errp)
{
    bool return_flat = has_flat && flat;

    return bdrv_named_nodes_list(return_flat, errp);
}

XDbgBlockGraph *qmp_x_debug_query_block_graph(Error **errp)
{
    return bdrv_get_xdbg_block_graph(errp);
}

void qmp_blockdev_backup(BlockdevBackup *backup, Error **errp)
{
    TransactionAction action = {
        .type = TRANSACTION_ACTION_KIND_BLOCKDEV_BACKUP,
        .u.blockdev_backup.data = backup,
    };
    blockdev_do_action(&action, errp);
}

/* Parameter check and block job starting for drive mirroring.
 * Caller should hold @device and @target's aio context (must be the same).
 **/
static void blockdev_mirror_common(const char *job_id, BlockDriverState *bs,
                                   BlockDriverState *target,
                                   bool has_replaces, const char *replaces,
                                   enum MirrorSyncMode sync,
                                   BlockMirrorBackingMode backing_mode,
                                   bool zero_target,
                                   bool has_speed, int64_t speed,
                                   bool has_granularity, uint32_t granularity,
                                   bool has_buf_size, int64_t buf_size,
                                   bool has_on_source_error,
                                   BlockdevOnError on_source_error,
                                   bool has_on_target_error,
                                   BlockdevOnError on_target_error,
                                   bool has_unmap, bool unmap,
                                   bool has_filter_node_name,
                                   const char *filter_node_name,
                                   bool has_copy_mode, MirrorCopyMode copy_mode,
                                   bool has_auto_finalize, bool auto_finalize,
                                   bool has_auto_dismiss, bool auto_dismiss,
                                   Error **errp)
{
    BlockDriverState *unfiltered_bs;
    int job_flags = JOB_DEFAULT;

    if (!has_speed) {
        speed = 0;
    }
    if (!has_on_source_error) {
        on_source_error = BLOCKDEV_ON_ERROR_REPORT;
    }
    if (!has_on_target_error) {
        on_target_error = BLOCKDEV_ON_ERROR_REPORT;
    }
    if (!has_granularity) {
        granularity = 0;
    }
    if (!has_buf_size) {
        buf_size = 0;
    }
    if (!has_unmap) {
        unmap = true;
    }
    if (!has_filter_node_name) {
        filter_node_name = NULL;
    }
    if (!has_copy_mode) {
        copy_mode = MIRROR_COPY_MODE_BACKGROUND;
    }
    if (has_auto_finalize && !auto_finalize) {
        job_flags |= JOB_MANUAL_FINALIZE;
    }
    if (has_auto_dismiss && !auto_dismiss) {
        job_flags |= JOB_MANUAL_DISMISS;
    }

    if (granularity != 0 && (granularity < 512 || granularity > 1048576 * 64)) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "granularity",
                   "a value in range [512B, 64MB]");
        return;
    }
    if (granularity & (granularity - 1)) {
        error_setg(errp, QERR_INVALID_PARAMETER_VALUE, "granularity",
                   "a power of 2");
        return;
    }

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_MIRROR_SOURCE, errp)) {
        return;
    }
    if (bdrv_op_is_blocked(target, BLOCK_OP_TYPE_MIRROR_TARGET, errp)) {
        return;
    }

    if (!bdrv_backing_chain_next(bs) && sync == MIRROR_SYNC_MODE_TOP) {
        sync = MIRROR_SYNC_MODE_FULL;
    }

    if (!has_replaces) {
        /* We want to mirror from @bs, but keep implicit filters on top */
        unfiltered_bs = bdrv_skip_implicit_filters(bs);
        if (unfiltered_bs != bs) {
            replaces = unfiltered_bs->node_name;
            has_replaces = true;
        }
    }

    if (has_replaces) {
        BlockDriverState *to_replace_bs;
        AioContext *replace_aio_context;
        int64_t bs_size, replace_size;

        bs_size = bdrv_getlength(bs);
        if (bs_size < 0) {
            error_setg_errno(errp, -bs_size, "Failed to query device's size");
            return;
        }

        to_replace_bs = check_to_replace_node(bs, replaces, errp);
        if (!to_replace_bs) {
            return;
        }

        replace_aio_context = bdrv_get_aio_context(to_replace_bs);
        aio_context_acquire(replace_aio_context);
        replace_size = bdrv_getlength(to_replace_bs);
        aio_context_release(replace_aio_context);

        if (replace_size < 0) {
            error_setg_errno(errp, -replace_size,
                             "Failed to query the replacement node's size");
            return;
        }
        if (bs_size != replace_size) {
            error_setg(errp, "cannot replace image with a mirror image of "
                             "different size");
            return;
        }
    }

    /* pass the node name to replace to mirror start since it's loose coupling
     * and will allow to check whether the node still exist at mirror completion
     */
    mirror_start(job_id, bs, target,
                 has_replaces ? replaces : NULL, job_flags,
                 speed, granularity, buf_size, sync, backing_mode, zero_target,
                 on_source_error, on_target_error, unmap, filter_node_name,
                 copy_mode, errp);
}

void qmp_drive_mirror(DriveMirror *arg, Error **errp)
{
    BlockDriverState *bs;
    BlockDriverState *target_backing_bs, *target_bs;
    AioContext *aio_context;
    AioContext *old_context;
    BlockMirrorBackingMode backing_mode;
    Error *local_err = NULL;
    QDict *options = NULL;
    int flags;
    int64_t size;
    const char *format = arg->format;
    bool zero_target;
    int ret;

    bs = qmp_get_root_bs(arg->device, errp);
    if (!bs) {
        return;
    }

    /* Early check to avoid creating target */
    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_MIRROR_SOURCE, errp)) {
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    if (!arg->has_mode) {
        arg->mode = NEW_IMAGE_MODE_ABSOLUTE_PATHS;
    }

    if (!arg->has_format) {
        format = (arg->mode == NEW_IMAGE_MODE_EXISTING
                  ? NULL : bs->drv->format_name);
    }

    flags = bs->open_flags | BDRV_O_RDWR;
    target_backing_bs = bdrv_cow_bs(bdrv_skip_filters(bs));
    if (!target_backing_bs && arg->sync == MIRROR_SYNC_MODE_TOP) {
        arg->sync = MIRROR_SYNC_MODE_FULL;
    }
    if (arg->sync == MIRROR_SYNC_MODE_NONE) {
        target_backing_bs = bs;
    }

    size = bdrv_getlength(bs);
    if (size < 0) {
        error_setg_errno(errp, -size, "bdrv_getlength failed");
        goto out;
    }

    if (arg->has_replaces) {
        if (!arg->has_node_name) {
            error_setg(errp, "a node-name must be provided when replacing a"
                             " named node of the graph");
            goto out;
        }
    }

    if (arg->mode == NEW_IMAGE_MODE_ABSOLUTE_PATHS) {
        backing_mode = MIRROR_SOURCE_BACKING_CHAIN;
    } else {
        backing_mode = MIRROR_OPEN_BACKING_CHAIN;
    }

    /* Don't open backing image in create() */
    flags |= BDRV_O_NO_BACKING;

    if ((arg->sync == MIRROR_SYNC_MODE_FULL || !target_backing_bs)
        && arg->mode != NEW_IMAGE_MODE_EXISTING)
    {
        /* create new image w/o backing file */
        assert(format);
        bdrv_img_create(arg->target, format,
                        NULL, NULL, NULL, size, flags, false, &local_err);
    } else {
        /* Implicit filters should not appear in the filename */
        BlockDriverState *explicit_backing =
            bdrv_skip_implicit_filters(target_backing_bs);

        switch (arg->mode) {
        case NEW_IMAGE_MODE_EXISTING:
            break;
        case NEW_IMAGE_MODE_ABSOLUTE_PATHS:
            /* create new image with backing file */
            bdrv_refresh_filename(explicit_backing);
            bdrv_img_create(arg->target, format,
                            explicit_backing->filename,
                            explicit_backing->drv->format_name,
                            NULL, size, flags, false, &local_err);
            break;
        default:
            abort();
        }
    }

    if (local_err) {
        error_propagate(errp, local_err);
        goto out;
    }

    options = qdict_new();
    if (arg->has_node_name) {
        qdict_put_str(options, "node-name", arg->node_name);
    }
    if (format) {
        qdict_put_str(options, "driver", format);
    }

    /* Mirroring takes care of copy-on-write using the source's backing
     * file.
     */
    target_bs = bdrv_open(arg->target, NULL, options, flags, errp);
    if (!target_bs) {
        goto out;
    }

    zero_target = (arg->sync == MIRROR_SYNC_MODE_FULL &&
                   (arg->mode == NEW_IMAGE_MODE_EXISTING ||
                    !bdrv_has_zero_init(target_bs)));


    /* Honor bdrv_try_change_aio_context() context acquisition requirements. */
    old_context = bdrv_get_aio_context(target_bs);
    aio_context_release(aio_context);
    aio_context_acquire(old_context);

    ret = bdrv_try_change_aio_context(target_bs, aio_context, NULL, errp);
    if (ret < 0) {
        bdrv_unref(target_bs);
        aio_context_release(old_context);
        return;
    }

    aio_context_release(old_context);
    aio_context_acquire(aio_context);

    blockdev_mirror_common(arg->has_job_id ? arg->job_id : NULL, bs, target_bs,
                           arg->has_replaces, arg->replaces, arg->sync,
                           backing_mode, zero_target,
                           arg->has_speed, arg->speed,
                           arg->has_granularity, arg->granularity,
                           arg->has_buf_size, arg->buf_size,
                           arg->has_on_source_error, arg->on_source_error,
                           arg->has_on_target_error, arg->on_target_error,
                           arg->has_unmap, arg->unmap,
                           false, NULL,
                           arg->has_copy_mode, arg->copy_mode,
                           arg->has_auto_finalize, arg->auto_finalize,
                           arg->has_auto_dismiss, arg->auto_dismiss,
                           errp);
    bdrv_unref(target_bs);
out:
    aio_context_release(aio_context);
}

void qmp_blockdev_mirror(bool has_job_id, const char *job_id,
                         const char *device, const char *target,
                         bool has_replaces, const char *replaces,
                         MirrorSyncMode sync,
                         bool has_speed, int64_t speed,
                         bool has_granularity, uint32_t granularity,
                         bool has_buf_size, int64_t buf_size,
                         bool has_on_source_error,
                         BlockdevOnError on_source_error,
                         bool has_on_target_error,
                         BlockdevOnError on_target_error,
                         bool has_filter_node_name,
                         const char *filter_node_name,
                         bool has_copy_mode, MirrorCopyMode copy_mode,
                         bool has_auto_finalize, bool auto_finalize,
                         bool has_auto_dismiss, bool auto_dismiss,
                         Error **errp)
{
    BlockDriverState *bs;
    BlockDriverState *target_bs;
    AioContext *aio_context;
    AioContext *old_context;
    BlockMirrorBackingMode backing_mode = MIRROR_LEAVE_BACKING_CHAIN;
    bool zero_target;
    int ret;

    bs = qmp_get_root_bs(device, errp);
    if (!bs) {
        return;
    }

    target_bs = bdrv_lookup_bs(target, target, errp);
    if (!target_bs) {
        return;
    }

    zero_target = (sync == MIRROR_SYNC_MODE_FULL);

    /* Honor bdrv_try_change_aio_context() context acquisition requirements. */
    old_context = bdrv_get_aio_context(target_bs);
    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(old_context);

    ret = bdrv_try_change_aio_context(target_bs, aio_context, NULL, errp);

    aio_context_release(old_context);
    aio_context_acquire(aio_context);

    if (ret < 0) {
        goto out;
    }

    blockdev_mirror_common(has_job_id ? job_id : NULL, bs, target_bs,
                           has_replaces, replaces, sync, backing_mode,
                           zero_target, has_speed, speed,
                           has_granularity, granularity,
                           has_buf_size, buf_size,
                           has_on_source_error, on_source_error,
                           has_on_target_error, on_target_error,
                           true, true,
                           has_filter_node_name, filter_node_name,
                           has_copy_mode, copy_mode,
                           has_auto_finalize, auto_finalize,
                           has_auto_dismiss, auto_dismiss,
                           errp);
out:
    aio_context_release(aio_context);
}

/*
 * Get a block job using its ID. Called with job_mutex held.
 */
static BlockJob *find_block_job_locked(const char *id, Error **errp)
{
    BlockJob *job;

    assert(id != NULL);

    job = block_job_get_locked(id);

    if (!job) {
        error_set(errp, ERROR_CLASS_DEVICE_NOT_ACTIVE,
                  "Block job '%s' not found", id);
        return NULL;
    }

    return job;
}

void qmp_block_job_set_speed(const char *device, int64_t speed, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(device, errp);

    if (!job) {
        return;
    }

    block_job_set_speed_locked(job, speed, errp);
}

void qmp_block_job_cancel(const char *device,
                          bool has_force, bool force, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(device, errp);

    if (!job) {
        return;
    }

    if (!has_force) {
        force = false;
    }

    if (job_user_paused_locked(&job->job) && !force) {
        error_setg(errp, "The block job for device '%s' is currently paused",
                   device);
        return;
    }

    trace_qmp_block_job_cancel(job);
    job_user_cancel_locked(&job->job, force, errp);
}

void qmp_block_job_pause(const char *device, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(device, errp);

    if (!job) {
        return;
    }

    trace_qmp_block_job_pause(job);
    job_user_pause_locked(&job->job, errp);
}

void qmp_block_job_resume(const char *device, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(device, errp);

    if (!job) {
        return;
    }

    trace_qmp_block_job_resume(job);
    job_user_resume_locked(&job->job, errp);
}

void qmp_block_job_complete(const char *device, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(device, errp);

    if (!job) {
        return;
    }

    trace_qmp_block_job_complete(job);
    job_complete_locked(&job->job, errp);
}

void qmp_block_job_finalize(const char *id, Error **errp)
{
    BlockJob *job;

    JOB_LOCK_GUARD();
    job = find_block_job_locked(id, errp);

    if (!job) {
        return;
    }

    trace_qmp_block_job_finalize(job);
    job_ref_locked(&job->job);
    job_finalize_locked(&job->job, errp);

    job_unref_locked(&job->job);
}

void qmp_block_job_dismiss(const char *id, Error **errp)
{
    BlockJob *bjob;
    Job *job;

    JOB_LOCK_GUARD();
    bjob = find_block_job_locked(id, errp);

    if (!bjob) {
        return;
    }

    trace_qmp_block_job_dismiss(bjob);
    job = &bjob->job;
    job_dismiss_locked(&job, errp);
}

void qmp_change_backing_file(const char *device,
                             const char *image_node_name,
                             const char *backing_file,
                             Error **errp)
{
    BlockDriverState *bs = NULL;
    AioContext *aio_context;
    BlockDriverState *image_bs = NULL;
    Error *local_err = NULL;
    bool ro;
    int ret;

    bs = qmp_get_root_bs(device, errp);
    if (!bs) {
        return;
    }

    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    image_bs = bdrv_lookup_bs(NULL, image_node_name, &local_err);
    if (local_err) {
        error_propagate(errp, local_err);
        goto out;
    }

    if (!image_bs) {
        error_setg(errp, "image file not found");
        goto out;
    }

    if (bdrv_find_base(image_bs) == image_bs) {
        error_setg(errp, "not allowing backing file change on an image "
                         "without a backing file");
        goto out;
    }

    /* even though we are not necessarily operating on bs, we need it to
     * determine if block ops are currently prohibited on the chain */
    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_CHANGE, errp)) {
        goto out;
    }

    /* final sanity check */
    if (!bdrv_chain_contains(bs, image_bs)) {
        error_setg(errp, "'%s' and image file are not in the same chain",
                   device);
        goto out;
    }

    /* if not r/w, reopen to make r/w */
    ro = bdrv_is_read_only(image_bs);

    if (ro) {
        if (bdrv_reopen_set_read_only(image_bs, false, errp) != 0) {
            goto out;
        }
    }

    ret = bdrv_change_backing_file(image_bs, backing_file,
                                   image_bs->drv ? image_bs->drv->format_name : "",
                                   false);

    if (ret < 0) {
        error_setg_errno(errp, -ret, "Could not change backing file to '%s'",
                         backing_file);
        /* don't exit here, so we can try to restore open flags if
         * appropriate */
    }

    if (ro) {
        bdrv_reopen_set_read_only(image_bs, true, errp);
    }

out:
    aio_context_release(aio_context);
}

void qmp_blockdev_add(BlockdevOptions *options, Error **errp)
{
    BlockDriverState *bs;
    QObject *obj;
    Visitor *v = qobject_output_visitor_new(&obj);
    QDict *qdict;

    visit_type_BlockdevOptions(v, NULL, &options, &error_abort);
    visit_complete(v, &obj);
    qdict = qobject_to(QDict, obj);

    qdict_flatten(qdict);

    if (!qdict_get_try_str(qdict, "node-name")) {
        error_setg(errp, "'node-name' must be specified for the root node");
        goto fail;
    }

    bs = bds_tree_init(qdict, errp);
    if (!bs) {
        goto fail;
    }

    bdrv_set_monitor_owned(bs);

fail:
    visit_free(v);
}

void qmp_blockdev_reopen(BlockdevOptionsList *reopen_list, Error **errp)
{
    BlockReopenQueue *queue = NULL;
    GSList *drained = NULL;
    GSList *p;

    /* Add each one of the BDS that we want to reopen to the queue */
    for (; reopen_list != NULL; reopen_list = reopen_list->next) {
        BlockdevOptions *options = reopen_list->value;
        BlockDriverState *bs;
        AioContext *ctx;
        QObject *obj;
        Visitor *v;
        QDict *qdict;

        /* Check for the selected node name */
        if (!options->has_node_name) {
            error_setg(errp, "node-name not specified");
            goto fail;
        }

        bs = bdrv_find_node(options->node_name);
        if (!bs) {
            error_setg(errp, "Failed to find node with node-name='%s'",
                       options->node_name);
            goto fail;
        }

        /* Put all options in a QDict and flatten it */
        v = qobject_output_visitor_new(&obj);
        visit_type_BlockdevOptions(v, NULL, &options, &error_abort);
        visit_complete(v, &obj);
        visit_free(v);

        qdict = qobject_to(QDict, obj);

        qdict_flatten(qdict);

        ctx = bdrv_get_aio_context(bs);
        aio_context_acquire(ctx);

        bdrv_subtree_drained_begin(bs);
        queue = bdrv_reopen_queue(queue, bs, qdict, false);
        drained = g_slist_prepend(drained, bs);

        aio_context_release(ctx);
    }

    /* Perform the reopen operation */
    bdrv_reopen_multiple(queue, errp);
    queue = NULL;

fail:
    bdrv_reopen_queue_free(queue);
    for (p = drained; p; p = p->next) {
        BlockDriverState *bs = p->data;
        AioContext *ctx = bdrv_get_aio_context(bs);

        aio_context_acquire(ctx);
        bdrv_subtree_drained_end(bs);
        aio_context_release(ctx);
    }
    g_slist_free(drained);
}

void qmp_blockdev_del(const char *node_name, Error **errp)
{
    AioContext *aio_context;
    BlockDriverState *bs;

    GLOBAL_STATE_CODE();

    bs = bdrv_find_node(node_name);
    if (!bs) {
        error_setg(errp, "Failed to find node with node-name='%s'", node_name);
        return;
    }
    if (bdrv_has_blk(bs)) {
        error_setg(errp, "Node %s is in use", node_name);
        return;
    }
    aio_context = bdrv_get_aio_context(bs);
    aio_context_acquire(aio_context);

    if (bdrv_op_is_blocked(bs, BLOCK_OP_TYPE_DRIVE_DEL, errp)) {
        goto out;
    }

    if (!QTAILQ_IN_USE(bs, monitor_list)) {
        error_setg(errp, "Node %s is not owned by the monitor",
                   bs->node_name);
        goto out;
    }

    if (bs->refcnt > 1) {
        error_setg(errp, "Block device %s is in use",
                   bdrv_get_device_or_node_name(bs));
        goto out;
    }

    QTAILQ_REMOVE(&monitor_bdrv_states, bs, monitor_list);
    bdrv_unref(bs);

out:
    aio_context_release(aio_context);
}

static BdrvChild *bdrv_find_child(BlockDriverState *parent_bs,
                                  const char *child_name)
{
    BdrvChild *child;

    QLIST_FOREACH(child, &parent_bs->children, next) {
        if (strcmp(child->name, child_name) == 0) {
            return child;
        }
    }

    return NULL;
}

void qmp_x_blockdev_change(const char *parent, bool has_child,
                           const char *child, bool has_node,
                           const char *node, Error **errp)
{
    BlockDriverState *parent_bs, *new_bs = NULL;
    BdrvChild *p_child;

    parent_bs = bdrv_lookup_bs(parent, parent, errp);
    if (!parent_bs) {
        return;
    }

    if (has_child == has_node) {
        if (has_child) {
            error_setg(errp, "The parameters child and node are in conflict");
        } else {
            error_setg(errp, "Either child or node must be specified");
        }
        return;
    }

    if (has_child) {
        p_child = bdrv_find_child(parent_bs, child);
        if (!p_child) {
            error_setg(errp, "Node '%s' does not have child '%s'",
                       parent, child);
            return;
        }
        bdrv_del_child(parent_bs, p_child, errp);
    }

    if (has_node) {
        new_bs = bdrv_find_node(node);
        if (!new_bs) {
            error_setg(errp, "Node '%s' not found", node);
            return;
        }
        bdrv_add_child(parent_bs, new_bs, errp);
    }
}

BlockJobInfoList *qmp_query_block_jobs(Error **errp)
{
    BlockJobInfoList *head = NULL, **tail = &head;
    BlockJob *job;

    JOB_LOCK_GUARD();

    for (job = block_job_next_locked(NULL); job;
         job = block_job_next_locked(job)) {
        BlockJobInfo *value;

        if (block_job_is_internal(job)) {
            continue;
        }
        value = block_job_query_locked(job, errp);
        if (!value) {
            qapi_free_BlockJobInfoList(head);
            return NULL;
        }
        QAPI_LIST_APPEND(tail, value);
    }

    return head;
}

void qmp_x_blockdev_set_iothread(const char *node_name, StrOrNull *iothread,
                                 bool has_force, bool force, Error **errp)
{
    AioContext *old_context;
    AioContext *new_context;
    BlockDriverState *bs;

    bs = bdrv_find_node(node_name);
    if (!bs) {
        error_setg(errp, "Failed to find node with node-name='%s'", node_name);
        return;
    }

    /* Protects against accidents. */
    if (!(has_force && force) && bdrv_has_blk(bs)) {
        error_setg(errp, "Node %s is associated with a BlockBackend and could "
                         "be in use (use force=true to override this check)",
                         node_name);
        return;
    }

    if (iothread->type == QTYPE_QSTRING) {
        IOThread *obj = iothread_by_id(iothread->u.s);
        if (!obj) {
            error_setg(errp, "Cannot find iothread %s", iothread->u.s);
            return;
        }

        new_context = iothread_get_aio_context(obj);
    } else {
        new_context = qemu_get_aio_context();
    }

    old_context = bdrv_get_aio_context(bs);
    aio_context_acquire(old_context);

    bdrv_try_change_aio_context(bs, new_context, NULL, errp);

    aio_context_release(old_context);
}

QemuOptsList qemu_common_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_common_drive_opts.head),
    .desc = {
        {
            .name = "snapshot",
            .type = QEMU_OPT_BOOL,
            .help = "enable/disable snapshot mode",
        },{
            .name = "aio",
            .type = QEMU_OPT_STRING,
            .help = "host AIO implementation (threads, native, io_uring)",
        },{
            .name = BDRV_OPT_CACHE_WB,
            .type = QEMU_OPT_BOOL,
            .help = "Enable writeback mode",
        },{
            .name = "format",
            .type = QEMU_OPT_STRING,
            .help = "disk format (raw, qcow2, ...)",
        },{
            .name = "rerror",
            .type = QEMU_OPT_STRING,
            .help = "read error action",
        },{
            .name = "werror",
            .type = QEMU_OPT_STRING,
            .help = "write error action",
        },{
            .name = BDRV_OPT_READ_ONLY,
            .type = QEMU_OPT_BOOL,
            .help = "open drive file as read-only",
        },

        THROTTLE_OPTS,

        {
            .name = "throttling.group",
            .type = QEMU_OPT_STRING,
            .help = "name of the block throttling group",
        },{
            .name = "copy-on-read",
            .type = QEMU_OPT_BOOL,
            .help = "copy read data from backing file into image file",
        },{
            .name = "detect-zeroes",
            .type = QEMU_OPT_STRING,
            .help = "try to optimize zero writes (off, on, unmap)",
        },{
            .name = "stats-account-invalid",
            .type = QEMU_OPT_BOOL,
            .help = "whether to account for invalid I/O operations "
                    "in the statistics",
        },{
            .name = "stats-account-failed",
            .type = QEMU_OPT_BOOL,
            .help = "whether to account for failed I/O operations "
                    "in the statistics",
        },
        { /* end of list */ }
    },
};

QemuOptsList qemu_drive_opts = {
    .name = "drive",
    .head = QTAILQ_HEAD_INITIALIZER(qemu_drive_opts.head),
    .desc = {
        /*
         * no elements => accept any params
         * validation will happen later
         */
        { /* end of list */ }
    },
};
