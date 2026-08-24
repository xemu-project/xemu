//
// xemu Guest Kernel RPC filesystem planning / preflight helpers
//
#include "kernel-rpc-filesystem.hh"
#include "kernel-rpc-filesystem-internal.hh"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>

namespace XemuKernelFs {

uint64_t UpdateContentHash(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kContentHashPrime;
    }
    return hash;
}

namespace {

bool HashHostFile(const std::filesystem::path &path, uint64_t &hash,
                  std::string &error)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Could not open host file for integrity hashing: " + path.u8string();
        return false;
    }
    hash = kContentHashBasis;
    std::array<char, 64 * 1024> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize got = stream.gcount();
        if (got > 0) {
            hash = UpdateContentHash(hash, buffer.data(), static_cast<size_t>(got));
        }
    }
    if (!stream.eof()) {
        error = "Could not completely hash host file: " + path.u8string();
        return false;
    }
    return true;
}



int PartitionNumber(char partition)
{
    switch (static_cast<char>(std::toupper(static_cast<unsigned char>(partition)))) {
    case 'E': return 1;
    case 'C': return 2;
    case 'X': return 3;
    case 'Y': return 4;
    case 'Z': return 5;
    case 'F': return 6;
    case 'G': return 7;
    default: return -1;
    }
}

std::string FoldCaseInsensitive(const std::string &value)
{
    std::string folded = value;
    for (char &ch : folded) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return folded;
}

const std::vector<XemuFatxHdd::Entry> *DestinationChildren(
    const XemuFatxHdd::Partition &partition,
    const std::vector<std::string> &components,
    std::string &error)
{
    if (components.empty()) {
        return &partition.entries;
    }
    const XemuFatxHdd::Entry *entry = XemuFatxHdd::FindEntry(partition, components);
    if (!entry || !entry->directory) {
        error = "The selected FATX import destination directory no longer exists.";
        return nullptr;
    }
    return &entry->children;
}

bool AppendImportFile(const std::filesystem::path &host,
                      char partition,
                      std::vector<std::string> components,
                      std::vector<TransferEntry> &entries,
                      uint64_t &total_bytes,
                      uint64_t &total_operations,
                      uint32_t &file_count,
                      std::string &error)
{
    namespace fs = std::filesystem;
    if (entries.size() >= kImportMaxEntries) {
        error = "Kernel Import exceeded the 4096-entry safety limit.";
        return false;
    }

    std::error_code ec;
    const fs::file_status status = fs::symlink_status(host, ec);
    if (ec || fs::is_symlink(status) || !fs::is_regular_file(status)) {
        error = "Kernel Import requires a normal host file (not a symlink/junction): " +
                host.u8string();
        return false;
    }
    const uint64_t size = static_cast<uint64_t>(fs::file_size(host, ec));
    if (ec) {
        error = "Could not determine host file size: " + host.u8string();
        return false;
    }
    if (size > kImportMaxTotalBytes - total_bytes) {
        error = "Kernel Import exceeds the 64 MiB total-data safety limit.";
        return false;
    }

    TransferEntry item;
    item.host_path = host.u8string();
    item.components = std::move(components);
    item.file_size = size;
    if (!ReadHostWriteTime(host, item.host_write_time, error) ||
        !HashHostFile(host, item.host_content_hash, error)) {
        return false;
    }
    item.directory = false;
    item.fatx_path = FatxPathForPartition(partition, item.components);
    if (!NativePathForPartition(partition, item.components,
                                item.native_path, error)) {
        return false;
    }
    entries.push_back(std::move(item));
    total_bytes += size;
    ++file_count;
    total_operations += size == 0 ? 1u :
        (size + kImportChunkBytes - 1u) / kImportChunkBytes;
    return true;
}

