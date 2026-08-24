//
// xemu RAW Cheat Engine - read-only XDVDFS disc browser helpers
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
#include <functional>
#include <string>
#include <vector>

namespace XemuXdvdfs {

struct Entry {
    std::string name;
    uint8_t attributes = 0;
    uint32_t size = 0;
    uint32_t start_sector = 0;
    uint64_t disc_offset = 0;
    std::vector<Entry> children;

    bool IsDirectory() const { return (attributes & 0x10u) != 0; }
};

struct Disc {
    bool valid = false;
    uint64_t filesystem_base = 0;
    uint32_t root_sector = 0;
    uint32_t root_size = 0;
    std::vector<Entry> root_entries;
    size_t file_count = 0;
    size_t directory_count = 0;
};

using Reader = std::function<bool(uint64_t offset, void *buffer, size_t size)>;

bool Parse(const Reader &reader, uint64_t image_size, Disc &disc,
           std::string &error);
const Entry *FindRootFile(const Disc &disc, const char *name);
const Entry *FindEntry(const Disc &disc, const std::vector<std::string> &path);

} // namespace XemuXdvdfs
