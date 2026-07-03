/*
 * xemu Redump ISO support
 *
 * Copyright (c) 2026 JBW89
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or
 * later.  See the COPYING file in the top-level directory.
 */
#include "qemu/osdep.h"
#include "qemu/bswap.h"
#include "system/xemu-redump.h"

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

static bool xemu_redump_seek(FILE *file, uint64_t offset)
{
#ifdef _WIN32
    if (offset > INT64_MAX) {
        return false;
    }

    return _fseeki64(file, (__int64)offset, SEEK_SET) == 0;
#else
    if (offset > INT64_MAX) {
        return false;
    }

    return fseeko(file, (off_t)offset, SEEK_SET) == 0;
#endif
}

static bool xemu_redump_get_file_size(FILE *file, uint64_t *file_size)
{
#ifdef _WIN32
    __int64 current;
    __int64 end;

    current = _ftelli64(file);
    if (current < 0) {
        return false;
    }

    if (_fseeki64(file, 0, SEEK_END) != 0) {
        return false;
    }

    end = _ftelli64(file);
    if (end < 0) {
        return false;
    }

    if (_fseeki64(file, current, SEEK_SET) != 0) {
        return false;
    }
#else
    off_t current;
    off_t end;

    current = ftello(file);
    if (current < 0) {
        return false;
    }

    if (fseeko(file, 0, SEEK_END) != 0) {
        return false;
    }

    end = ftello(file);
    if (end < 0) {
        return false;
    }

    if (fseeko(file, current, SEEK_SET) != 0) {
        return false;
    }
#endif

    *file_size = end;
    return true;
}

static bool xemu_redump_add_u64(uint64_t a, uint64_t b, uint64_t *result)
{
    if (a > UINT64_MAX - b) {
        return false;
    }

    *result = a + b;
    return true;
}

static bool xemu_redump_sector_to_offset(uint64_t partition_offset,
                                         uint32_t sector, uint64_t *offset)
{
    uint64_t sector_offset;

    sector_offset = (uint64_t)sector * XEMU_XDVDFS_SECTOR_SIZE;
    return xemu_redump_add_u64(partition_offset, sector_offset, offset);
}

static bool xemu_redump_extent_in_file(uint64_t offset, uint64_t size,
                                       uint64_t file_size)
{
    if (size > file_size) {
        return false;
    }

    return offset <= file_size - size;
}

static bool xemu_redump_read_at(FILE *file, uint64_t offset, void *buffer,
                                size_t size, uint64_t file_size)
{
    if (!xemu_redump_extent_in_file(offset, size, file_size)) {
        return false;
    }

    if (!xemu_redump_seek(file, offset)) {
        return false;
    }

    return fread(buffer, 1, size, file) == size;
}

static bool xemu_redump_has_xdvdfs_magic(FILE *file, uint64_t offset,
                                         uint64_t file_size)
{
    uint8_t sector[XEMU_XDVDFS_SECTOR_SIZE];

    if (!xemu_redump_read_at(file, offset, sector, sizeof(sector),
                             file_size)) {
        return false;
    }

    return memcmp(sector, xemu_xdvdfs_magic,
                  sizeof(xemu_xdvdfs_magic) - 1) == 0 &&
           memcmp(sector + XEMU_XDVDFS_MAGIC_OFFSET, xemu_xdvdfs_magic,
                  sizeof(xemu_xdvdfs_magic) - 1) == 0;
}

static bool xemu_redump_entry_name_is_default_xbe(const uint8_t *name,
                                                  uint8_t name_length)
{
    if (name_length != sizeof(xemu_xdvdfs_default_xbe) - 1) {
        return false;
    }

    return g_ascii_strncasecmp((const char *)name, xemu_xdvdfs_default_xbe,
                               name_length) == 0;
}

static bool xemu_redump_entry_file_is_valid(uint64_t partition_offset,
                                            uint64_t file_size,
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

    if (!xemu_redump_sector_to_offset(partition_offset, entry_sector,
                                      &entry_offset)) {
        return false;
    }

    return xemu_redump_extent_in_file(entry_offset, entry_size, file_size);
}

