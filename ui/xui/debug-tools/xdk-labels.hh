//
// xemu RAW Cheat Engine - local XDK symbol/signature importer
//
// The importer reads a user's local Xbox XDK static libraries and derives
// fingerprints/symbol names at runtime.  No XDK object code is built into xemu
// and the local cache stores fingerprints/relocation metadata only.
//
#pragma once

#include "xbe-labels.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace XemuXdkLabels {

struct Status {
    uint16_t build = 0;
    size_t required_libraries = 0;
    size_t found_libraries = 0;
    size_t signatures = 0;
    size_t exact_matches = 0;
    bool source_found = false;
    bool cache_found = false;
    bool cache_loaded = false;
    bool cache_rebuilt = false;
    bool needs_build = false;
    std::string source_directory;
    std::string cache_path;
    std::string message;
};

// Load a previously generated local XDK fingerprint cache and match it against
// the current XBE.  If rebuild_cache is true, rebuild the cache from local XDK
// .lib files first.  The cache intentionally contains no executable bytes.
bool Process(const std::string &xdk_root, const std::string &cache_root,
             const std::vector<uint8_t> &xbe_file,
             const XemuXbeLabels::Database &xbe_database,
             bool rebuild_cache,
             std::vector<XemuXbeLabels::Label> &labels,
             Status &status, std::string &error);

} // namespace XemuXdkLabels
