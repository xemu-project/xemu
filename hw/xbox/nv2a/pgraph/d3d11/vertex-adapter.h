// xemu NV2A to D3D11 CPU-owned vertex/index adapter
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef XEMU_NV2A_PGRAPH_D3D11_VERTEX_ADAPTER_H
#define XEMU_NV2A_PGRAPH_D3D11_VERTEX_ADAPTER_H

#include <cstdint>
#include <vector>

#include "d3d11_bridge.h"

struct NV2AState;
struct PGRAPHState;

namespace xemu {

enum class D3D11VertexAdapterStatus : uint8_t {
    Ready,
    Invalid,
    Unsupported,
};

/* Legacy position/color data used by the small D3D11 draw primitive.  The
 * vectors are owned by the plan, so no pointer into PGRAPH or VRAM escapes
 * this adapter. */
struct D3D11CanonicalVertex {
    float position[3] = {};
    float color[4] = {};
};

/* Canonical float4 values for all 16 NV2A VSH input registers.  Supported
 * array formats are F, UB_D3D, UB_OGL, S1, S32K, and CMP; inactive attributes
 * retain their inline_value defaults. */
struct D3D11CanonicalVshInputs {
    float values[D3D11_BRIDGE_VERTEX_ATTRIBUTES][4] = {};
};

struct D3D11VertexPlan {
    std::vector<D3D11CanonicalVertex> vertices;
    std::vector<D3D11CanonicalVshInputs> vsh_inputs;
    std::vector<uint32_t> indices;
};

/* Build a dense, triangle-list vertex/index plan from current PGRAPH draw
 * inputs.  The legacy draw fields remain position/color, while vsh_inputs
 * retains all 16 canonical float4 attributes for a programmable VSH fallback.
 * vram_size is the VRAM window size used by DMA objects. Inline-buffer values
 * marked populated are per-vertex float4 values; unpopulated attributes are
 * uniform inline_value values. On failure, output is atomically cleared and
 * never contains a partially built plan. */
D3D11VertexAdapterStatus d3d11_build_vertex_plan(NV2AState *d,
                                                 const PGRAPHState *pg,
                                                 uint64_t vram_size,
                                                 D3D11VertexPlan *output);

using D3D11VertexPlanStatus = D3D11VertexAdapterStatus;

} // namespace xemu

#endif // XEMU_NV2A_PGRAPH_D3D11_VERTEX_ADAPTER_H
