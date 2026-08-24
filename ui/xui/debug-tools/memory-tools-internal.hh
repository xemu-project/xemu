//
// xemu Memory Viewer / Search / x86 Debugger
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include "memory-tools.hh"
#include "guest-pause-guard.hh"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

#include <glib.h>

namespace xemu_memory_tools_internal {

constexpr size_t kRowBytes = 16;
constexpr size_t kPageSize = 0x1000;
constexpr uint64_t kMaxScanBytes = 256ull * 1024ull * 1024ull;
constexpr size_t kMaxDisplayedResults = 100000;
constexpr size_t kMaxSearchResults = 5000000;

static uint32_t load_le(const uint8_t *p, size_t size)
{
    uint32_t v = 0;
    for (size_t i = 0; i < size; ++i) {
        v |= (uint32_t)p[i] << (i * 8);
    }
    return v;
}

static void store_le(uint8_t *p, size_t size, uint32_t value)
{
    for (size_t i = 0; i < size; ++i) {
        p[i] = (uint8_t)((value >> (i * 8)) & 0xFFu);
    }
}

static float raw_to_float(uint32_t raw)
{
    float value;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

static unsigned char ascii_lower(unsigned char ch)
{
    return ch >= 'A' && ch <= 'Z' ? (unsigned char)(ch + ('a' - 'A')) : ch;
}

static void format_hex_byte(uint8_t value, char out[3])
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    out[0] = kHex[(value >> 4) & 0x0Fu];
    out[1] = kHex[value & 0x0Fu];
    out[2] = '\0';
}

static void format_hex_u32(uint32_t value, char out[9])
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    for (unsigned i = 0; i < 8; ++i) {
        const unsigned shift = (7u - i) * 4u;
        out[i] = kHex[(value >> shift) & 0x0Fu];
    }
    out[8] = '\0';
}

static constexpr const char *kMemoryColumnLabels[16] = {
    "0", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "A", "B", "C", "D", "E", "F",
};

static bool ascii_equal_case_insensitive(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (ascii_lower((unsigned char)*a) != ascii_lower((unsigned char)*b)) {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

static bool ascii_contains_case_insensitive(const std::string &text,
                                            const char *needle)
{
    if (needle == nullptr || *needle == '\0') {
        return true;
    }
    const size_t needle_len = std::strlen(needle);
    if (needle_len > text.size()) {
        return false;
    }
    for (size_t i = 0; i + needle_len <= text.size(); ++i) {
        size_t j = 0;
        while (j < needle_len &&
               ascii_lower((unsigned char)text[i + j]) ==
                   ascii_lower((unsigned char)needle[j])) {
            ++j;
        }
        if (j == needle_len) {
            return true;
        }
    }
    return false;
}

static bool ascii_starts_with_case_insensitive(const char *text,
                                                const char *prefix)
{
    while (*prefix != '\0') {
        if (*text == '\0' ||
            ascii_lower((unsigned char)*text) !=
                ascii_lower((unsigned char)*prefix)) {
            return false;
        }
        ++text;
        ++prefix;
    }
    return true;
}

static void format_disassembly_bytes(char *dst, size_t dst_size,
                                     const uint8_t *bytes, size_t size)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    if (dst_size == 0) {
        return;
    }

    const size_t count = std::min(size, (dst_size - 1) / 3);
    size_t out = 0;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t value = bytes[i];
        dst[out++] = kHex[value >> 4];
        dst[out++] = kHex[value & 0x0Fu];
        dst[out++] = ' ';
    }
    dst[out] = '\0';
}

static size_t find_disassembly_row(const XemuCheatDisasmRow *rows,
                                   size_t row_count, uint32_t address)
{
    if (rows == nullptr || row_count == 0) {
        return (size_t)-1;
    }

    const XemuCheatDisasmRow *begin = rows;
    const XemuCheatDisasmRow *end = rows + row_count;
    const XemuCheatDisasmRow *it = std::lower_bound(
        begin, end, address,
        [](const XemuCheatDisasmRow &row, uint32_t value) {
            return row.virtual_address < value;
        });

    if (it != end && it->virtual_address == address) {
        return (size_t)(it - begin);
    }
    if (it == begin) {
        return (size_t)-1;
    }

    --it;
    const uint64_t row_end =
        (uint64_t)it->virtual_address + std::max<uint8_t>(it->size, 1);
    return (uint64_t)address < row_end ? (size_t)(it - begin) : (size_t)-1;
}

static std::string make_dump_timestamp()
{
    GDateTime *now = g_date_time_new_now_local();
    if (!now) {
        return "00000000-000000";
    }
    gchar *stamp = g_date_time_format(now, "%Y%m%d-%H%M%S");
    std::string result = stamp ? stamp : "00000000-000000";
    g_free(stamp);
    g_date_time_unref(now);
    return result;
}

} // namespace xemu_memory_tools_internal
