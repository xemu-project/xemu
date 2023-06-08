#include "fatx.h"

#define FATX_SIGNATURE 0x58544146

// This is from libfatx
#pragma pack(1)
struct fatx_superblock {
    uint32_t signature;
    uint32_t volume_id;
    uint32_t sectors_per_cluster;
    uint32_t root_cluster;
    uint16_t unknown1;
    uint8_t  padding[4078];
};
#pragma pack()

bool create_fatx_image(const char* filename, unsigned int size)
{
    unsigned char zero = 0x00;
    unsigned int empty_fat = 0xfffffff8;

    FILE *fp = qemu_fopen(filename, "wb");
    if (fp != NULL)
    {
        struct fatx_superblock superblock;
        memset(&superblock, 0xff, sizeof(struct fatx_superblock));

        superblock.signature = FATX_SIGNATURE;
        superblock.sectors_per_cluster = 4;
        superblock.volume_id = (uint32_t)rand();
        superblock.root_cluster = 1;
        superblock.unknown1 = 0;

        // Write the fatx superblock.
        fwrite(&superblock, sizeof(superblock), 1, fp);

        // Write the FAT
        fwrite(&empty_fat, sizeof(empty_fat), 1, fp);

        // Fill the rest of the space with zeros
        for (unsigned int i = ftell(fp); i < size; i++)
            fwrite(&zero, 1, 1, fp);

        fflush(fp);
        fclose(fp);

        return true;
    }

    return false;
}
