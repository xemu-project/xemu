//
// Shared Xbox HDD FATX snapshot service.
//
// Provides coherent raw snapshots for backend verification and display-enriched
// snapshots for the HDD UI. Filesystem mutation code only marks the service
// dirty; the UI refreshes on its next draw without a backend->window dependency.
//
#pragma once

#include "fatx-hdd.hh"

#include <cstdint>
#include <string>

class HddSnapshotService
{
public:
    struct FatxCompareItem {
        char source_partition = '?';
        std::vector<std::string> source_components;
        uint64_t source_directory_entry_offset = 0;
        uint32_t source_first_cluster = 0;
        uint16_t source_modified_time = 0;
        uint16_t source_modified_date = 0;
        uint8_t source_attributes = 0;
        char destination_partition = '?';
        std::vector<std::string> destination_components;
        uint64_t file_size = 0;
        bool directory = false;
    };

    struct CapacityInfo {
        char partition = '?';
        uint32_t bytes_per_cluster = 0;
        uint64_t free_bytes = 0;
        uint64_t total_data_bytes = 0;
    };

    struct PerformanceStats {
        uint64_t raw_calls = 0;
        uint64_t raw_total_us = 0;
        uint64_t raw_max_us = 0;
        uint64_t partition_calls = 0;
        uint64_t partition_total_us = 0;
        uint64_t partition_max_us = 0;
        uint64_t partition_set_calls = 0;
        uint64_t partition_set_total_us = 0;
        uint64_t partition_set_max_us = 0;
        uint64_t partition_set_partitions = 0;
        uint64_t display_calls = 0;
        uint64_t display_total_us = 0;
        uint64_t display_max_us = 0;
        uint64_t source_chunk_calls = 0;
        uint64_t source_chunk_total_us = 0;
        uint64_t source_chunk_bytes = 0;
        uint64_t content_verify_calls = 0;
        uint64_t content_verify_total_us = 0;
        uint64_t content_verify_bytes = 0;
    };
    bool BuildRawSnapshot(XemuFatxHdd::Snapshot &snapshot,
                          std::string &status) const;
    bool BuildDisplaySnapshot(XemuFatxHdd::Snapshot &snapshot,
                              std::string &status) const;
    bool BuildRawPartitionSnapshot(char partition,
                                   XemuFatxHdd::Snapshot &snapshot,
                                   std::string &status) const;
    bool BuildRawPartitionSetSnapshot(const std::vector<char> &partitions,
                                      XemuFatxHdd::Snapshot &snapshot,
                                      std::string &status) const;
    bool VerifyFatxCopyContents(const std::vector<FatxCompareItem> &items,
                                std::string &error) const;
    bool QueryPartitionCapacity(char partition, CapacityInfo &capacity,
                                std::string &error) const;

    bool ReadFatxFileChunk(char partition,
                           const std::vector<std::string> &components,
                           uint64_t expected_entry_offset,
                           uint32_t expected_first_cluster,
                           uint16_t expected_modified_time,
                           uint16_t expected_modified_date,
                           uint8_t expected_attributes,
                           uint64_t expected_size, uint64_t file_offset,
                           size_t max_bytes, std::vector<uint8_t> &chunk,
                           std::string &error) const;

    void NotifyFilesystemChanged() { ++m_change_generation; }
    uint64_t ChangeGeneration() const { return m_change_generation; }
    PerformanceStats GetPerformanceStats() const { return m_stats; }
    void ResetPerformanceStats() const { m_stats = {}; }

private:
    uint64_t m_change_generation = 1;
    mutable PerformanceStats m_stats;
};

extern HddSnapshotService hdd_snapshot_service;
