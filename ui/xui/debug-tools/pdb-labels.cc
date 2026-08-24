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

#include "pdb-labels.hh"
#include "binary-utils.hh"
#include "label-symbol-utils.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace XemuPdbLabels {
namespace {

using XemuDebugBinaryUtils::read_le16;
using XemuDebugBinaryUtils::read_le32;
using XemuDebugBinaryUtils::range_inside;

constexpr char kMsfMagic[32] = {
    'M','i','c','r','o','s','o','f','t',' ','C','/','C','+','+',' ','M','S','F',' ',
    '7','.','0','0','\r','\n',0x1a,'D','S',0,0,0
};
constexpr uint32_t kNilStreamSize = 0xFFFFFFFFu;
constexpr uint16_t kInvalidStream = 0xFFFFu;
constexpr uint16_t kSPub32 = 0x110Eu;
constexpr size_t kMaxStreams = 65536;
constexpr size_t kMaxPdbSymbols = 500000;
constexpr size_t kMaxPdbBytes = 512ull * 1024ull * 1024ull;

static int32_t sle32(const uint8_t *p)
{
    return (int32_t)read_le32(p);
}

static bool valid_block_size(uint32_t value)
{
    return value == 512 || value == 1024 || value == 2048 || value == 4096;
}

class MsfFile
{
public:
    bool Open(const std::vector<uint8_t> &file, std::string &error)
    {
        m_file = &file;
        m_sizes.clear();
        m_blocks.clear();
        if (file.size() < 56 || file.size() > kMaxPdbBytes ||
            std::memcmp(file.data(), kMsfMagic, sizeof(kMsfMagic)) != 0) {
            error = "PDB is not a supported Microsoft MSF 7.00 file.";
            return false;
        }

        m_block_size = read_le32(file.data() + 32);
        const uint32_t num_blocks = read_le32(file.data() + 40);
        const uint32_t directory_bytes = read_le32(file.data() + 44);
        const uint32_t block_map = read_le32(file.data() + 52);
        if (!valid_block_size(m_block_size) || num_blocks == 0 ||
            (uint64_t)num_blocks * m_block_size != file.size() ||
            directory_bytes < 4 || directory_bytes > file.size() ||
            block_map >= num_blocks) {
            error = "PDB MSF superblock is invalid or unsupported.";
            return false;
        }

        const uint32_t directory_block_count =
            (directory_bytes + m_block_size - 1) / m_block_size;
        if ((uint64_t)directory_block_count * 4 > m_block_size) {
            error = "PDB stream directory block map is too large for this importer.";
            return false;
        }

        const uint64_t map_offset = (uint64_t)block_map * m_block_size;
        if (!range_inside(map_offset, (uint64_t)directory_block_count * 4,
                          file.size())) {
            error = "PDB stream directory block map is truncated.";
            return false;
        }

        std::vector<uint8_t> directory;
        directory.reserve(directory_bytes);
        for (uint32_t i = 0; i < directory_block_count; ++i) {
            const uint32_t block = read_le32(file.data() + map_offset + i * 4);
            if (block >= num_blocks) {
                error = "PDB stream directory references an invalid block.";
                return false;
            }
            const uint64_t off = (uint64_t)block * m_block_size;
            const size_t remaining = directory_bytes - directory.size();
            const size_t take = std::min<size_t>(m_block_size, remaining);
            directory.insert(directory.end(), file.begin() + off,
                             file.begin() + off + take);
        }

        size_t pos = 0;
        if (directory.size() < 4) {
            error = "PDB stream directory is truncated.";
            return false;
        }
        const uint32_t stream_count = read_le32(directory.data());
        pos = 4;
        if (stream_count == 0 || stream_count > kMaxStreams ||
            !range_inside(pos, (uint64_t)stream_count * 4, directory.size())) {
            error = "PDB stream count is invalid.";
            return false;
        }
        m_sizes.resize(stream_count);
        for (uint32_t i = 0; i < stream_count; ++i) {
            m_sizes[i] = read_le32(directory.data() + pos);
            pos += 4;
        }
        m_blocks.resize(stream_count);
        for (uint32_t i = 0; i < stream_count; ++i) {
            if (m_sizes[i] == kNilStreamSize) {
                continue;
            }
            const uint32_t count =
                (m_sizes[i] + m_block_size - 1) / m_block_size;
            if (!range_inside(pos, (uint64_t)count * 4, directory.size())) {
                error = "PDB stream block list is truncated.";
                return false;
            }
            m_blocks[i].reserve(count);
            for (uint32_t j = 0; j < count; ++j) {
                const uint32_t block = read_le32(directory.data() + pos);
                pos += 4;
                if (block >= num_blocks) {
                    error = "PDB stream references an invalid block.";
                    return false;
                }
                m_blocks[i].push_back(block);
            }
        }
        return true;
    }

