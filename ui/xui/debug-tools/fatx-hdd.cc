//
// xemu FATX HDD snapshot/parser + restricted mutation helpers
//
// The on-disk structure and retail partition layout follow the GPLv2 libfatx
// implementation by Matt Borgerson (mborgerson/fatx). This xemu parser is
// intentionally read-only and consumes the active QEMU BlockBackend through a
// callback instead of opening the host image file. Snapshot/export paths are
// read-only; explicitly requested mutations are kept in a separate API.
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "fatx-hdd.hh"
#include "binary-utils.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <unordered_set>
#include <utility>

namespace XemuFatxHdd {
namespace {

constexpr uint32_t kFatxSignature = 0x58544146u; // "FATX" on little endian disk
constexpr uint64_t kSuperblockSize = 4096;
constexpr uint64_t kFatOffset = 4096;
constexpr uint32_t kSectorSize = 512;
constexpr uint32_t kMaxFilename = 42;
constexpr uint8_t kDeleted = 0xE5;
constexpr uint8_t kEnd1 = 0xFF;
constexpr uint8_t kEnd2 = 0x00;
constexpr uint8_t kDirectoryAttribute = 0x10;
constexpr size_t kRawDirectoryEntrySize = 64;
constexpr size_t kMaxEntries = 200000;
constexpr unsigned kMaxDepth = 64;
constexpr size_t kMaxDirectoryClusters = 65536;

struct PartitionLayout {
    char letter;
    const char *label;
    uint64_t offset;
    uint64_t size;
};

// Standard Original Xbox retail layout. F is detected separately from the
// remaining bytes of larger homebrew/LBA48 images.
constexpr PartitionLayout kRetailPartitions[] = {
    {'C', "System", 0x8CA80000ull, 0x01F400000ull},
    {'E', "Data",   0xABE80000ull, 0x1312D6000ull},
    {'X', "Cache",  0x00080000ull, 0x02EE00000ull},
    {'Y', "Cache",  0x2EE80000ull, 0x02EE00000ull},
    {'Z', "Cache",  0x5DC80000ull, 0x02EE00000ull},
};
constexpr uint64_t kExtendedFOffset = 0x1DD156000ull;

// LBA48 v3 / XBPartitioner partition table written to HDD sector 0.
// The 16-byte signature is followed by padding to 0x30, then fourteen
// 32-byte entries: 16-byte name, flags, LBA start, LBA size, reserved.
// Partition6/F and Partition7/G are entries 5 and 6 respectively.
constexpr size_t kXbpTableBytes = 512u;
constexpr size_t kXbpEntryOffset = 0x30u;
constexpr size_t kXbpEntryBytes = 0x20u;
constexpr size_t kXbpEntryCount = 14u;
constexpr uint32_t kXbpInUseFlag = 0x80000000u;
constexpr char kXbpMagic[17] = "****PARTINFO****";

constexpr auto ReadLe16 = XemuDebugBinaryUtils::read_le16;
constexpr auto ReadLe32 = XemuDebugBinaryUtils::read_le32;
constexpr auto RangeInside = XemuDebugBinaryUtils::range_inside;

static uint64_t RoundUp4096(uint64_t value)
{
    return (value + 4095ull) & ~4095ull;
}

static bool ValidSectorsPerCluster(uint32_t value)
{
    return value != 0 && value <= 1024 && (value & (value - 1)) == 0;
}


static bool ParseXbpExtendedLayouts(ReadCallback read, void *opaque,
                                    uint64_t image_size,
                                    std::vector<PartitionLayout> &layouts,
                                    bool &table_present)
{
    layouts.clear();
    table_present = false;
    if (!read || image_size < kXbpTableBytes) {
        return false;
    }

    std::array<uint8_t, kXbpTableBytes> sector{};
    if (!read(opaque, 0, sector.data(), sector.size())) {
        return false;
    }
    if (std::memcmp(sector.data(), kXbpMagic, 16) != 0) {
        return true;
    }
    table_present = true;

    struct Candidate {
        char letter;
        const char *label;
        size_t index;
    };
    constexpr Candidate candidates[] = {
        {'F', "Extended F", 5u},
        {'G', "Extended G", 6u},
    };

    for (const Candidate &candidate : candidates) {
        if (candidate.index >= kXbpEntryCount) {
            continue;
        }
        const size_t offset = kXbpEntryOffset + candidate.index * kXbpEntryBytes;
        if (offset + kXbpEntryBytes > sector.size()) {
            continue;
        }
        const uint8_t *raw = sector.data() + offset;
        const uint32_t flags = ReadLe32(raw + 16);
        const uint32_t lba_start = ReadLe32(raw + 20);
        const uint32_t lba_size = ReadLe32(raw + 24);
        if ((flags & kXbpInUseFlag) == 0 || lba_start == 0 || lba_size == 0) {
            continue;
        }

        const uint64_t byte_offset = (uint64_t)lba_start * kSectorSize;
        const uint64_t byte_size = (uint64_t)lba_size * kSectorSize;
        if (byte_offset < kExtendedFOffset || byte_size < kSuperblockSize ||
            !RangeInside(byte_offset, byte_size, image_size)) {
            continue;
        }
        layouts.push_back({candidate.letter, candidate.label,
                           byte_offset, byte_size});
    }

    if (layouts.size() == 2) {
        const PartitionLayout &a = layouts[0];
        const PartitionLayout &b = layouts[1];
        const uint64_t a_end = a.offset + a.size;
        const uint64_t b_end = b.offset + b.size;
        if (!(a_end <= b.offset || b_end <= a.offset)) {
            layouts.clear();
        }
    }
    return true;
}

static std::string SafeFilename(const uint8_t *data, size_t length)
{
    std::string out;
    out.reserve(length);
    char escaped[5];
    for (size_t i = 0; i < length; ++i) {
        const uint8_t c = data[i];
        if (c >= 0x20 && c <= 0x7E) {
            out.push_back((char)c);
        } else {
            std::snprintf(escaped, sizeof(escaped), "\\x%02X", c);
            out += escaped;
        }
    }
    return out;
}

class Parser {
public:
    Parser(ReadCallback read, void *opaque, uint64_t image_size)
        : m_read(read), m_opaque(opaque), m_image_size(image_size)
    {
    }

