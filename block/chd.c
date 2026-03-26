/*
 * Block driver for MAME CHD CD-ROM images (via libchdr).
 *
 * Copyright (c) 2025
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
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "block/block-io.h"
#include "block/block_int.h"
#include "qemu/module.h"
#include "qemu/error-report.h"

#include <libchdr/chd.h>
#include <libchdr/cdrom.h>

#define CHD_TRACK_PADDING 4
#define CD_SYNC_SIZE 12
#define CD_HEADER_SIZE 4
#define CD_MODE1_DATA_OFFSET (CD_SYNC_SIZE + CD_HEADER_SIZE)

typedef struct ChdTrack {
    uint32_t start_fad;
    uint32_t end_fad;
    int32_t chd_offset;
    uint32_t sector_size;
} ChdTrack;

typedef struct BDRVChdState {
    CoMutex lock;
    chd_file *chd;
    uint8_t *hunk_buf;
    uint32_t hunkbytes;
    uint32_t sph; /* frames per hunk */
    int32_t cur_hunk;
    ChdTrack *tracks;
    unsigned int num_tracks;
    uint32_t total_2048_sectors;
    uint8_t sector_cache[2048];
    uint32_t cache_lba;
} BDRVChdState;

/*
 * libchdr opens by host path (stdio). The file child is a protocol BDS; its
 * path lives in filename/exact_filename, not backing_file (which
 * bdrv_get_full_backing_filename() uses).
 */
static char *chd_dup_host_path(BlockDriverState *file_bs, Error **errp)
{
    bdrv_refresh_filename(file_bs);

    if (file_bs->exact_filename[0]) {
        return g_strdup(file_bs->exact_filename);
    }
    if (file_bs->filename[0] && !strstart(file_bs->filename, "json:", NULL)) {
        return g_strdup(file_bs->filename);
    }

    error_setg(errp,
               "CHD needs a plain local file path for libchdr "
               "(file node has no resolvable path)");
    return NULL;
}

static uint32_t chd_track_sector_size(const char *type)
{
    if (!strcmp(type, "MODE1") || !strcmp(type, "MODE1/2048")) {
        return 2048;
    }
    if (!strcmp(type, "MODE1_RAW") || !strcmp(type, "MODE1/2352")) {
        return 2352;
    }
    return 0;
}

static void chd_extract_cooked2048(const uint8_t *frame, uint32_t sector_size,
                                   uint8_t *out)
{
    if (sector_size == 2048) {
        memcpy(out, frame, 2048);
    } else if (sector_size == 2352) {
        memcpy(out, frame + CD_MODE1_DATA_OFFSET, 2048);
    } else {
        memset(out, 0, 2048);
    }
}

static ChdTrack *chd_find_track(BDRVChdState *s, uint32_t fad)
{
    unsigned i;

    for (i = 0; i < s->num_tracks; i++) {
        if (fad >= s->tracks[i].start_fad && fad <= s->tracks[i].end_fad) {
            return &s->tracks[i];
        }
    }
    return NULL;
}

static int chd_read_lba(BDRVChdState *s, uint32_t lba, uint8_t *out,
                        Error **errp)
{
    uint32_t fad = lba + 150;
    ChdTrack *tr = chd_find_track(s, fad);
    uint32_t fad_offs, hunk, hunk_ofs;
    chd_error cerr;
    const uint8_t *frame;

    if (!tr) {
        error_setg(errp, "CHD: LBA %" PRIu32 " out of range", lba);
        return -EIO;
    }
    if (!tr->sector_size) {
        error_setg(errp, "CHD: unsupported track type at LBA %" PRIu32, lba);
        return -ENOTSUP;
    }

    fad_offs = fad + tr->chd_offset;
    hunk = fad_offs / s->sph;
    hunk_ofs = fad_offs % s->sph;

    if (hunk != s->cur_hunk) {
        cerr = chd_read(s->chd, hunk, s->hunk_buf);
        if (cerr != CHDERR_NONE) {
            error_setg(errp, "CHD read failed: %s", chd_error_string(cerr));
            return -EIO;
        }
        s->cur_hunk = hunk;
    }

    frame = s->hunk_buf + (uint64_t)hunk_ofs * CD_FRAME_SIZE;
    chd_extract_cooked2048(frame, tr->sector_size, out);
    return 0;
}

