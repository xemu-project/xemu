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

#include "map-labels.hh"
#include "label-symbol-utils.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace XemuMapLabels {
namespace {

using XemuLabelSymbolUtils::upper_ascii;

constexpr size_t kMaxMapSymbols = 200000;

struct SegmentInfo {
    uint32_t number = 0;
    uint32_t span = 0;
    std::vector<std::string> contributions;
    int section_index = -1;
};

struct RawSymbol {
    uint32_t segment = 0;
    uint32_t offset = 0;
    uint32_t rva_plus_base = 0;
    std::string name;
    bool function = false;
};

static std::string trim(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(),
                                        [](unsigned char ch) {
                                            return std::isspace(ch) != 0;
                                        });
    const auto last = std::find_if_not(value.rbegin(), value.rend(),
                                       [](unsigned char ch) {
                                           return std::isspace(ch) != 0;
                                       }).base();
    if (first >= last) {
        return {};
    }
    return std::string(first, last);
}

static bool parse_hex(const std::string &text, uint32_t &value)
{
    if (text.empty() || text.size() > 8) {
        return false;
    }
    uint64_t out = 0;
    for (unsigned char ch : text) {
        unsigned digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        } else {
            return false;
        }
        out = (out << 4) | digit;
    }
    value = (uint32_t)out;
    return true;
}

static bool parse_segment_address(const std::string &token,
                                  uint32_t &segment, uint32_t &offset)
{
    const size_t colon = token.find(':');
    if (colon == std::string::npos || colon == 0 ||
        colon + 1 >= token.size()) {
        return false;
    }
    return parse_hex(token.substr(0, colon), segment) &&
           parse_hex(token.substr(colon + 1), offset);
}

static std::string strip_leading_dot(std::string value)
{
    while (!value.empty() && value.front() == '.') {
        value.erase(value.begin());
    }
    return value;
}

static int contribution_score(const std::string &contribution,
                              const std::string &section)
{
    const std::string c = upper_ascii(contribution);
    const std::string s = upper_ascii(section);
    if (c == s) {
        return 100;
    }

    const size_t dollar = c.find('$');
    const std::string dollar_base =
        dollar == std::string::npos ? c : c.substr(0, dollar);
    if (dollar_base == s) {
        return 95;
    }
    if (strip_leading_dot(dollar_base) == strip_leading_dot(s)) {
        return 90;
    }

    // Xbox linker library sections commonly have contribution suffixes such
    // as D3D_RD, XMV_RW, DSOUND_URW, and XGRPH_RD.
    if (!s.empty() && c.size() > s.size() &&
        c.compare(0, s.size(), s) == 0 &&
        (c[s.size()] == '_' || c[s.size()] == '$')) {
        return 85;
    }
    if (!s.empty()) {
        const std::string bare_s = strip_leading_dot(s);
        const std::string bare_c = strip_leading_dot(c);
        if (bare_c.size() > bare_s.size() &&
            bare_c.compare(0, bare_s.size(), bare_s) == 0 &&
            (bare_c[bare_s.size()] == '_' || bare_c[bare_s.size()] == '$')) {
            return 80;
        }
    }
    return 0;
}

static void resolve_segments(std::map<uint32_t, SegmentInfo> &segments,
                             const XemuXbeLabels::Database &database)
{
    std::set<int> used;
    for (auto &entry : segments) {
        SegmentInfo &segment = entry.second;
        int best_index = -1;
        int best_score = 0;
        bool tied = false;
        for (size_t i = 0; i < database.sections.size(); ++i) {
            if (used.count((int)i) != 0) {
                continue;
            }
            int score = 0;
            for (const std::string &name : segment.contributions) {
                score = std::max(score,
                                 contribution_score(name,
                                                    database.sections[i].name));
            }
            if (score > best_score) {
                best_score = score;
                best_index = (int)i;
                tied = false;
            } else if (score != 0 && score == best_score) {
                tied = true;
            }
        }
        if (best_score != 0 && !tied) {
            segment.section_index = best_index;
            used.insert(best_index);
        }
    }
}

static bool parse_header_hex(const std::string &line, const char *marker,
                             uint32_t &value)
{
    const size_t pos = line.find(marker);
    if (pos == std::string::npos) {
        return false;
    }
    std::string rest = trim(line.substr(pos + std::char_traits<char>::length(marker)));
    const size_t space = rest.find_first_of(" \t(");
    if (space != std::string::npos) {
        rest.resize(space);
    }
    return parse_hex(rest, value);
}

} // namespace