bool BuildFolderPlanRecursive(const std::filesystem::path &host,
                              char partition,
                              std::vector<std::string> components,
                              unsigned depth,
                              std::vector<TransferEntry> &entries,
                              uint64_t &total_bytes,
                              uint64_t &total_operations,
                              uint32_t &file_count,
                              uint32_t &directory_count,
                              std::string &error)
{
    namespace fs = std::filesystem;
    if (depth > kImportMaxDepth) {
        error = "Kernel Import exceeded the 16-level directory-depth safety limit.";
        return false;
    }
    if (entries.size() >= kImportMaxEntries) {
        error = "Kernel Import exceeded the 4096-entry safety limit.";
        return false;
    }

    TransferEntry directory_item;
    directory_item.host_path = host.u8string();
    directory_item.components = components;
    if (!ReadHostWriteTime(host, directory_item.host_write_time, error)) {
        return false;
    }
    directory_item.directory = true;
    directory_item.fatx_path = FatxPathForPartition(partition, components);
    if (!NativePathForPartition(partition, components,
                                directory_item.native_path, error)) {
        return false;
    }
    entries.push_back(std::move(directory_item));
    ++directory_count;
    ++total_operations;

    std::vector<fs::directory_entry> children;
    std::error_code iter_error;
    fs::directory_iterator it(host, iter_error);
    fs::directory_iterator end;
    if (iter_error) {
        error = "Could not enumerate host folder: " + host.u8string();
        return false;
    }
    for (; it != end; it.increment(iter_error)) {
        if (iter_error) {
            error = "Host folder enumeration failed: " + host.u8string();
            return false;
        }
        children.push_back(*it);
    }
    if (iter_error) {
        error = "Host folder enumeration failed: " + host.u8string();
        return false;
    }
    std::sort(children.begin(), children.end(),
              [](const fs::directory_entry &a, const fs::directory_entry &b) {
                  return a.path().filename().u8string() <
                         b.path().filename().u8string();
              });

    std::unordered_map<std::string, std::string> folded_names;
    folded_names.reserve(children.size());
    for (const fs::directory_entry &child : children) {
        const std::string name = child.path().filename().u8string();
        const std::string folded = FoldCaseInsensitive(name);
        const auto [it, inserted] = folded_names.emplace(folded, name);
        if (!inserted) {
            error = "Host folder contains names that collide case-insensitively on FATX: " +
                    it->second + " / " + name;
            return false;
        }
    }

    for (const fs::directory_entry &child : children) {
        const std::string name = child.path().filename().u8string();
        std::string name_error;
        if (!IsSafeFatxImportComponent(name, name_error)) {
            error = "Host item '" + name +
                    "' is not safe for FATX import: " + name_error;
            return false;
        }
        std::error_code status_error;
        const fs::file_status status = child.symlink_status(status_error);
        if (status_error || fs::is_symlink(status)) {
            error = "Kernel Import refuses symlinks/junctions or unreadable host items: " +
                    child.path().u8string();
            return false;
        }

        std::vector<std::string> child_components = components;
        child_components.push_back(name);
        if (fs::is_directory(status)) {
            if (!BuildFolderPlanRecursive(child.path(), partition,
                                          std::move(child_components), depth + 1u,
                                          entries, total_bytes, total_operations,
                                          file_count, directory_count, error)) {
                return false;
            }
        } else if (fs::is_regular_file(status)) {
            if (!AppendImportFile(child.path(), partition,
                                  std::move(child_components), entries,
                                  total_bytes, total_operations, file_count,
                                  error)) {
                return false;
            }
        } else {
            error = "Kernel Import only accepts regular files and folders: " +
                    child.path().u8string();
            return false;
        }
    }
    return true;
}

bool ValidateImportDestination(const XemuFatxHdd::Snapshot &snapshot,
                               char partition_letter,
                               const std::vector<std::string> &destination_components,
                               const std::string &root_name,
                               std::string &error)
{
    const char partition_key = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    if (!IsKernelWritablePartition(partition_key)) {
        error = "The selected FATX partition does not have a supported Xbox kernel device mapping.";
        return false;
    }
    const XemuFatxHdd::Partition *partition =
        XemuFatxHdd::FindPartition(snapshot, partition_key);
    if (!partition || !partition->available) {
        error = std::string("Xbox ") + partition_key +
                ": FATX partition is not available.";
        return false;
    }

    const std::vector<XemuFatxHdd::Entry> *children =
        DestinationChildren(*partition, destination_components, error);
    if (!children) {
        return false;
    }
    if (FindChildNoCase(*children, root_name) != nullptr) {
        error = "Destination already exists. Kernel Import never overwrites or merges an existing FATX item.";
        return false;
    }
    return true;
}

} // namespace

