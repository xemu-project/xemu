//
// xemu RAW Cheat Engine - Microsoft linker .map label importer
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
#include <vector>

#include "xbe-labels.hh"

namespace XemuMapLabels {

struct Status {
    bool parsed = false;
    bool timestamp_match = false;
    bool layout_match = false;
    uint32_t timestamp = 0;
    uint32_t preferred_load_address = 0;
    size_t parsed_symbols = 0;
    size_t function_symbols = 0;
    size_t data_symbols = 0;
    size_t resolved_labels = 0;
    size_t unresolved_symbols = 0;
    size_t skipped_symbols = 0;
    size_t mapped_segments = 0;
    std::string title;
    std::string message;
};

// Parse a Microsoft linker MAP file and resolve segment:offset symbols against
// the sections of the currently mounted XBE.  MAP addresses are accepted only
// when the MAP timestamp matches the XBE's original PE timestamp and the MAP
// segment layout fits the parsed XBE sections.  This prevents an older/newer
// MAP from silently attaching correct names to the wrong code addresses.
bool ParseAndResolve(const std::string &text,
                     const XemuXbeLabels::Database &database,
                     uint32_t xbe_pe_timestamp,
                     std::vector<XemuXbeLabels::Label> &labels,
                     Status &status, std::string &error);

} // namespace XemuMapLabels