    bool ParsePartition(Partition &part)
    {
        part.entries.clear();
        part.available = false;

        if (!m_read || part.size < kSuperblockSize ||
            !RangeInside(part.offset, part.size, m_image_size)) {
            part.status = "Partition is outside the mounted HDD image.";
            return false;
        }

        std::array<uint8_t, kSuperblockSize> superblock{};
        if (!Read(part.offset, superblock.data(), superblock.size())) {
            part.status = "Unable to read FATX superblock.";
            return false;
        }
        if (ReadLe32(superblock.data()) != kFatxSignature) {
            part.status = "No FATX signature.";
            return false;
        }

        part.volume_id = ReadLe32(superblock.data() + 4);
        part.sectors_per_cluster = ReadLe32(superblock.data() + 8);
        part.root_cluster = ReadLe32(superblock.data() + 12);
        if (!ValidSectorsPerCluster(part.sectors_per_cluster)) {
            part.status = "Invalid FATX sectors-per-cluster value.";
            return false;
        }

        const uint64_t bytes_per_cluster64 =
            (uint64_t)part.sectors_per_cluster * kSectorSize;
        if (bytes_per_cluster64 > std::numeric_limits<uint32_t>::max()) {
            part.status = "FATX cluster size is too large.";
            return false;
        }
        part.bytes_per_cluster = (uint32_t)bytes_per_cluster64;

        uint64_t fat_entries = part.size / bytes_per_cluster64 + 1;
        part.fat_bits = fat_entries < 0xFFF0ull ? 16 : 32;
        uint64_t fat_size = RoundUp4096(fat_entries * (part.fat_bits / 8));
        if (fat_size + kFatOffset >= part.size) {
            part.status = "FATX allocation table exceeds partition size.";
            return false;
        }

        m_partition = &part;
        m_fat_offset = part.offset + kFatOffset;
        m_fat_size = fat_size;
        m_cluster_offset = m_fat_offset + fat_size;
        m_num_clusters =
            (part.size - fat_size - kFatOffset) / bytes_per_cluster64 + 1;
        m_total_entries = 0;
        m_active_directories.clear();

        if (!ValidCluster(part.root_cluster)) {
            part.status = "FATX root cluster is outside the partition.";
            return false;
        }

        std::string error;
        if (!ReadDirectory(part.root_cluster, 0, part.entries, error)) {
            part.status = error.empty() ? "Unable to read FATX root directory."
                                        : error;
            return false;
        }

        part.available = true;
        char text[128];
        std::snprintf(text, sizeof(text),
                      "FAT%u, %u KiB clusters, %zu entries",
                      part.fat_bits, part.bytes_per_cluster / 1024,
                      CountEntries(part.entries));
        part.status = text;
        return true;
    }

private:
    bool Read(uint64_t offset, void *buffer, size_t size) const
    {
        return RangeInside(offset, size, m_image_size) &&
               m_read(m_opaque, offset, buffer, size);
    }

    bool ValidCluster(uint32_t cluster) const
    {
        return cluster >= 1 && (uint64_t)cluster < m_num_clusters + 1;
    }

    bool ClusterOffset(uint32_t cluster, uint64_t &offset) const
    {
        if (!ValidCluster(cluster)) {
            return false;
        }
        const uint64_t rel = (uint64_t)(cluster - 1) *
                             m_partition->bytes_per_cluster;
        if (m_cluster_offset < m_partition->offset ||
            rel > std::numeric_limits<uint64_t>::max() - m_cluster_offset) {
            return false;
        }
        offset = m_cluster_offset + rel;
        return RangeInside(offset, m_partition->bytes_per_cluster,
                           m_partition->offset + m_partition->size);
    }

    enum class NextClusterResult { Next, End, Error };

    NextClusterResult NextCluster(uint32_t cluster, uint32_t &next) const
    {
        if (!ValidCluster(cluster)) {
            return NextClusterResult::Error;
        }
        const uint32_t entry_bytes = m_partition->fat_bits / 8;
        const uint64_t offset = m_fat_offset + (uint64_t)cluster * entry_bytes;
        if (!RangeInside(offset, entry_bytes, m_fat_offset + m_fat_size)) {
            return NextClusterResult::Error;
        }

        uint8_t raw[4] = {};
        if (!Read(offset, raw, entry_bytes)) {
            return NextClusterResult::Error;
        }
        uint32_t value = entry_bytes == 2 ? ReadLe16(raw) : ReadLe32(raw);
        const uint32_t reserved = entry_bytes == 2 ? 0xFFF0u : 0xFFFFFFF0u;
        const uint32_t end = entry_bytes == 2 ? 0xFFFFu : 0xFFFFFFFFu;
        if (value == end) {
            return NextClusterResult::End;
        }
        if (value == 0 || value >= reserved || !ValidCluster(value)) {
            return NextClusterResult::Error;
        }
        next = value;
        return NextClusterResult::Next;
    }

