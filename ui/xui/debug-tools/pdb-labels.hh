//
// xemu RAW Cheat Engine - Microsoft PDB label importer
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "xbe-labels.hh"

namespace XemuPdbLabels {

struct Identity {
    bool valid = false;
    std::array<uint8_t, 16> guid{};
    uint32_t age = 0;
    std::string path;
};

struct Status {
    bool parsed = false;
    bool xbe_identity_found = false;
    bool guid_match = false;
    bool age_match = false;
    bool layout_match = false;
    uint32_t pdb_age = 0;
    uint32_t xbe_age = 0;
    uint16_t machine = 0;
    size_t public_symbols = 0;
    size_t function_symbols = 0;
    size_t data_symbols = 0;
    size_t resolved_labels = 0;
    size_t unresolved_symbols = 0;
    size_t skipped_symbols = 0;
    size_t mapped_sections = 0;
    std::string pdb_guid;
    std::string xbe_guid;
    std::string xbe_pdb_path;
    std::string message;
};

// Locate the CodeView RSDS record embedded in an XBE and return its PDB
// identity.  This is metadata only; no PDB contents are embedded or retained.
bool ExtractXbeIdentity(const std::vector<uint8_t> &xbe_file,
                        Identity &identity);

std::string FormatGuid(const std::array<uint8_t, 16> &guid);

// Parse a Microsoft MSF 7.00 PDB locally and import public CodeView symbols.
// Labels are accepted only when GUID + Age match the current XBE and the PDB's
// original PE section layout fits the parsed XBE section table.  Physical
// addresses are never persisted; resulting labels remain XBE-section-relative.
bool ParseAndResolve(const std::vector<uint8_t> &pdb_file,
                     const std::vector<uint8_t> &xbe_file,
                     const XemuXbeLabels::Database &database,
                     std::vector<XemuXbeLabels::Label> &labels,
                     Status &status, std::string &error);

} // namespace XemuPdbLabels
