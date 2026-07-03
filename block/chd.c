/*
 * QEMU Block driver for CHD images
 *
 * Copyright (c) 2026 JBW89
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 *
 * CHD is the MAME Compressed Hunks of Data format. This driver exposes a CHD
 * as a read-only DVD/raw block image and applies xemu's Xbox DVD
 * game-partition offset automatically when a Redump-style Xbox DVD image is
 * stored inside the CHD. MAME CD-CHDs use a track/frame layout and are not
 * treated as Xbox DVD images here.
 */
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "block/block-io.h"
#include "block/block_int.h"
#include "qemu/bswap.h"
#include "qemu/module.h"
#include "libchdr/chd.h"

#define CHD_MAGIC "MComprHD"
#define CHD_MAGIC_SIZE 8

#define XEMU_XDVDFS_SECTOR_SIZE 2048
#define XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR 32
#define XEMU_REDUMP_GAME_PARTITION_SECTOR 0x30600
#define XEMU_XDVDFS_MAGIC_OFFSET 0x7ec
#define XEMU_XDVDFS_ROOT_SECTOR_OFFSET 0x14
#define XEMU_XDVDFS_ROOT_SIZE_OFFSET 0x18
#define XEMU_XDVDFS_ENTRY_MIN_SIZE 14
#define XEMU_XDVDFS_ENTRY_NAME_OFFSET 14
#define XEMU_XDVDFS_ENTRY_OFFSET_SHIFT 2
#define XEMU_XDVDFS_ENTRY_MAX_DEPTH 1024
#define XEMU_XDVDFS_ENTRY_DIRECTORY 0x10
#define XEMU_XDVDFS_MAX_ROOT_DIRECTORY_SIZE (16 * 1024 * 1024)

static const char xemu_xdvdfs_magic[] = "MICROSOFT*XBOX*MEDIA";
static const char xemu_xdvdfs_default_xbe[] = "default.xbe";

typedef struct BDRVCHDFile {
    BdrvChild *child;
    int64_t pos;
    int64_t size;
} BDRVCHDFile;

typedef struct BDRVCHDState {
    CoMutex lock;
    BDRVCHDFile file;
    chd_file *chd;
    const chd_header *header;
    uint8_t *hunk;
    uint32_t current_hunk;
    uint64_t logical_bytes;
    uint64_t data_offset;
    uint64_t visible_bytes;
} BDRVCHDState;

static uint64_t chd_qemu_fsize(void *opaque)
{
    BDRVCHDFile *file = opaque;

    if (file->size < 0) {
        file->size = bdrv_getlength(file->child->bs);
    }

    return file->size < 0 ? UINT64_MAX : file->size;
}

static size_t chd_qemu_fread(void *ptr, size_t size, size_t nmemb,
                             void *opaque)
{
    BDRVCHDFile *file = opaque;
    size_t bytes;
    int ret;

    if (size != 0 && nmemb > SIZE_MAX / size) {
        return 0;
    }

    bytes = size * nmemb;
    if (bytes == 0) {
        return 0;
    }
    if (bytes > INT64_MAX || file->pos > INT64_MAX - (int64_t)bytes) {
        return 0;
    }

    ret = bdrv_pread(file->child, file->pos, (int64_t)bytes, ptr, 0);
    if (ret < 0) {
        return 0;
    }

    file->pos += (int64_t)bytes;
    return nmemb;
}

static int chd_qemu_fclose(void *opaque)
{
    (void)opaque;
    return 0;
}

static int chd_qemu_fseek(void *opaque, int64_t offset, int whence)
{
    BDRVCHDFile *file = opaque;
    int64_t base = 0;
    int64_t new_pos;
    uint64_t size;

    switch (whence) {
    case SEEK_SET:
        base = 0;
        break;
    case SEEK_CUR:
        base = file->pos;
        break;
    case SEEK_END:
        size = chd_qemu_fsize(file);
        if (size > INT64_MAX) {
            return -1;
        }
        base = size;
        break;
    default:
        return -1;
    }

    if ((offset > 0 && base > INT64_MAX - offset) ||
        (offset < 0 && base < INT64_MIN - offset)) {
        return -1;
    }

    new_pos = base + offset;
    if (new_pos < 0) {
        return -1;
    }

    file->pos = new_pos;
    return 0;
}

static const core_file_callbacks chd_qemu_callbacks = {
    .fsize = chd_qemu_fsize,
    .fread = chd_qemu_fread,
    .fclose = chd_qemu_fclose,
    .fseek = chd_qemu_fseek,
};