    bool ReadDirectory(uint32_t first_cluster, unsigned depth,
                       std::vector<Entry> &entries, std::string &error)
    {
        if (depth > kMaxDepth) {
            error = "FATX directory nesting exceeds safety limit.";
            return false;
        }
        if (!ValidCluster(first_cluster)) {
            error = "FATX directory points to an invalid cluster.";
            return false;
        }
        if (!m_active_directories.insert(first_cluster).second) {
            error = "FATX directory cycle detected.";
            return false;
        }

        struct ActiveGuard {
            std::unordered_set<uint32_t> &set;
            uint32_t cluster;
            ~ActiveGuard() { set.erase(cluster); }
        } guard{m_active_directories, first_cluster};

        std::unordered_set<uint32_t> chain_seen;
        uint32_t cluster = first_cluster;
        std::vector<uint8_t> buffer(m_partition->bytes_per_cluster);

        for (;;) {
            if (!chain_seen.insert(cluster).second) {
                error = "FATX directory cluster-chain cycle detected.";
                return false;
            }
            if (chain_seen.size() > kMaxDirectoryClusters) {
                error = "FATX directory cluster chain exceeds safety limit.";
                return false;
            }
            uint64_t cluster_offset = 0;
            if (!ClusterOffset(cluster, cluster_offset) ||
                !Read(cluster_offset, buffer.data(), buffer.size())) {
                error = "Unable to read FATX directory cluster.";
                return false;
            }

            const size_t count = buffer.size() / kRawDirectoryEntrySize;
            for (size_t i = 0; i < count; ++i) {
                const uint8_t *raw = buffer.data() + i * kRawDirectoryEntrySize;
                const uint8_t filename_len = raw[0];
                if (filename_len == kEnd1 || filename_len == kEnd2) {
                    return true;
                }
                if (filename_len == kDeleted) {
                    continue;
                }
                if (filename_len > kMaxFilename) {
                    error = "Invalid FATX directory filename length.";
                    return false;
                }
                if (++m_total_entries > kMaxEntries) {
                    error = "FATX directory entry count exceeds safety limit.";
                    return false;
                }

                Entry entry;
                entry.attributes = raw[1];
                entry.directory_entry_offset =
                    cluster_offset + i * kRawDirectoryEntrySize;
                entry.name.assign(reinterpret_cast<const char *>(raw + 2),
                                  filename_len);
                entry.display_name = SafeFilename(raw + 2, filename_len);
                entry.first_cluster = ReadLe32(raw + 44);
                entry.file_size = ReadLe32(raw + 48);
                entry.modified_time = ReadLe16(raw + 52);
                entry.modified_date = ReadLe16(raw + 54);
                entry.directory = (entry.attributes & kDirectoryAttribute) != 0;

                if (entry.directory) {
                    if (!ReadDirectory(entry.first_cluster, depth + 1,
                                       entry.children, error)) {
                        return false;
                    }
                }
                entries.emplace_back(std::move(entry));
            }

            uint32_t next = 0;
            const NextClusterResult result = NextCluster(cluster, next);
            if (result == NextClusterResult::Next) {
                cluster = next;
                continue;
            }
            if (result == NextClusterResult::End) {
                // A well-formed FATX directory normally has an explicit 0x00
                // or 0xFF entry before chain end. Accept chain end read-only;
                // the snapshot already contains every complete entry.
                return true;
            }
            error = "Invalid FATX directory cluster chain.";
            return false;
        }
    }

    static size_t CountEntries(const std::vector<Entry> &entries)
    {
        size_t count = entries.size();
        for (const Entry &entry : entries) {
            count += CountEntries(entry.children);
        }
        return count;
    }