static bool xemu_redump_find_default_xbe_in_tree(const uint8_t *directory,
                                                 uint32_t directory_size,
                                                 uint32_t entry_offset,
                                                 uint16_t depth,
                                                 uint64_t partition_offset,
                                                 uint64_t file_size)
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
        xemu_redump_entry_name_is_default_xbe(
            entry + XEMU_XDVDFS_ENTRY_NAME_OFFSET, name_length) &&
        xemu_redump_entry_file_is_valid(partition_offset, file_size, entry)) {
        return true;
    }

    left_entry_offset = lduw_le_p(entry);
    if (left_entry_offset) {
        child_offset = (uint32_t)left_entry_offset <<
                       XEMU_XDVDFS_ENTRY_OFFSET_SHIFT;
        if (xemu_redump_find_default_xbe_in_tree(
                directory, directory_size, child_offset, depth + 1,
                partition_offset, file_size)) {
            return true;
        }
    }

    right_entry_offset = lduw_le_p(entry + 2);
    if (right_entry_offset) {
        child_offset = (uint32_t)right_entry_offset <<
                       XEMU_XDVDFS_ENTRY_OFFSET_SHIFT;
        if (xemu_redump_find_default_xbe_in_tree(
                directory, directory_size, child_offset, depth + 1,
                partition_offset, file_size)) {
            return true;
        }
    }

    return false;
}

static bool xemu_redump_find_default_xbe_linear(const uint8_t *directory,
                                                uint32_t directory_size,
                                                uint64_t partition_offset,
                                                uint64_t file_size)
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
            xemu_redump_entry_name_is_default_xbe(
                entry + XEMU_XDVDFS_ENTRY_NAME_OFFSET, name_length) &&
            xemu_redump_entry_file_is_valid(partition_offset, file_size,
                                            entry)) {
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

static bool xemu_redump_validate_xdvdfs_partition(FILE *file,
                                                  uint64_t partition_offset,
                                                  uint64_t file_size)
{
    uint8_t sector[XEMU_XDVDFS_SECTOR_SIZE];
    uint8_t *root_directory;
    uint32_t root_sector;
    uint32_t root_directory_size;
    uint64_t volume_descriptor_offset;
    uint64_t root_directory_offset;
    bool valid;

    if (!xemu_redump_sector_to_offset(
            partition_offset, XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR,
            &volume_descriptor_offset)) {
        return false;
    }

    if (!xemu_redump_read_at(file, volume_descriptor_offset, sector,
                             sizeof(sector), file_size)) {
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

    if (!xemu_redump_sector_to_offset(partition_offset, root_sector,
                                      &root_directory_offset)) {
        return false;
    }

    if (!xemu_redump_extent_in_file(root_directory_offset,
                                    root_directory_size, file_size)) {
        return false;
    }

    root_directory = g_try_malloc(root_directory_size);
    if (!root_directory) {
        return false;
    }

    if (!xemu_redump_read_at(file, root_directory_offset, root_directory,
                             root_directory_size, file_size)) {
        g_free(root_directory);
        return false;
    }

    valid = xemu_redump_find_default_xbe_in_tree(
                root_directory, root_directory_size, 0, 0,
                partition_offset, file_size) ||
            xemu_redump_find_default_xbe_linear(
                root_directory, root_directory_size, partition_offset,
                file_size);

    g_free(root_directory);
    return valid;
}

bool xemu_redump_detect_game_partition(const char *path, uint64_t *offset)
{
    const uint64_t game_partition_offset =
        (uint64_t)XEMU_REDUMP_GAME_PARTITION_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    const uint64_t xiso_volume_descriptor_offset =
        (uint64_t)XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    FILE *file;
    bool detected;
    uint64_t file_size;

    if (!path || !path[0]) {
        return false;
    }

    file = qemu_fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (!xemu_redump_get_file_size(file, &file_size)) {
        fclose(file);
        return false;
    }

    if (xemu_redump_has_xdvdfs_magic(file, xiso_volume_descriptor_offset,
                                     file_size)) {
        fclose(file);
        return false;
    }

    detected = xemu_redump_validate_xdvdfs_partition(file,
                                                     game_partition_offset,
                                                     file_size);
    fclose(file);

    if (detected && offset) {
        *offset = game_partition_offset;
    }

    return detected;
}
