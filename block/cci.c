/*
 * CCI Xbox disc container (xemu-cci)
 *
 * Format per Team-Resurgent XboxToolkit:
 *   https://github.com/Team-Resurgent/XboxToolkit/blob/main/XboxToolkit/CCIContainerReader.cs
 *
 * Header (32 bytes LE): magic "CCIM" (0x4D494343), hdr_size 32, uncompressed_size,
 * index_offset, block_size 2048, version 1, index_alignment 2.
 * Sectors = uncompressed_size / 2048; index has sectors+1 uint32 entries:
 *   position = (entry & 0x7FFFFFFF) << alignment; LZ4 = entry & 0x80000000.
 * Compressed sector blob (matches XboxToolkit CCISectorDecoder): byte0 = trailer
 * padding length; bytes [1 .. span-1-trailer] are one LZ4 block (not raw bytes
 * skipped after byte0 — K4os.Decode uses offset 0, length size-(pad+1)).
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "qemu/ctype.h"
#include "block/block_int.h"
#include "block/block-io.h"
#include "block/qdict.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/module.h"
#include "qemu/memalign.h"
#include "qemu/iov.h"
#include "lz4.h"
#include <glib.h>

#define CCI_LOGICAL_SECTOR_SIZE 2048
#define CCI_MAGIC_LE 0x4D494343u
#define CCI_HDR_SIZE 32u

/* Set XEMU_CCI_DEBUG=1 for probe + per-sector read logs (very noisy). */
static bool cci_verbose(void)
{
    static int inited, on;

    if (!inited) {
        const char *e = getenv("XEMU_CCI_DEBUG");

        on = e && e[0] && e[0] != '0';
        inited = 1;
    }
    return on;
}

#define cci_log(...) error_report("cci: " __VA_ARGS__)

typedef struct BDRVCciState {
    uint32_t *index;
    uint32_t n_index;
    uint64_t total_sectors;
} BDRVCciState;

static void cci_clear(BDRVCciState *s)
{
    g_free(s->index);
    s->index = NULL;
    s->n_index = 0;
    s->total_sectors = 0;
}

static bool cci_suffix_ci(const char *name, const char *suf)
{
    size_t nl = strlen(name), sl = strlen(suf);
    size_t i;

    if (nl < sl) {
        return false;
    }
    name += nl - sl;
    for (i = 0; i < sl; i++) {
        if (qemu_tolower((unsigned char)name[i]) !=
            qemu_tolower((unsigned char)suf[i])) {
            return false;
        }
    }
    return true;
}