    ReadCallback m_read = nullptr;
    void *m_opaque = nullptr;
    uint64_t m_image_size = 0;
    Partition *m_partition = nullptr;
    uint64_t m_fat_offset = 0;
    uint64_t m_fat_size = 0;
    uint64_t m_cluster_offset = 0;
    uint64_t m_num_clusters = 0;
    size_t m_total_entries = 0;
    std::unordered_set<uint32_t> m_active_directories;
};

} // namespace

bool BuildSnapshot(ReadCallback read, void *opaque, uint64_t image_size,
                   Snapshot &snapshot)
{
    snapshot = {};
    snapshot.image_size = image_size;
    if (!read || image_size == 0) {
        snapshot.status = "Xbox HDD block device is not available.";
        return false;
    }

    snapshot.hdd_available = true;
    Parser parser(read, opaque, image_size);
    for (const PartitionLayout &layout : kRetailPartitions) {
        Partition part;
        part.letter = layout.letter;
        part.label = layout.label;
        part.offset = layout.offset;
        part.size = layout.size;
        parser.ParsePartition(part);
        snapshot.partitions.emplace_back(std::move(part));
    }

    std::vector<PartitionLayout> extended_layouts;
    bool xbp_table_present = false;
    const bool xbp_read_ok = ParseXbpExtendedLayouts(
        read, opaque, image_size, extended_layouts, xbp_table_present);
    if (xbp_table_present) {
        // When an XBP/LBA48 v3 table is present, it is authoritative. Never
        // fall back to treating all remaining bytes as F:, because that would
        // overlap a valid G: partition if the table is split F/G.
        for (const PartitionLayout &layout : extended_layouts) {
            Partition part;
            part.letter = layout.letter;
            part.label = layout.label;
            part.offset = layout.offset;
            part.size = layout.size;
            parser.ParsePartition(part);
            snapshot.partitions.emplace_back(std::move(part));
        }
    } else if (xbp_read_ok && image_size > kExtendedFOffset + kSuperblockSize) {
        // Legacy .06/F-only layouts have no sector-0 table. Preserve the
        // historical behavior in that case: Partition6/F owns the remaining
        // bytes after the retail layout.
        Partition part;
        part.letter = 'F';
        part.label = "Extended F (legacy)";
        part.offset = kExtendedFOffset;
        part.size = image_size - kExtendedFOffset;
        parser.ParsePartition(part);
        if (part.available) {
            snapshot.partitions.emplace_back(std::move(part));
        }
    }

    size_t available = 0;
    for (const Partition &part : snapshot.partitions) {
        if (part.available) {
            ++available;
        }
    }
    snapshot.status = available != 0
        ? "FATX snapshot captured from ide0-hd0."
        : "HDD is present, but no readable FATX partitions were found.";
    return available != 0;
}


bool BuildPartitionSnapshot(ReadCallback read, void *opaque,
                            uint64_t image_size, char partition_letter,
                            Snapshot &snapshot)
{
    snapshot = {};
    snapshot.image_size = image_size;
    if (!read || image_size == 0) {
        snapshot.status = "Xbox HDD block device is not available.";
        return false;
    }
    snapshot.hdd_available = true;
    const char wanted = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));

    PartitionLayout selected = {'?', "", 0, 0};
    bool found = false;
    for (const PartitionLayout &layout : kRetailPartitions) {
        if (layout.letter == wanted) {
            selected = layout;
            found = true;
            break;
        }
    }

    if (!found && (wanted == 'F' || wanted == 'G')) {
        std::vector<PartitionLayout> extended_layouts;
        bool xbp_table_present = false;
        const bool xbp_read_ok = ParseXbpExtendedLayouts(
            read, opaque, image_size, extended_layouts, xbp_table_present);
        if (xbp_table_present) {
            for (const PartitionLayout &layout : extended_layouts) {
                if (layout.letter == wanted) {
                    selected = layout;
                    found = true;
                    break;
                }
            }
        } else if (xbp_read_ok && wanted == 'F' &&
                   image_size > kExtendedFOffset + kSuperblockSize) {
            selected = {'F', "Extended F (legacy)", kExtendedFOffset,
                        image_size - kExtendedFOffset};
            found = true;
        }
    }

    if (!found) {
        snapshot.status = std::string("FATX volume ") + wanted +
                          ": is not present on this HDD layout.";
        return false;
    }

    Parser parser(read, opaque, image_size);
    Partition part;
    part.letter = selected.letter;
    part.label = selected.label;
    part.offset = selected.offset;
    part.size = selected.size;
    parser.ParsePartition(part);
    const bool available = part.available;
    snapshot.partitions.emplace_back(std::move(part));
    snapshot.status = available
        ? std::string("Fresh FATX ") + wanted + ": partition snapshot captured."
        : std::string("FATX volume ") + wanted + ": is not readable.";
    return available;
}


bool ReadFileRange(ReadCallback read, void *read_opaque, uint64_t image_size,
                   const Partition &partition, const Entry &entry,
                   uint64_t file_offset, size_t max_bytes,
                   std::vector<uint8_t> &output, std::string &error)
{
    output.clear();
    error.clear();
    if (!read || entry.directory || !partition.available ||
        partition.bytes_per_cluster == 0 ||
        (partition.fat_bits != 16 && partition.fat_bits != 32) ||
        file_offset > entry.file_size) {
        error = "Invalid FATX file-range parameters.";
        return false;
    }
    const uint64_t wanted = std::min<uint64_t>(max_bytes, entry.file_size - file_offset);
    if (wanted == 0) return true;

    const uint64_t cluster_size = partition.bytes_per_cluster;
    const uint64_t fat_entries = partition.size / cluster_size + 1;
    const uint64_t fat_size = RoundUp4096(fat_entries * (partition.fat_bits / 8));
    if (fat_size + kFatOffset >= partition.size) {
        error = "Invalid FATX allocation table.";
        return false;
    }
    const uint64_t fat_offset = partition.offset + kFatOffset;
    const uint64_t cluster_offset = fat_offset + fat_size;
    const uint64_t num_clusters = (partition.size - fat_size - kFatOffset) / cluster_size + 1;
    auto valid_cluster=[&](uint32_t c){return c>=1 && (uint64_t)c<num_clusters+1;};
    auto next_cluster=[&](uint32_t c,uint32_t &next)->bool {
        const uint32_t eb=partition.fat_bits/8;
        const uint64_t o=fat_offset+(uint64_t)c*eb;
        if(!valid_cluster(c)||!RangeInside(o,eb,fat_offset+fat_size)||!RangeInside(o,eb,image_size)) return false;
        uint8_t raw[4]={}; if(!read(read_opaque,o,raw,eb)) return false;
        const uint32_t v=eb==2?ReadLe16(raw):ReadLe32(raw);
        const uint32_t reserved=eb==2?0xFFF0u:0xFFFFFFF0u;
        const uint32_t end=eb==2?0xFFFFu:0xFFFFFFFFu;
        if(v==end||v==0||v>=reserved||!valid_cluster(v)) return false;
        next=v; return true;
    };
    if (!valid_cluster(entry.first_cluster)) { error="FATX file points to an invalid first cluster."; return false; }
    uint32_t cluster=entry.first_cluster;
    uint64_t skip_clusters=file_offset/cluster_size;
    std::unordered_set<uint32_t> seen;
    for(uint64_t i=0;i<skip_clusters;i++) {
        if(!seen.insert(cluster).second){error="FATX file cluster-chain cycle detected.";return false;}
        uint32_t next=0; if(!next_cluster(cluster,next)){error="FATX file cluster chain ended before requested range.";return false;} cluster=next;
    }
    uint64_t in_cluster=file_offset%cluster_size;
    uint64_t remaining=wanted;
    output.reserve((size_t)wanted);
    while(remaining) {
        if(!seen.insert(cluster).second){error="FATX file cluster-chain cycle detected.";return false;}
        const uint64_t rel=(uint64_t)(cluster-1)*cluster_size;
        const uint64_t disk=cluster_offset+rel+in_cluster;
        const size_t amount=(size_t)std::min<uint64_t>(remaining,cluster_size-in_cluster);
        if(!RangeInside(disk,amount,partition.offset+partition.size)||!RangeInside(disk,amount,image_size)) {error="FATX file range is outside the partition.";return false;}
        const size_t old=output.size(); output.resize(old+amount);
        if(!read(read_opaque,disk,output.data()+old,amount)){error="Unable to read FATX file data.";return false;}
        remaining-=amount; in_cluster=0;
        if(remaining){uint32_t next=0;if(!next_cluster(cluster,next)){error="FATX file cluster chain ended before requested range.";return false;}cluster=next;}
    }
    return true;
}

