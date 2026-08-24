//
// Read-only FATX -> host export service for the Xbox HDD browser.
//
// Export owns fresh FATX source re-resolution, host-name sanitization,
// collision-safe destination creation, and recursive byte streaming. It never
// writes to FATX and is intentionally independent from ImGui/HDD window state.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace XemuHddExport {

struct Target {
    char partition = '?';
    std::vector<std::string> components;
    bool directory = false;
};

struct Result {
    std::string host_path;
    uint64_t byte_count = 0;
    uint32_t file_count = 0;
    uint32_t directory_count = 0;
    bool directory = false;
};

bool ExportToHost(const Target &target, const std::string &destination,
                  Result &result, std::string &error);

} // namespace XemuHddExport
