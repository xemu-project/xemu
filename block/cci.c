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
 * Multi-slice: each .N.cci file has its own header; global LBAs are contiguous
 * (slice 0: 0..S0-1, slice 1: S0.., etc.).
 */

#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "qemu/ctype.h"
#include "block/block_int.h"
#include "block/block-io.h"
#include "block/qdict.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "qemu/memalign.h"
#include "qemu/iov.h"
#include "lz4.h"
#include <glib.h>

#define CCI_LOGICAL_SECTOR_SIZE 2048
#define CCI_MAGIC_LE 0x4D494343u
#define CCI_HDR_SIZE 32u
#define CCI_MAX_PARTS 8
#define CCI_MAX_EXTRA (CCI_MAX_PARTS - 1)

typedef struct CCIPart {
    BdrvChild *child;
    uint64_t start_sector;
    uint64_t end_sector;
    uint32_t *index;
    uint32_t n_index;
} CCIPart;

typedef struct BDRVCciState {
    char *source_file_path;
    CCIPart part[CCI_MAX_PARTS];
    int n_parts;
    uint64_t total_sectors;
} BDRVCciState;

static void cci_part_clear(BlockDriverState *bs, CCIPart *p)
{
    g_free(p->index);
    p->index = NULL;
    p->n_index = 0;
    p->start_sector = 0;
    p->end_sector = 0;
    if (p->child && p->child != bs->file) {
        bdrv_unref_child(bs, p->child);
    }
    p->child = NULL;
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

static int cci_load_part(BlockDriverState *bs, BdrvChild *ch, CCIPart *p,
                         uint64_t cum_sectors, Error **errp)
{
    uint8_t hdr[32];
    uint64_t uncompressed_size, index_offset;
    uint32_t magic, hdr_size, block_size;
    uint8_t version, index_align;
    uint64_t sectors;
    uint32_t entries;
    int ret;
    size_t i;

    ret = bdrv_pread(ch, 0, sizeof(hdr), hdr, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "CCI: could not read header");
        return ret;
    }

    magic = ldl_le_p(hdr);
    hdr_size = ldl_le_p(hdr + 4);
    uncompressed_size = ldq_le_p(hdr + 8);
    index_offset = ldq_le_p(hdr + 16);
    block_size = ldl_le_p(hdr + 24);
    version = hdr[28];
    index_align = hdr[29];

    if (magic != CCI_MAGIC_LE) {
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
        error_setg(errp, "CCI: zero sectors");
        return -EINVAL;
    }
    if (cum_sectors > UINT64_MAX - sectors) {
        error_setg(errp, "CCI: total sector count overflow");
        return -EINVAL;
    }

    entries = (uint32_t)sectors + 1;
    if (entries < 2) {
        return -EINVAL;
    }

    p->index = g_try_new(uint32_t, entries);
    if (!p->index) {
        return -ENOMEM;
    }
    p->n_index = entries;

    ret = bdrv_pread(ch, (int64_t)index_offset, (int64_t)entries * 4,
                     p->index, 0);
    if (ret < 0) {
        error_setg_errno(errp, -ret, "CCI: could not read index");
        goto fail;
    }

    for (i = 0; i < entries; i++) {
        p->index[i] = ldl_le_p((uint8_t *)&p->index[i]);
    }

    p->child = ch;
    p->start_sector = cum_sectors;
    p->end_sector = cum_sectors + sectors - 1;
    return 0;

fail:
    g_free(p->index);
    p->index = NULL;
    p->n_index = 0;
    return ret;
}