bool ReadFileRangeSequential(ReadCallback read, void *read_opaque,
                             uint64_t image_size, const Partition &partition,
                             const Entry &entry, uint64_t file_offset,
                             size_t max_bytes, FileReadCursor &cursor,
                             std::vector<uint8_t> &output, std::string &error)
{
    output.clear();
    error.clear();
    if (!read || entry.directory || !partition.available ||
        partition.bytes_per_cluster == 0 ||
        (partition.fat_bits != 16 && partition.fat_bits != 32) ||
        file_offset > entry.file_size) {
        error = "Invalid sequential FATX file-range parameters.";
        return false;
    }

    const uint64_t wanted = std::min<uint64_t>(
        max_bytes, entry.file_size - file_offset);
    if (wanted == 0) {
        cursor.next_offset = file_offset;
        return true;
    }

    const uint64_t cluster_size = partition.bytes_per_cluster;
    const uint64_t fat_entries = partition.size / cluster_size + 1;
    const uint64_t fat_size = RoundUp4096(
        fat_entries * (partition.fat_bits / 8));
    if (fat_size + kFatOffset >= partition.size) {
        error = "Invalid FATX allocation table.";
        return false;
    }
    const uint64_t fat_offset = partition.offset + kFatOffset;
    const uint64_t cluster_offset = fat_offset + fat_size;
    const uint64_t num_clusters =
        (partition.size - fat_size - kFatOffset) / cluster_size + 1;
    auto valid_cluster = [&](uint32_t cluster) {
        return cluster >= 1 && static_cast<uint64_t>(cluster) < num_clusters + 1;
    };
    auto next_cluster = [&](uint32_t cluster, uint32_t &next) -> bool {
        const uint32_t entry_bytes = partition.fat_bits / 8;
        const uint64_t offset = fat_offset +
                                static_cast<uint64_t>(cluster) * entry_bytes;
        if (!valid_cluster(cluster) ||
            !RangeInside(offset, entry_bytes, fat_offset + fat_size) ||
            !RangeInside(offset, entry_bytes, image_size)) {
            return false;
        }
        uint8_t raw[4] = {};
        if (!read(read_opaque, offset, raw, entry_bytes)) {
            return false;
        }
        const uint32_t value = entry_bytes == 2 ? ReadLe16(raw) : ReadLe32(raw);
        const uint32_t reserved = entry_bytes == 2 ? 0xFFF0u : 0xFFFFFFF0u;
        const uint32_t end = entry_bytes == 2 ? 0xFFFFu : 0xFFFFFFFFu;
        if (value == end || value == 0 || value >= reserved ||
            !valid_cluster(value)) {
            return false;
        }
        next = value;
        return true;
    };
    auto remember_cluster = [&](uint32_t cluster) -> bool {
        return cursor.visited_clusters.insert(cluster).second;
    };

    uint32_t cluster = entry.first_cluster;
    uint64_t in_cluster = file_offset % cluster_size;
    const bool continuing = cursor.valid &&
                            cursor.next_offset == file_offset &&
                            valid_cluster(cursor.cluster) &&
                            cursor.in_cluster <= cluster_size;
    if (continuing) {
        cluster = cursor.cluster;
        in_cluster = cursor.in_cluster;
        if (cursor.visited_clusters.empty()) {
            cursor.visited_clusters.insert(cluster);
        }
    } else {
        cursor = {};
        if (!valid_cluster(cluster)) {
            error = "FATX file points to an invalid first cluster.";
            return false;
        }
        remember_cluster(cluster);
        const uint64_t skip = file_offset / cluster_size;
        for (uint64_t i = 0; i < skip; ++i) {
            uint32_t next = 0;
            if (!next_cluster(cluster, next)) {
                error = "FATX file cluster chain ended before requested range.";
                return false;
            }
            if (!remember_cluster(next)) {
                error = "FATX file cluster-chain cycle detected.";
                return false;
            }
            cluster = next;
        }
    }

    // A previous sequential request may end exactly at a cluster boundary.
    // Advance here rather than performing a zero-byte read from that cluster.
    if (in_cluster == cluster_size) {
        uint32_t next = 0;
        if (!next_cluster(cluster, next)) {
            error = "FATX file cluster chain ended before requested range.";
            return false;
        }
        if (!remember_cluster(next)) {
            error = "FATX file cluster-chain cycle detected.";
            return false;
        }
        cluster = next;
        in_cluster = 0;
    }

    uint64_t remaining = wanted;
    output.reserve(static_cast<size_t>(wanted));
    while (remaining != 0) {
        const uint64_t disk = cluster_offset +
                              static_cast<uint64_t>(cluster - 1) * cluster_size +
                              in_cluster;
        const size_t amount = static_cast<size_t>(std::min<uint64_t>(
            remaining, cluster_size - in_cluster));
        if (amount == 0 ||
            !RangeInside(disk, amount, partition.offset + partition.size) ||
            !RangeInside(disk, amount, image_size)) {
            error = amount == 0
                ? "FATX sequential file read made no forward progress."
                : "FATX file range is outside the partition.";
            return false;
        }
        const size_t old_size = output.size();
        output.resize(old_size + amount);
        if (!read(read_opaque, disk, output.data() + old_size, amount)) {
            error = "Unable to read FATX file data.";
            return false;
        }
        remaining -= amount;
        in_cluster += amount;
        if (remaining != 0) {
            uint32_t next = 0;
            if (in_cluster != cluster_size || !next_cluster(cluster, next)) {
                error = "FATX file cluster chain ended before requested range.";
                return false;
            }
            if (!remember_cluster(next)) {
                error = "FATX file cluster-chain cycle detected.";
                return false;
            }
            cluster = next;
            in_cluster = 0;
        }
    }

    cursor.valid = true;
    cursor.next_offset = file_offset + output.size();
    cursor.cluster = cluster;
    cursor.in_cluster = static_cast<uint32_t>(in_cluster);
    return true;
}

