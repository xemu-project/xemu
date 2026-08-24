// v2.87 current regression ownership.
#include "fatx-hdd.hh"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

constexpr uint64_t kCOffset = 0x8CA80000ull;
constexpr uint64_t kCSize = 0x01F400000ull;
constexpr uint64_t kEOffset = 0xABE80000ull;
constexpr uint64_t kESize = 0x1312D6000ull;
constexpr uint64_t kRetailDiskSize = 0x1DD156000ull;
constexpr uint32_t kClusterSize = 16 * 1024;
constexpr uint64_t kCFatOffset = kCOffset + 4096;
constexpr uint64_t kCFatSize = 0x10000;
constexpr uint64_t kCDataOffset = kCFatOffset + kCFatSize;

struct SparseImage {
    std::map<uint64_t, std::vector<uint8_t>> chunks;
};

void PutLe16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void PutLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

uint64_t RoundUp4096(uint64_t value)
{
    return (value + 4095ull) & ~4095ull;
}

bool SparseRead(void *opaque, uint64_t offset, void *buffer, size_t size);

uint64_t DataOffset(uint64_t partition_offset, uint64_t partition_size)
{
    const uint64_t fat_entries = partition_size / kClusterSize + 1;
    const uint64_t fat_bits = fat_entries < 0xFFF0ull ? 16 : 32;
    const uint64_t fat_size = RoundUp4096(fat_entries * (fat_bits / 8));
    return partition_offset + 4096 + fat_size;
}

void SetFatEntry(SparseImage &image, uint64_t partition_offset,
                 uint64_t partition_size, uint32_t cluster, uint32_t value)
{
    const uint64_t fat_entries = partition_size / kClusterSize + 1;
    const uint64_t fat_bits = fat_entries < 0xFFF0ull ? 16 : 32;
    const uint64_t entry_bytes = fat_bits / 8;
    std::vector<uint8_t> raw(entry_bytes, 0);
    if (entry_bytes == 2) {
        PutLe16(raw.data(), (uint16_t)value);
    } else {
        PutLe32(raw.data(), value);
    }
    image.chunks[partition_offset + 4096 + (uint64_t)cluster * entry_bytes] =
        std::move(raw);
}


bool SparseRead(void *opaque, uint64_t offset, void *buffer, size_t size)
{
    auto &image = *static_cast<SparseImage *>(opaque);
    std::memset(buffer, 0, size);
    uint8_t *out = static_cast<uint8_t *>(buffer);
    for (const auto &[base, data] : image.chunks) {
        const uint64_t end = base + data.size();
        const uint64_t request_end = offset + size;
        if (end <= offset || base >= request_end) {
            continue;
        }
        const uint64_t begin = base > offset ? base : offset;
        const uint64_t finish = end < request_end ? end : request_end;
        std::memcpy(out + (begin - offset), data.data() + (begin - base),
                    (size_t)(finish - begin));
    }
    return true;
}


bool VectorWrite(void *opaque, const void *buffer, size_t size)
{
    auto &out = *static_cast<std::vector<uint8_t> *>(opaque);
    const auto *bytes = static_cast<const uint8_t *>(buffer);
    out.insert(out.end(), bytes, bytes + size);
    return true;
}

void AddEntry(std::vector<uint8_t> &cluster, size_t index, const char *name,
              uint8_t attrs, uint32_t first_cluster, uint32_t size,
              uint16_t date, uint16_t time)
{
    uint8_t *raw = cluster.data() + index * 64;
    const size_t len = std::strlen(name);
    assert(len <= 42);
    raw[0] = (uint8_t)len;
    raw[1] = attrs;
    std::memcpy(raw + 2, name, len);
    PutLe32(raw + 44, first_cluster);
    PutLe32(raw + 48, size);
    PutLe16(raw + 52, time);
    PutLe16(raw + 54, date);
}

std::vector<uint8_t> Utf16Le(const char *text)
{
    std::vector<uint8_t> out{0xFF, 0xFE};
    while (*text) {
        out.push_back((uint8_t)*text++);
        out.push_back(0);
    }
    return out;
}

