//
// xemu RAW Cheat Engine - XBE-derived disassembler labels
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// Xbox kernel ordinal names are derived from XboxDev/nxdk's
// lib/xboxkrnl/xboxkrnl.exe.def (CC0-1.0).
//

#include "xbe-labels.hh"
#include "binary-utils.hh"
#include "label-symbol-utils.hh"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace XemuXbeLabels {
namespace {

using XemuDebugBinaryUtils::read_le32;
using XemuDebugBinaryUtils::range_inside;
using XemuLabelSymbolUtils::upper_ascii;

constexpr uint32_t kXbeMagic = 0x48454258u; // "XBEH"
constexpr uint32_t kRetailEntryKey = 0xA8FC57ABu;
constexpr uint32_t kDebugEntryKey = 0x94859D4Bu;
constexpr uint32_t kRetailThunkKey = 0x5B6D40B6u;
constexpr uint32_t kDebugThunkKey = 0xEFB1F152u;
constexpr size_t kHeaderMinimum = 0x178;
constexpr size_t kSectionHeaderSize = 56;
constexpr size_t kMaxSections = 512;
constexpr size_t kMaxLabels = 50000;
constexpr size_t kMaxKernelThunks = 4096;
constexpr size_t kMinStringLength = 6;
constexpr size_t kMaxStringLength = 256;

struct Section {
    std::string name;
    uint32_t flags = 0;
    uint32_t virtual_address = 0;
    uint32_t virtual_size = 0;
    uint32_t raw_address = 0;
    uint32_t raw_size = 0;
};

struct StringCandidate {
    uint32_t virtual_address = 0;
    std::string text;
    std::string label_stem;
    bool rtti = false;
};

struct KernelName {
    uint16_t ordinal;
    const char *name;
};

static constexpr KernelName kKernelNames[] = {
#include "xboxkrnl-ordinals.inc"
};

static bool read_u32(const std::vector<uint8_t> &file, size_t offset,
                     uint32_t &value)
{
    if (!range_inside(offset, 4, file.size())) {
        return false;
    }
    value = read_le32(file.data() + offset);
    return true;
}

static std::string read_header_string(const std::vector<uint8_t> &file,
                                      uint32_t base, uint32_t address)
{
    if (address < base) {
        return {};
    }
    const uint64_t offset = (uint64_t)address - base;
    if (offset >= file.size()) {
        return {};
    }
    const size_t max_len =
        (size_t)std::min<uint64_t>(64, file.size() - offset);
    size_t len = 0;
    while (len < max_len && file[(size_t)offset + len] != 0) {
        ++len;
    }
    if (len == 0 || len == max_len) {
        return {};
    }
    return std::string(reinterpret_cast<const char *>(file.data() + offset),
                       len);
}

static std::string sanitize_label(const std::string &text)
{
    std::string out;
    out.reserve(std::min<size_t>(text.size(), 64));
    bool last_underscore = false;
    for (unsigned char ch : text) {
        if (std::isalnum(ch)) {
            out.push_back((char)ch);
            last_underscore = false;
        } else if (!last_underscore && !out.empty()) {
            out.push_back('_');
            last_underscore = true;
        }
        if (out.size() >= 64) {
            break;
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    if (out.empty()) {
        out = "label";
    }
    return out;
}

static std::string rtti_stem(const std::string &text)
{
    std::string value = text;
    if (value.rfind(".?AV", 0) == 0 || value.rfind(".?AU", 0) == 0) {
        value.erase(0, 4);
    }
    if (value.size() >= 2 &&
        value.compare(value.size() - 2, 2, "@@") == 0) {
        value.resize(value.size() - 2);
    }
    return sanitize_label(value);
}

static bool looks_like_function_name(const std::string &text)
{
    if (text.find("::") != std::string::npos) {
        return true;
    }
    return text.size() >= 3 &&
           text.compare(text.size() - 2, 2, "()") == 0;
}

static bool useful_ascii_string(const uint8_t *p, size_t len)
{
    if (len < kMinStringLength || len > kMaxStringLength) {
        return false;
    }
    size_t alpha = 0;
    size_t useful = 0;
    for (size_t i = 0; i < len; ++i) {
        const unsigned char ch = p[i];
        if (std::isalpha(ch)) {
            ++alpha;
        }
        if (std::isalnum(ch) || ch == ' ' || ch == '_' || ch == ':' ||
            ch == '.' || ch == '/' || ch == '\\' || ch == '-' ||
            ch == '(' || ch == ')' || ch == '%' || ch == '$' ||
            ch == '[' || ch == ']') {
            ++useful;
        }
    }
    return alpha >= 3 && useful * 100 >= len * 70;
}

static const char *kernel_name(uint32_t ordinal)
{
    const auto it = std::lower_bound(
        std::begin(kKernelNames), std::end(kKernelNames), ordinal,
        [](const KernelName &entry, uint32_t value) {
            return entry.ordinal < value;
        });
    return it != std::end(kKernelNames) && it->ordinal == ordinal
               ? it->name
               : nullptr;
}

static bool decode_inside_image(uint32_t encoded, uint32_t retail_key,
                                uint32_t debug_key, uint32_t image_base,
                                uint32_t image_size, uint32_t &decoded)
{
    const uint32_t candidates[] = {
        encoded ^ retail_key,
        encoded ^ debug_key,
    };
    const uint64_t image_end = (uint64_t)image_base + image_size;
    for (uint32_t candidate : candidates) {
        if ((uint64_t)candidate >= image_base &&
            (uint64_t)candidate < image_end) {
            decoded = candidate;
            return true;
        }
    }
    return false;
}

static const Section *section_for_va(const std::vector<Section> &sections,
                                     uint32_t address)
{
    for (const Section &section : sections) {
        const uint64_t end = (uint64_t)section.virtual_address +
                             section.raw_size;
        if ((uint64_t)address >= section.virtual_address &&
            (uint64_t)address < end) {
            return &section;
        }
    }
    return nullptr;
}

static bool va_to_file_offset(const std::vector<Section> &sections,
                              uint32_t address, uint32_t &offset)
{
    const Section *section = section_for_va(sections, address);
    if (section == nullptr) {
        return false;
    }
    offset = section->raw_address + (address - section->virtual_address);
    return true;
}

static void add_label(Database &db, uint32_t address, Type type,
                      std::string name)
{
    if (name.empty() || db.labels.size() >= kMaxLabels) {
        return;
    }
    Label label;
    label.virtual_address = address;
    label.type = type;
    label.name = std::move(name);
    db.labels.push_back(std::move(label));
}

static uint32_t infer_function_start(const uint8_t *code, size_t code_size,
                                     uint32_t section_va, size_t xref_offset)
{
    const size_t lower = xref_offset > 0x200 ? xref_offset - 0x200 : 0;
    for (size_t pos = xref_offset; pos > lower; --pos) {
        const size_t i = pos - 1;
        if (code[i] == 0xC3 || code[i] == 0xC2) {
            break;
        }
        if (i + 3 <= code_size &&
            code[i] == 0x55 && code[i + 1] == 0x8B &&
            code[i + 2] == 0xEC) {
            return section_va + (uint32_t)i;
        }
    }
    return 0;
}

static int source_priority(Source source)
{
    switch (source) {
    case Source::Manual: return 0;
    case Source::Pdb: return 1;
    case Source::Map: return 2;
    case Source::Xdk: return 3;
    case Source::Xbe: return 4;
    }
    return 99;
}

static int confidence_priority(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Exact: return 0;
    case Confidence::Manual: return 1;
    case Confidence::High: return 2;
    case Confidence::Inferred: return 3;
    }
    return 99;
}

static int type_priority(Type type)
{
    switch (type) {
    case Type::Entry: return 0;
    case Type::Function: return 1;
    case Type::Symbol: return 2;
    case Type::Inferred: return 3;
    case Type::Xref: return 4;
    case Type::Kernel: return 5;
    case Type::Section: return 6;
    case Type::Rtti: return 7;
    case Type::String: return 8;
    }
    return 99;
}

static void attach_section_locations(Database &db)
{
    for (Label &label : db.labels) {
        label.has_section_location = false;
        label.section_name.clear();
        label.section_offset = 0;
        for (const SectionInfo &section : db.sections) {
            const uint64_t span = std::max<uint32_t>(section.virtual_size,
                                                     section.raw_size);
            const uint64_t end = (uint64_t)section.virtual_address + span;
            if ((uint64_t)label.virtual_address >= section.virtual_address &&
                (uint64_t)label.virtual_address < end) {
                label.section_name = section.name;
                label.section_offset = label.virtual_address -
                                       section.virtual_address;
                label.has_section_location = true;
                break;
            }
        }
    }
}

} // namespace

const char *TypeName(Type type)
{
    switch (type) {
    case Type::Entry: return "ENTRY";
    case Type::Section: return "SECTION";
    case Type::Kernel: return "KERNEL";
    case Type::String: return "STRING";
    case Type::Xref: return "XREF";
    case Type::Rtti: return "RTTI";
    case Type::Inferred: return "INFERRED";
    case Type::Function: return "FUNCTION";
    case Type::Symbol: return "SYMBOL";
    }
    return "UNKNOWN";
}

bool TypeFromName(const std::string &name, Type &type)
{
    const std::string value = upper_ascii(name);
    for (int i = (int)Type::Entry; i <= (int)Type::Symbol; ++i) {
        const Type candidate = (Type)i;
        if (value == TypeName(candidate)) {
            type = candidate;
            return true;
        }
    }
    return false;
}

const char *SourceName(Source source)
{
    switch (source) {
    case Source::Xbe: return "XBE";
    case Source::Xdk: return "XDK";
    case Source::Pdb: return "PDB";
    case Source::Map: return "MAP";
    case Source::Manual: return "MANUAL";
    }
    return "UNKNOWN";
}

bool SourceFromName(const std::string &name, Source &source)
{
    const std::string value = upper_ascii(name);
    for (int i = (int)Source::Xbe; i <= (int)Source::Manual; ++i) {
        const Source candidate = (Source)i;
        if (value == SourceName(candidate)) {
            source = candidate;
            return true;
        }
    }
    return false;
}

const char *ConfidenceName(Confidence confidence)
{
    switch (confidence) {
    case Confidence::Exact: return "EXACT";
    case Confidence::High: return "HIGH";
    case Confidence::Inferred: return "INFERRED";
    case Confidence::Manual: return "MANUAL";
    }
    return "UNKNOWN";
}

bool ConfidenceFromName(const std::string &name, Confidence &confidence)
{
    const std::string value = upper_ascii(name);
    for (int i = (int)Confidence::Exact; i <= (int)Confidence::Manual; ++i) {
        const Confidence candidate = (Confidence)i;
        if (value == ConfidenceName(candidate)) {
            confidence = candidate;
            return true;
        }
    }
    return false;
}

bool Build(const std::vector<uint8_t> &file, Database &db, std::string &error)
{
    db = {};
    error.clear();
    if (file.size() < kHeaderMinimum || read_le32(file.data()) != kXbeMagic) {
        error = "default.xbe does not contain a valid XBE header.";
        return false;
    }

    uint32_t base = 0;
    uint32_t image_size = 0;
    uint32_t section_count = 0;
    uint32_t section_headers_va = 0;
    uint32_t encoded_entry = 0;
    uint32_t tls_va = 0;
    uint32_t encoded_thunk = 0;
    uint32_t library_version_count = 0;
    uint32_t library_versions_va = 0;
    if (!read_u32(file, 0x104, base) ||
        !read_u32(file, 0x10C, image_size) ||
        !read_u32(file, 0x11C, section_count) ||
        !read_u32(file, 0x120, section_headers_va) ||
        !read_u32(file, 0x128, encoded_entry) ||
        !read_u32(file, 0x12C, tls_va) ||
        !read_u32(file, 0x158, encoded_thunk) ||
        !read_u32(file, 0x160, library_version_count) ||
        !read_u32(file, 0x164, library_versions_va)) {
        error = "default.xbe header is truncated.";
        return false;
    }
    if (base == 0 || image_size == 0 ||
        section_count == 0 || section_count > kMaxSections ||
        section_headers_va < base) {
        error = "default.xbe has invalid image/section metadata.";
        return false;
    }

    const uint64_t section_headers_offset =
        (uint64_t)section_headers_va - base;
    if (!range_inside(section_headers_offset,
                      (uint64_t)section_count * kSectionHeaderSize,
                      file.size())) {
        error = "default.xbe section headers are outside the file.";
        return false;
    }

    std::vector<Section> sections;
    sections.reserve(section_count);
    for (uint32_t i = 0; i < section_count; ++i) {
        const size_t off =
            (size_t)section_headers_offset + (size_t)i * kSectionHeaderSize;
        Section section;
        section.flags = read_le32(file.data() + off + 0);
        section.virtual_address = read_le32(file.data() + off + 4);
        section.virtual_size = read_le32(file.data() + off + 8);
        section.raw_address = read_le32(file.data() + off + 12);
        section.raw_size = read_le32(file.data() + off + 16);
        const uint32_t name_va = read_le32(file.data() + off + 20);
        section.name = read_header_string(file, base, name_va);
        if (section.name.empty()) {
            char fallback[24];
            std::snprintf(fallback, sizeof(fallback), "section_%u", i);
            section.name = fallback;
        }
        if (!range_inside(section.raw_address, section.raw_size, file.size())) {
            error = "default.xbe section '" + section.name +
                    "' points outside the file.";
            return false;
        }
        sections.push_back(section);
        db.sections.push_back({section.name, section.virtual_address,
                               section.virtual_size, section.raw_size,
                               section.raw_address, section.flags});
        add_label(db, section.virtual_address, Type::Section,
                  "section_" + sanitize_label(section.name));
    }
    db.image_base = base;
    db.image_size = image_size;

    uint32_t entry = 0;
    if (decode_inside_image(encoded_entry, kRetailEntryKey, kDebugEntryKey,
                            base, image_size, entry)) {
        add_label(db, entry, Type::Entry, "XBE_EntryPoint");
    }
    if ((uint64_t)tls_va >= base &&
        (uint64_t)tls_va < (uint64_t)base + image_size) {
        add_label(db, tls_va, Type::Section, "XBE_TLS");
    }
    if ((uint64_t)library_versions_va >= base &&
        (uint64_t)library_versions_va < (uint64_t)base + image_size) {
        add_label(db, library_versions_va, Type::Section,
                  "XBE_LibraryVersions");
    }
    if (library_version_count <= 256 && library_versions_va >= base) {
        const uint64_t library_offset = (uint64_t)library_versions_va - base;
        if (range_inside(library_offset,
                         (uint64_t)library_version_count * 16, file.size())) {
            for (uint32_t i = 0; i < library_version_count; ++i) {
                const uint8_t *entry = file.data() + library_offset + i * 16;
                char name[9] = {};
                std::memcpy(name, entry, 8);
                std::string library_name(name);
                while (!library_name.empty() &&
                       (library_name.back() == ' ' || library_name.back() == '\0')) {
                    library_name.pop_back();
                }
                if (library_name.empty()) {
                    continue;
                }
                LibraryVersion version;
                version.name = library_name;
                version.major = (uint16_t)entry[8] | ((uint16_t)entry[9] << 8);
                version.minor = (uint16_t)entry[10] | ((uint16_t)entry[11] << 8);
                version.build = (uint16_t)entry[12] | ((uint16_t)entry[13] << 8);
                version.qfe = (uint16_t)entry[14] | ((uint16_t)entry[15] << 8);
                db.libraries.push_back(std::move(version));
            }
        }
    }

    uint32_t thunk_va = 0;
    if (decode_inside_image(encoded_thunk, kRetailThunkKey, kDebugThunkKey,
                            base, image_size, thunk_va)) {
        add_label(db, thunk_va, Type::Kernel, "XBE_KernelThunkTable");
        uint32_t thunk_file_offset = 0;
        if (va_to_file_offset(sections, thunk_va, thunk_file_offset)) {
            for (size_t i = 0; i < kMaxKernelThunks; ++i) {
                const uint64_t off = (uint64_t)thunk_file_offset + i * 4;
                if (!range_inside(off, 4, file.size())) {
                    break;
                }
                const uint32_t value = read_le32(file.data() + off);
                if (value == 0 || (value & 0x80000000u) == 0) {
                    break;
                }
                const uint32_t ordinal = value & 0x7FFFFFFFu;
                const char *name = kernel_name(ordinal);
                char fallback[48];
                if (name == nullptr) {
                    std::snprintf(fallback, sizeof(fallback),
                                  "ordinal_%u", ordinal);
                    name = fallback;
                }
                add_label(db, thunk_va + (uint32_t)(i * 4), Type::Kernel,
                          std::string("kernel_") + name);
            }
        }
    }

    std::vector<StringCandidate> strings;
    strings.reserve(8192);
    for (const Section &section : sections) {
        // Main .text is overwhelmingly executable code. Other Xbox library
        // sections sometimes contain diagnostics next to code, so keep those.
        if (section.name == ".text" || section.raw_size == 0) {
            continue;
        }
        const uint8_t *bytes = file.data() + section.raw_address;
        size_t i = 0;
        while (i < section.raw_size) {
            if (bytes[i] < 0x20 || bytes[i] > 0x7E) {
                ++i;
                continue;
            }
            size_t end = i;
            while (end < section.raw_size && bytes[end] >= 0x20 &&
                   bytes[end] <= 0x7E && end - i < kMaxStringLength) {
                ++end;
            }
            const size_t len = end - i;
            if (useful_ascii_string(bytes + i, len)) {
                StringCandidate candidate;
                candidate.virtual_address =
                    section.virtual_address + (uint32_t)i;
                candidate.text.assign(
                    reinterpret_cast<const char *>(bytes + i), len);
                candidate.rtti =
                    candidate.text.rfind(".?AV", 0) == 0 ||
                    candidate.text.rfind(".?AU", 0) == 0;
                candidate.label_stem =
                    candidate.rtti ? rtti_stem(candidate.text)
                                   : sanitize_label(candidate.text);
                add_label(db, candidate.virtual_address,
                          candidate.rtti ? Type::Rtti : Type::String,
                          std::string(candidate.rtti ? "rtti_" : "str_") +
                              candidate.label_stem);
                strings.push_back(std::move(candidate));
            }
            i = end > i ? end : i + 1;
        }
    }

    std::unordered_map<uint32_t, size_t> string_by_va;
    string_by_va.reserve(strings.size() * 2 + 1);
    for (size_t i = 0; i < strings.size(); ++i) {
        string_by_va.emplace(strings[i].virtual_address, i);
    }

    std::unordered_set<uint64_t> seen_xrefs;
    std::unordered_set<uint64_t> seen_inferred;
    for (const Section &section : sections) {
        // Keep automatic code xrefs/inferred function names on the title's
        // primary .text section. Library/data sections can legally contain
        // byte patterns that resemble instructions and would create noisy
        // false xrefs without a full relocation/symbol stream.
        if (section.name != ".text" || section.raw_size < 5) {
            continue;
        }
        const uint8_t *code = file.data() + section.raw_address;
        for (size_t i = 0; i + 5 <= section.raw_size; ++i) {
            const uint8_t opcode = code[i];
            if (opcode != 0x68 && !(opcode >= 0xB8 && opcode <= 0xBF)) {
                continue;
            }
            const uint32_t target = read_le32(code + i + 1);
            const auto found = string_by_va.find(target);
            if (found == string_by_va.end()) {
                continue;
            }

            const StringCandidate &candidate = strings[found->second];
            const uint32_t xref_va = section.virtual_address + (uint32_t)i;
            const uint64_t xref_key = ((uint64_t)xref_va << 32) | target;
            if (seen_xrefs.insert(xref_key).second) {
                add_label(db, xref_va, Type::Xref,
                          "xref_" + candidate.label_stem);
            }

            if (looks_like_function_name(candidate.text)) {
                const uint32_t function_va =
                    infer_function_start(code, section.raw_size,
                                         section.virtual_address, i);
                const uint64_t inferred_key =
                    ((uint64_t)function_va << 32) | target;
                if (function_va != 0 &&
                    seen_inferred.insert(inferred_key).second) {
                    add_label(db, function_va, Type::Inferred,
                              "~" + candidate.label_stem);
                }
            }
        }
    }

    for (Label &label : db.labels) {
        label.source = Source::Xbe;
        label.confidence = label.type == Type::Inferred
                               ? Confidence::Inferred
                               : Confidence::Exact;
    }
    attach_section_locations(db);
    SortAndUnique(db);

    if (db.labels.empty()) {
        error = "default.xbe parsed but produced no useful labels.";
        return false;
    }
    return true;
}

bool ResolveSectionLocation(const Database &db,
                            const std::string &section_name,
                            uint32_t section_offset, uint32_t &virtual_address)
{
    for (const SectionInfo &section : db.sections) {
        if (section.name != section_name) {
            continue;
        }
        const uint64_t span = std::max<uint32_t>(section.virtual_size,
                                                 section.raw_size);
        if ((uint64_t)section_offset >= span) {
            return false;
        }
        const uint64_t resolved = (uint64_t)section.virtual_address +
                                  section_offset;
        if (resolved > 0xFFFFFFFFull) {
            return false;
        }
        virtual_address = (uint32_t)resolved;
        return true;
    }
    return false;
}

void SortAndUnique(Database &db)
{
    std::sort(db.labels.begin(), db.labels.end(),
              [](const Label &a, const Label &b) {
                  if (a.virtual_address != b.virtual_address) {
                      return a.virtual_address < b.virtual_address;
                  }
                  const int as = source_priority(a.source);
                  const int bs = source_priority(b.source);
                  if (as != bs) {
                      return as < bs;
                  }
                  const int ap = type_priority(a.type);
                  const int bp = type_priority(b.type);
                  if (ap != bp) {
                      return ap < bp;
                  }
                  if (a.type != b.type) {
                      return (int)a.type < (int)b.type;
                  }
                  if (a.name != b.name) {
                      return a.name < b.name;
                  }
                  return confidence_priority(a.confidence) <
                         confidence_priority(b.confidence);
              });
    db.labels.erase(
        std::unique(db.labels.begin(), db.labels.end(),
                    [](const Label &a, const Label &b) {
                        return a.virtual_address == b.virtual_address &&
                               a.type == b.type && a.source == b.source &&
                               a.name == b.name;
                    }),
        db.labels.end());
}

void Append(Database &db, const std::vector<Label> &labels)
{
    db.labels.insert(db.labels.end(), labels.begin(), labels.end());
}

void Merge(Database &db, const std::vector<Label> &labels)
{
    Append(db, labels);
    SortAndUnique(db);
}

const Label *PrimaryAt(const Database &db, uint32_t virtual_address)
{
    const auto it = std::lower_bound(
        db.labels.begin(), db.labels.end(), virtual_address,
        [](const Label &label, uint32_t address) {
            return label.virtual_address < address;
        });
    if (it == db.labels.end() || it->virtual_address != virtual_address) {
        return nullptr;
    }
    return &*it;
}

} // namespace XemuXbeLabels