uint64_t ExpectedCommittedFileSize(uint64_t file_offset, uint32_t chunk_bytes)
{
    if (file_offset > UINT64_MAX - chunk_bytes) {
        return UINT64_MAX;
    }
    return file_offset + chunk_bytes;
}

uint64_t EstimateTransferRequiredBytes(const TransferPlan &plan,
                                       uint32_t bytes_per_cluster)
{
    if (bytes_per_cluster == 0) {
        return std::numeric_limits<uint64_t>::max();
    }
    uint64_t clusters = 0;
    for (const TransferEntry &item : plan.entries) {
        if (item.directory) {
            ++clusters; // every new FATX directory owns at least one cluster
        } else if (item.file_size != 0) {
            clusters += (item.file_size + bytes_per_cluster - 1u) /
                        bytes_per_cluster;
        }
    }
    // Conservatively reserve directory-entry growth. Existing parent slack is
    // deliberately ignored so capacity preflight fails safe near a full disk.
    const uint64_t metadata_bytes = plan.entries.size() * 64ull;
    clusters += (metadata_bytes + bytes_per_cluster - 1u) / bytes_per_cluster;
    if (clusters > std::numeric_limits<uint64_t>::max() / bytes_per_cluster) {
        return std::numeric_limits<uint64_t>::max();
    }
    return clusters * bytes_per_cluster;
}

bool EqualsNoCase(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) {
            return false;
        }
    }
    return true;
}

const XemuFatxHdd::Entry *FindChildNoCase(
    const std::vector<XemuFatxHdd::Entry> &entries, const std::string &name)
{
    for (const XemuFatxHdd::Entry &entry : entries) {
        if (EqualsNoCase(entry.name, name)) {
            return &entry;
        }
    }
    return nullptr;
}

bool IsSafeFatxImportComponent(const std::string &name, std::string &error)
{
    error.clear();
    if (name.empty() || name.size() > kFatxMaxComponentBytes) {
        error = "FATX import names must be 1-42 bytes long.";
        return false;
    }
    if (name == "." || name == "..") {
        error = "FATX import names cannot be . or ...";
        return false;
    }
    if (name.back() == ' ' || name.back() == '.') {
        error = "FATX import names cannot end in a space or period.";
        return false;
    }
    constexpr const char *forbidden = "\\/:*?\"<>|";
    for (unsigned char ch : name) {
        if (ch < 0x20u || ch >= 0x7fu || std::strchr(forbidden, ch) != nullptr) {
            error = "FATX import accepts portable ASCII names and rejects \\/:*?\"<>| and control characters.";
            return false;
        }
    }
    return true;
}

bool IsKernelWritablePartition(char partition)
{
    return PartitionNumber(partition) > 0;
}

std::string FatxPathForPartition(char partition,
                                 const std::vector<std::string> &components)
{
    const char letter = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition)));
    std::string path;
    path += letter;
    path += ":\\";
    for (size_t i = 0; i < components.size(); ++i) {
        if (i != 0) {
            path += "\\";
        }
        path += components[i];
    }
    return path;
}

bool NativePathForPartition(char partition,
                            const std::vector<std::string> &components,
                            std::string &path,
                            std::string &error)
{
    error.clear();
    const int number = PartitionNumber(partition);
    if (number < 0) {
        error = std::string("No Xbox kernel device-object mapping is known for drive ") +
                static_cast<char>(std::toupper(static_cast<unsigned char>(partition))) + ":.";
        path.clear();
        return false;
    }

    path = "\\Device\\Harddisk0\\Partition" + std::to_string(number);
    for (const std::string &component : components) {
        path += "\\";
        path += component;
    }
    return true;
}