static int cci_load(BlockDriverState *bs, Error **errp)
{
    BDRVCciState *s = bs->opaque;
    BdrvChild *ch = bs->file;
    uint8_t hdr[32];
    uint64_t uncompressed_size, index_offset;
    uint32_t magic, hdr_size, block_size;
    uint8_t version, index_align;
    uint64_t sectors;
    uint32_t entries;
    int ret;
    size_t i;

    cci_log("load: begin (backing=%s)",
            ch && ch->bs ? (ch->bs->filename ?: "?") : "(no child)");

    ret = bdrv_pread(ch, 0, sizeof(hdr), hdr, 0);
    if (ret < 0) {
        cci_log("load: header pread failed ret=%d", ret);
        error_setg_errno(errp, -ret, "CCI: could not read header");
        return ret;
    }

    cci_log("load: header read ok (%zu bytes)", sizeof(hdr));

    magic = ldl_le_p(hdr);
    hdr_size = ldl_le_p(hdr + 4);
    uncompressed_size = ldq_le_p(hdr + 8);
    index_offset = ldq_le_p(hdr + 16);
    block_size = ldl_le_p(hdr + 24);
    version = hdr[28];
    index_align = hdr[29];

    cci_log("load: parsed magic=0x%" PRIx32 " hdr_size=%" PRIu32
            " uncompressed_size=%" PRIu64 " index_offset=%" PRIu64
            " block_size=%" PRIu32 " version=%u index_align=%u",
            magic, hdr_size, uncompressed_size, index_offset,
            block_size, version, index_align);

    if (magic != CCI_MAGIC_LE) {
        cci_log("load: bad magic");
        error_setg(errp, "CCI: invalid magic (expected CCIM container)");
        return -EINVAL;
    }
    if (hdr_size != CCI_HDR_SIZE) {
        error_setg(errp, "CCI: invalid header size %" PRIu32, hdr_size);
        return -EINVAL;
    }
    if (block_size != CCI_LOGICAL_SECTOR_SIZE) {
        error_setg(errp, "CCI: unsupported block size %" PRIu32, block_size);
        return -EINVAL;
    }
    if (version != 1) {
        error_setg(errp, "CCI: unsupported version %u", version);
        return -EINVAL;
    }
    if (index_align != 2) {
        error_setg(errp, "CCI: unsupported index alignment %u", index_align);
        return -EINVAL;
    }

    if (uncompressed_size > UINT64_MAX - CCI_LOGICAL_SECTOR_SIZE ||
        uncompressed_size / CCI_LOGICAL_SECTOR_SIZE > UINT32_MAX) {
        error_setg(errp, "CCI: uncompressed size overflow");
        return -EINVAL;
    }

    sectors = uncompressed_size / CCI_LOGICAL_SECTOR_SIZE;
    if (sectors == 0) {
        cci_log("load: zero sectors");
        error_setg(errp, "CCI: zero sectors");
        return -EINVAL;
    }

    entries = (uint32_t)sectors + 1;
    if (entries < 2) {
        cci_log("load: entries < 2");
        return -EINVAL;
    }

    cci_log("load: sectors=%" PRIu64 " index_entries=%" PRIu32
            " index_bytes=%" PRIu64,
            sectors, entries, (uint64_t)entries * 4u);

    s->index = g_try_new(uint32_t, entries);
    if (!s->index) {
        cci_log("load: OOM allocating index");
        return -ENOMEM;
    }
    s->n_index = entries;

    ret = bdrv_pread(ch, (int64_t)index_offset, (int64_t)entries * 4,
                     s->index, 0);
    if (ret < 0) {
        cci_log("load: index pread failed ret=%d", ret);
        error_setg_errno(errp, -ret, "CCI: could not read index");
        goto fail;
    }

    for (i = 0; i < entries; i++) {
        s->index[i] = ldl_le_p((uint8_t *)&s->index[i]);
    }

    cci_log("load: done total_sectors=%" PRIu64
            " index[0]=0x%" PRIx32 " index[last]=0x%" PRIx32,
            sectors, s->index[0], s->index[entries - 1]);

    s->total_sectors = sectors;
    return 0;

fail:
    g_free(s->index);
    s->index = NULL;
    s->n_index = 0;
    return ret;
}

static int coroutine_fn cci_read_one_sector(BdrvChild *ch, uint32_t *index,
                                            uint32_t n_index, uint64_t lba,
                                            uint8_t *out)
{
    uint32_t e0, e1;
    uint64_t pos0, pos1;
    bool lz4f;
    uint32_t span;
    int ret;

    if (lba + 1 >= n_index) {
        cci_log("read_sector: lba %" PRIu64 " out of index (n_index=%" PRIu32 ")",
                lba, n_index);
        return -EIO;
    }

    e0 = index[lba];
    e1 = index[lba + 1];
    pos0 = (uint64_t)(e0 & 0x7fffffffu) << 2;
    pos1 = (uint64_t)(e1 & 0x7fffffffu) << 2;
    lz4f = (e0 & 0x80000000u) != 0;

    if (pos1 < pos0 || pos1 - pos0 > 32 * 1024 * 1024) {
        cci_log("read_sector: lba %" PRIu64 " bad span pos0=%" PRIu64
                " pos1=%" PRIu64 " e0=0x%" PRIx32 " e1=0x%" PRIx32,
                lba, pos0, pos1, e0, e1);
        return -EIO;
    }
    span = (uint32_t)(pos1 - pos0);

    if (cci_verbose()) {
        cci_log("read_sector: lba=%" PRIu64 " raw=%s span=%" PRIu32
                " file_off=%" PRIu64 "-%" PRIu64,
                lba, lz4f ? "lz4" : "raw", span, pos0, pos1);
    }

    if (span == CCI_LOGICAL_SECTOR_SIZE && !lz4f) {
        ret = bdrv_co_pread(ch, (int64_t)pos0,
                            CCI_LOGICAL_SECTOR_SIZE, out, 0);
        if (ret < 0 && cci_verbose()) {
            cci_log("read_sector: raw pread lba=%" PRIu64 " ret=%d", lba, ret);
        }
        return ret < 0 ? ret : 0;
    }

    {
        uint8_t *buf = qemu_try_blockalign(ch->bs, span);

        if (!buf) {
            cci_log("read_sector: lba=%" PRIu64 " OOM align %" PRIu32,
                    lba, span);
            return -ENOMEM;
        }
        ret = bdrv_co_pread(ch, (int64_t)pos0, span, buf, 0);
        if (ret < 0) {
            cci_log("read_sector: compressed pread lba=%" PRIu64
                    " span=%" PRIu32 " ret=%d",
                    lba, span, ret);
            qemu_vfree(buf);
            return ret;
        }

        if (span < 2) {
            cci_log("read_sector: lba=%" PRIu64 " span=%" PRIu32 " too small",
                    lba, span);
            qemu_vfree(buf);
            return -EIO;
        }

        {
            /*
             * First byte is a trailer-pad count: the (span-1) bytes after it end
             * with @trail bytes that are not part of the LZ4 block (see
             * Team-Resurgent XboxToolkit CCISectorDecoder).
             */
            unsigned int trail = buf[0];
            int comp_len = (int)span - 1 - (int)trail;

            if (trail >= span - 1 || comp_len < 1 ||
                comp_len > (int)span - 1) {
                cci_log("read_sector: lba=%" PRIu64 " bad trail=%u span=%"
                        PRIu32 " comp_len=%d",
                        lba, trail, span, comp_len);
                qemu_vfree(buf);
                return -EIO;
            }
            if (cci_verbose()) {
                cci_log("read_sector: LZ4 lba=%" PRIu64 " comp_len=%d trail=%u",
                        lba, comp_len, trail);
            }
            ret = LZ4_decompress_safe((const char *)buf + 1,
                                      (char *)out, comp_len,
                                      CCI_LOGICAL_SECTOR_SIZE);
            qemu_vfree(buf);
            if (ret != CCI_LOGICAL_SECTOR_SIZE) {
                cci_log("read_sector: LZ4 lba=%" PRIu64 " got %d expected %d",
                        lba, ret, CCI_LOGICAL_SECTOR_SIZE);
                return ret < 0 ? ret : -EIO;
            }
        }
    }
    return 0;
}

