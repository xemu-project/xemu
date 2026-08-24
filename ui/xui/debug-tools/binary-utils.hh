//
// xemu RAW Cheat Engine - shared binary parsing helpers
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef XEMU_UI_XUI_DEBUG_TOOLS_BINARY_UTILS_HH
#define XEMU_UI_XUI_DEBUG_TOOLS_BINARY_UTILS_HH

#include <cstdint>

namespace XemuDebugBinaryUtils {

inline uint16_t read_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

inline uint32_t read_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

inline bool range_inside(uint64_t offset, uint64_t size, uint64_t limit)
{
    return offset <= limit && size <= limit - offset;
}

} // namespace XemuDebugBinaryUtils

#endif // XEMU_UI_XUI_DEBUG_TOOLS_BINARY_UTILS_HH
