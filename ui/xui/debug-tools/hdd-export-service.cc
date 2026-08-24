//
// Read-only FATX -> host export service.
//
#include "hdd-export-service.hh"
#include "host-export-utils.hh"

#include "disc-block-io.h"
#include "fatx-hdd.hh"
#include "guest-pause-guard.hh"

#include <glib/gstdio.h>

#include <cstdio>
#include <cerrno>
#include <fcntl.h>
#include <filesystem>
#include <system_error>

namespace XemuHddExport {
namespace {

using XemuHostExportUtils::HostSafeName;
using XemuHostExportUtils::NumberedCandidate;

bool ReadHddBlock(void *opaque, uint64_t offset, void *buffer, size_t size)
{
    return xemu_disc_block_pread((XemuDiscBlockHandle)opaque, offset, buffer,
                                 size);
}

bool WriteHostFile(void *opaque, const void *buffer, size_t size)
{
    FILE *fp = static_cast<FILE *>(opaque);
    return fp && (size == 0 || std::fwrite(buffer, 1, size, fp) == size);
}

bool OpenUniqueHostFile(const std::string &requested, std::string &actual,
                        FILE *&fp, std::string &error)
{
    actual.clear(); fp = nullptr;
    for (unsigned i = 0; i < 10000; ++i) {
        const std::string candidate = NumberedCandidate(requested, i);
        int flags = O_CREAT | O_EXCL | O_WRONLY;
#ifdef O_BINARY
        flags |= O_BINARY;
#endif
        const int fd = g_open(candidate.c_str(), flags, 0666);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            error = "Unable to exclusively create export file: " + candidate;
            return false;
        }
        fp = fdopen(fd, "wb");
        if (!fp) {
            g_close(fd, nullptr); g_remove(candidate.c_str());
            error = "Unable to open the exclusively-created export file: " + candidate;
            return false;
        }
        actual = candidate;
        return true;
    }
    error = "Could not claim a unique host export filename after 10000 attempts.";
    return false;
}

bool CreateUniqueHostDirectory(const std::string &requested, std::string &actual,
                               std::string &error)
{
    namespace fs = std::filesystem;
    actual.clear();
    for (unsigned i = 0; i < 10000; ++i) {
        const fs::path candidate(NumberedCandidate(requested, i));
        std::error_code ec;
        if (fs::create_directory(candidate, ec)) {
            actual = candidate.string();
            return true;
        }
        if (ec == std::errc::file_exists) {
            continue;
        }
        if (ec) {
            error = "Unable to exclusively create export folder: " + candidate.string();
            return false;
        }
    }
    error = "Could not claim a unique host export folder after 10000 attempts.";
    return false;
}

bool ExportEntryRecursive(XemuDiscBlockHandle hdd, uint64_t length,
                          const XemuFatxHdd::Partition &partition,
                          const XemuFatxHdd::Entry &entry,
                          const std::string &requested_path, size_t depth,
                          Result &result, std::string &actual_path,
                          std::string &error)
{
    namespace fs = std::filesystem;
    if (depth > 64) { error = "Export directory depth exceeds safety limit."; return false; }
    if (entry.directory) {
        if (!CreateUniqueHostDirectory(requested_path, actual_path, error)) return false;
        ++result.directory_count;
        for (const XemuFatxHdd::Entry &child : entry.children) {
            const std::string child_requested =
                (fs::path(actual_path) / HostSafeName(XemuFatxHdd::DisplayName(child))).string();
            std::string child_actual;
            if (!ExportEntryRecursive(hdd, length, partition, child,
                                      child_requested, depth + 1, result,
                                      child_actual, error)) return false;
        }
        return true;
    }
    FILE *fp = nullptr;
    if (!OpenUniqueHostFile(requested_path, actual_path, fp, error)) return false;
    bool ok = XemuFatxHdd::StreamFile(ReadHddBlock, hdd, length, partition,
                                      entry, WriteHostFile, fp, error);
    if (std::fclose(fp) != 0 && ok) { error = "Unable to finalize export file: " + actual_path; ok = false; }
    if (!ok) { g_remove(actual_path.c_str()); return false; }
    ++result.file_count; result.byte_count += entry.file_size;
    return true;
}

} // namespace

bool ExportToHost(const Target &target, const std::string &destination,
                  Result &result, std::string &error)
{
    namespace fs = std::filesystem;
    result = {};
    error.clear();
    if (target.components.empty()) {
        error = "Select a FATX file or folder to export.";
        return false;
    }

    XemuDiscBlockHandle hdd = xemu_disc_block_by_name("ide0-hd0");
    if (!xemu_disc_block_is_available(hdd)) {
        error = "Xbox HDD (ide0-hd0) is not available.";
        return false;
    }
    const int64_t signed_length = xemu_disc_block_get_length(hdd);
    if (signed_length <= 0) {
        error = "Unable to determine Xbox HDD size.";
        return false;
    }
    const uint64_t length = static_cast<uint64_t>(signed_length);

    // Fresh targeted metadata + all data bytes are read under one guest pause.
    XemuDebugGuestPauseGuard pause;
    if (!pause.IsValid()) {
        error = "Unable to pause the Xbox for a coherent HDD export.";
        return false;
    }
    XemuFatxHdd::Snapshot fresh;
    if (!XemuFatxHdd::BuildPartitionSnapshot(ReadHddBlock, hdd, length,
                                             target.partition, fresh)) {
        error = fresh.status.empty()
            ? "Unable to refresh the selected FATX volume for export."
            : fresh.status;
        return false;
    }
    const XemuFatxHdd::Partition *partition =
        XemuFatxHdd::FindPartition(fresh, target.partition);
    const XemuFatxHdd::Entry *entry = partition
        ? XemuFatxHdd::FindEntry(*partition, target.components) : nullptr;
    if (!partition || !entry || entry->directory != target.directory) {
        error = "The selected HDD item changed or no longer exists. Refresh and try again.";
        return false;
    }

    const std::string leaf = HostSafeName(XemuFatxHdd::DisplayName(*entry));
    const std::string requested = (fs::path(destination) / leaf).string();
    std::string host_path;
    result.directory = entry->directory;
    if (!ExportEntryRecursive(hdd, length, *partition, *entry, requested, 0,
                              result, host_path, error)) {
        if (entry->directory && !host_path.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(fs::path(host_path), cleanup_error);
        }
        return false;
    }
    result.host_path = host_path;
    return true;
}

} // namespace XemuHddExport
