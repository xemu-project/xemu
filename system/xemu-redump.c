#include "qemu/osdep.h"
#include "system/xemu-redump.h"

#define XEMU_XDVDFS_SECTOR_SIZE 2048
#define XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR 32
#define XEMU_REDUMP_GAME_PARTITION_SECTOR 0x30600
#define XEMU_XDVDFS_MAGIC_OFFSET 0x7ec

static const char xemu_xdvdfs_magic[] = "MICROSOFT*XBOX*MEDIA";

static bool xemu_redump_has_xdvdfs_magic(FILE *file, uint64_t offset)
{
    uint8_t sector[XEMU_XDVDFS_SECTOR_SIZE];

    if (offset > LONG_MAX) {
        return false;
    }

    if (fseek(file, (long)offset, SEEK_SET) != 0) {
        return false;
    }

    if (fread(sector, 1, sizeof(sector), file) != sizeof(sector)) {
        return false;
    }

    return memcmp(sector, xemu_xdvdfs_magic,
                  sizeof(xemu_xdvdfs_magic) - 1) == 0 &&
           memcmp(sector + XEMU_XDVDFS_MAGIC_OFFSET, xemu_xdvdfs_magic,
                  sizeof(xemu_xdvdfs_magic) - 1) == 0;
}

bool xemu_redump_detect_game_partition(const char *path, uint64_t *offset)
{
    const uint64_t game_partition_offset =
        (uint64_t)XEMU_REDUMP_GAME_PARTITION_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    const uint64_t xiso_volume_descriptor_offset =
        (uint64_t)XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    const uint64_t volume_descriptor_offset =
        game_partition_offset +
        (uint64_t)XEMU_XDVDFS_VOLUME_DESCRIPTOR_SECTOR *
        XEMU_XDVDFS_SECTOR_SIZE;
    FILE *file;
    bool detected;

    if (!path || !path[0]) {
        return false;
    }

    file = qemu_fopen(path, "rb");
    if (!file) {
        return false;
    }

    if (xemu_redump_has_xdvdfs_magic(file, xiso_volume_descriptor_offset)) {
        fclose(file);
        return false;
    }

    detected = xemu_redump_has_xdvdfs_magic(file, volume_descriptor_offset);
    fclose(file);

    if (detected && offset) {
        *offset = game_partition_offset;
    }

    return detected;
}