    bool ReadStream(uint32_t index, std::vector<uint8_t> &out,
                    std::string &error) const
    {
        out.clear();
        if (m_file == nullptr || index >= m_sizes.size() ||
            m_sizes[index] == kNilStreamSize) {
            error = "PDB references a missing stream.";
            return false;
        }
        const uint32_t size = m_sizes[index];
        out.reserve(size);
        for (uint32_t block : m_blocks[index]) {
            const uint64_t off = (uint64_t)block * m_block_size;
            const size_t remaining = size - out.size();
            const size_t take = std::min<size_t>(m_block_size, remaining);
            if (!range_inside(off, take, m_file->size())) {
                error = "PDB stream data is truncated.";
                return false;
            }
            out.insert(out.end(), m_file->begin() + off,
                       m_file->begin() + off + take);
        }
        if (out.size() != size) {
            error = "PDB stream size does not match its block list.";
            return false;
        }
        return true;
    }

    size_t StreamCount() const { return m_sizes.size(); }

private:
    const std::vector<uint8_t> *m_file = nullptr;
    uint32_t m_block_size = 0;
    std::vector<uint32_t> m_sizes;
    std::vector<std::vector<uint32_t>> m_blocks;
};

struct PdbSection {
    std::string name;
    uint32_t virtual_size = 0;
    uint32_t rva = 0;
};

struct PublicSymbol {
    uint32_t flags = 0;
    uint32_t offset = 0;
    uint16_t segment = 0;
    std::string name;
};

static bool ascii_iequals(const std::string &a, const std::string &b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::toupper((unsigned char)a[i]) !=
            std::toupper((unsigned char)b[i])) {
            return false;
        }
    }
    return true;
}

static std::string display_symbol(const std::string &name)
{
    if (name.rfind("___@@_PchSym_", 0) == 0 ||
        name.rfind("??_C@", 0) == 0) {
        return {};
    }
    return XemuLabelSymbolUtils::display_microsoft_symbol(name);
}

static bool parse_pdb_header(const MsfFile &msf, Identity &identity,
                             std::string &error)
{
    std::vector<uint8_t> stream;
    if (!msf.ReadStream(1, stream, error) || stream.size() < 28) {
        if (error.empty()) {
            error = "PDB Info stream is truncated.";
        }
        return false;
    }
    identity = {};
    identity.valid = true;
    identity.age = read_le32(stream.data() + 8);
    std::copy_n(stream.data() + 12, identity.guid.size(), identity.guid.begin());
    return true;
}