static bool chd_uses_cd_codec(const chd_header *header)
{
    for (int i = 0; i < ARRAY_SIZE(header->compression); i++) {
        switch (header->compression[i]) {
        case CHD_CODEC_CD_ZLIB:
        case CHD_CODEC_CD_LZMA:
        case CHD_CODEC_CD_FLAC:
        case CHD_CODEC_CD_ZSTD:
            return true;
        default:
            break;
        }
    }

    return false;
}

static int chd_probe(const uint8_t *buf, int buf_size, const char *filename)
{
    (void)filename;

    if (buf_size >= CHD_MAGIC_SIZE && !memcmp(buf, CHD_MAGIC, CHD_MAGIC_SIZE)) {
        return 100;
    }

    return 0;
}

static int chd_read_bytes(BDRVCHDState *s, uint64_t offset,
                          uint8_t *buf, size_t bytes)
{
    size_t done = 0;

    while (done < bytes) {
        uint64_t pos = offset + done;
        uint64_t hunk;
        uint64_t available = 0;
        size_t hunk_offset;
        size_t copy;
        chd_error err;

        if (pos < offset) {
            return -EIO;
        }

        if (pos < s->logical_bytes) {
            available = s->logical_bytes - pos;
        }

        if (!available) {
            memset(buf + done, 0, bytes - done);
            return 0;
        }

        hunk = pos / s->header->hunkbytes;
        if (hunk > UINT32_MAX) {
            return -EIO;
        }

        if (s->current_hunk != hunk) {
            err = chd_read(s->chd, hunk, s->hunk);
            if (err != CHDERR_NONE) {
                return -EIO;
            }
            s->current_hunk = hunk;
        }

        hunk_offset = pos % s->header->hunkbytes;
        copy = MIN(bytes - done, s->header->hunkbytes - hunk_offset);
        copy = MIN(copy, (size_t)MIN(available, SIZE_MAX));

        memcpy(buf + done, s->hunk + hunk_offset, copy);
        done += copy;
    }

    return 0;
}

static bool chd_add_u64(uint64_t a, uint64_t b, uint64_t *result)
{
    if (a > UINT64_MAX - b) {
        return false;
    }

    *result = a + b;
    return true;
}

static bool chd_sector_to_offset(uint64_t partition_offset, uint32_t sector,
                                 uint64_t *offset)
{
    uint64_t sector_offset = (uint64_t)sector * XEMU_XDVDFS_SECTOR_SIZE;

    return chd_add_u64(partition_offset, sector_offset, offset);
}

static bool chd_extent_in_image(BDRVCHDState *s, uint64_t offset,
                                uint64_t bytes)
{
    if (bytes > s->logical_bytes) {
        return false;
    }

    return offset <= s->logical_bytes - bytes;
}

static bool chd_read_exact(BDRVCHDState *s, uint64_t offset, void *buf,
                           size_t bytes)
{
    if (!chd_extent_in_image(s, offset, bytes)) {
        return false;
    }

    return chd_read_bytes(s, offset, buf, bytes) == 0;
}

static bool chd_entry_name_is_default_xbe(const uint8_t *name,
                                          uint8_t name_length)
{
    if (name_length != sizeof(xemu_xdvdfs_default_xbe) - 1) {
        return false;
    }

    return g_ascii_strncasecmp((const char *)name, xemu_xdvdfs_default_xbe,
                               name_length) == 0;
}

static bool chd_entry_file_is_valid(BDRVCHDState *s, uint64_t partition_offset,
                                    const uint8_t *entry)
{
    uint32_t entry_sector;
    uint32_t entry_size;
    uint64_t entry_offset;

    entry_sector = ldl_le_p(entry + 4);
    entry_size = ldl_le_p(entry + 8);

    if (!entry_sector || !entry_size) {
        return false;
    }

    if (!chd_sector_to_offset(partition_offset, entry_sector, &entry_offset)) {
        return false;
    }

    return chd_extent_in_image(s, entry_offset, entry_size);
}

