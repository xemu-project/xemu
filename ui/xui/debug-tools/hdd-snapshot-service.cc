//
// Shared Xbox HDD FATX snapshot service.
//
#include "hdd-snapshot-service.hh"

#include "disc-block-io.h"
#include "guest-pause-guard.hh"

#include <algorithm>
#include <chrono>
#include <cctype>

HddSnapshotService hdd_snapshot_service;

namespace {

bool ReadHddBlock(void *opaque, uint64_t offset, void *buffer, size_t size)
{
    return xemu_disc_block_pread((XemuDiscBlockHandle)opaque, offset, buffer,
                                 size);
}

bool OpenHdd(XemuDiscBlockHandle &hdd, uint64_t &length, std::string &status)
{
    hdd = xemu_disc_block_by_name("ide0-hd0");
    if (!xemu_disc_block_is_available(hdd)) {
        status = "Xbox HDD (ide0-hd0) is not available.";
        return false;
    }
    const int64_t signed_length = xemu_disc_block_get_length(hdd);
    if (signed_length <= 0) {
        status = "Unable to determine Xbox HDD size.";
        return false;
    }
    length = static_cast<uint64_t>(signed_length);
    return true;
}

class ScopedPerfMeasurement
{
public:
    ScopedPerfMeasurement(uint64_t &calls, uint64_t &total_us,
                          uint64_t *max_us = nullptr,
                          const uint64_t *quantity = nullptr,
                          uint64_t *quantity_total = nullptr,
                          const std::vector<uint8_t> *byte_vector = nullptr)
        : m_start(std::chrono::steady_clock::now()),
          m_calls(calls), m_total_us(total_us), m_max_us(max_us),
          m_quantity(quantity), m_quantity_total(quantity_total),
          m_byte_vector(byte_vector)
    {
    }

    ~ScopedPerfMeasurement()
    {
        const uint64_t us = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now() - m_start)
                .count());
        ++m_calls;
        m_total_us += us;
        if (m_max_us) {
            *m_max_us = std::max(*m_max_us, us);
        }
        if (m_quantity_total) {
            if (m_quantity) {
                *m_quantity_total += *m_quantity;
            } else if (m_byte_vector) {
                *m_quantity_total += m_byte_vector->size();
            }
        }
    }

    ScopedPerfMeasurement(const ScopedPerfMeasurement &) = delete;
    ScopedPerfMeasurement &operator=(const ScopedPerfMeasurement &) = delete;

private:
    std::chrono::steady_clock::time_point m_start;
    uint64_t &m_calls;
    uint64_t &m_total_us;
    uint64_t *m_max_us;
    const uint64_t *m_quantity;
    uint64_t *m_quantity_total;
    const std::vector<uint8_t> *m_byte_vector;
};

} // namespace

bool HddSnapshotService::BuildRawSnapshot(XemuFatxHdd::Snapshot &snapshot,
                                          std::string &status) const
{
    ScopedPerfMeasurement perf(m_stats.raw_calls, m_stats.raw_total_us,
                               &m_stats.raw_max_us);
    snapshot = {};
    status.clear();

    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    if (!OpenHdd(hdd, length, status)) {
        return false;
    }

    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        status = "Unable to pause the Xbox for a coherent HDD snapshot.";
        return false;
    }
    const bool ok = XemuFatxHdd::BuildSnapshot(ReadHddBlock, hdd, length, snapshot);
    status = snapshot.status;
    if (!ok && status.empty()) {
        status = "Unable to build Xbox HDD FATX snapshot.";
    }
    return ok;
}

bool HddSnapshotService::BuildRawPartitionSnapshot(
    char partition, XemuFatxHdd::Snapshot &snapshot, std::string &status) const
{
    ScopedPerfMeasurement perf(m_stats.partition_calls,
                               m_stats.partition_total_us,
                               &m_stats.partition_max_us);

    snapshot = {};
    status.clear();
    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    if (!OpenHdd(hdd, length, status)) {
        return false;
    }
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        status = "Unable to pause the Xbox for a coherent FATX partition snapshot.";
        return false;
    }
    const bool ok = XemuFatxHdd::BuildPartitionSnapshot(
        ReadHddBlock, hdd, length, partition, snapshot);
    status = snapshot.status;
    if (!ok && status.empty()) {
        status = std::string("Unable to build fresh FATX ") + partition +
                 ": partition snapshot.";
    }
    return ok;
}

