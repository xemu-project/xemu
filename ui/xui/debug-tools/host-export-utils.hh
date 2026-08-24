//
// xemu Debug Tools - shared host export naming helpers
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

namespace XemuHostExportUtils {

inline bool IsReservedWindowsDeviceName(const std::string &name)
{
    std::string base = name;
    const size_t dot = base.find('.');
    if (dot != std::string::npos) {
        base.resize(dot);
    }
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL") {
        return true;
    }
    return base.size() == 4 &&
           (base.rfind("COM", 0) == 0 || base.rfind("LPT", 0) == 0) &&
           base[3] >= '1' && base[3] <= '9';
}

inline std::string HostSafeName(const std::string &name)
{
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            out.push_back('_');
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    if (!out.empty() && (out.back() == ' ' || out.back() == '.')) {
        out.back() = '_';
    }
    if (out.empty() || out == "." || out == "..") {
        out = "_";
    }
    if (IsReservedWindowsDeviceName(out)) {
        out.insert(out.begin(), '_');
    }
    return out;
}

inline std::string NumberedCandidate(const std::string &requested,
                                     unsigned index)
{
    namespace fs = std::filesystem;
    const fs::path path(requested);
    if (index == 0) {
        return path.string();
    }
    return (path.parent_path() /
            (path.stem().string() + " (" + std::to_string(index) + ")" +
             path.extension().string())).string();
}

} // namespace XemuHostExportUtils
