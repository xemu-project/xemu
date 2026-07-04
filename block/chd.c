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

/*
 * XDVDFS on-disk layout constants. Once a CHD is decompressed to its logical
 * image, the bootable Xbox filesystem lives either at offset 0 (a plain xiso)
 * or at the game-partition sector (a Redump-style full disc dump). XDVDFS
 * uses 2048-byte sectors and starts with a volume descriptor 32 sectors into
 * the partition, which carries the magic string and the root directory
 * location.
 *
 * Directory entry layout (little-endian), min 14 bytes plus the name:
 *   0x00  uint16  left sibling offset  (in 4-byte units)
 *   0x02  uint16  right sibling offset (in 4-byte units)
 *   0x04  uint32  first data sector    (relative to the partition)
 *   0x08  uint32  data size in bytes
 *   0x0c  uint8   attributes (bit 0x10 = directory)
 *   0x0d  uint8   name length
 *   0x0e  char[]  name (name length bytes, then padded to 4)
 */
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
/* Sanity cap so a bogus root size can't trigger a huge allocation. */
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
    uint8_t *hunk;              /* single decompressed hunk cache */
    uint32_t current_hunk;      /* index cached in hunk; valid iff hunk_valid */
    bool hunk_valid;            /* separate flag: every uint32 is a valid index */
    uint64_t logical_bytes;     /* full decompressed image size */
    uint64_t data_offset;       /* offset added to guest reads (game partition) */
    uint64_t visible_bytes;     /* size exposed to the guest = logical - offset */
} BDRVCHDState;

/*
 * libchdr reads the container through a set of stdio-like callbacks. These
 * bridge them onto the underlying QEMU BdrvChild so the CHD file itself is
 * accessed through the block layer (honouring its backing protocol) rather
 * than a raw FILE*. All offsets are clamped to INT64 range because the block
 * layer's read/seek take signed 64-bit positions.
 */
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

/*
 * Read bytes from the decompressed logical image. CHD stores data in
 * fixed-size hunks; we decompress one hunk at a time into s->hunk and copy
 * out the requested span, looping across hunk boundaries. Reads that extend
 * past the end of the logical image are zero-filled, matching how the block
 * layer expects a short image to behave.
 */
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

        /* Decompress the hunk unless it is already cached. hunk_valid must be
         * checked separately: UINT32_MAX is a legal hunk index, so it cannot
         * double as an "empty cache" sentinel in current_hunk. */
        if (!s->hunk_valid || s->current_hunk != hunk) {
            err = chd_read(s->chd, hunk, s->hunk);
            if (err != CHDERR_NONE) {
                return -EIO;
            }
            s->current_hunk = hunk;
            s->hunk_valid = true;
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

/*
 * XDVDFS stores each directory as an on-disk binary tree: every entry holds
 * 16-bit offsets (in 4-byte units) to its left and right sibling. We walk the
 * tree looking for a "default.xbe" entry whose data extent lies inside the
 * image, which marks the partition as a bootable Xbox filesystem.
 *
 * The child offsets come from untrusted image data, so the tree may be
 * malformed: children can point back at earlier entries, forming cycles or a
 * DAG whose left/right links converge on shared nodes. Without a bound, such
 * input makes the recursion explore an exponential number of paths (up to
 * 2^depth) and hangs the emulator while opening the image. Two independent
 * limits keep the walk finite for any input:
 *   - depth bounds the recursion (and thus stack usage) to MAX_DEPTH levels;
 *   - budget bounds the total number of entries visited to the most that can
 *     physically fit in the directory, so a converging or cyclic tree is
 *     abandoned instead of being re-expanded over and over.
 * A well-formed acyclic tree visits each entry at most once and trips neither
 * limit; if we do bail out, the linear fallback scan still covers the
 * directory.
 */
static bool chd_find_default_xbe_in_tree(BDRVCHDState *s,
                                         const uint8_t *directory,
                                         uint32_t directory_size,
                                         uint32_t entry_offset,
                                         uint16_t depth,
                                         uint32_t *budget,
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

    /* Stop once we have visited as many entries as could exist; beyond that
     * we must be revisiting nodes of a malformed (cyclic/converging) tree. */
    if (*budget == 0) {
        return false;
    }
    (*budget)--;

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
                                         child_offset, depth + 1, budget,
                                         partition_offset)) {
            return true;
        }
    }

    right_entry_offset = lduw_le_p(entry + 2);
    if (right_entry_offset) {
        child_offset = (uint32_t)right_entry_offset <<
                       XEMU_XDVDFS_ENTRY_OFFSET_SHIFT;
        if (chd_find_default_xbe_in_tree(s, directory, directory_size,
                                         child_offset, depth + 1, budget,
                                         partition_offset)) {
            return true;
        }
    }

    return false;
}

/*
 * Fallback enumeration that ignores the sibling pointers and walks the packed
 * entries sequentially, advancing by each entry's padded size. This cannot
 * loop or overrun: every step consumes at least ENTRY_MIN_SIZE bytes and
 * stops once fewer than that remain, so it is bounded by the directory size.
 * Used when the tree walk is abandoned as malformed.
 */
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

    /*
     * Try the binary-tree walk first, capping the total entries it may visit
     * at the most that can fit in the directory (each entry is at least
     * ENTRY_MIN_SIZE bytes). If the tree is malformed and we abandon it, fall
     * back to a linear scan of the packed entries, which is inherently
     * bounded by the directory size.
     */
    uint32_t budget = root_directory_size / XEMU_XDVDFS_ENTRY_MIN_SIZE + 1;
    valid = chd_find_default_xbe_in_tree(s, root_directory,
                                         root_directory_size, 0, 0, &budget,
                                         partition_offset) ||
            chd_find_default_xbe_linear(s, root_directory,
                                        root_directory_size,
                                        partition_offset);

    g_free(root_directory);
    return valid;
}

/*
 * Decide how to present the decompressed image to the guest. If a valid
 * XDVDFS filesystem already starts at offset 0 the image is a plain xiso and
 * is exposed as-is. Otherwise, if one is found at the Redump game-partition
 * offset, that offset is hidden from the guest: reads are shifted by
 * data_offset and only the partition-sized tail is made visible, so the guest
 * sees a bare game disc. If neither matches, the full image is exposed
 * unchanged.
 */
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
    s->hunk_valid = false;
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