bool HddSnapshotService::BuildRawPartitionSetSnapshot(
    const std::vector<char> &partitions, XemuFatxHdd::Snapshot &snapshot,
    std::string &status) const
{
    uint64_t captured_partitions = 0;
    ScopedPerfMeasurement perf(m_stats.partition_set_calls,
                               m_stats.partition_set_total_us,
                               &m_stats.partition_set_max_us,
                               &captured_partitions,
                               &m_stats.partition_set_partitions);

    snapshot = {};
    status.clear();
    if (partitions.empty()) {
        status = "No FATX partitions were requested for preflight.";
        return false;
    }
    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    if (!OpenHdd(hdd, length, status)) {
        return false;
    }
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        status = "Unable to pause the Xbox for coherent FATX preflight snapshots.";
        return false;
    }
    snapshot.hdd_available = true;
    snapshot.image_size = length;
    std::vector<char> seen;
    for (char letter : partitions) {
        letter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
        if (std::find(seen.begin(), seen.end(), letter) != seen.end()) {
            continue;
        }
        seen.push_back(letter);
        XemuFatxHdd::Snapshot one;
        if (!XemuFatxHdd::BuildPartitionSnapshot(ReadHddBlock, hdd, length,
                                                  letter, one)) {
            status = one.status.empty()
                ? std::string("Unable to refresh FATX ") + letter + ": for preflight."
                : one.status;
            snapshot = {};
            return false;
        }
        for (XemuFatxHdd::Partition &partition : one.partitions) {
            snapshot.partitions.push_back(std::move(partition));
            ++captured_partitions;
        }
    }
    snapshot.status = "Fresh partition-scoped FATX preflight snapshot captured.";
    status = snapshot.status;
    return !snapshot.partitions.empty();
}

bool HddSnapshotService::BuildDisplaySnapshot(XemuFatxHdd::Snapshot &snapshot,
                                              std::string &status) const
{
    ScopedPerfMeasurement perf(m_stats.display_calls,
                               m_stats.display_total_us,
                               &m_stats.display_max_us);
    snapshot = {};
    status.clear();

    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    if (!OpenHdd(hdd, length, status)) {
        return false;
    }

    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        status = "Unable to pause the Xbox for a coherent HDD display snapshot.";
        return false;
    }
    const bool ok = XemuFatxHdd::BuildSnapshot(ReadHddBlock, hdd, length, snapshot);
    status = snapshot.status;
    if (!ok) {
        if (status.empty()) {
            status = "Unable to build Xbox HDD FATX snapshot.";
        }
        return false;
    }

    std::string metadata_warning;
    XemuFatxHdd::PopulateXboxMetadata(ReadHddBlock, hdd, length, snapshot,
                                      metadata_warning);
    if (!metadata_warning.empty()) {
        if (!status.empty()) {
            status += " ";
        }
        status += metadata_warning;
    }
    return true;
}



bool HddSnapshotService::QueryPartitionCapacity(
    char partition_letter, CapacityInfo &capacity, std::string &error) const
{
    capacity = {};
    error.clear();
    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    std::string status;
    if (!OpenHdd(hdd, length, status)) {
        error = status;
        return false;
    }
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        error = "Unable to pause the Xbox for coherent FATX free-space preflight.";
        return false;
    }
    XemuFatxHdd::Snapshot snapshot;
    if (!XemuFatxHdd::BuildPartitionSnapshot(ReadHddBlock, hdd, length,
                                              partition_letter, snapshot)) {
        error = snapshot.status.empty()
            ? "Unable to refresh FATX partition for free-space preflight."
            : snapshot.status;
        return false;
    }
    const XemuFatxHdd::Partition *partition =
        XemuFatxHdd::FindPartition(snapshot, partition_letter);
    if (!partition || !partition->available) {
        error = "FATX partition is unavailable for free-space preflight.";
        return false;
    }
    capacity.partition = partition_letter;
    capacity.bytes_per_cluster = partition->bytes_per_cluster;
    return XemuFatxHdd::QueryFreeSpace(ReadHddBlock, hdd, length, *partition,
                                       capacity.free_bytes,
                                       capacity.total_data_bytes, error);
}