namespace {

static bool EqualsAsciiNoCase(const std::string &a, const char *b)
{
    size_t n = std::char_traits<char>::length(b);
    if (a.size() != n) {
        return false;
    }
    for (size_t i = 0; i < n; ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (ca >= 'A' && ca <= 'Z') ca = static_cast<unsigned char>(ca + ('a' - 'A'));
        if (cb >= 'A' && cb <= 'Z') cb = static_cast<unsigned char>(cb + ('a' - 'A'));
        if (ca != cb) {
            return false;
        }
    }
    return true;
}

static Entry *FindChildNoCase(std::vector<Entry> &entries, const char *name)
{
    for (Entry &entry : entries) {
        if (EqualsAsciiNoCase(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

static void AppendUtf8(std::string &out, uint32_t cp)
{
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

static std::string DecodeMetadataText(const std::vector<uint8_t> &bytes)
{
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        std::string out;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            uint32_t cp = static_cast<uint32_t>(bytes[i]) |
                          (static_cast<uint32_t>(bytes[i + 1]) << 8);
            if (cp == 0) {
                break;
            }
            if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < bytes.size()) {
                uint32_t low = static_cast<uint32_t>(bytes[i + 2]) |
                               (static_cast<uint32_t>(bytes[i + 3]) << 8);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                }
            }
            if (cp == '\r') {
                continue;
            }
            AppendUtf8(out, cp);
        }
        return out;
    }

    std::string out;
    out.reserve(bytes.size());
    for (uint8_t c : bytes) {
        if (c == 0) {
            break;
        }
        if (c != '\r') {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

static std::string MetadataValue(const std::vector<uint8_t> &bytes,
                                 const char *key)
{
    const std::string text = DecodeMetadataText(bytes);
    const std::string prefix = std::string(key) + "=";
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) {
            end = text.size();
        }
        std::string line = text.substr(pos, end - pos);
        size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos) {
            line.erase(0, first);
        }
        if (line.size() >= prefix.size()) {
            bool match = true;
            for (size_t i = 0; i < prefix.size(); ++i) {
                unsigned char a = static_cast<unsigned char>(line[i]);
                unsigned char b = static_cast<unsigned char>(prefix[i]);
                if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
                if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
                if (a != b) {
                    match = false;
                    break;
                }
            }
            if (match) {
                std::string value = line.substr(prefix.size());
                while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
                    value.pop_back();
                }
                return value;
            }
        }
        if (end == text.size()) {
            break;
        }
        pos = end + 1;
    }
    return {};
}

struct VectorWriter {
    std::vector<uint8_t> *bytes = nullptr;
    size_t limit = 0;
};

static bool WriteVector(void *opaque, const void *buffer, size_t size)
{
    VectorWriter *writer = static_cast<VectorWriter *>(opaque);
    if (!writer || !writer->bytes || size > writer->limit - writer->bytes->size()) {
        return false;
    }
    const uint8_t *src = static_cast<const uint8_t *>(buffer);
    writer->bytes->insert(writer->bytes->end(), src, src + size);
    return true;
}

static bool ReadSmallFile(ReadCallback read, void *opaque, uint64_t image_size,
                          const Partition &partition, const Entry &entry,
                          std::vector<uint8_t> &bytes)
{
    constexpr size_t kMaxMetadataBytes = 64 * 1024;
    bytes.clear();
    if (entry.directory || entry.file_size > kMaxMetadataBytes) {
        return false;
    }
    bytes.reserve(entry.file_size);
    VectorWriter writer{&bytes, kMaxMetadataBytes};
    std::string error;
    return StreamFile(read, opaque, image_size, partition, entry,
                      WriteVector, &writer, error);
}

static void PopulateTitleAreaMetadata(ReadCallback read, void *opaque,
                                      uint64_t image_size,
                                      Partition &partition, Entry &area,
                                      bool save_names)
{
    for (Entry &title : area.children) {
        if (!title.directory) {
            continue;
        }
        if (const Entry *meta = FindChildNoCase(title.children, "TitleMeta.xbx")) {
            std::vector<uint8_t> bytes;
            if (ReadSmallFile(read, opaque, image_size, partition, *meta, bytes)) {
                title.friendly_name = MetadataValue(bytes, "TitleName");
            }
        }

        if (!save_names) {
            continue;
        }
        for (Entry &save : title.children) {
            if (!save.directory) {
                continue;
            }
            if (const Entry *meta = FindChildNoCase(save.children, "SaveMeta.xbx")) {
                std::vector<uint8_t> bytes;
                if (ReadSmallFile(read, opaque, image_size, partition, *meta, bytes)) {
                    save.friendly_name = MetadataValue(bytes, "Name");
                }
            }
        }
    }
}

} // namespace

