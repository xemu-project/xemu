//
// xemu RAW Cheat Engine - portable .xlabel label packs
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include "xbe-labels.hh"

#include <cstdint>
#include <string>
#include <vector>

namespace XemuLabelPacks {

constexpr uint32_t kFormatVersion = 1;

struct Identity {
    uint32_t title_id = 0;
    std::string game_id;
    std::string name;
    std::string header_sha256;
    std::string xbe_sha256;
};

struct Header {
    uint32_t format_version = 0;
    uint32_t title_id = 0;
    bool title_id_valid = false;
    std::string game_id;
    std::string name;
    std::string header_sha256;
    std::string xbe_sha256;
};

struct Pack {
    Header header;
    std::vector<XemuXbeLabels::Label> labels;
};

bool Parse(const std::string &text, Pack &pack, std::string &error);
bool Serialize(const Identity &identity,
               const XemuXbeLabels::Database &database,
               std::string &text, std::string &error);

// Header Hash follows the same prefix-matching rule used by the Cheat files.
// XBE Hash is an additional revision guard when both sides have one available.
bool Matches(const Header &header, const Identity &identity,
             std::string &reason);

// Resolve section-relative entries against the current XBE.  Physical
// addresses are never stored or loaded by this format.
bool Resolve(const Pack &pack, const XemuXbeLabels::Database &current_xbe,
             std::vector<XemuXbeLabels::Label> &labels,
             size_t &unresolved, std::string &error);

} // namespace XemuLabelPacks
