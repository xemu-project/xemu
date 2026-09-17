/*
 * CCI Xbox disc container - QEMU block driver.
 *
 * This is a thin adapter: the CCI container format (header, index, LZ4 sector
 * decoding, multi-file layout) is implemented by the standalone libCCI project;
 * see https://github.com/Team-Resurgent/libCCI and its FORMAT.md. Here we only
 * bridge libCCI's read callback to the QEMU block layer.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "block/block_int.h"
#include "block/block-io.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/iov.h"
#include "qemu/ctype.h"
#include "qemu/memalign.h"

#include "cci/cci.h"

typedef struct BDRVCciState {
    cci_reader *reader;
} BDRVCciState;

/* Map a libCCI status to a negative errno for the block layer. */
static int cci_to_errno(int e)
{
    switch (e) {
    case CCI_OK:        return 0;
    case CCI_ERR_NOMEM: return -ENOMEM;
    case CCI_ERR_IO:    return -EIO;
    default:            return -EINVAL; /* format / range / lz4 / unsupported */
    }
}

/* libCCI read callback: fulfil a read from the backing block child. */
static int64_t cci_bdrv_read(void *ctx, uint64_t offset, void *buf, size_t len)
{
    BdrvChild *ch = ctx;
    int ret = bdrv_pread(ch, (int64_t)offset, (int64_t)len, buf, 0);
    return ret < 0 ? CCI_ERR_IO : (int64_t)len;
}

static int cci_open(BlockDriverState *bs, QDict *options, int flags,
                    Error **errp)
{
    BDRVCciState *s = bs->opaque;
    int ret;

    GLOBAL_STATE_CODE();

    memset(s, 0, sizeof(*s));

    bdrv_open_child(NULL, options, "file", bs, &child_of_bds,
                    BDRV_CHILD_DATA | BDRV_CHILD_PRIMARY, false, errp);
    if (!bs->file) {
        return -EINVAL;
    }

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    /* libCCI parses the header + index here; bs->file stays owned by QEMU. */
    ret = cci_reader_open(&s->reader, cci_bdrv_read, bs->file, NULL);
    if (ret != CCI_OK) {
        error_setg(errp, "CCI: %s", cci_strerror(ret));
        return cci_to_errno(ret);
    }

    bdrv_refresh_filename(bs->file->bs);

    bs->sg = false;
    bs->supported_write_flags = BDRV_REQ_WRITE_UNCHANGED;
    bs->supported_zero_flags = BDRV_REQ_WRITE_UNCHANGED;
    return 0;
}

static void cci_close(BlockDriverState *bs)
{
    BDRVCciState *s = bs->opaque;
    cci_reader_free(s->reader);
    s->reader = NULL;
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_preadv(BlockDriverState *bs, int64_t offset, int64_t bytes,
              QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    BDRVCciState *s = bs->opaque;
    uint8_t *secbuf;
    int64_t end;
    uint64_t first_lba, last_lba, lba;
    int ret = 0;

    if (offset < 0 || bytes < 0) {
        return -EINVAL;
    }
    if (bytes == 0) {
        return 0;
    }
    end = offset + bytes;
    if (end > bdrv_co_getlength(bs)) {
        return -EINVAL;
    }

    secbuf = qemu_try_blockalign(bs, CCI_SECTOR_SIZE);
    if (!secbuf) {
        return -ENOMEM;
    }

    first_lba = (uint64_t)offset / CCI_SECTOR_SIZE;
    last_lba = (uint64_t)(end - 1) / CCI_SECTOR_SIZE;

    for (lba = first_lba; lba <= last_lba; lba++) {
        int64_t sec_start = (int64_t)lba * CCI_SECTOR_SIZE;
        int64_t copy_start = MAX(sec_start, offset);
        int64_t copy_end = MIN(sec_start + CCI_SECTOR_SIZE, end);
        size_t sec_off = copy_start - sec_start;

        ret = cci_reader_read_sector(s->reader, lba, secbuf);
        if (ret != CCI_OK) {
            ret = cci_to_errno(ret);
            goto out;
        }
        qemu_iovec_from_buf(qiov, copy_start - offset, secbuf + sec_off,
                            copy_end - copy_start);
    }
    ret = 0;

out:
    qemu_vfree(secbuf);
    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_pwritev(BlockDriverState *bs, int64_t offset, int64_t bytes,
               QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    return -EROFS;
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_block_status(BlockDriverState *bs, unsigned int mode, int64_t offset,
                    int64_t bytes, int64_t *pnum, int64_t *map,
                    BlockDriverState **file)
{
    *pnum = bytes;
    *map = -1;
    *file = NULL;
    return BDRV_BLOCK_DATA;
}

static int64_t coroutine_fn GRAPH_RDLOCK
cci_co_getlength(BlockDriverState *bs)
{
    BDRVCciState *s = bs->opaque;
    uint64_t sz = cci_reader_size(s->reader);
    return sz > INT64_MAX ? INT64_MAX : (int64_t)sz;
}

static void GRAPH_RDLOCK cci_refresh_limits(BlockDriverState *bs, Error **errp)
{
    bs->bl.has_variable_length = false;
}

static void cci_child_perm(BlockDriverState *bs, BdrvChild *c,
                           BdrvChildRole role, BlockReopenQueue *reopen_queue,
                           uint64_t parent_perm, uint64_t parent_shared,
                           uint64_t *nperm, uint64_t *nshared)
{
    bdrv_default_perms(bs, c, role, reopen_queue, parent_perm, parent_shared,
                       nperm, nshared);
    *nperm &= ~(BLK_PERM_WRITE | BLK_PERM_RESIZE);
}

static bool cci_filename_has_suffix_ci(const char *name, const char *suffix)
{
    size_t nl = strlen(name), sl = strlen(suffix);
    size_t i;

    if (nl < sl) {
        return false;
    }
    name += nl - sl;
    for (i = 0; i < sl; i++) {
        if (qemu_tolower(name[i]) != qemu_tolower(suffix[i])) {
            return false;
        }
    }
    return true;
}

static int cci_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    if (buf_size >= 4 &&
        ((uint32_t)buf[0] | (buf[1] << 8) | (buf[2] << 16) |
         ((uint32_t)buf[3] << 24)) == CCI_MAGIC) {
        return 200;
    }
    if (filename) {
        char *base = g_path_get_basename(filename);
        bool match = cci_filename_has_suffix_ci(base, ".cci");
        g_free(base);
        if (match) {
            return 100;
        }
    }
    return 0;
}

static BlockDriver bdrv_cci = {
    .format_name          = "cci",
    .instance_size        = sizeof(BDRVCciState),
    .is_format            = true,
    .bdrv_probe           = cci_probe,
    .bdrv_open            = cci_open,
    .bdrv_close           = cci_close,
    .bdrv_child_perm      = cci_child_perm,
    .bdrv_co_preadv       = cci_co_preadv,
    .bdrv_co_pwritev      = cci_co_pwritev,
    .bdrv_co_block_status = cci_co_block_status,
    .bdrv_co_getlength    = cci_co_getlength,
    .bdrv_refresh_limits  = cci_refresh_limits,
};

static void bdrv_cci_init(void)
{
    bdrv_register(&bdrv_cci);
}

block_init(bdrv_cci_init);