static bool chd_find_default_xbe_in_tree(BDRVCHDState *s,
                                         const uint8_t *directory,
                                         uint32_t directory_size,
                                         uint32_t entry_offset,
                                         uint16_t depth,
                                         uint64_t partition_offset)
{
    const uint8_t *entry;
    uint16_t left_entry_offset;
    uint16_t right_entry_offset;
    uint8_t attributes;
    uint8_t name_length;
    uint32_t child_offset;

    if (depth > XEMU_XDVDFS_ENTRY_MAX_DEPTH) {
        return false;
    }

    if (entry_offset > directory_size ||
        directory_size - entry_offset < XEMU_XDVDFS_ENTRY_MIN_SIZE) {
        return false;
    }

    entry = directory + entry_offset;
    name_length = entry[13];

    if (name_length > directory_size - entry_offset -
        XEMU_XDVDFS_ENTRY_MIN_SIZE) {
        return false;
    }

    attributes = entry[12];
    if (!(attributes & XEMU_XDVDFS_ENTRY_DIRECTORY) &&
        chd_entry_name_is_default_xbe(
            entry + XEMU_XDVDFS_ENTRY_NAME_OFFSET, name_length) &&
        chd_entry_file_is_valid(s, partition_offset, entry)) {
        return true;
    }

    left_entry_offset = lduw_le_p(entry);
    if (left_entry_offset) {
        child_offset = (uint32_t)left_entry_offset <<
                       XEMU_XDVDFS_ENTRY_OFFSET_SHIFT;
        if (chd_find_default_xbe_in_tree(s, directory, directory_size,
                                         child_offset, depth + 1,
                                         partition_offset)) {
            return true;
        }
    }

    right_entry_offset = lduw_le_p(entry + 2);
    if (right_entry_offset) {
        child_offset = (uint32_t)right_entry_offset <<
                       XEMU_XDVDFS_ENTRY_OFFSET_SHIFT;
        if (chd_find_default_xbe_in_tree(s, directory, directory_size,
                                         child_offset, depth + 1,
                                         partition_offset)) {
            return true;
        }
    }

    return false;
}

static bool chd_find_default_xbe_linear(BDRVCHDState *s,
                                        const uint8_t *directory,
                                        uint32_t directory_size,
                                        uint64_t partition_offset)
{
    uint32_t entry_offset = 0;

    while (directory_size - entry_offset >= XEMU_XDVDFS_ENTRY_MIN_SIZE) {
        const uint8_t *entry = directory + entry_offset;
        uint8_t attributes = entry[12];
        uint8_t name_length = entry[13];
        uint32_t entry_size;

        if (name_length > directory_size - entry_offset -
            XEMU_XDVDFS_ENTRY_MIN_SIZE) {
            return false;
        }

        if (!(attributes & XEMU_XDVDFS_ENTRY_DIRECTORY) &&
            chd_entry_name_is_default_xbe(
                entry + XEMU_XDVDFS_ENTRY_NAME_OFFSET, name_length) &&
            chd_entry_file_is_valid(s, partition_offset, entry)) {
            return true;
        }

        entry_size = QEMU_ALIGN_UP(XEMU_XDVDFS_ENTRY_MIN_SIZE + name_length,
                                   4);
        if (!entry_size || entry_size > directory_size - entry_offset) {
            return false;
        }

        entry_offset += entry_size;
    }

    return false;
}

static bool chd_validate_xdvdfs_partition(BDRVCHDState *s,
                                          uint64_t partition_offset)
{
    uint8_t sector[XEMU_XDVDFS_SECTOR_SIZE];
    uint8_t *root_directory;
    uint32_t root_sector;
    uint32_t root_directory_size;
    uint64_t volume_descriptor_offset;
    uint64_t root_directory_offset;
    bool valid;

    if (!chd_sector_to_offset(partition_offset,
                              XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR,
                              &volume_descriptor_offset)) {
        return false;
    }

    if (!chd_read_exact(s, volume_descriptor_offset, sector,
                        sizeof(sector))) {
        return false;
    }

    if (memcmp(sector, xemu_xdvdfs_magic,
               sizeof(xemu_xdvdfs_magic) - 1) != 0 ||
        memcmp(sector + XEMU_XDVDFS_MAGIC_OFFSET, xemu_xdvdfs_magic,
               sizeof(xemu_xdvdfs_magic) - 1) != 0) {
        return false;
    }

    root_sector = ldl_le_p(sector + XEMU_XDVDFS_ROOT_SECTOR_OFFSET);
    root_directory_size = ldl_le_p(sector + XEMU_XDVDFS_ROOT_SIZE_OFFSET);

    if (!root_sector || !root_directory_size ||
        root_directory_size > XEMU_XDVDFS_MAX_ROOT_DIRECTORY_SIZE) {
        return false;
    }

    if (!chd_sector_to_offset(partition_offset, root_sector,
                              &root_directory_offset)) {
        return false;
    }

    if (!chd_extent_in_image(s, root_directory_offset, root_directory_size)) {
        return false;
    }

    root_directory = g_try_malloc(root_directory_size);
    if (!root_directory) {
        return false;
    }

    if (!chd_read_exact(s, root_directory_offset, root_directory,
                        root_directory_size)) {
        g_free(root_directory);
        return false;
    }

    valid = chd_find_default_xbe_in_tree(s, root_directory,
                                         root_directory_size, 0, 0,
                                         partition_offset) ||
            chd_find_default_xbe_linear(s, root_directory,
                                        root_directory_size,
                                        partition_offset);

    g_free(root_directory);
    return valid;
}