bool BuildDeletePlan(const XemuFatxHdd::Snapshot &snapshot,
                     char partition_letter,
                     const std::vector<std::string> &components,
                     bool expected_directory,
                     std::vector<DeleteEntry> &plan,
                     std::string &error)
{
    error.clear();
    plan.clear();
    const char partition_key = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    if (!IsKernelWritablePartition(partition_key)) {
        error = "The selected FATX partition does not have a supported Xbox kernel device mapping.";
        return false;
    }
    if (components.empty()) {
        error = "Deleting an entire FATX partition root is intentionally blocked.";
        return false;
    }

    const XemuFatxHdd::Partition *partition =
        XemuFatxHdd::FindPartition(snapshot, partition_key);
    const XemuFatxHdd::Entry *entry = partition
        ? XemuFatxHdd::FindEntry(*partition, components) : nullptr;
    if (!partition || !partition->available || !entry ||
        entry->directory != expected_directory) {
        error = "The selected FATX item changed or no longer exists. Refresh the HDD view and try again.";
        return false;
    }

    const auto make_entry = [&](const XemuFatxHdd::Entry &node,
                                std::vector<std::string> path,
                                DeleteEntry &item) -> bool {
        item.components = std::move(path);
        item.file_size = node.file_size;
        item.directory_entry_offset = node.directory_entry_offset;
        item.first_cluster = node.first_cluster;
        item.modified_time = node.modified_time;
        item.modified_date = node.modified_date;
        item.attributes = node.attributes;
        item.directory = node.directory;
        item.fatx_path = FatxPathForPartition(partition_key, item.components);
        return NativePathForPartition(partition_key, item.components,
                                      item.native_path, error);
    };

    if (!entry->directory) {
        DeleteEntry item;
        if (!make_entry(*entry, components, item)) {
            return false;
        }
        plan.push_back(std::move(item));
        return true;
    }

    const auto append = [&](const auto &self, const XemuFatxHdd::Entry &node,
                            std::vector<std::string> path,
                            unsigned depth, std::string &plan_error) -> bool {
        if (depth > kDeleteMaxDepth) {
            plan_error = "Recursive Kernel Delete exceeded the directory-depth safety limit.";
            return false;
        }
        if (plan.size() >= kDeleteMaxEntries) {
            plan_error = "Recursive Kernel Delete exceeded the 4096-entry safety limit.";
            return false;
        }

        for (const XemuFatxHdd::Entry &child : node.children) {
            std::vector<std::string> child_components = path;
            child_components.push_back(child.name);
            if (child.directory) {
                if (!self(self, child, std::move(child_components),
                          depth + 1u, plan_error)) {
                    return false;
                }
            } else {
                if (plan.size() >= kDeleteMaxEntries) {
                    plan_error = "Recursive Kernel Delete exceeded the 4096-entry safety limit.";
                    return false;
                }
                DeleteEntry item;
                if (!make_entry(child, std::move(child_components), item)) {
                    plan_error = error;
                    return false;
                }
                plan.push_back(std::move(item));
            }
        }

        if (plan.size() >= kDeleteMaxEntries) {
            plan_error = "Recursive Kernel Delete exceeded the 4096-entry safety limit.";
            return false;
        }
        DeleteEntry directory_item;
        if (!make_entry(node, std::move(path), directory_item)) {
            plan_error = error;
            return false;
        }
        plan.push_back(std::move(directory_item));
        return true;
    };

    if (!append(append, *entry, components, 0u, error) || plan.empty()) {
        if (error.empty()) {
            error = "Could not build the recursive Kernel Delete plan.";
        }
        plan.clear();
        return false;
    }
    return true;
}

DeletePlanSummary SummarizeDeletePlan(const std::vector<DeleteEntry> &plan)
{
    DeletePlanSummary summary;
    for (const DeleteEntry &item : plan) {
        if (item.directory) {
            ++summary.directory_count;
        } else {
            ++summary.file_count;
        }
    }
    return summary;
}

bool SameDeletePlan(const std::vector<DeleteEntry> &a,
                    const std::vector<DeleteEntry> &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].components != b[i].components ||
            a[i].directory != b[i].directory ||
            a[i].native_path != b[i].native_path ||
            a[i].file_size != b[i].file_size ||
            a[i].directory_entry_offset != b[i].directory_entry_offset ||
            a[i].first_cluster != b[i].first_cluster ||
            a[i].modified_time != b[i].modified_time ||
            a[i].modified_date != b[i].modified_date ||
            a[i].attributes != b[i].attributes) {
            return false;
        }
    }
    return true;
}