static int chd_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    if (buf_size < 8 || memcmp(buf, "MComprHD", 8)) {
        return 0;
    }
    if (filename) {
        size_t len = strlen(filename);
        if (len >= 4) {
            const char *s = filename + len - 4;
            if (g_ascii_strcasecmp(s, ".chd") == 0) {
                return 100;
            }
        }
    }
    return 10;
}

static int chd_bdrv_open(BlockDriverState *bs, QDict *options, int flags,
                         Error **errp)
{
    BDRVChdState *s = bs->opaque;
    char *path = NULL;
    chd_error cerr;
    uint32_t total_frames = 150;
    uint32_t chd_frame_offset = 0;
    int ret;

    GLOBAL_STATE_CODE();

    bdrv_graph_rdlock_main_loop();
    ret = bdrv_apply_auto_read_only(bs, NULL, errp);
    bdrv_graph_rdunlock_main_loop();
    if (ret < 0) {
        return ret;
    }

    ret = bdrv_open_file_child(NULL, options, "file", bs, errp);
    if (ret < 0) {
        return ret;
    }

    GRAPH_RDLOCK_GUARD_MAINLOOP();

    path = chd_dup_host_path(bs->file->bs, errp);
    if (!path) {
        if (errp && *errp) {
            error_report("CHD: %s", error_get_pretty(*errp));
        } else {
            error_report("CHD: could not resolve host path for libchdr");
        }
        return -EINVAL;
    }

    cerr = chd_open(path, CHD_OPEN_READ, NULL, &s->chd);
    g_free(path);
    path = NULL;

    if (cerr != CHDERR_NONE) {
        error_report("CHD: libchdr open failed: %s", chd_error_string(cerr));
        error_setg(errp, "Could not open CHD: %s", chd_error_string(cerr));
        return -EINVAL;
    }

    {
        const chd_header *hdr = chd_get_header(s->chd);
        char temp[512];
        uint32_t temp_len, tag;
        uint8_t metaflags;

        s->hunkbytes = hdr->hunkbytes;
        if (s->hunkbytes == 0 || s->hunkbytes % CD_FRAME_SIZE) {
            error_setg(errp, "Invalid CHD hunk size for CD-ROM");
            ret = -EINVAL;
            goto fail;
        }
        s->sph = s->hunkbytes / CD_FRAME_SIZE;
        s->hunk_buf = g_malloc(s->hunkbytes);
        s->cur_hunk = -1;

        for (;;) {
            char type[32], subtype[32], pgtype[32], pgsub[32];
            int tkid = -1, frames = 0, pregap = 0, postgap = 0, padframes = 0;
            ChdTrack tr;
            uint32_t sector_size;
            int padded;
            int nscan;

            cerr = chd_get_metadata(s->chd, CDROM_TRACK_METADATA2_TAG,
                                    s->num_tracks, temp, sizeof(temp),
                                    &temp_len, &tag, &metaflags);
            if (cerr == CHDERR_NONE) {
                nscan = sscanf(temp, CDROM_TRACK_METADATA2_FORMAT, &tkid,
                               type, subtype, &frames, &pregap, pgtype, pgsub,
                               &postgap);
                if (nscan < 4) {
                    error_setg(errp, "Invalid CHDv2 CD track metadata");
                    ret = -EINVAL;
                    goto fail;
                }
            } else if ((cerr = chd_get_metadata(s->chd, CDROM_TRACK_METADATA_TAG,
                                               s->num_tracks, temp,
                                               sizeof(temp), &temp_len, &tag,
                                               &metaflags)) == CHDERR_NONE) {
                pregap = postgap = 0;
                nscan = sscanf(temp, CDROM_TRACK_METADATA_FORMAT, &tkid, type,
                               subtype, &frames);
                if (nscan < 4) {
                    error_setg(errp, "Invalid CD track metadata");
                    ret = -EINVAL;
                    goto fail;
                }
            } else {
                break;
            }

            if (strcmp(subtype, "NONE") != 0 || pregap != 0 || postgap != 0) {
                error_setg(errp,
                           "CHD track pregap/postgap/subtypes are not supported");
                ret = -ENOTSUP;
                goto fail;
            }
            if (tkid != (int)s->num_tracks + 1) {
                error_setg(errp, "Unexpected CHD track numbering");
                ret = -EINVAL;
                goto fail;
            }

            sector_size = chd_track_sector_size(type);
            if (!strcmp(type, "AUDIO")) {
                error_setg(errp, "CHD audio tracks are not supported for DVD-ROM");
                ret = -ENOTSUP;
                goto fail;
            }
            if (sector_size == 0) {
                error_setg(errp, "Unsupported CHD track type '%s'", type);
                ret = -ENOTSUP;
                goto fail;
            }

            tr.start_fad = total_frames;
            total_frames += frames;
            tr.end_fad = total_frames - 1 - padframes;
            tr.chd_offset = (int32_t)chd_frame_offset - (int32_t)tr.start_fad;
            tr.sector_size = sector_size;

            s->tracks = g_renew(ChdTrack, s->tracks, s->num_tracks + 1);
            s->tracks[s->num_tracks] = tr;
            s->num_tracks++;

            padded = (frames + CHD_TRACK_PADDING - 1) / CHD_TRACK_PADDING;
            chd_frame_offset += padded * CHD_TRACK_PADDING;
        }

        if (s->num_tracks == 0) {
            error_setg(errp, "Not a CD-ROM CHD (no tracks)");
            ret = -EINVAL;
            goto fail;
        }

        s->total_2048_sectors = total_frames - 150;
        if (s->total_2048_sectors == 0) {
            error_setg(errp, "Empty CHD image");
            ret = -EINVAL;
            goto fail;
        }

        bs->total_sectors = (int64_t)s->total_2048_sectors * 4;
    }

    s->cache_lba = UINT32_MAX;
    qemu_co_mutex_init(&s->lock);
    return 0;

fail:
    g_free(s->hunk_buf);
    s->hunk_buf = NULL;
    g_free(s->tracks);
    s->tracks = NULL;
    s->num_tracks = 0;
    if (s->chd) {
        chd_close(s->chd);
        s->chd = NULL;
    }
    return ret;
}