static int coroutine_fn cci_read_one_sector(CCIPart *p, uint64_t local_sector,
                                            uint8_t *out)
{
    uint32_t e0, e1;
    uint64_t pos0, pos1;
    bool lz4f;
    uint32_t span;
    int ret;

    if (local_sector + 1 >= p->n_index) {
        return -EIO;
    }

    e0 = p->index[local_sector];
    e1 = p->index[local_sector + 1];
    pos0 = (uint64_t)(e0 & 0x7fffffffu) << 2;
    pos1 = (uint64_t)(e1 & 0x7fffffffu) << 2;
    lz4f = (e0 & 0x80000000u) != 0;

    if (pos1 < pos0 || pos1 - pos0 > 32 * 1024 * 1024) {
        return -EIO;
    }
    span = (uint32_t)(pos1 - pos0);

    if (span == CCI_LOGICAL_SECTOR_SIZE && !lz4f) {
        ret = bdrv_co_pread(p->child, (int64_t)pos0,
                            CCI_LOGICAL_SECTOR_SIZE, out, 0);
        return ret < 0 ? ret : 0;
    }

    {
        uint8_t *buf = qemu_try_blockalign(p->child->bs, span);

        if (!buf) {
            return -ENOMEM;
        }
        ret = bdrv_co_pread(p->child, (int64_t)pos0, span, buf, 0);
        if (ret < 0) {
            qemu_vfree(buf);
            return ret;
        }

        if (span < 2) {
            qemu_vfree(buf);
            return -EIO;
        }

        {
            unsigned int pad = buf[0];
            int comp_len = (int)span - (int)pad - 1;

            if (comp_len < 1 || comp_len > (int)span) {
                qemu_vfree(buf);
                return -EIO;
            }
            ret = LZ4_decompress_safe((const char *)buf + 1 + pad,
                                      (char *)out, comp_len,
                                      CCI_LOGICAL_SECTOR_SIZE);
            qemu_vfree(buf);
            if (ret != CCI_LOGICAL_SECTOR_SIZE) {
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
    int pi;

    memset(out, 0, CCI_LOGICAL_SECTOR_SIZE);
    if (lba >= s->total_sectors) {
        return 0;
    }

    for (pi = 0; pi < s->n_parts; pi++) {
        CCIPart *p = &s->part[pi];
        if (lba >= p->start_sector && lba <= p->end_sector) {
            return cci_read_one_sector(p, lba - p->start_sector, out);
        }
    }
    return -EIO;
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
        return -EINVAL;
    }
    if (bytes == 0) {
        return 0;
    }

    end = offset + bytes;
    if (end > bdrv_co_getlength(bs)) {
        return -EINVAL;
    }

    first_lba = (uint64_t)offset / CCI_LOGICAL_SECTOR_SIZE;
    last_lba = (uint64_t)(end - 1) / CCI_LOGICAL_SECTOR_SIZE;

    secbuf = qemu_try_blockalign(bs, CCI_LOGICAL_SECTOR_SIZE);
    if (!secbuf) {
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
    const char *extra;
    int ret, ej;
    uint64_t cum_sec = 0;
    char **lines = NULL;

    GLOBAL_STATE_CODE();

    memset(s, 0, sizeof(*s));

    bdrv_open_child(NULL, options, "file", bs, &child_of_bds,
                    BDRV_CHILD_DATA | BDRV_CHILD_PRIMARY, false, errp);
    if (!bs->file) {
        return -EINVAL;
    }

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    ret = cci_load_part(bs, bs->file, &s->part[0], cum_sec, errp);
    if (ret < 0) {
        return ret;
    }
    cum_sec = s->part[0].end_sector + 1;
    s->n_parts = 1;

    bdrv_refresh_filename(bs->file->bs);

    extra = qdict_get_try_str(options, "cci-extra-parts");
    if (extra && *extra) {
        lines = g_strsplit(extra, "\n", 0);
        int li;

        for (li = 0; lines[li]; li++) {
            BlockDriverState *cbs;
            char cname[32];
            BdrvChild *ch;
            int sn;

            if (!*lines[li]) {
                continue;
            }
            if (s->n_parts >= CCI_MAX_PARTS) {
                error_setg(errp, "Too many CCI parts (max %d)", CCI_MAX_PARTS);
                ret = -EINVAL;
                goto fail_parts;
            }

            cbs = bdrv_open(lines[li], NULL, NULL, BDRV_O_AUTO_RDONLY, errp);
            if (!cbs) {
                ret = -EIO;
                goto fail_parts;
            }

            bdrv_ref(cbs);
            sn = snprintf(cname, sizeof(cname), "cci-part-%d", s->n_parts + 1);
            if (sn < 0 || sn >= (int)sizeof(cname)) {
                bdrv_unref(cbs);
                error_setg(errp, "CCI child name overflow");
                ret = -EINVAL;
                goto fail_parts;
            }

            ch = bdrv_attach_child(bs, cbs, cname, &child_of_bds,
                                   BDRV_CHILD_DATA, errp);
            bdrv_unref(cbs);
            if (!ch) {
                ret = -EINVAL;
                goto fail_parts;
            }

            ret = cci_load_part(bs, ch, &s->part[s->n_parts], cum_sec, errp);
            if (ret < 0) {
                bdrv_unref_child(bs, ch);
                goto fail_parts;
            }
            cum_sec = s->part[s->n_parts].end_sector + 1;
            s->n_parts++;
        }
        g_strfreev(lines);
        lines = NULL;
    }

    s->total_sectors = cum_sec;

    {
        GString *src = g_string_new(bs->file->bs->filename);

        for (ej = 1; ej < s->n_parts; ej++) {
            bdrv_refresh_filename(s->part[ej].child->bs);
            g_string_append_c(src, ';');
            g_string_append(src, s->part[ej].child->bs->filename);
        }
        s->source_file_path = g_string_free(src, FALSE);
    }

    bs->sg = false;
    bs->supported_write_flags = BDRV_REQ_WRITE_UNCHANGED;
    bs->supported_zero_flags = BDRV_REQ_WRITE_UNCHANGED;

    return 0;

fail_parts:
    g_strfreev(lines);
    while (s->n_parts > 1) {
        s->n_parts--;
        cci_part_clear(bs, &s->part[s->n_parts]);
    }
    cci_part_clear(bs, &s->part[0]);
    s->total_sectors = 0;
    return ret;
}

static void GRAPH_RDLOCK cci_refresh_limits(BlockDriverState *bs, Error **errp)
{
    bs->bl.has_variable_length = false;
}

static void cci_close(BlockDriverState *bs)
{
    BDRVCciState *s = bs->opaque;
    int i;

    for (i = s->n_parts - 1; i >= 0; i--) {
        cci_part_clear(bs, &s->part[i]);
    }
    s->n_parts = 0;
    g_free(s->source_file_path);
    s->source_file_path = NULL;
    s->total_sectors = 0;
}

static int cci_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    const char *base;

    if (buf_size >= 4 && ldl_le_p(buf) == CCI_MAGIC_LE) {
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

static const char *const cci_strong_runtime_opts[] = {
    "cci-extra-parts",
    NULL
};

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
    .strong_runtime_opts    = cci_strong_runtime_opts,
};

static void bdrv_cci_init(void)
{
    bdrv_register(&bdrv_cci);
}

block_init(bdrv_cci_init);