bool SameImportPlan(const TransferPlan &a, const TransferPlan &b)
{
    if (a.kind != b.kind ||
        a.partition != b.partition ||
        a.destination_components != b.destination_components ||
        a.source_is_directory != b.source_is_directory ||
        a.synthetic_directory != b.synthetic_directory ||
        a.source_from_fatx != b.source_from_fatx ||
        a.source_partition != b.source_partition ||
        a.source_components != b.source_components ||
        a.delete_source_after_copy != b.delete_source_after_copy ||
        !SameDeletePlan(a.source_delete_plan, b.source_delete_plan) ||
        a.source_path != b.source_path ||
        a.root_name != b.root_name ||
        a.total_bytes != b.total_bytes ||
        a.file_count != b.file_count ||
        a.directory_count != b.directory_count ||
        a.total_operations != b.total_operations ||
        a.entries.size() != b.entries.size()) {
        return false;
    }
    for (size_t i = 0; i < a.entries.size(); ++i) {
        const TransferEntry &left = a.entries[i];
        const TransferEntry &right = b.entries[i];
        if (left.host_path != right.host_path ||
            left.source_from_fatx != right.source_from_fatx ||
            left.source_partition != right.source_partition ||
            left.source_components != right.source_components ||
            left.source_directory_entry_offset != right.source_directory_entry_offset ||
            left.source_first_cluster != right.source_first_cluster ||
            left.source_modified_time != right.source_modified_time ||
            left.source_modified_date != right.source_modified_date ||
            left.source_attributes != right.source_attributes ||
            left.fatx_path != right.fatx_path ||
            left.native_path != right.native_path ||
            left.components != right.components ||
            left.file_size != right.file_size ||
            left.host_write_time != right.host_write_time ||
            left.host_content_hash != right.host_content_hash ||
            left.directory != right.directory) {
            return false;
        }
    }
    return true;
}

bool BuildRelocatePlan(const XemuFatxHdd::Snapshot &snapshot,
                       char partition_letter,
                       const std::vector<std::string> &source_components,
                       bool expected_directory,
                       const std::vector<std::string> &destination_parent,
                       const std::string &destination_name,
                       RelocatePlan &plan,
                       std::string &error)
{
    error.clear();
    plan = {};
    const char partition = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    if (!IsKernelWritablePartition(partition) || source_components.empty()) {
        error = "Kernel Rename/Move requires a normal FATX item on a writable volume.";
        return false;
    }
    std::string name_error;
    if (!IsSafeFatxImportComponent(destination_name, name_error)) {
        error = "Destination name is not safe for FATX: " + name_error;
        return false;
    }
    const XemuFatxHdd::Partition *volume = XemuFatxHdd::FindPartition(snapshot, partition);
    if (!volume || !volume->available) {
        error = std::string("FATX volume ") + partition + ": is unavailable.";
        return false;
    }
    const XemuFatxHdd::Entry *source = XemuFatxHdd::FindEntry(*volume, source_components);
    if (!source || source->directory != expected_directory) {
        error = "The source item changed before Kernel Rename/Move.";
        return false;
    }
    const std::vector<XemuFatxHdd::Entry> *children = &volume->entries;
    if (!destination_parent.empty()) {
        const XemuFatxHdd::Entry *parent = XemuFatxHdd::FindEntry(*volume, destination_parent);
        if (!parent || !parent->directory) {
            error = "The destination directory is unavailable.";
            return false;
        }
        children = &parent->children;
    }
    if (expected_directory && destination_parent.size() >= source_components.size() &&
        std::equal(source_components.begin(), source_components.end(),
                   destination_parent.begin(), [](const std::string &a, const std::string &b) {
                       return EqualsNoCase(a, b);
                   })) {
        error = "A folder cannot be moved into itself or one of its descendants.";
        return false;
    }
    for (const XemuFatxHdd::Entry &child : *children) {
        if (EqualsNoCase(child.name, destination_name)) {
            error = "The destination already contains an item named '" + destination_name + "'.";
            return false;
        }
    }
    std::vector<std::string> destination = destination_parent;
    destination.push_back(destination_name);
    std::string source_native;
    std::string destination_native;
    if (!NativePathForPartition(partition, source_components, source_native, error) ||
        !NativePathForPartition(partition, destination, destination_native, error)) {
        return false;
    }
    plan.partition = partition;
    plan.directory = expected_directory;
    plan.source_components = source_components;
    plan.destination_components = std::move(destination);
    plan.source_fatx_path = FatxPathForPartition(partition, source_components);
    plan.destination_fatx_path = FatxPathForPartition(partition, plan.destination_components);
    plan.source_native_path = std::move(source_native);
    plan.destination_native_path = std::move(destination_native);
    plan.file_size = source->file_size;
    plan.directory_entry_offset = source->directory_entry_offset;
    plan.first_cluster = source->first_cluster;
    plan.modified_time = source->modified_time;
    plan.modified_date = source->modified_date;
    plan.attributes = source->attributes;
    return true;
}