static bool parse_dbi(const MsfFile &msf, uint32_t &sym_stream,
                      uint16_t &machine, uint32_t &age,
                      std::vector<PdbSection> &sections,
                      std::string &error)
{
    std::vector<uint8_t> dbi;
    if (!msf.ReadStream(3, dbi, error) || dbi.size() < 64) {
        if (error.empty()) {
            error = "PDB DBI stream is truncated.";
        }
        return false;
    }
    if (sle32(dbi.data()) != -1) {
        error = "PDB DBI stream signature is unsupported.";
        return false;
    }

    age = read_le32(dbi.data() + 8);
    sym_stream = read_le16(dbi.data() + 20);
    const int32_t mod_size = sle32(dbi.data() + 24);
    const int32_t contrib_size = sle32(dbi.data() + 28);
    const int32_t section_map_size = sle32(dbi.data() + 32);
    const int32_t source_size = sle32(dbi.data() + 36);
    const int32_t type_server_size = sle32(dbi.data() + 40);
    const int32_t optional_size = sle32(dbi.data() + 48);
    const int32_t ec_size = sle32(dbi.data() + 52);
    machine = read_le16(dbi.data() + 58);
    if (mod_size < 0 || contrib_size < 0 || section_map_size < 0 ||
        source_size < 0 || type_server_size < 0 || optional_size < 0 ||
        ec_size < 0 || sym_stream == kInvalidStream ||
        sym_stream >= msf.StreamCount()) {
        error = "PDB DBI stream contains invalid sizes or stream indices.";
        return false;
    }

    const uint64_t optional_offset =
        64ull + (uint32_t)mod_size + (uint32_t)contrib_size +
        (uint32_t)section_map_size + (uint32_t)source_size +
        (uint32_t)type_server_size + (uint32_t)ec_size;
    if (!range_inside(optional_offset, (uint32_t)optional_size, dbi.size()) ||
        optional_size < 12) {
        error = "PDB optional debug header is missing section-header metadata.";
        return false;
    }
    const uint16_t section_header_stream =
        read_le16(dbi.data() + optional_offset + 5 * 2);
    if (section_header_stream == kInvalidStream ||
        section_header_stream >= msf.StreamCount()) {
        error = "PDB does not contain an original executable section-header stream.";
        return false;
    }

    std::vector<uint8_t> section_bytes;
    if (!msf.ReadStream(section_header_stream, section_bytes, error) ||
        section_bytes.empty() || section_bytes.size() % 40 != 0) {
        if (error.empty()) {
            error = "PDB section-header stream is malformed.";
        }
        return false;
    }
    sections.clear();
    sections.reserve(section_bytes.size() / 40);
    for (size_t off = 0; off < section_bytes.size(); off += 40) {
        const uint8_t *rec = section_bytes.data() + off;
        size_t name_len = 0;
        while (name_len < 8 && rec[name_len] != 0) {
            ++name_len;
        }
        PdbSection section;
        section.name.assign(reinterpret_cast<const char *>(rec), name_len);
        section.virtual_size = read_le32(rec + 8);
        section.rva = read_le32(rec + 12);
        sections.push_back(std::move(section));
    }
    return true;
}

static bool parse_public_symbols(const MsfFile &msf, uint32_t sym_stream,
                                 std::vector<PublicSymbol> &symbols,
                                 std::string &error)
{
    std::vector<uint8_t> bytes;
    if (!msf.ReadStream(sym_stream, bytes, error)) {
        return false;
    }
    symbols.clear();
    size_t pos = 0;
    while (pos < bytes.size()) {
        if (!range_inside(pos, 4, bytes.size())) {
            error = "PDB CodeView symbol stream ends with a truncated record.";
            return false;
        }
        const uint16_t record_length = read_le16(bytes.data() + pos);
        const uint16_t kind = read_le16(bytes.data() + pos + 2);
        if (record_length < 2 ||
            !range_inside(pos + 2, record_length, bytes.size())) {
            error = "PDB CodeView symbol record length is invalid.";
            return false;
        }
        const size_t record_end = pos + 2 + record_length;
        if (kind == kSPub32) {
            const size_t data_off = pos + 4;
            if (!range_inside(data_off, 11, record_end)) {
                error = "PDB S_PUB32 record is truncated.";
                return false;
            }
            PublicSymbol symbol;
            symbol.flags = read_le32(bytes.data() + data_off);
            symbol.offset = read_le32(bytes.data() + data_off + 4);
            symbol.segment = read_le16(bytes.data() + data_off + 8);
            const char *name = reinterpret_cast<const char *>(bytes.data() + data_off + 10);
            const size_t max_name = record_end - (data_off + 10);
            const void *nul = std::memchr(name, 0, max_name);
            if (nul == nullptr) {
                error = "PDB S_PUB32 symbol name is not terminated.";
                return false;
            }
            symbol.name.assign(name, static_cast<const char *>(nul) - name);
            if (symbols.size() >= kMaxPdbSymbols) {
                error = "PDB contains too many public symbols.";
                return false;
            }
            symbols.push_back(std::move(symbol));
        }
        pos = record_end;
    }
    return true;
}

