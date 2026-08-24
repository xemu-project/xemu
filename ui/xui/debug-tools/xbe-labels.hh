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
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace XemuXbeLabels {

enum class Type {
    Entry = 0,
    Section,
    Kernel,
    String,
    Xref,
    Rtti,
    Inferred,
    Function,
    Symbol,
};

// Where a label name originally came from.  A portable .xlabel file is only
// a transport; it preserves this provenance instead of changing it to PACK.
enum class Source {
    Xbe = 0,
    Xdk,
    Pdb,
    Map,
    Manual,
};

enum class Confidence {
    Exact = 0,
    High,
    Inferred,
    Manual,
};

struct SectionInfo {
    std::string name;
    uint32_t virtual_address = 0;
    uint32_t virtual_size = 0;
    uint32_t raw_size = 0;
    uint32_t raw_address = 0;
    uint32_t flags = 0;
};

struct LibraryVersion {
    std::string name;
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t build = 0;
    uint16_t qfe = 0;
};

struct Label {
    uint32_t virtual_address = 0;
    Type type = Type::String;
    std::string name;
    Source source = Source::Xbe;
    Confidence confidence = Confidence::Exact;

    // Stable XBE-relative location used by portable .xlabel packs. Physical
    // addresses are intentionally never persisted; they are resolved from the
    // running Xbox page tables when shown in the debugger.
    std::string section_name;
    uint32_t section_offset = 0;
    bool has_section_location = false;
};

struct Database {
    std::vector<Label> labels;
    std::vector<SectionInfo> sections;
    std::vector<LibraryVersion> libraries;
    uint32_t image_base = 0;
    uint32_t image_size = 0;
};

bool Build(const std::vector<uint8_t> &xbe_file, Database &database,
           std::string &error);
const Label *PrimaryAt(const Database &database, uint32_t virtual_address);
const char *TypeName(Type type);
bool TypeFromName(const std::string &name, Type &type);
const char *SourceName(Source source);
bool SourceFromName(const std::string &name, Source &source);
const char *ConfidenceName(Confidence confidence);
bool ConfidenceFromName(const std::string &name, Confidence &confidence);

// Resolve a section-relative location against the currently parsed XBE.
bool ResolveSectionLocation(const Database &database,
                            const std::string &section_name,
                            uint32_t section_offset,
                            uint32_t &virtual_address);

// Append labels without sorting so callers combining several sources can batch
// the canonical sort/unique once at the end. Merge() retains its old immediate
// sorted behavior for standalone callers.
void Append(Database &database, const std::vector<Label> &labels);
void Merge(Database &database, const std::vector<Label> &labels);
void SortAndUnique(Database &database);

} // namespace XemuXbeLabels