bool SameRelocatePlan(const RelocatePlan &a, const RelocatePlan &b)
{
    return a.partition == b.partition && a.directory == b.directory &&
        a.source_components == b.source_components &&
        a.destination_components == b.destination_components &&
        a.source_fatx_path == b.source_fatx_path &&
        a.destination_fatx_path == b.destination_fatx_path &&
        a.source_native_path == b.source_native_path &&
        a.destination_native_path == b.destination_native_path &&
        a.file_size == b.file_size &&
        a.directory_entry_offset == b.directory_entry_offset &&
        a.first_cluster == b.first_cluster &&
        a.modified_time == b.modified_time && a.modified_date == b.modified_date &&
        a.attributes == b.attributes;
}

bool ValidateImportHostRoot(const std::string &host_path, std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    if (host_path.empty()) {
        error = "No host folder was selected for Kernel Import.";
        return false;
    }

    std::error_code ec;
    const fs::path root = fs::u8path(host_path).lexically_normal();
    const fs::file_status root_status = fs::symlink_status(root, ec);
    if (ec || !fs::is_directory(root_status) || fs::is_symlink(root_status)) {
        error = "Kernel Import requires a normal host folder (not a symlink/junction).";
        return false;
    }
    const std::string root_name = root.filename().u8string();
    std::string name_error;
    if (!IsSafeFatxImportComponent(root_name, name_error)) {
        error = "Host root folder '" + root_name +
                "' is not safe for FATX import: " + name_error;
        return false;
    }
    return true;
}

bool ValidateImportHostFile(const std::string &host_path, std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    if (host_path.empty()) {
        error = "No host file was selected for Kernel Import.";
        return false;
    }
    std::error_code ec;
    const fs::path file = fs::u8path(host_path).lexically_normal();
    const fs::file_status status = fs::symlink_status(file, ec);
    if (ec || !fs::is_regular_file(status) || fs::is_symlink(status)) {
        error = "Kernel Import requires a normal host file (not a symlink/junction).";
        return false;
    }
    std::string name_error;
    if (!IsSafeFatxImportComponent(file.filename().u8string(), name_error)) {
        error = "Host file '" + file.filename().u8string() +
                "' is not safe for FATX import: " + name_error;
        return false;
    }
    return true;
}

bool BuildImportFolderPlanAtDestination(
    const std::string &host_path,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition_letter,
    const std::vector<std::string> &destination_components,
    TransferPlan &plan,
    std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    plan = {};
    if (!ValidateImportHostRoot(host_path, error)) {
        return false;
    }

    const char partition_key = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    const fs::path root = fs::u8path(host_path).lexically_normal();
    const std::string root_name = root.filename().u8string();
    if (!ValidateImportDestination(snapshot, partition_key,
                                   destination_components, root_name, error)) {
        return false;
    }

    std::vector<std::string> root_components = destination_components;
    root_components.push_back(root_name);
    std::vector<TransferEntry> entries;
    uint64_t total_bytes = 0;
    uint64_t total_operations = 0;
    uint32_t file_count = 0;
    uint32_t directory_count = 0;
    if (!BuildFolderPlanRecursive(root, partition_key, std::move(root_components),
                                  0u, entries, total_bytes, total_operations,
                                  file_count, directory_count, error) ||
        entries.empty()) {
        if (error.empty()) {
            error = "Could not build the Kernel Import folder plan.";
        }
        return false;
    }

    plan.partition = partition_key;
    plan.destination_components = destination_components;
    plan.kind = TransferKind::HostImport;
    plan.source_is_directory = true;
    plan.source_path = root.u8string();
    plan.root_name = root_name;
    plan.entries = std::move(entries);
    plan.total_bytes = total_bytes;
    plan.file_count = file_count;
    plan.directory_count = directory_count;
    plan.total_operations = total_operations;
    return true;
}