bool QueryFreeSpace(ReadCallback read, void *read_opaque, uint64_t image_size,
                    const Partition &partition, uint64_t &free_bytes,
                    uint64_t &total_data_bytes, std::string &error)
{
    free_bytes = 0;
    total_data_bytes = 0;
    error.clear();
    if (!read || !partition.available || partition.bytes_per_cluster == 0 ||
        (partition.fat_bits != 16 && partition.fat_bits != 32)) {
        error = "Invalid FATX capacity query parameters.";
        return false;
    }
    const uint32_t entry_bytes = partition.fat_bits / 8;
    const uint64_t fat_entries = partition.size / partition.bytes_per_cluster + 1;
    const uint64_t fat_size = RoundUp4096(fat_entries * entry_bytes);
    if (fat_size + kFatOffset >= partition.size) {
        error = "FATX capacity query found an invalid allocation table.";
        return false;
    }
    const uint64_t num_clusters =
        (partition.size - fat_size - kFatOffset) / partition.bytes_per_cluster + 1;
    const uint64_t fat_offset = partition.offset + kFatOffset;
    constexpr size_t kScanBytes = 64 * 1024;
    const size_t entries_per_scan = kScanBytes / entry_bytes;
    std::vector<uint8_t> buffer(entries_per_scan * entry_bytes);
    uint64_t free_clusters = 0;
    uint64_t cluster = 1;
    while (cluster <= num_clusters) {
        const size_t count = static_cast<size_t>(
            std::min<uint64_t>(entries_per_scan, num_clusters - cluster + 1));
        const size_t bytes = count * entry_bytes;
        const uint64_t offset = fat_offset + cluster * entry_bytes;
        if (!RangeInside(offset, bytes, partition.offset + partition.size) ||
            !RangeInside(offset, bytes, image_size) ||
            !read(read_opaque, offset, buffer.data(), bytes)) {
            error = "Unable to read FATX allocation table for free-space preflight.";
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            const uint8_t *raw = buffer.data() + i * entry_bytes;
            const uint32_t value = entry_bytes == 2 ? ReadLe16(raw) : ReadLe32(raw);
            if (value == 0) {
                ++free_clusters;
            }
        }
        cluster += count;
    }
    total_data_bytes = num_clusters * static_cast<uint64_t>(partition.bytes_per_cluster);
    free_bytes = free_clusters * static_cast<uint64_t>(partition.bytes_per_cluster);
    return true;
}

const Partition *FindPartition(const Snapshot &snapshot, char letter)
{
    for (const Partition &partition : snapshot.partitions) {
        if (partition.letter == letter) {
            return &partition;
        }
    }
    return nullptr;
}

Partition *FindPartition(Snapshot &snapshot, char letter)
{
    for (Partition &partition : snapshot.partitions) {
        if (partition.letter == letter) {
            return &partition;
        }
    }
    return nullptr;
}

const Entry *FindEntry(const Partition &partition,
                       const std::vector<std::string> &path)
{
    const std::vector<Entry> *entries = &partition.entries;
    const Entry *current = nullptr;
    for (const std::string &part : path) {
        current = nullptr;
        for (const Entry &entry : *entries) {
            if (entry.name == part) {
                current = &entry;
                break;
            }
        }
        if (!current) {
            return nullptr;
        }
        entries = &current->children;
    }
    return current;
}

Entry *FindEntry(Partition &partition, const std::vector<std::string> &path)
{
    std::vector<Entry> *entries = &partition.entries;
    Entry *current = nullptr;
    for (const std::string &part : path) {
        current = nullptr;
        for (Entry &entry : *entries) {
            if (entry.name == part) {
                current = &entry;
                break;
            }
        }
        if (!current) {
            return nullptr;
        }
        entries = &current->children;
    }
    return current;
}

std::string DisplayName(const Entry &entry)
{
    const std::string &base = entry.display_name.empty()
        ? entry.name : entry.display_name;
    if (entry.friendly_name.empty()) {
        return base;
    }
    return base + " - " + entry.friendly_name;
}

