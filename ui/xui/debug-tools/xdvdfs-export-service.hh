//
// Read-only XDVDFS -> host export service for Current Game / Disc Contents.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace XemuXdvdfsExport {

struct Target {
    std::vector<std::string> components;
    bool directory = false;
    uint32_t size = 0;
    uint32_t start_sector = 0;
};

struct Result {
    std::string host_path;
    uint64_t byte_count = 0;
    uint32_t file_count = 0;
    uint32_t directory_count = 0;
    bool directory = false;
};

// Reopens/reparses the currently mounted ide0-cd1 XDVDFS tree, re-resolves the
// selected path, and exports it using collision-safe create-new host paths.
// The mounted disc backend identity is checked before every data read so a
// media swap cannot silently produce a mixed export.
bool ExportToHost(const Target &target, const std::string &destination,
                  Result &result, std::string &error);

} // namespace XemuXdvdfsExport