static void chd_refresh_limits(BlockDriverState *bs, Error **errp)
{
    bs->bl.request_alignment = BDRV_SECTOR_SIZE;
}

static int coroutine_fn GRAPH_RDLOCK
chd_co_preadv(BlockDriverState *bs, int64_t offset, int64_t bytes,
              QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    BDRVChdState *s = bs->opaque;
    int ret = 0;
    uint64_t sector_num;
    int nb_sectors;
    int i;

    assert(QEMU_IS_ALIGNED(offset, BDRV_SECTOR_SIZE));
    assert(QEMU_IS_ALIGNED(bytes, BDRV_SECTOR_SIZE));

    sector_num = offset >> BDRV_SECTOR_BITS;
    nb_sectors = bytes >> BDRV_SECTOR_BITS;

    qemu_co_mutex_lock(&s->lock);

    for (i = 0; i < nb_sectors; i++) {
        uint64_t abs_sec = sector_num + i;
        uint32_t lba = abs_sec / 4;
        uint32_t slice = abs_sec % 4;
        Error *local_err = NULL;

        if (lba >= s->total_2048_sectors) {
            ret = -EIO;
            goto out;
        }

        if (s->cache_lba != lba) {
            if (chd_read_lba(s, lba, s->sector_cache, &local_err) < 0) {
                ret = -EIO;
                error_report_err(local_err);
                goto out;
            }
            s->cache_lba = lba;
        }

        qemu_iovec_from_buf(qiov, (size_t)i << BDRV_SECTOR_BITS,
                            s->sector_cache + slice * BDRV_SECTOR_SIZE,
                            BDRV_SECTOR_SIZE);
    }

out:
    qemu_co_mutex_unlock(&s->lock);
    return ret;
}

static void chd_bdrv_close(BlockDriverState *bs)
{
    BDRVChdState *s = bs->opaque;

    g_free(s->hunk_buf);
    g_free(s->tracks);
    if (s->chd) {
        chd_close(s->chd);
    }
}

static BlockDriver bdrv_chd = {
    .format_name            = "chd",
    .instance_size          = sizeof(BDRVChdState),
    .bdrv_probe             = chd_probe,
    .bdrv_open              = chd_bdrv_open,
    .bdrv_child_perm        = bdrv_default_perms,
    .bdrv_refresh_limits    = chd_refresh_limits,
    .bdrv_co_preadv         = chd_co_preadv,
    .bdrv_close             = chd_bdrv_close,
    .is_format              = true,
};

static void bdrv_chd_init(void)
{
    bdrv_register(&bdrv_chd);
}

block_init(bdrv_chd_init);
