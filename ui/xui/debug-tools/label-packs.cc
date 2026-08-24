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

#include "label-packs.hh"
#include "label-symbol-utils.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace XemuLabelPacks {
namespace {

static std::string trim(const std::string &value)
{
    size_t first = 0;
    while (first < value.size() &&
           std::isspace((unsigned char)value[first])) {
        ++first;
    }
    size_t last = value.size();
    while (last > first && std::isspace((unsigned char)value[last - 1])) {
        --last;
    }
    return value.substr(first, last - first);
}

static bool parse_hex_u32(const std::string &text, uint32_t &value)
{
    std::string v = trim(text);
    if (v.size() >= 2 && v[0] == '0' && (v[1] == 'x' || v[1] == 'X')) {
        v.erase(0, 2);
    }
    if (v.empty() || v.size() > 8) {
        return false;
    }
    uint64_t result = 0;
    for (char ch : v) {
        unsigned digit = 0;
        if (ch >= '0' && ch <= '9') digit = (unsigned)(ch - '0');
        else if (ch >= 'a' && ch <= 'f') digit = 10u + (unsigned)(ch - 'a');
        else if (ch >= 'A' && ch <= 'F') digit = 10u + (unsigned)(ch - 'A');
        else return false;
        result = (result << 4) | digit;
    }
    value = (uint32_t)result;
    return true;
}

static bool parse_game_id(const std::string &text, uint32_t &title_id)
{
    std::string value = XemuLabelSymbolUtils::upper_ascii(trim(text));
    if (value.size() >= 2 && value[0] == '0' && value[1] == 'X') {
        value.erase(0, 2);
    }
    if (value.size() == 8) {
        return parse_hex_u32(value, title_id);
    }
    if (value.size() == 6 &&
        value[0] >= 0x21 && value[0] <= 0x7e &&
        value[1] >= 0x21 && value[1] <= 0x7e) {
        uint32_t low = 0;
        if (!parse_hex_u32(value.substr(2), low)) {
            return false;
        }
        title_id = ((uint32_t)(uint8_t)value[0] << 24) |
                   ((uint32_t)(uint8_t)value[1] << 16) | low;
        return true;
    }
    return false;
}

static bool hash_matches(const std::string &wanted,
                         const std::string &current)
{
    std::string a = XemuLabelSymbolUtils::upper_ascii(trim(wanted));
    std::string b = XemuLabelSymbolUtils::upper_ascii(trim(current));
    if (a.size() >= 2 && a[0] == '0' && a[1] == 'X') {
        a.erase(0, 2);
    }
    if (b.size() >= 2 && b[0] == '0' && b[1] == 'X') {
        b.erase(0, 2);
    }
    if (a.empty() || b.empty() || a.size() > b.size()) {
        return false;
    }
    for (char ch : a) {
        if (!std::isxdigit((unsigned char)ch)) {
            return false;
        }
    }
    return b.compare(0, a.size(), a) == 0;
}

static std::string escape_field(const std::string &value)
{
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == '|') {
            out.push_back('\\');
            out.push_back(ch);
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

static bool split_fields(const std::string &line,
                         std::vector<std::string> &fields)
{
    fields.clear();
    std::string current;
    bool escaped = false;
    for (char ch : line) {
        if (escaped) {
            if (ch == 'n') current.push_back('\n');
            else if (ch == 'r') current.push_back('\r');
            else current.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
        } else if (ch == '|') {
            fields.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (escaped) {
        return false;
    }
    fields.push_back(current);
    return true;
}

static void write_hex8(std::ostringstream &out, uint32_t value)
{
    char buffer[9];
    std::snprintf(buffer, sizeof(buffer), "%08X", value);
    out << buffer;
}

} // namespace

bool Parse(const std::string &text, Pack &pack, std::string &error)
{
    pack = {};
    error.clear();
    std::istringstream stream(text);
    std::string line;
    bool in_labels = false;
    size_t line_number = 0;

    while (std::getline(stream, line)) {
        ++line_number;
        std::string t = trim(line);
        if (t.empty() || t[0] == ';' || t[0] == '#') {
            continue;
        }
        if (!t.empty() && t.front() == '[' && t.back() == ']') {
            in_labels = XemuLabelSymbolUtils::upper_ascii(t) == "[LABELS]";
            continue;
        }
        if (!in_labels && t[0] == '^') {
            const size_t eq = t.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            const std::string lhs = XemuLabelSymbolUtils::upper_ascii(trim(t.substr(0, eq)));
            const std::string rhs = trim(t.substr(eq + 1));
            const size_t colon = rhs.find(':');
            const std::string key = colon == std::string::npos
                                        ? std::string()
                                        : XemuLabelSymbolUtils::upper_ascii(trim(rhs.substr(0, colon)));
            const std::string value = colon == std::string::npos
                                          ? rhs
                                          : trim(rhs.substr(colon + 1));
            if (lhs == "^1" && (key.empty() || key == "HASH")) {
                pack.header.header_sha256 = XemuLabelSymbolUtils::upper_ascii(value);
            } else if (lhs == "^2" &&
                       (key.empty() || key == "GAMEID" || key == "GAME ID")) {
                pack.header.game_id = XemuLabelSymbolUtils::upper_ascii(value);
                pack.header.title_id_valid =
                    parse_game_id(value, pack.header.title_id);
            } else if (lhs == "^3" && (key.empty() || key == "NAME")) {
                pack.header.name = value;
            } else if (lhs == "^4" &&
                       (key.empty() || key == "XBEHASH" ||
                        key == "XBE HASH" || key == "XBE_SHA256" ||
                        key == "XBE SHA-256")) {
                pack.header.xbe_sha256 = XemuLabelSymbolUtils::upper_ascii(value);
            } else if (lhs == "^5" &&
                       (key.empty() || key == "FORMAT" ||
                        key == "FORMATVERSION" || key == "FORMAT VERSION")) {
                char *end = nullptr;
                const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
                if (end != value.c_str() && end != nullptr && *end == '\0' &&
                    parsed <= 0xfffffffful) {
                    pack.header.format_version = (uint32_t)parsed;
                }
            }
            continue;
        }
        if (!in_labels) {
            continue;
        }

        std::vector<std::string> fields;
        if (!split_fields(line, fields) || fields.size() != 7) {
            error = "Invalid .xlabel label record on line " +
                    std::to_string(line_number) + ".";
            return false;
        }

        XemuXbeLabels::Label label;
        label.section_name = fields[0];
        label.has_section_location = label.section_name != "@VA";
        if (!parse_hex_u32(fields[1], label.section_offset) ||
            !parse_hex_u32(fields[2], label.virtual_address)) {
            error = "Invalid .xlabel address on line " +
                    std::to_string(line_number) + ".";
            return false;
        }
        if (!XemuXbeLabels::TypeFromName(trim(fields[3]), label.type) ||
            !XemuXbeLabels::SourceFromName(trim(fields[4]), label.source) ||
            !XemuXbeLabels::ConfidenceFromName(trim(fields[5]),
                                               label.confidence)) {
            error = "Invalid .xlabel type/source/confidence on line " +
                    std::to_string(line_number) + ".";
            return false;
        }
        label.name = fields[6];
        if (label.name.empty()) {
            error = "Empty .xlabel label name on line " +
                    std::to_string(line_number) + ".";
            return false;
        }
        pack.labels.push_back(std::move(label));
    }

    if (pack.header.format_version == 0) {
        error = "Missing ^5 = FORMAT header in .xlabel file.";
        return false;
    }
    if (pack.header.format_version != kFormatVersion) {
        error = "Unsupported .xlabel format version " +
                std::to_string(pack.header.format_version) + ".";
        return false;
    }
    if (!pack.header.title_id_valid) {
        error = "Missing or invalid ^2 = GameID header in .xlabel file.";
        return false;
    }
    if (pack.header.header_sha256.empty()) {
        error = "Missing ^1 = Hash header in .xlabel file.";
        return false;
    }
    if (pack.labels.empty()) {
        error = ".xlabel file contains no [LABELS] records.";
        return false;
    }
    return true;
}

bool Serialize(const Identity &identity,
               const XemuXbeLabels::Database &database,
               std::string &text, std::string &error)
{
    text.clear();
    error.clear();
    if (identity.title_id == 0 || identity.header_sha256.empty()) {
        error = "Current game identity is incomplete; cannot save .xlabel.";
        return false;
    }
    if (database.labels.empty()) {
        error = "There are no current labels to save.";
        return false;
    }

    std::ostringstream out;
    out << "; XEMU - CHEATS portable label pack\n";
    out << "; Physical addresses are intentionally not stored.\n";
    out << "; Section-relative XBE locations are the persistent label master.\n\n";
    out << "^1 = Hash: " << XemuLabelSymbolUtils::upper_ascii(identity.header_sha256) << "\n";
    out << "^2 = GameID: " << XemuLabelSymbolUtils::upper_ascii(identity.game_id) << "\n";
    out << "^3 = NAME: " << identity.name << "\n";
    out << "^4 = XBEHASH: " << XemuLabelSymbolUtils::upper_ascii(identity.xbe_sha256) << "\n";
    out << "^5 = FORMAT: " << kFormatVersion << "\n\n";
    out << "[LABELS]\n";
    out << "; SECTION|OFFSET|VIRTUAL_HINT|TYPE|SOURCE|CONFIDENCE|LABEL\n";

    for (const XemuXbeLabels::Label &label : database.labels) {
        if (label.has_section_location && !label.section_name.empty()) {
            out << escape_field(label.section_name) << '|';
            write_hex8(out, label.section_offset);
        } else {
            out << "@VA|";
            write_hex8(out, label.virtual_address);
        }
        out << '|';
        write_hex8(out, label.virtual_address);
        out << '|' << XemuXbeLabels::TypeName(label.type)
            << '|' << XemuXbeLabels::SourceName(label.source)
            << '|' << XemuXbeLabels::ConfidenceName(label.confidence)
            << '|' << escape_field(label.name) << '\n';
    }
    text = out.str();
    return true;
}

bool Matches(const Header &header, const Identity &identity,
             std::string &reason)
{
    reason.clear();
    if (!header.title_id_valid || header.title_id != identity.title_id) {
        reason = "GameID does not match the running XBE.";
        return false;
    }
    if (!hash_matches(header.header_sha256, identity.header_sha256)) {
        reason = "Header SHA-256 does not match the running XBE.";
        return false;
    }
    if (!header.xbe_sha256.empty() && !identity.xbe_sha256.empty() &&
        !hash_matches(header.xbe_sha256, identity.xbe_sha256)) {
        reason = "default.xbe SHA-256 does not match this revision.";
        return false;
    }
    return true;
}

bool Resolve(const Pack &pack, const XemuXbeLabels::Database &current_xbe,
             std::vector<XemuXbeLabels::Label> &labels,
             size_t &unresolved, std::string &error)
{
    labels.clear();
    unresolved = 0;
    error.clear();
    labels.reserve(pack.labels.size());

    for (const XemuXbeLabels::Label &stored : pack.labels) {
        XemuXbeLabels::Label label = stored;
        uint32_t resolved = 0;
        if (stored.has_section_location) {
            if (!XemuXbeLabels::ResolveSectionLocation(
                    current_xbe, stored.section_name,
                    stored.section_offset, resolved)) {
                ++unresolved;
                continue;
            }
            label.virtual_address = resolved;
        } else {
            // Header matching already established this as the same XBE
            // revision.  @VA is used only for labels outside an XBE section.
            label.virtual_address = stored.virtual_address;
        }
        labels.push_back(std::move(label));
    }

    if (labels.empty()) {
        error = "No .xlabel records could be resolved against the current XBE.";
        return false;
    }
    return true;
}

} // namespace XemuLabelPacks