bool BuildImportFilePlanAtDestination(
    const std::string &host_path,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition_letter,
    const std::vector<std::string> &destination_components,
    TransferPlan &plan,
    std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    plan = {};
    if (!ValidateImportHostFile(host_path, error)) {
        return false;
    }

    const char partition_key = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    const fs::path file = fs::u8path(host_path).lexically_normal();
    const std::string root_name = file.filename().u8string();
    if (!ValidateImportDestination(snapshot, partition_key,
                                   destination_components, root_name, error)) {
        return false;
    }

    std::vector<std::string> components = destination_components;
    components.push_back(root_name);
    std::vector<TransferEntry> entries;
    uint64_t total_bytes = 0;
    uint64_t total_operations = 0;
    uint32_t file_count = 0;
    if (!AppendImportFile(file, partition_key, std::move(components), entries,
                          total_bytes, total_operations, file_count, error)) {
        return false;
    }

    plan.partition = partition_key;
    plan.destination_components = destination_components;
    plan.kind = TransferKind::HostImport;
    plan.source_is_directory = false;
    plan.source_path = file.u8string();
    plan.root_name = root_name;
    plan.entries = std::move(entries);
    plan.total_bytes = total_bytes;
    plan.file_count = file_count;
    plan.directory_count = 0;
    plan.total_operations = total_operations;
    return true;
}

bool BuildFatxCopyPlan(const XemuFatxHdd::Snapshot &snapshot,
                       char source_partition_letter,
                       const std::vector<std::string> &source_components,
                       bool source_directory,
                       char destination_partition_letter,
                       const std::vector<std::string> &destination_components,
                       bool delete_source_after_copy,
                       TransferPlan &plan,
                       std::string &error)
{
    error.clear();
    plan = {};
    const char source_partition = static_cast<char>(std::toupper((unsigned char)source_partition_letter));
    const char destination_partition = static_cast<char>(std::toupper((unsigned char)destination_partition_letter));
    if (!IsKernelWritablePartition(source_partition) || !IsKernelWritablePartition(destination_partition) || source_components.empty()) {
        error = "FATX Copy requires a normal source item and writable source/destination volumes.";
        return false;
    }
    const XemuFatxHdd::Partition *src_volume = XemuFatxHdd::FindPartition(snapshot, source_partition);
    const XemuFatxHdd::Entry *src_root = src_volume ? XemuFatxHdd::FindEntry(*src_volume, source_components) : nullptr;
    if (!src_volume || !src_volume->available || !src_root || src_root->directory != source_directory) {
        error = "FATX Copy source is no longer available.";
        return false;
    }
    if (source_partition == destination_partition && source_directory &&
        destination_components.size() >= source_components.size() &&
        std::equal(source_components.begin(), source_components.end(), destination_components.begin(),
                   [](const std::string &a,const std::string &b){return EqualsNoCase(a,b);})) {
        error = "A folder cannot be copied/moved into itself or one of its descendants.";
        return false;
    }
    if (!ValidateImportDestination(snapshot, destination_partition, destination_components, src_root->name, error)) return false;

    std::vector<TransferEntry> entries;
    uint64_t total_bytes=0,total_operations=0;
    uint32_t file_count=0,directory_count=0;
    auto append = [&](auto &&self, const XemuFatxHdd::Entry &src,
                      std::vector<std::string> src_path,
                      std::vector<std::string> dst_path, unsigned depth)->bool {
        if (depth > kImportMaxDepth || entries.size() >= kImportMaxEntries) { error="FATX Copy exceeded the entry/depth safety limit."; return false; }
        TransferEntry item;
        item.source_from_fatx=true;
        item.source_partition=source_partition;
        item.source_components=src_path;
        item.source_directory_entry_offset=src.directory_entry_offset;
        item.source_first_cluster=src.first_cluster;
        item.source_modified_time=src.modified_time;
        item.source_modified_date=src.modified_date;
        item.source_attributes=src.attributes;
        item.components=dst_path;
        item.file_size=src.file_size;
        item.directory=src.directory;
        item.fatx_path=FatxPathForPartition(destination_partition,item.components);
        if(!NativePathForPartition(destination_partition,item.components,item.native_path,error)) return false;
        entries.push_back(item);
        if(src.directory){
            ++directory_count; ++total_operations;
            for(const auto &child:src.children){auto sp=src_path;sp.push_back(child.name);auto dp=dst_path;dp.push_back(child.name);if(!self(self,child,std::move(sp),std::move(dp),depth+1))return false;}
        }else{
            if(src.file_size > kImportMaxTotalBytes-total_bytes){error="FATX Copy exceeds the 64 MiB total-data safety limit.";return false;}
            total_bytes+=src.file_size;++file_count;
            total_operations += src.file_size==0?1u:(src.file_size+kImportChunkBytes-1u)/kImportChunkBytes;
        }
        return true;
    };
    std::vector<std::string> dst_root=destination_components; dst_root.push_back(src_root->name);
    if(!append(append,*src_root,source_components,std::move(dst_root),0u)) return false;

    plan.partition=destination_partition;
    plan.destination_components=destination_components;
    plan.source_is_directory=source_directory;
    plan.source_from_fatx=true;
    plan.source_partition=source_partition;
    plan.source_components=source_components;
    plan.root_name=src_root->name;
    plan.entries=std::move(entries);
    plan.total_bytes=total_bytes; plan.file_count=file_count; plan.directory_count=directory_count; plan.total_operations=total_operations;
    plan.delete_source_after_copy=delete_source_after_copy;
    if(delete_source_after_copy){
        if(!BuildDeletePlan(snapshot,source_partition,source_components,source_directory,plan.source_delete_plan,error)) return false;
    }
    return true;
}