static void chd_detect_xbox_game_partition(BDRVCHDState *s)
{
    const uint64_t game_partition_offset =
        (uint64_t)XEMU_REDUMP_GAME_PARTITION_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    if (chd_validate_xdvdfs_partition(s, 0)) {
        return;
    }

    if (chd_validate_xdvdfs_partition(s, game_partition_offset)) {
        s->data_offset = game_partition_offset;
        s->visible_bytes = s->logical_bytes - game_partition_offset;
    }
}

static int chd_bdrv_open(BlockDriverState *bs, QDict *options, int flags,
                         Error **errp)
{
    BDRVCHDState *s = bs->opaque;
    chd_error err;
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

    s->file.child = bs->file;
    s->file.pos = 0;
    s->file.size = bdrv_getlength(bs->file->bs);
    if (s->file.size < 0) {
        error_setg_errno(errp, -s->file.size, "Could not get CHD file size");
        return s->file.size;
    }

    err = chd_open_core_file_callbacks(&chd_qemu_callbacks, &s->file,
                                       CHD_OPEN_READ, NULL, &s->chd);
    if (err != CHDERR_NONE) {
        error_setg(errp, "Could not open CHD image: %s",
                   chd_error_string(err));
        return -EINVAL;
    }

    s->header = chd_get_header(s->chd);
    if (!s->header || !s->header->hunkbytes || !s->header->logicalbytes) {
        error_setg(errp, "Invalid CHD header");
        ret = -EINVAL;
        goto fail;
    }
    if (chd_uses_cd_codec(s->header)) {
        error_setg(errp, "CD CHD images are not supported; use DVD/raw CHD");
        ret = -ENOTSUP;
        goto fail;
    }

    s->logical_bytes = s->header->logicalbytes;
    s->visible_bytes = s->logical_bytes;
    s->current_hunk = UINT32_MAX;
    s->hunk = g_try_malloc(s->header->hunkbytes);
    if (!s->hunk) {
        error_setg(errp, "Could not allocate CHD hunk buffer");
        ret = -ENOMEM;
        goto fail;
    }

    chd_detect_xbox_game_partition(s);

    if (s->visible_bytes > BDRV_MAX_LENGTH) {
        error_setg(errp, "CHD image is too large");
        ret = -EFBIG;
        goto fail;
    }

    bs->total_sectors = DIV_ROUND_UP(s->visible_bytes, BDRV_SECTOR_SIZE);
    qemu_co_mutex_init(&s->lock);
    return 0;

fail:
    g_free(s->hunk);
    if (s->chd) {
        chd_close(s->chd);
        s->chd = NULL;
    }
    return ret;
}

static void chd_refresh_limits(BlockDriverState *bs, Error **errp)
{
    (void)errp;
    bs->bl.request_alignment = BDRV_SECTOR_SIZE;
}

static int coroutine_fn GRAPH_RDLOCK
chd_co_preadv(BlockDriverState *bs, int64_t offset, int64_t bytes,
              QEMUIOVector *qiov, BdrvRequestFlags flags)
{
    BDRVCHDState *s = bs->opaque;
    uint8_t *buf;
    int ret;

    (void)flags;

    assert(QEMU_IS_ALIGNED(offset, BDRV_SECTOR_SIZE));
    assert(QEMU_IS_ALIGNED(bytes, BDRV_SECTOR_SIZE));

    if (offset < 0 || bytes < 0) {
        return -EINVAL;
    }
    if (bytes == 0) {
        return 0;
    }
    if (bytes > SIZE_MAX ||
        (uint64_t)offset > UINT64_MAX - s->data_offset) {
        return -EIO;
    }

    buf = g_try_malloc((size_t)bytes);
    if (!buf) {
        return -ENOMEM;
    }

    qemu_co_mutex_lock(&s->lock);
    ret = chd_read_bytes(s, s->data_offset + offset, buf, (size_t)bytes);
    qemu_co_mutex_unlock(&s->lock);

    if (ret == 0) {
        qemu_iovec_from_buf(qiov, 0, buf, (size_t)bytes);
    }

    g_free(buf);
    return ret;
}

static void chd_bdrv_close(BlockDriverState *bs)
{
    BDRVCHDState *s = bs->opaque;

    g_free(s->hunk);
    if (s->chd) {
        chd_close(s->chd);
    }
}

static BlockDriver bdrv_chd = {
    .format_name = "chd",
    .instance_size = sizeof(BDRVCHDState),
    .bdrv_probe = chd_probe,
    .bdrv_open = chd_bdrv_open,
    .bdrv_child_perm = bdrv_default_perms,
    .bdrv_refresh_limits = chd_refresh_limits,
    .bdrv_co_preadv = chd_co_preadv,
    .bdrv_close = chd_bdrv_close,
    .is_format = true,
};

static void bdrv_chd_init(void)
{
    bdrv_register(&bdrv_chd);
}

block_init(bdrv_chd_init);