static int coroutine_fn cci_decode_sector(BlockDriverState *bs, uint64_t lba,
                                          uint8_t *out)
{
    BDRVCciState *s = bs->opaque;

    memset(out, 0, CCI_LOGICAL_SECTOR_SIZE);
    if (lba >= s->total_sectors) {
        return 0;
    }

    return cci_read_one_sector(bs->file, s->index, s->n_index, lba, out);
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_preadv(BlockDriverState *bs, int64_t offset, int64_t bytes,
              QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    int ret = 0;
    uint8_t *secbuf = NULL;
    int64_t end;
    uint64_t first_lba, last_lba, lba;

    if (bytes < 0 || offset < 0) {
        cci_log("preadv: bad args offset=%" PRId64 " bytes=%" PRId64,
                offset, bytes);
        return -EINVAL;
    }
    if (bytes == 0) {
        return 0;
    }

    end = offset + bytes;
    if (end > bdrv_co_getlength(bs)) {
        cci_log("preadv: past EOF offset=%" PRId64 " bytes=%" PRId64
                " end=%" PRId64 " len=%" PRId64,
                offset, bytes, end, bdrv_co_getlength(bs));
        return -EINVAL;
    }

    first_lba = (uint64_t)offset / CCI_LOGICAL_SECTOR_SIZE;
    last_lba = (uint64_t)(end - 1) / CCI_LOGICAL_SECTOR_SIZE;

    {
        static unsigned preadv_count;

        if (preadv_count < 16u) {
            cci_log("preadv[%u]: offset=%" PRId64 " bytes=%" PRId64
                    " lba %" PRIu64 "-%" PRIu64,
                    preadv_count, offset, bytes, first_lba, last_lba);
            preadv_count++;
        } else if (cci_verbose()) {
            cci_log("preadv: offset=%" PRId64 " bytes=%" PRId64
                    " lba %" PRIu64 "-%" PRIu64,
                    offset, bytes, first_lba, last_lba);
        }
    }

    secbuf = qemu_try_blockalign(bs, CCI_LOGICAL_SECTOR_SIZE);
    if (!secbuf) {
        cci_log("preadv: OOM secbuf");
        return -ENOMEM;
    }

    for (lba = first_lba; lba <= last_lba; lba++) {
        int64_t sec_start = (int64_t)lba * CCI_LOGICAL_SECTOR_SIZE;
        int64_t copy_start = MAX(sec_start, offset);
        int64_t copy_end = MIN(sec_start + CCI_LOGICAL_SECTOR_SIZE, end);
        int64_t chunk = copy_end - copy_start;
        size_t sec_off = copy_start - sec_start;

        ret = cci_decode_sector(bs, lba, secbuf);
        if (ret < 0) {
            cci_log("preadv: decode failed lba=%" PRIu64 " ret=%d",
                    lba, ret);
            goto out;
        }
        qemu_iovec_from_buf(qiov, (size_t)(copy_start - offset),
                            secbuf + sec_off, (size_t)chunk);
    }
out:
    qemu_vfree(secbuf);
    return ret;
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_block_status(BlockDriverState *bs, unsigned int mode,
                    int64_t offset, int64_t bytes, int64_t *pnum, int64_t *map,
                    BlockDriverState **file)
{
    *pnum = bytes;
    *map = -1;
    *file = NULL;
    /* Read-only compressed container: data present, no single host file offset */
    (void)mode;
    return BDRV_BLOCK_DATA;
}

static int64_t coroutine_fn GRAPH_RDLOCK
cci_co_getlength(BlockDriverState *bs)
{
    BDRVCciState *s = bs->opaque;

    if (s->total_sectors > INT64_MAX / CCI_LOGICAL_SECTOR_SIZE) {
        return INT64_MAX;
    }
    return (int64_t)s->total_sectors * CCI_LOGICAL_SECTOR_SIZE;
}

static int cci_open(BlockDriverState *bs, QDict *options, int flags_flag,
                    Error **errp)
{
    (void)flags_flag;
    BDRVCciState *s = bs->opaque;
    int ret;

    GLOBAL_STATE_CODE();

    cci_log("open: start");

    memset(s, 0, sizeof(*s));

    bdrv_open_child(NULL, options, "file", bs, &child_of_bds,
                    BDRV_CHILD_DATA | BDRV_CHILD_PRIMARY, false, errp);
    if (!bs->file) {
        cci_log("open: no file child");
        return -EINVAL;
    }

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    ret = cci_load(bs, errp);
    if (ret < 0) {
        cci_log("open: load failed ret=%d", ret);
        cci_clear(s);
        return ret;
    }

    bdrv_refresh_filename(bs->file->bs);
    cci_log("open: ok file=%s", bs->file->bs->filename ?: "?");

    bs->sg = false;
    bs->supported_write_flags = BDRV_REQ_WRITE_UNCHANGED;
    bs->supported_zero_flags = BDRV_REQ_WRITE_UNCHANGED;

    return 0;
}

static void GRAPH_RDLOCK cci_refresh_limits(BlockDriverState *bs, Error **errp)
{
    bs->bl.has_variable_length = false;
}

static void cci_close(BlockDriverState *bs)
{
    cci_log("close");
    cci_clear(bs->opaque);
}

static int cci_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    const char *base;

    if (buf_size >= 4 && ldl_le_p(buf) == CCI_MAGIC_LE) {
        if (cci_verbose()) {
            cci_log("probe: magic match score=200");
        }
        return 200;
    }
    if (filename) {
        base = strrchr(filename, '/');
        base = base ? base + 1 : filename;
#ifdef _WIN32
        {
            const char *b2 = strrchr(base, '\\');
            if (b2) {
                base = b2 + 1;
            }
        }
#endif
        if (cci_suffix_ci(base, ".cci")) {
            if (cci_verbose()) {
                cci_log("probe: .cci suffix score=100 (%s)", base);
            }
            return 100;
        }
    }
    return 0;
}

static void cci_child_perm(BlockDriverState *bs, BdrvChild *c,
                           BdrvChildRole role,
                           BlockReopenQueue *reopen_queue,
                           uint64_t parent_perm, uint64_t parent_shared,
                           uint64_t *nperm, uint64_t *nshared)
{
    bdrv_default_perms(bs, c, role, reopen_queue, parent_perm,
                       parent_shared, nperm, nshared);
    *nperm &= ~(BLK_PERM_WRITE | BLK_PERM_RESIZE);
    *nperm |= parent_perm & (BLK_PERM_WRITE | BLK_PERM_RESIZE);
}

static int coroutine_fn GRAPH_RDLOCK
cci_co_pwritev(BlockDriverState *bs, int64_t offset, int64_t bytes,
               QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    return -EROFS;
}

BlockDriver bdrv_cci = {
    .format_name            = "cci",
    .instance_size          = sizeof(BDRVCciState),
    .is_format              = true,
    .bdrv_probe             = cci_probe,
    .bdrv_open              = cci_open,
    .bdrv_close             = cci_close,
    .bdrv_child_perm        = cci_child_perm,
    .bdrv_co_preadv         = cci_co_preadv,
    .bdrv_co_pwritev        = cci_co_pwritev,
    .bdrv_co_block_status   = cci_co_block_status,
    .bdrv_co_getlength      = cci_co_getlength,
    .bdrv_refresh_limits    = cci_refresh_limits,
};

static void bdrv_cci_init(void)
{
    bdrv_register(&bdrv_cci);
}

block_init(bdrv_cci_init);