bool BuildCreateDirectoryPlanAtDestination(
    const std::string &name,
    const XemuFatxHdd::Snapshot &snapshot,
    char partition_letter,
    const std::vector<std::string> &destination_components,
    TransferPlan &plan,
    std::string &error)
{
    error.clear();
    plan = {};
    std::string name_error;
    if (!IsSafeFatxImportComponent(name, name_error)) {
        error = "Folder name is not safe for FATX: " + name_error;
        return false;
    }
    const char partition_key = static_cast<char>(
        std::toupper(static_cast<unsigned char>(partition_letter)));
    if (!ValidateImportDestination(snapshot, partition_key,
                                   destination_components, name, error)) {
        return false;
    }
    std::vector<std::string> components = destination_components;
    components.push_back(name);
    std::string native_path;
    if (!NativePathForPartition(partition_key, components, native_path, error)) {
        return false;
    }
    TransferEntry item;
    item.fatx_path = FatxPathForPartition(partition_key, components);
    item.native_path = std::move(native_path);
    item.components = std::move(components);
    item.directory = true;

    plan.partition = partition_key;
    plan.destination_components = destination_components;
    plan.kind = TransferKind::CreateFatxDirectory;
    plan.source_is_directory = true;
    plan.synthetic_directory = true;
    plan.root_name = name;
    plan.entries.push_back(std::move(item));
    plan.directory_count = 1;
    plan.total_operations = 1;
    return true;
}

bool ValidateImportHostEntryMetadata(const TransferEntry &item, std::string &error)
{
    namespace fs = std::filesystem;
    error.clear();
    if (item.source_from_fatx || (item.directory && item.host_path.empty())) {
        return true;
    }
    std::error_code ec;
    const fs::path host = fs::u8path(item.host_path);
    const fs::file_status status = fs::symlink_status(host, ec);
    if (ec || fs::is_symlink(status) ||
        (item.directory ? !fs::is_directory(status) : !fs::is_regular_file(status))) {
        error = "A host item changed or became unavailable before Kernel Import: " + item.host_path;
        return false;
    }
    int64_t write_time = 0;
    if (!ReadHostWriteTime(host, write_time, error) || write_time != item.host_write_time) {
        if (error.empty()) error = "A host item changed before Kernel Import: " + item.host_path;
        return false;
    }
    if (!item.directory) {
        const uint64_t size = static_cast<uint64_t>(fs::file_size(host, ec));
        if (ec || size != item.file_size) {
            error = "A host file size changed before Kernel Import: " + item.host_path;
            return false;
        }
    }
    return true;
}

} // namespace XemuKernelFs