bool StreamFile(ReadCallback read, void *read_opaque, uint64_t image_size,
                const Partition &partition, const Entry &entry,
                WriteCallback write, void *write_opaque, std::string &error)
{
    error.clear();
    if (!read || !write || entry.directory || !partition.available ||
        partition.bytes_per_cluster == 0 ||
        (partition.fat_bits != 16 && partition.fat_bits != 32)) {
        error = "Invalid FATX file export parameters.";
        return false;
    }
    if (entry.file_size == 0) {
        return true;
    }

    const uint64_t cluster_size = partition.bytes_per_cluster;
    const uint64_t fat_entries = partition.size / cluster_size + 1;
    const uint64_t fat_size = RoundUp4096(fat_entries * (partition.fat_bits / 8));
    if (fat_size + kFatOffset >= partition.size) {
        error = "Invalid FATX allocation table.";
        return false;
    }
    const uint64_t fat_offset = partition.offset + kFatOffset;
    const uint64_t cluster_offset = fat_offset + fat_size;
    const uint64_t num_clusters =
        (partition.size - fat_size - kFatOffset) / cluster_size + 1;

    auto valid_cluster = [num_clusters](uint32_t cluster) {
        return cluster >= 1 && static_cast<uint64_t>(cluster) < num_clusters + 1;
    };
    auto cluster_disk_offset = [&](uint32_t cluster, uint64_t &offset) {
        if (!valid_cluster(cluster)) {
            return false;
        }
        const uint64_t rel = static_cast<uint64_t>(cluster - 1) * cluster_size;
        if (rel > std::numeric_limits<uint64_t>::max() - cluster_offset) {
            return false;
        }
        offset = cluster_offset + rel;
        return RangeInside(offset, cluster_size, partition.offset + partition.size) &&
               RangeInside(offset, cluster_size, image_size);
    };
    auto next_cluster = [&](uint32_t cluster, uint32_t &next) {
        const uint32_t entry_bytes = partition.fat_bits / 8;
        const uint64_t offset = fat_offset + static_cast<uint64_t>(cluster) * entry_bytes;
        if (!valid_cluster(cluster) ||
            !RangeInside(offset, entry_bytes, fat_offset + fat_size) ||
            !RangeInside(offset, entry_bytes, image_size)) {
            return false;
        }
        uint8_t raw[4] = {};
        if (!read(read_opaque, offset, raw, entry_bytes)) {
            return false;
        }
        const uint32_t value = entry_bytes == 2 ? ReadLe16(raw) : ReadLe32(raw);
        const uint32_t reserved = entry_bytes == 2 ? 0xFFF0u : 0xFFFFFFF0u;
        const uint32_t end = entry_bytes == 2 ? 0xFFFFu : 0xFFFFFFFFu;
        if (value == end || value == 0 || value >= reserved || !valid_cluster(value)) {
            return false;
        }
        next = value;
        return true;
    };

    if (!valid_cluster(entry.first_cluster)) {
        error = "FATX file points to an invalid first cluster.";
        return false;
    }

    uint64_t remaining = entry.file_size;
    uint32_t cluster = entry.first_cluster;
    std::unordered_set<uint32_t> seen;
    std::vector<uint8_t> buffer(static_cast<size_t>(cluster_size));
    const uint64_t needed_clusters = (remaining + cluster_size - 1) / cluster_size;

    while (remaining != 0) {
        if (!seen.insert(cluster).second || seen.size() > needed_clusters) {
            error = "FATX file cluster-chain cycle detected.";
            return false;
        }
        uint64_t offset = 0;
        if (!cluster_disk_offset(cluster, offset)) {
            error = "FATX file cluster is outside the partition.";
            return false;
        }
        const size_t amount = static_cast<size_t>(std::min<uint64_t>(remaining, cluster_size));
        if (!read(read_opaque, offset, buffer.data(), amount)) {
            error = "Unable to read FATX file data.";
            return false;
        }
        if (!write(write_opaque, buffer.data(), amount)) {
            error = "Unable to write exported host file.";
            return false;
        }
        remaining -= amount;
        if (remaining == 0) {
            break;
        }
        uint32_t next = 0;
        if (!next_cluster(cluster, next)) {
            error = "FATX file cluster chain ended before file size.";
            return false;
        }
        cluster = next;
    }
    return true;
}


bool PopulateXboxMetadata(ReadCallback read, void *opaque, uint64_t image_size,
                          Snapshot &snapshot, std::string &warning)
{
    warning.clear();
    Partition *data = FindPartition(snapshot, 'E');
    if (!data || !data->available) {
        return false;
    }

    if (Entry *udata = FindChildNoCase(data->entries, "UDATA")) {
        if (udata->directory) {
            PopulateTitleAreaMetadata(read, opaque, image_size, *data, *udata,
                                      true);
        }
    }
    if (Entry *tdata = FindChildNoCase(data->entries, "TDATA")) {
        if (tdata->directory) {
            PopulateTitleAreaMetadata(read, opaque, image_size, *data, *tdata,
                                      false);
        }
    }
    return true;
}

std::string FormatTimestamp(uint16_t date, uint16_t time)
{
    if (date == 0 && time == 0) {
        return {};
    }
    const unsigned year = ((date >> 9) & 0x7F) + 2000;
    const unsigned month = (date >> 5) & 0x0F;
    const unsigned day = date & 0x1F;
    const unsigned hour = (time >> 11) & 0x1F;
    const unsigned minute = (time >> 5) & 0x3F;
    const unsigned second = (time & 0x1F) * 2;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u %02u:%02u:%02u",
                  year, month, day, hour, minute, second);
    return buffer;
}

} // namespace XemuFatxHdd