bool HddSnapshotService::VerifyFatxCopyContents(
    const std::vector<FatxCompareItem> &items, std::string &error) const
{
    uint64_t verified_bytes = 0;
    ScopedPerfMeasurement perf(m_stats.content_verify_calls,
                               m_stats.content_verify_total_us, nullptr,
                               &verified_bytes, &m_stats.content_verify_bytes);
    error.clear();
    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    std::string status;
    if (!OpenHdd(hdd, length, status)) {
        error = status;
        return false;
    }

    // One fresh coherent snapshot anchors both source identity and destination
    // identity for the byte-for-byte verification pass. The guest remains
    // paused while source and destination ranges are compared.
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        error = "Unable to pause the Xbox for coherent FATX Copy verification.";
        return false;
    }
    XemuFatxHdd::Snapshot source_snapshot;
    XemuFatxHdd::Snapshot destination_snapshot;
    if (items.empty()) {
        error = "FATX Copy final verification received an empty transfer tree.";
        return false;
    }
    const char source_letter = items.front().source_partition;
    const char destination_letter = items.front().destination_partition;
    if (!XemuFatxHdd::BuildPartitionSnapshot(
            ReadHddBlock, hdd, length, source_letter, source_snapshot)) {
        error = source_snapshot.status.empty()
            ? "Unable to refresh FATX source partition for Copy content verification."
            : source_snapshot.status;
        return false;
    }
    if (destination_letter != source_letter &&
        !XemuFatxHdd::BuildPartitionSnapshot(
            ReadHddBlock, hdd, length, destination_letter, destination_snapshot)) {
        error = destination_snapshot.status.empty()
            ? "Unable to refresh FATX destination partition for Copy content verification."
            : destination_snapshot.status;
        return false;
    }

    // Content verification is host-side/read-only. A larger compare chunk
    // reduces repeated FAT-chain traversal without changing any Xbox RPC write
    // transaction or its per-operation fresh FATX verification.
    constexpr size_t kCompareChunkBytes = 1024u * 1024u;
    std::vector<uint8_t> source_bytes;
    std::vector<uint8_t> destination_bytes;
    source_bytes.reserve(kCompareChunkBytes);
    destination_bytes.reserve(kCompareChunkBytes);
    for (const FatxCompareItem &item : items) {
        if (item.source_partition != source_letter ||
            item.destination_partition != destination_letter) {
            error = "FATX Copy content verification received a mixed-partition plan.";
            return false;
        }
        const XemuFatxHdd::Partition *src_partition =
            XemuFatxHdd::FindPartition(source_snapshot, item.source_partition);
        const XemuFatxHdd::Snapshot &dst_snapshot =
            destination_letter == source_letter ? source_snapshot : destination_snapshot;
        const XemuFatxHdd::Partition *dst_partition =
            XemuFatxHdd::FindPartition(dst_snapshot, item.destination_partition);
        const XemuFatxHdd::Entry *src = src_partition
            ? XemuFatxHdd::FindEntry(*src_partition, item.source_components)
            : nullptr;
        const XemuFatxHdd::Entry *dst = dst_partition
            ? XemuFatxHdd::FindEntry(*dst_partition, item.destination_components)
            : nullptr;

        if (!src_partition || !src_partition->available || !dst_partition ||
            !dst_partition->available || !src || !dst ||
            src->directory != item.directory || dst->directory != item.directory ||
            src->directory_entry_offset != item.source_directory_entry_offset ||
            src->first_cluster != item.source_first_cluster ||
            src->modified_time != item.source_modified_time ||
            src->modified_date != item.source_modified_date ||
            src->attributes != item.source_attributes ||
            (!item.directory && (src->file_size != item.file_size ||
                                 dst->file_size != item.file_size))) {
            error = "FATX Copy source/destination tree identity changed before final verification.";
            return false;
        }

        if (item.directory) {
            continue;
        }

        uint64_t offset = 0;
        XemuFatxHdd::FileReadCursor source_cursor;
        XemuFatxHdd::FileReadCursor destination_cursor;
        while (offset < item.file_size) {
            std::string read_error;
            const size_t request = static_cast<size_t>(
                std::min<uint64_t>(kCompareChunkBytes, item.file_size - offset));
            if (!XemuFatxHdd::ReadFileRangeSequential(ReadHddBlock, hdd, length,
                                            *src_partition, *src, offset, request,
                                            source_cursor, source_bytes, read_error)) {
                error = "Unable to read FATX Copy source during content verification: " + read_error;
                return false;
            }
            if (!XemuFatxHdd::ReadFileRangeSequential(ReadHddBlock, hdd, length,
                                            *dst_partition, *dst, offset, request,
                                            destination_cursor, destination_bytes, read_error)) {
                error = "Unable to read FATX Copy destination during content verification: " + read_error;
                return false;
            }
            verified_bytes += source_bytes.size();
            if (source_bytes != destination_bytes) {
                error = "FATX Copy destination data does not match the unchanged source byte-for-byte.";
                return false;
            }
            offset += source_bytes.size();
            if (source_bytes.empty() && offset < item.file_size) {
                error = "FATX Copy content verification made no forward progress.";
                return false;
            }
        }
    }
    return true;
}

