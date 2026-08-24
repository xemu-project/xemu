//
// Read-only XDVDFS -> host export service.
//
#include "xdvdfs-export-service.hh"
#include "host-export-utils.hh"

#include "binary-utils.hh"
#include "disc-block-io.h"
#include "xdvdfs-disc.hh"

#include <glib/gstdio.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fcntl.h>
#include <system_error>
#include <vector>

namespace XemuXdvdfsExport {
namespace {

using XemuHostExportUtils::HostSafeName;
using XemuHostExportUtils::NumberedCandidate;
using XemuDebugBinaryUtils::range_inside;

constexpr size_t kCopyChunkSize = 1024 * 1024;
constexpr size_t kMaxExportDepth = 64;

bool OpenUniqueHostFile(const std::string &requested, std::string &actual,
                        FILE *&fp, std::string &error)
{
    actual.clear();
    fp = nullptr;
    for (unsigned i = 0; i < 10000; ++i) {
        const std::string candidate = NumberedCandidate(requested, i);
        int flags = O_CREAT | O_EXCL | O_WRONLY;
#ifdef O_BINARY
        flags |= O_BINARY;
#endif
        const int fd = g_open(candidate.c_str(), flags, 0666);
        if (fd < 0) {
            if (errno == EEXIST) {
                continue;
            }
            error = "Unable to exclusively create disc export file: " + candidate;
            return false;
        }
        fp = fdopen(fd, "wb");
        if (!fp) {
            g_close(fd, nullptr);
            g_remove(candidate.c_str());
            error = "Unable to open the exclusively-created disc export file: " + candidate;
            return false;
        }
        actual = candidate;
        return true;
    }
    error = "Could not claim a unique disc export filename after 10000 attempts.";
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
            error = "Unable to exclusively create disc export folder: " +
                    candidate.string();
            return false;
        }
    }
    error = "Could not claim a unique disc export folder after 10000 attempts.";
    return false;
}

struct DiscReader {
    XemuDiscBlockHandle disc = nullptr;
    uintptr_t identity = 0;
    uint64_t length = 0;

    bool Read(uint64_t offset, void *buffer, size_t size) const
    {
        return disc && xemu_disc_block_identity(disc) == identity &&
               range_inside(offset, size, length) &&
               xemu_disc_block_pread(disc, offset, buffer, size);
    }
};

bool ExportEntryRecursive(const DiscReader &reader,
                          const XemuXdvdfs::Entry &entry,
                          const std::string &requested_path, size_t depth,
                          Result &result, std::string &actual_path,
                          std::string &error)
{
    namespace fs = std::filesystem;
    if (depth > kMaxExportDepth) {
        error = "Disc export directory depth exceeds the safety limit.";
        return false;
    }

    if (entry.IsDirectory()) {
        if (!CreateUniqueHostDirectory(requested_path, actual_path, error)) {
            return false;
        }
        ++result.directory_count;
        for (const XemuXdvdfs::Entry &child : entry.children) {
            const std::string child_requested =
                (fs::path(actual_path) / HostSafeName(child.name)).string();
            std::string child_actual;
            if (!ExportEntryRecursive(reader, child, child_requested, depth + 1,
                                      result, child_actual, error)) {
                return false;
            }
        }
        return true;
    }

    FILE *fp = nullptr;
    if (!OpenUniqueHostFile(requested_path, actual_path, fp, error)) {
        return false;
    }

    std::vector<uint8_t> buffer;
    buffer.resize(std::min<uint64_t>(kCopyChunkSize,
                                     std::max<uint64_t>(entry.size, 1)));
    uint64_t done = 0;
    bool ok = true;
    while (done < entry.size) {
        const size_t chunk = static_cast<size_t>(std::min<uint64_t>(
            buffer.size(), static_cast<uint64_t>(entry.size) - done));
        if (!reader.Read(entry.disc_offset + done, buffer.data(), chunk)) {
            error = "The mounted disc changed or a disc data read failed during export.";
            ok = false;
            break;
        }
        if (std::fwrite(buffer.data(), 1, chunk, fp) != chunk) {
            error = "Unable to write disc export file: " + actual_path;
            ok = false;
            break;
        }
        done += chunk;
    }
    if (std::fclose(fp) != 0 && ok) {
        error = "Unable to finalize disc export file: " + actual_path;
        ok = false;
    }
    if (!ok) {
        g_remove(actual_path.c_str());
        return false;
    }

    ++result.file_count;
    result.byte_count += entry.size;
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
        error = "Select a disc file or folder to export.";
        return false;
    }

    XemuDiscBlockHandle disc = xemu_disc_block_by_name("ide0-cd1");
    if (!xemu_disc_block_is_available(disc)) {
        error = "Xbox DVD (ide0-cd1) is not available.";
        return false;
    }
    const int64_t signed_length = xemu_disc_block_get_length(disc);
    if (signed_length <= 0) {
        error = "Unable to determine mounted disc size.";
        return false;
    }

    DiscReader reader;
    reader.disc = disc;
    reader.identity = xemu_disc_block_identity(disc);
    reader.length = static_cast<uint64_t>(signed_length);

    XemuXdvdfs::Disc fresh;
    if (!XemuXdvdfs::Parse(
            [&reader](uint64_t offset, void *buffer, size_t size) {
                return reader.Read(offset, buffer, size);
            },
            reader.length, fresh, error)) {
        if (error.empty()) {
            error = "Unable to refresh the mounted XDVDFS directory.";
        }
        return false;
    }

    const XemuXdvdfs::Entry *entry = XemuXdvdfs::FindEntry(fresh, target.components);
    if (!entry || entry->IsDirectory() != target.directory ||
        entry->size != target.size || entry->start_sector != target.start_sector) {
        error = "The selected disc item changed or no longer exists. Refresh and try again.";
        return false;
    }

    std::error_code ec;
    const fs::path destination_path(destination);
    if (!fs::is_directory(destination_path, ec) || ec) {
        error = "Select an existing host destination folder for disc export.";
        return false;
    }

    const std::string requested =
        (destination_path / HostSafeName(entry->name)).string();
    std::string host_path;
    result.directory = entry->IsDirectory();
    if (!ExportEntryRecursive(reader, *entry, requested, 0, result,
                              host_path, error)) {
        if (entry->IsDirectory() && !host_path.empty()) {
            std::error_code cleanup_error;
            fs::remove_all(fs::path(host_path), cleanup_error);
        }
        return false;
    }
    result.host_path = host_path;
    return true;
}

} // namespace XemuXdvdfsExport