bool ParseAndResolve(const std::string &text,
                     const XemuXbeLabels::Database &database,
                     uint32_t xbe_pe_timestamp,
                     std::vector<XemuXbeLabels::Label> &labels,
                     Status &status, std::string &error)
{
    labels.clear();
    status = {};
    error.clear();
    if (text.empty()) {
        error = "MAP file is empty.";
        status.message = error;
        return false;
    }
    if (database.sections.empty()) {
        error = "Current XBE has no parsed section table.";
        status.message = error;
        return false;
    }

    std::map<uint32_t, SegmentInfo> segments;
    std::vector<RawSymbol> symbols;
    symbols.reserve(50000);

    bool in_symbols = false;
    bool have_timestamp = false;
    std::istringstream input(text);
    std::string line;
    bool title_seen = false;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::string stripped = trim(line);
        if (!title_seen && !stripped.empty()) {
            status.title = stripped;
            title_seen = true;
        }
        uint32_t header_value = 0;
        if (parse_header_hex(line, "Timestamp is", header_value)) {
            status.timestamp = header_value;
            have_timestamp = true;
        }
        if (parse_header_hex(line, "Preferred load address is", header_value)) {
            status.preferred_load_address = header_value;
        }
        if (line.find("Publics by Value") != std::string::npos) {
            in_symbols = true;
            continue;
        }

        if (!in_symbols) {
            unsigned segment = 0, start = 0, length = 0;
            char name[256] = {};
            char klass[64] = {};
            if (std::sscanf(line.c_str(), " %x:%x %xH %255s %63s",
                            &segment, &start, &length, name, klass) == 5 &&
                segment != 0) {
                SegmentInfo &info = segments[(uint32_t)segment];
                info.number = (uint32_t)segment;
                info.span = std::max<uint32_t>(
                    info.span, (uint32_t)start + (uint32_t)length);
                if (std::find(info.contributions.begin(),
                              info.contributions.end(), name) ==
                    info.contributions.end()) {
                    info.contributions.emplace_back(name);
                }
            }
            continue;
        }

        if (stripped.empty() || stripped == "Static symbols" ||
            stripped.find("entry point at") == 0) {
            continue;
        }
        std::istringstream row(line);
        std::string address_token, symbol_name, rva_token;
        if (!(row >> address_token >> symbol_name >> rva_token)) {
            continue;
        }
        RawSymbol symbol;
        if (!parse_segment_address(address_token, symbol.segment,
                                   symbol.offset) ||
            !parse_hex(rva_token, symbol.rva_plus_base)) {
            continue;
        }
        std::string token;
        while (row >> token) {
            if (token == "f") {
                symbol.function = true;
            }
        }
        symbol.name = std::move(symbol_name);
        if (symbols.size() >= kMaxMapSymbols) {
            error = "MAP file contains too many symbols.";
            status.message = error;
            return false;
        }
        symbols.push_back(std::move(symbol));
    }

    status.parsed = true;
    status.parsed_symbols = symbols.size();
    for (const RawSymbol &symbol : symbols) {
        if (symbol.function) {
            ++status.function_symbols;
        } else {
            ++status.data_symbols;
        }
    }

    if (!have_timestamp) {
        error = "MAP timestamp was not found; addresses cannot be validated safely.";
        status.message = error;
        return false;
    }
    status.timestamp_match = xbe_pe_timestamp != 0 &&
                             status.timestamp == xbe_pe_timestamp;
    if (!status.timestamp_match) {
        char msg[192];
        std::snprintf(msg, sizeof(msg),
                      "MAP PE timestamp mismatch: MAP %08X, current XBE %08X. No labels were applied.",
                      status.timestamp, xbe_pe_timestamp);
        error = msg;
        status.message = error;
        return false;
    }

    resolve_segments(segments, database);
    status.layout_match = true;
    for (const auto &entry : segments) {
        const SegmentInfo &segment = entry.second;
        if (segment.section_index < 0) {
            continue;
        }
        ++status.mapped_segments;
        const auto &section = database.sections[(size_t)segment.section_index];
        const uint64_t span = std::max<uint32_t>(section.virtual_size,
                                                 section.raw_size);
        if ((uint64_t)segment.span > span) {
            status.layout_match = false;
        }
    }
    if (status.mapped_segments == 0) {
        status.layout_match = false;
    }
    if (!status.layout_match) {
        error = "MAP segment layout does not fit the current XBE sections. No labels were applied.";
        status.message = error;
        return false;
    }

    labels.reserve(symbols.size());
    for (const RawSymbol &symbol : symbols) {
        if (symbol.segment == 0) {
            ++status.skipped_symbols;
            continue;
        }
        const auto seg_it = segments.find(symbol.segment);
        if (seg_it == segments.end() || seg_it->second.section_index < 0) {
            ++status.unresolved_symbols;
            continue;
        }
        const auto &section =
            database.sections[(size_t)seg_it->second.section_index];
        const uint64_t span = std::max<uint32_t>(section.virtual_size,
                                                 section.raw_size);
        if ((uint64_t)symbol.offset >= span) {
            ++status.unresolved_symbols;
            continue;
        }
        const std::string name = XemuLabelSymbolUtils::display_microsoft_symbol(symbol.name);
        if (name.empty()) {
            ++status.skipped_symbols;
            continue;
        }
        const uint64_t va = (uint64_t)section.virtual_address + symbol.offset;
        if (va > 0xFFFFFFFFull) {
            ++status.unresolved_symbols;
            continue;
        }

        XemuXbeLabels::Label label;
        label.virtual_address = (uint32_t)va;
        label.type = symbol.function ? XemuXbeLabels::Type::Function
                                     : XemuXbeLabels::Type::Symbol;
        label.name = name;
        label.source = XemuXbeLabels::Source::Map;
        label.confidence = XemuXbeLabels::Confidence::Exact;
        label.section_name = section.name;
        label.section_offset = symbol.offset;
        label.has_section_location = true;
        labels.push_back(std::move(label));
    }

    // Remove exact duplicates produced by COMDAT/static entries while keeping
    // aliases at the same address available in the label browser.
    XemuLabelSymbolUtils::sort_and_dedupe_labels(labels);

    status.resolved_labels = labels.size();
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "MAP exact match: %zu label(s) resolved from %zu symbol(s) across %zu XBE section(s).",
                  status.resolved_labels, status.parsed_symbols,
                  status.mapped_segments);
    status.message = msg;
    return true;
}

} // namespace XemuMapLabels