bool HddSnapshotService::ReadFatxFileChunk(
    char partition_letter, const std::vector<std::string> &components,
    uint64_t expected_entry_offset, uint32_t expected_first_cluster,
    uint16_t expected_modified_time, uint16_t expected_modified_date,
    uint8_t expected_attributes, uint64_t expected_size, uint64_t file_offset, size_t max_bytes,
    std::vector<uint8_t> &chunk, std::string &error) const
{
    ScopedPerfMeasurement perf(m_stats.source_chunk_calls,
                               m_stats.source_chunk_total_us, nullptr, nullptr,
                               &m_stats.source_chunk_bytes, &chunk);
    chunk.clear();
    error.clear();
    XemuDiscBlockHandle hdd = nullptr;
    uint64_t length = 0;
    std::string status;
    if (!OpenHdd(hdd, length, status)) {
        error = status;
        return false;
    }
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        error = "Unable to pause the Xbox before reading a coherent FATX Copy source chunk.";
        return false;
    }
    XemuFatxHdd::Snapshot snapshot;
    if (!XemuFatxHdd::BuildPartitionSnapshot(
            ReadHddBlock, hdd, length, partition_letter, snapshot)) {
        error = snapshot.status.empty() ? "Unable to refresh FATX source partition before Copy." : snapshot.status;
        return false;
    }
    const XemuFatxHdd::Partition *partition = XemuFatxHdd::FindPartition(snapshot, partition_letter);
    const XemuFatxHdd::Entry *entry = partition ? XemuFatxHdd::FindEntry(*partition, components) : nullptr;
    if (!partition || !entry || entry->directory ||
        entry->directory_entry_offset != expected_entry_offset ||
        entry->first_cluster != expected_first_cluster ||
        entry->modified_time != expected_modified_time ||
        entry->modified_date != expected_modified_date ||
        entry->attributes != expected_attributes ||
        entry->file_size != expected_size) {
        error = "FATX Copy source changed before the next file chunk was read.";
        return false;
    }
    return XemuFatxHdd::ReadFileRange(ReadHddBlock, hdd, length, *partition, *entry,
                                      file_offset, max_bytes, chunk, error);
}
