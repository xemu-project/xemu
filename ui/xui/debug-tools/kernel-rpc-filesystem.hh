//
// xemu Guest Kernel RPC filesystem planning / preflight helpers
//
// This layer contains no ImGui state and performs no raw FATX writes. It owns
// reusable path, delete-plan, and host-import validation shared by the
// experimental Kernel RPC UI and the HDD browser frontend.
//
#pragma once

#include "fatx-hdd.hh"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace XemuKernelFs {

constexpr uint32_t kImportChunkBytes = 0x0000d000u;
constexpr uint64_t kImportMaxTotalBytes = 64ull * 1024ull * 1024ull;
constexpr size_t kImportMaxEntries = 4096u;
constexpr unsigned kImportMaxDepth = 16u;
constexpr size_t kFatxMaxComponentBytes = 42u;
constexpr size_t kDeleteMaxEntries = 4096u;
constexpr unsigned kDeleteMaxDepth = 16u;
constexpr uint64_t kContentHashBasis = 1469598103934665603ull;
constexpr uint64_t kContentHashPrime = 1099511628211ull;

uint64_t UpdateContentHash(uint64_t hash, const void *data, size_t size);

struct DeleteEntry {
    std::string fatx_path;
    std::string native_path;
    std::vector<std::string> components;
    uint64_t file_size = 0;
    uint64_t directory_entry_offset = 0;
    uint32_t first_cluster = 0;
    uint16_t modified_time = 0;
    uint16_t modified_date = 0;
    uint8_t attributes = 0;
    bool directory = false;
};

struct DeletePlanSummary {
    uint32_t file_count = 0;
    uint32_t directory_count = 0;
};


struct RelocatePlan {
    char partition = '?';
    bool directory = false;
    std::vector<std::string> source_components;
    std::vector<std::string> destination_components;
    std::string source_fatx_path;
    std::string destination_fatx_path;
    std::string source_native_path;
    std::string destination_native_path;
    uint64_t file_size = 0;
    uint64_t directory_entry_offset = 0;
    uint32_t first_cluster = 0;
    uint16_t modified_time = 0;
    uint16_t modified_date = 0;
    uint8_t attributes = 0;
};

struct TransferEntry {
    std::string host_path;
    bool source_from_fatx = false;
    char source_partition = '?';
    std::vector<std::string> source_components;
    uint64_t source_directory_entry_offset = 0;
    uint32_t source_first_cluster = 0;
    uint16_t source_modified_time = 0;
    uint16_t source_modified_date = 0;
    uint8_t source_attributes = 0;
    std::string fatx_path;
    std::string native_path;
    std::vector<std::string> components;
    uint64_t file_size = 0;
    int64_t host_write_time = 0;
    uint64_t host_content_hash = kContentHashBasis;
    bool directory = false;
};

class ImportHostStream {
public:
    ImportHostStream();
    ~ImportHostStream();
    ImportHostStream(ImportHostStream &&) noexcept;
    ImportHostStream &operator=(ImportHostStream &&) noexcept;
    ImportHostStream(const ImportHostStream &) = delete;
    ImportHostStream &operator=(const ImportHostStream &) = delete;

    void Reset();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    friend bool LoadImportFileChunk(ImportHostStream &stream,
                                    const TransferEntry &item,
                                    uint64_t file_offset,
                                    std::vector<uint8_t> &chunk,
                                    uint32_t &chunk_bytes,
                                    uint64_t &expected_file_size,
                                    std::string &error);
};

enum class TransferKind {
    HostImport,
    CreateFatxDirectory,
    FatxCopy,
    CrossVolumeMove,
};

struct TransferPlan {
    TransferKind kind = TransferKind::HostImport;
    // General HDD-browser destination identity.
    char partition = '?';
    std::vector<std::string> destination_components;
    bool source_is_directory = true;
    bool synthetic_directory = false;
    bool source_from_fatx = false;
    char source_partition = '?';
    std::vector<std::string> source_components;
    bool delete_source_after_copy = false;
    std::vector<DeleteEntry> source_delete_plan;

    // Host/FATX source + create-only destination leaf.
    std::string source_path;
    std::string root_name;

    std::vector<TransferEntry> entries;
    uint64_t total_bytes = 0;
    uint32_t file_count = 0;
    uint32_t directory_count = 0;
    uint64_t total_operations = 0;
};