void AddPartitionSuperblock(SparseImage &image, uint64_t offset,
                            uint32_t volume_id, uint32_t root_cluster,
                            uint32_t sectors_per_cluster = 32u)
{
    std::vector<uint8_t> superblock(4096, 0xFF);
    PutLe32(superblock.data() + 0, 0x58544146u);
    PutLe32(superblock.data() + 4, volume_id);
    PutLe32(superblock.data() + 8, sectors_per_cluster);
    PutLe32(superblock.data() + 12, root_cluster);
    image.chunks[offset] = std::move(superblock);
}

} // namespace

int main()
{
    SparseImage image;

    AddPartitionSuperblock(image, kCOffset, 0x12345678u, 1u);

    std::vector<uint8_t> root(kClusterSize, 0xFF);
    const uint16_t date = (uint16_t)((26u << 9) | (8u << 5) | 18u); // 2026-08-18
    const uint16_t time = (uint16_t)((17u << 11) | (45u << 5) | 15u); // 17:45:30
    AddEntry(root, 0, "foo.txt", 0, 2, 123, date, time);
    AddEntry(root, 1, "folder", 0x10, 3, 0, date, time);
    root[2 * 64] = 0xFF;
    image.chunks[kCDataOffset] = root;

    std::vector<uint8_t> child(kClusterSize, 0xFF);
    AddEntry(child, 0, "bar.bin", 0x01, 4, 456, date, time);
    child[1 * 64] = 0x00;
    image.chunks[kCDataOffset + 2ull * kClusterSize] = child; // cluster 3

    std::vector<uint8_t> foo_data(123);
    for (size_t i = 0; i < foo_data.size(); ++i) {
        foo_data[i] = (uint8_t)(i ^ 0x5A);
    }
    image.chunks[kCDataOffset + kClusterSize] = foo_data; // cluster 2

    // Build a minimal E:\UDATA\54510109 tree with TitleMeta/SaveMeta so the
    // read-only friendly-name pass is exercised against real FATX metadata.
    AddPartitionSuperblock(image, kEOffset, 0xCAFEBABEu, 1u);
    const uint64_t e_data = DataOffset(kEOffset, kESize);

    std::vector<uint8_t> e_root(kClusterSize, 0xFF);
    AddEntry(e_root, 0, "UDATA", 0x10, 2, 0, date, time);
    e_root[64] = 0xFF;
    image.chunks[e_data] = e_root;

    std::vector<uint8_t> udata(kClusterSize, 0xFF);
    AddEntry(udata, 0, "54510109", 0x10, 3, 0, date, time);
    udata[64] = 0xFF;
    image.chunks[e_data + kClusterSize] = udata;

    const std::vector<uint8_t> title_meta = Utf16Le("TitleName=Ratatouille\r\n");
    std::vector<uint8_t> title(kClusterSize, 0xFF);
    AddEntry(title, 0, "TitleMeta.xbx", 0, 4, (uint32_t)title_meta.size(), date, time);
    AddEntry(title, 1, "565190C2C935", 0x10, 5, 0, date, time);
    title[2 * 64] = 0xFF;
    image.chunks[e_data + 2ull * kClusterSize] = title;
    image.chunks[e_data + 3ull * kClusterSize] = title_meta;

    const std::vector<uint8_t> save_meta = Utf16Le("Name=Save Game 1\r\n");
    std::vector<uint8_t> save(kClusterSize, 0xFF);
    AddEntry(save, 0, "SaveMeta.xbx", 0, 6, (uint32_t)save_meta.size(), date, time);
    AddEntry(save, 1, "payload.bin", 0, 7, kClusterSize + 17, date, time);
    save[2 * 64] = 0xFF;
    image.chunks[e_data + 4ull * kClusterSize] = save;
    image.chunks[e_data + 5ull * kClusterSize] = save_meta;

    std::vector<uint8_t> payload_a(kClusterSize, 0xA5);
    std::vector<uint8_t> payload_b(17, 0x5A);
    image.chunks[e_data + 6ull * kClusterSize] = payload_a;
    image.chunks[e_data + 7ull * kClusterSize] = payload_b;

    // FAT chains used by the v2.04 delete test. E: is FAT32 in the retail
    // layout. Directories and single-cluster files terminate at 0xFFFFFFFF;
    // payload.bin spans clusters 7 -> 8.
    for (uint32_t cluster : {1u, 2u, 3u, 4u, 5u, 6u, 8u}) {
        SetFatEntry(image, kEOffset, kESize, cluster, 0xFFFFFFFFu);
    }
    SetFatEntry(image, kEOffset, kESize, 7u, 8u);

    XemuFatxHdd::Snapshot snapshot;
    const bool ok = XemuFatxHdd::BuildSnapshot(
        SparseRead, &image, kRetailDiskSize, snapshot);
    assert(ok);
    assert(snapshot.hdd_available);
    assert(snapshot.image_size == kRetailDiskSize);
    assert(snapshot.partitions.size() == 5);

    const XemuFatxHdd::Partition *c = XemuFatxHdd::FindPartition(snapshot, 'C');
    assert(c != nullptr);
    assert(c->offset == kCOffset);
    assert(c->size == kCSize);
    assert(c->available);
    assert(c->volume_id == 0x12345678u);
    assert(c->sectors_per_cluster == 32u);
    assert(c->bytes_per_cluster == kClusterSize);
    assert(c->fat_bits == 16u);
    assert(c->entries.size() == 2);
    assert(c->entries[0].name == "foo.txt");
    assert(!c->entries[0].directory);
    assert(c->entries[0].file_size == 123u);
    assert(c->entries[1].name == "folder");
    assert(c->entries[1].directory);
    assert(c->entries[1].children.size() == 1);
    assert(c->entries[1].children[0].name == "bar.bin");
    assert(c->entries[1].children[0].file_size == 456u);
    assert(c->entries[1].children[0].attributes == 0x01u);
    assert(XemuFatxHdd::FormatTimestamp(date, time) == "2026-08-18 17:45:30");

    std::vector<uint8_t> exported;
    std::string stream_error;
    assert(XemuFatxHdd::StreamFile(SparseRead, &image, kRetailDiskSize,
                                   *c, c->entries[0], VectorWrite, &exported,
                                   stream_error));
    assert(stream_error.empty());
    assert(exported == foo_data);

    // v2.42 sequential cursor must remember previously visited clusters
    // across calls. A 2 -> 2 FAT cycle must be rejected on the second chunk
    // rather than repeatedly returning the same cluster as new file data.
    XemuFatxHdd::Entry cyclic = c->entries[0];
    cyclic.first_cluster = 2;
    cyclic.file_size = kClusterSize * 2;
    SetFatEntry(image, kCOffset, kCSize, 2u, 2u);
    XemuFatxHdd::FileReadCursor cyclic_cursor;
    std::vector<uint8_t> cyclic_chunk;
    std::string cyclic_error;
    assert(XemuFatxHdd::ReadFileRangeSequential(
        SparseRead, &image, kRetailDiskSize, *c, cyclic, 0, kClusterSize,
        cyclic_cursor, cyclic_chunk, cyclic_error));
    assert(cyclic_chunk.size() == kClusterSize);
    assert(!XemuFatxHdd::ReadFileRangeSequential(
        SparseRead, &image, kRetailDiskSize, *c, cyclic, kClusterSize,
        kClusterSize, cyclic_cursor, cyclic_chunk, cyclic_error));
    assert(cyclic_error.find("cycle") != std::string::npos);
    SetFatEntry(image, kCOffset, kCSize, 2u, 0xFFFFu);

    std::string metadata_warning;
    assert(XemuFatxHdd::PopulateXboxMetadata(
        SparseRead, &image, kRetailDiskSize, snapshot, metadata_warning));
    assert(metadata_warning.empty());
    const XemuFatxHdd::Partition *e = XemuFatxHdd::FindPartition(snapshot, 'E');
    assert(e && e->available);
    const XemuFatxHdd::Entry *title_id =
        XemuFatxHdd::FindEntry(*e, {"UDATA", "54510109"});
    assert(title_id && title_id->friendly_name == "Ratatouille");
    assert(XemuFatxHdd::DisplayName(*title_id) == "54510109 - Ratatouille");
    const XemuFatxHdd::Entry *save_dir =
        XemuFatxHdd::FindEntry(*e, {"UDATA", "54510109", "565190C2C935"});
    assert(save_dir && save_dir->friendly_name == "Save Game 1");
    assert(XemuFatxHdd::DisplayName(*save_dir) == "565190C2C935 - Save Game 1");

    assert(save_dir->children.size() == 2);
    // v2.32: production FATX mutation has been decommissioned. Parser,
    // metadata, export, targeted snapshots and kernel-RPC planning remain
    // covered here; destructive behavior is tested at the kernel-RPC layer.

    for (const auto &part : snapshot.partitions) {
        if (part.letter != 'C' && part.letter != 'E') {
            assert(!part.available);
        }
    }

    // v2.19: XBP/LBA48 v3 sector-0 table is authoritative for F/G. The
    // parser must not let F consume G's range.
    SparseImage extended;
    constexpr uint32_t f_start = (uint32_t)(kRetailDiskSize / 512ull);
    constexpr uint32_t f_sectors = 0x00020000u; // 64 MiB
    constexpr uint32_t g_start = f_start + f_sectors;
    constexpr uint32_t g_sectors = 0x00020000u; // 64 MiB
    constexpr uint64_t f_offset = (uint64_t)f_start * 512ull;
    constexpr uint64_t f_size = (uint64_t)f_sectors * 512ull;
    constexpr uint64_t g_offset = (uint64_t)g_start * 512ull;
    constexpr uint64_t g_size = (uint64_t)g_sectors * 512ull;
    constexpr uint64_t extended_image_size = g_offset + g_size;

    std::vector<uint8_t> xbp(512, 0);
    std::memcpy(xbp.data(), "****PARTINFO****", 16);
    auto add_xbp = [&](size_t index, const char *name, uint32_t start, uint32_t sectors) {
        uint8_t *raw = xbp.data() + 0x30 + index * 0x20;
        std::memset(raw, ' ', 16);
        std::memcpy(raw, name, std::min<size_t>(16, std::strlen(name)));
        PutLe32(raw + 16, 0x80000000u);
        PutLe32(raw + 20, start);
        PutLe32(raw + 24, sectors);
    };
    add_xbp(5, "XBOX F", f_start, f_sectors);
    add_xbp(6, "XBOX G", g_start, g_sectors);
    extended.chunks[0] = xbp;

    AddPartitionSuperblock(extended, f_offset, 0xF00DF00Du, 1u, 32u);
    AddPartitionSuperblock(extended, g_offset, 0x600D600Du, 1u, 32u);
    std::vector<uint8_t> empty_cluster(kClusterSize, 0xFF);
    extended.chunks[DataOffset(f_offset, f_size)] = empty_cluster;
    extended.chunks[DataOffset(g_offset, g_size)] = empty_cluster;

    XemuFatxHdd::Snapshot extended_snapshot;
    assert(XemuFatxHdd::BuildSnapshot(SparseRead, &extended, extended_image_size,
                                      extended_snapshot));
    const XemuFatxHdd::Partition *f = XemuFatxHdd::FindPartition(extended_snapshot, 'F');
    const XemuFatxHdd::Partition *g = XemuFatxHdd::FindPartition(extended_snapshot, 'G');
    assert(f && f->available && g && g->available);
    assert(f->offset == f_offset && f->size == f_size);
    assert(g->offset == g_offset && g->size == g_size);
    assert(f->offset + f->size <= g->offset);

    std::cout << "PASS: FATX HDD parser metadata + export + targeted snapshots + XBP F/G core\n";
    return 0;
}
