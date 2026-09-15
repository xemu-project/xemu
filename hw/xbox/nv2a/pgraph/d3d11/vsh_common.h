//
// xemu NV2A D3D11 shared programmable VSH state
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

/* Shared limits and postprocess state for the CPU-authoritative NV2A VSH. */

#ifndef XEMU_NV2A_PGRAPH_D3D11_VSH_COMMON_H
#define XEMU_NV2A_PGRAPH_D3D11_VSH_COMMON_H

#include <cstdint>

namespace xemu {
namespace d3d11_vsh {

constexpr uint32_t kMaxInstructions = 136;
constexpr uint32_t kMaxTemps = 12;
constexpr uint32_t kMaxOutputs = 13;
constexpr uint32_t kMaxConstants = 192;

struct VshPostprocess {
    float surface_size[4];
    float clip_range[4];
};

} // namespace d3d11_vsh
} // namespace xemu

#endif // XEMU_NV2A_PGRAPH_D3D11_VSH_COMMON_H