// Compatibility names keep older out-of-tree integrations/tests source-stable;
// production code uses TransferEntry/TransferPlan from v2.40 forward.
using ImportEntry = TransferEntry;
using ImportPlan = TransferPlan;

uint64_t ExpectedCommittedFileSize(uint64_t file_offset, uint32_t chunk_bytes);
uint64_t EstimateTransferRequiredBytes(const TransferPlan &plan,
                                       uint32_t bytes_per_cluster);

bool EqualsNoCase(const std::string &a, const std::string &b);
const XemuFatxHdd::Entry *FindChildNoCase(
    const std::vector<XemuFatxHdd::Entry> &entries, const std::string &name);

bool IsSafeFatxImportComponent(const std::string &name, std::string &error);
bool IsKernelWritablePartition(char partition);
std::string FatxPathForPartition(char partition,
                                 const std::vector<std::string> &components);
bool NativePathForPartition(char partition,
                            const std::vector<std::string> &components,
                            std::string &path,
                            std::string &error);

// General HDD-browser delete planner. Partition roots are never valid delete
// targets; files become one-entry plans and directories are leaf-first.
bool BuildDeletePlan(const XemuFatxHdd::Snapshot &snapshot,
                     char partition,
                     const std::vector<std::string> &components,
                     bool expected_directory,
                     std::vector<DeleteEntry> &plan,
                     std::string &error);

DeletePlanSummary SummarizeDeletePlan(const std::vector<DeleteEntry> &plan);
bool SameDeletePlan(const std::vector<DeleteEntry> &a,
                    const std::vector<DeleteEntry> &b);
bool SameImportPlan(const ImportPlan &a, const ImportPlan &b);
bool BuildRelocatePlan(const XemuFatxHdd::Snapshot &snapshot,
                       char partition,
                       const std::vector<std::string> &source_components,
                       bool expected_directory,
                       const std::vector<std::string> &destination_parent,
                       const std::string &destination_name,
                       RelocatePlan &plan,
                       std::string &error);
bool SameRelocatePlan(const RelocatePlan &a, const RelocatePlan &b);

// Host-source validation used before taking a fresh HDD snapshot.
bool ValidateImportHostRoot(const std::string &host_path, std::string &error);
bool ValidateImportHostFile(const std::string &host_path, std::string &error);

// General HDD-browser create-only import planners. destination_components may
// be empty, which intentionally means the root of the selected FATX volume.
bool BuildImportFolderPlanAtDestination(
    const std::string &host_path,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition,
    const std::vector<std::string> &destination_components,
    ImportPlan &plan,
    std::string &error);
bool BuildImportFilePlanAtDestination(
    const std::string &host_path,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition,
    const std::vector<std::string> &destination_components,
    ImportPlan &plan,
    std::string &error);
bool BuildFatxCopyPlan(const XemuFatxHdd::Snapshot &snapshot,
                       char source_partition,
                       const std::vector<std::string> &source_components,
                       bool source_directory,
                       char destination_partition,
                       const std::vector<std::string> &destination_components,
                       bool delete_source_after_copy,
                       ImportPlan &plan,
                       std::string &error);
bool BuildCreateDirectoryPlanAtDestination(
    const std::string &name,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition,
    const std::vector<std::string> &destination_components,
    ImportPlan &plan,
    std::string &error);

// Revalidate immutable assumptions immediately before mutation and while
// streaming file chunks. These functions do not write to FATX or the host.
// Lightweight per-chunk identity check. Content integrity is covered by the
// confirmed-plan hash plus the executor's rolling hash of the bytes actually
// copied; this avoids re-hashing the entire host file for every 0xD000 chunk.
bool ValidateImportHostEntryMetadata(const ImportEntry &item, std::string &error);
bool LoadImportFileChunk(ImportHostStream &stream,
                         const ImportEntry &item,
                         uint64_t file_offset,
                         std::vector<uint8_t> &chunk,
                         uint32_t &chunk_bytes,
                         uint64_t &expected_file_size,
                         std::string &error);
bool LoadImportFileChunk(const ImportEntry &item,
                         uint64_t file_offset,
                         std::vector<uint8_t> &chunk,
                         uint32_t &chunk_bytes,
                         uint64_t &expected_file_size,
                         std::string &error);

} // namespace XemuKernelFs