static int find_xbe_section(const XemuXbeLabels::Database &database,
                            const PdbSection &pdb_section)
{
    // The PDB section-header stream describes the pre-XBE PE layout.  Xbox's
    // image conversion can move the section RVAs while preserving each
    // section's segment-relative offsets, so names are the stable bridge.
    for (size_t i = 0; i < database.sections.size(); ++i) {
        if (ascii_iequals(database.sections[i].name, pdb_section.name)) {
            return (int)i;
        }
    }
    return -1;
}

static bool resolve_section_layout(const std::vector<PdbSection> &pdb_sections,
                                   const XemuXbeLabels::Database &database,
                                   std::vector<int> &mapping,
                                   size_t &mapped_sections)
{
    mapping.assign(pdb_sections.size(), -1);
    mapped_sections = 0;
    bool text_seen = false;
    bool text_mapped = false;
    for (size_t i = 0; i < pdb_sections.size(); ++i) {
        const PdbSection &pdb = pdb_sections[i];
        if (ascii_iequals(pdb.name, ".reloc")) {
            continue;
        }
        if (ascii_iequals(pdb.name, ".text")) {
            text_seen = true;
        }
        const int xbe_index = find_xbe_section(database, pdb);
        if (xbe_index < 0) {
            continue;
        }
        const auto &xbe = database.sections[(size_t)xbe_index];
        const uint64_t xbe_span = std::max<uint32_t>(xbe.virtual_size,
                                                     xbe.raw_size);
        mapping[i] = xbe_index;
        ++mapped_sections;
        if ((uint64_t)pdb.virtual_size > xbe_span) {
            return false;
        }
        if (ascii_iequals(pdb.name, ".text")) {
            text_mapped = true;
        }
    }
    return mapped_sections != 0 && (!text_seen || text_mapped);
}

} // namespace

std::string FormatGuid(const std::array<uint8_t, 16> &guid)
{
    char text[64];
    std::snprintf(text, sizeof(text),
                  "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                  read_le32(guid.data()), read_le16(guid.data() + 4),
                  read_le16(guid.data() + 6), guid[8], guid[9], guid[10], guid[11],
                  guid[12], guid[13], guid[14], guid[15]);
    return text;
}

bool ExtractXbeIdentity(const std::vector<uint8_t> &xbe_file,
                        Identity &identity)
{
    identity = {};
    if (xbe_file.size() < 28) {
        return false;
    }
    for (size_t i = 0; i + 24 < xbe_file.size(); ++i) {
        if (std::memcmp(xbe_file.data() + i, "RSDS", 4) != 0) {
            continue;
        }
        const size_t path_start = i + 24;
        const size_t limit = std::min(xbe_file.size(), path_start + 1024);
        size_t end = path_start;
        while (end < limit && xbe_file[end] != 0) {
            const unsigned char ch = xbe_file[end];
            if (ch < 0x20 || ch > 0x7E) {
                break;
            }
            ++end;
        }
        if (end == path_start || end >= limit || xbe_file[end] != 0) {
            continue;
        }
        std::string path(reinterpret_cast<const char *>(xbe_file.data() + path_start),
                         end - path_start);
        std::string lower = path;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) { return (char)std::tolower(ch); });
        if (lower.size() < 4 || lower.rfind(".pdb") != lower.size() - 4) {
            continue;
        }
        identity.valid = true;
        std::copy_n(xbe_file.data() + i + 4, identity.guid.size(),
                    identity.guid.begin());
        identity.age = read_le32(xbe_file.data() + i + 20);
        identity.path = std::move(path);
        return true;
    }
    return false;
}

