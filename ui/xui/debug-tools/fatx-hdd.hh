//
// xemu FATX HDD snapshot/parser + restricted mutation helpers
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace XemuFatxHdd {

using ReadCallback = bool (*)(void *opaque, uint64_t offset, void *buffer,
                              size_t size);
using WriteCallback = bool (*)(void *opaque, const void *buffer, size_t size);

struct Entry {
    // Exact on-disk FATX filename bytes. This is the authoritative path token
    // used for lookup and Xbox kernel operations; it may not be valid UTF-8.
    std::string name;
    // Escaped UTF-8-safe rendering of `name` for UI/export display.
    std::string display_name;
    // Optional display-only name discovered from Xbox TitleMeta.xbx or
    // SaveMeta.xbx metadata. The FATX on-disk name remains authoritative.
    std::string friendly_name;
    uint8_t attributes = 0;
    // Absolute byte offset of this 64-byte FATX directory entry in the HDD
    // image. Used only by explicitly requested write operations after a fresh
    // snapshot/path re-resolution.
    uint64_t directory_entry_offset = 0;
    uint32_t first_cluster = 0;
    uint32_t file_size = 0;
    uint16_t modified_time = 0;
    uint16_t modified_date = 0;
    bool directory = false;
    std::vector<Entry> children;
};

struct Partition {
    char letter = '?';
    std::string label;
    uint64_t offset = 0;
    uint64_t size = 0;
    bool available = false;
    uint32_t volume_id = 0;
    uint32_t sectors_per_cluster = 0;
    uint32_t root_cluster = 0;
    uint32_t bytes_per_cluster = 0;
    uint8_t fat_bits = 0;
    std::string status;
    std::vector<Entry> entries;
};

struct Snapshot {
    bool hdd_available = false;
    uint64_t image_size = 0;
    std::string status;
    std::vector<Partition> partitions;
};

bool BuildSnapshot(ReadCallback read, void *opaque, uint64_t image_size,
                   Snapshot &snapshot);
// Build a fresh coherent snapshot for one FATX volume only. This keeps
// verification freshness while avoiding reparsing unrelated volumes.
bool BuildPartitionSnapshot(ReadCallback read, void *opaque,
                            uint64_t image_size, char partition_letter,
                            Snapshot &snapshot);

// Populate display-only UDATA/TDATA Title IDs and UDATA save-folder names from
// TitleMeta.xbx / SaveMeta.xbx. The snapshot remains read-only and the raw FATX
// names are never changed.
bool PopulateXboxMetadata(ReadCallback read, void *opaque, uint64_t image_size,
                          Snapshot &snapshot, std::string &warning);

// Stream one FATX file using metadata from a coherent snapshot. No writes are
// ever issued to the Xbox HDD; `write` receives only host-export bytes.
bool StreamFile(ReadCallback read, void *read_opaque, uint64_t image_size,
                const Partition &partition, const Entry &entry,
                WriteCallback write, void *write_opaque, std::string &error);
bool ReadFileRange(ReadCallback read, void *read_opaque, uint64_t image_size,
                   const Partition &partition, const Entry &entry,
                   uint64_t file_offset, size_t max_bytes,
                   std::vector<uint8_t> &output, std::string &error);

struct FileReadCursor {
    bool valid = false;
    uint64_t next_offset = 0;
    uint32_t cluster = 0;
    uint32_t in_cluster = 0;
    // Tracks cluster identity across sequential chunks so a corrupt FAT cycle
    // cannot evade detection merely because the caller uses a cached cursor.
    std::unordered_set<uint32_t> visited_clusters;
};
bool ReadFileRangeSequential(ReadCallback read, void *read_opaque,
                             uint64_t image_size, const Partition &partition,
                             const Entry &entry, uint64_t file_offset,
                             size_t max_bytes, FileReadCursor &cursor,
                             std::vector<uint8_t> &output, std::string &error);

// Read FAT allocation entries for an already-validated partition and report
// usable data capacity without mutating the volume. Intended for operation
// preflight, not per-chunk post-write verification.
bool QueryFreeSpace(ReadCallback read, void *read_opaque, uint64_t image_size,
                    const Partition &partition, uint64_t &free_bytes,
                    uint64_t &total_data_bytes, std::string &error);

// Production FATX access is read-only. All HDD mutation is routed through
// the running Xbox kernel/FATX driver via Guest Kernel RPC.

const Partition *FindPartition(const Snapshot &snapshot, char letter);
Partition *FindPartition(Snapshot &snapshot, char letter);
const Entry *FindEntry(const Partition &partition,
                       const std::vector<std::string> &path);
Entry *FindEntry(Partition &partition, const std::vector<std::string> &path);

std::string DisplayName(const Entry &entry);
std::string FormatTimestamp(uint16_t date, uint16_t time);

} // namespace XemuFatxHdd