bool ParseAndResolve(const std::vector<uint8_t> &pdb_file,
                     const std::vector<uint8_t> &xbe_file,
                     const XemuXbeLabels::Database &database,
                     std::vector<XemuXbeLabels::Label> &labels,
                     Status &status, std::string &error)
{
    labels.clear();
    status = {};
    error.clear();
    if (database.sections.empty() || database.image_base == 0) {
        error = "Current XBE has no parsed section table.";
        status.message = error;
        return false;
    }

    Identity xbe_identity;
    status.xbe_identity_found = ExtractXbeIdentity(xbe_file, xbe_identity);
    if (status.xbe_identity_found) {
        status.xbe_age = xbe_identity.age;
        status.xbe_guid = FormatGuid(xbe_identity.guid);
        status.xbe_pdb_path = xbe_identity.path;
    }

    MsfFile msf;
    if (!msf.Open(pdb_file, error)) {
        status.message = error;
        return false;
    }
    Identity pdb_identity;
    if (!parse_pdb_header(msf, pdb_identity, error)) {
        status.message = error;
        return false;
    }
    status.parsed = true;
    status.pdb_age = pdb_identity.age;
    status.pdb_guid = FormatGuid(pdb_identity.guid);

    uint32_t sym_stream = 0;
    uint32_t dbi_age = 0;
    std::vector<PdbSection> pdb_sections;
    if (!parse_dbi(msf, sym_stream, status.machine, dbi_age,
                   pdb_sections, error)) {
        status.message = error;
        return false;
    }
    if (dbi_age != pdb_identity.age) {
        error = "PDB Info and DBI stream Ages disagree; file is inconsistent.";
        status.message = error;
        return false;
    }

    std::vector<PublicSymbol> symbols;
    if (!parse_public_symbols(msf, sym_stream, symbols, error)) {
        status.message = error;
        return false;
    }
    status.public_symbols = symbols.size();
    for (const auto &symbol : symbols) {
        if ((symbol.flags & 0x3u) != 0) {
            ++status.function_symbols;
        } else {
            ++status.data_symbols;
        }
    }

    std::vector<int> section_mapping;
    status.layout_match = resolve_section_layout(
        pdb_sections, database, section_mapping, status.mapped_sections);

    if (!status.xbe_identity_found) {
        error = "Current XBE has no RSDS PDB identity; PDB symbols cannot be validated safely.";
        status.message = error;
        return false;
    }
    status.guid_match = pdb_identity.guid == xbe_identity.guid;
    status.age_match = pdb_identity.age == xbe_identity.age;
    if (!status.guid_match) {
        error = "PDB GUID does not match the current XBE. No PDB labels were applied.";
        status.message = error;
        return false;
    }
    if (!status.age_match) {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "PDB GUID matches the current XBE, but Age differs (PDB %u / XBE %u). Same symbol lineage, different link; no labels were applied.",
                      status.pdb_age, status.xbe_age);
        error = msg;
        status.message = error;
        return false;
    }
    if (!status.layout_match) {
        error = "PDB section layout does not fit the current XBE. No labels were applied.";
        status.message = error;
        return false;
    }

    labels.reserve(symbols.size());
    for (const auto &symbol : symbols) {
        if (symbol.segment == 0 || symbol.segment > section_mapping.size()) {
            ++status.unresolved_symbols;
            continue;
        }
        const int xbe_index = section_mapping[(size_t)symbol.segment - 1];
        if (xbe_index < 0) {
            ++status.unresolved_symbols;
            continue;
        }
        const auto &section = database.sections[(size_t)xbe_index];
        const uint64_t span = std::max<uint32_t>(section.virtual_size,
                                                 section.raw_size);
        if ((uint64_t)symbol.offset >= span) {
            ++status.unresolved_symbols;
            continue;
        }
        const std::string name = display_symbol(symbol.name);
        if (name.empty()) {
            ++status.skipped_symbols;
            continue;
        }
        const uint64_t va = (uint64_t)section.virtual_address + symbol.offset;
        if (va > std::numeric_limits<uint32_t>::max()) {
            ++status.unresolved_symbols;
            continue;
        }

        XemuXbeLabels::Label label;
        label.virtual_address = (uint32_t)va;
        label.type = (symbol.flags & 0x3u) != 0
                         ? XemuXbeLabels::Type::Function
                         : XemuXbeLabels::Type::Symbol;
        label.name = name;
        label.source = XemuXbeLabels::Source::Pdb;
        label.confidence = XemuXbeLabels::Confidence::Exact;
        label.section_name = section.name;
        label.section_offset = symbol.offset;
        label.has_section_location = true;
        labels.push_back(std::move(label));
    }

    XemuLabelSymbolUtils::sort_and_dedupe_labels(labels);

    status.resolved_labels = labels.size();
    char msg[320];
    std::snprintf(msg, sizeof(msg),
                  "PDB exact match: GUID + Age + section layout validated; %zu label(s) resolved from %zu public symbol(s).",
                  status.resolved_labels, status.public_symbols);
    status.message = msg;
    return true;
}

} // namespace XemuPdbLabels
