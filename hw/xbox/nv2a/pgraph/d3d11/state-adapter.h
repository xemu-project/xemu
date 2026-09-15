// xemu NV2A to D3D11 PGRAPH draw-state adapter
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

#ifndef XEMU_NV2A_PGRAPH_D3D11_STATE_ADAPTER_H
#define XEMU_NV2A_PGRAPH_D3D11_STATE_ADAPTER_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <cstdint>

extern "C" {
#include "../../nv2a_regs.h"
#include "../pgraph.h"
}

namespace xemu {

enum class D3D11PgraphStateStatus : uint8_t {
    Ready,
    Invalid,
    Unsupported,
};

/* D3D11 has no ownership of these dimensions.  They describe the view that
 * the caller is about to bind and are intentionally supplied per call. */
struct D3D11PgraphTargetDimensions {
    uint32_t width = 0;
    uint32_t height = 0;
};

enum class D3D11ShaderDepthConvention : uint8_t {
    ZeroToOne,
    MinusOneToOne,
};

/* NV2A surface clips use the source framebuffer's origin.  D3D11 RECTs use
 * a top-left origin, so the bottom-left form is converted explicitly. */
enum class D3D11PgraphClipOrigin : uint8_t {
    TopLeft,
    BottomLeft,
};

/* Capabilities are explicit so that this adapter does not depend on a
 * renderer, device, or the mutable PGRAPH scale cache. */
struct D3D11PgraphStateCapabilities {
    uint32_t surface_scale_factor = 1;
    uint32_t anti_aliasing = NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1;
    D3D11ShaderDepthConvention shader_depth_convention =
        D3D11ShaderDepthConvention::ZeroToOne;
    D3D11PgraphClipOrigin clip_origin = D3D11PgraphClipOrigin::BottomLeft;
};

/* This is a complete, native-state description.  It owns no D3D11 object;
 * the caller can pass its members directly to Create*State and the context. */
struct D3D11PgraphDrawState {
    bool has_viewport = false;
    D3D11_VIEWPORT viewport = {};
    bool has_scissor = false;
    D3D11_RECT scissor = {};

    D3D11_RASTERIZER_DESC rasterizer = {};
    D3D11_DEPTH_STENCIL_DESC depth_stencil = {};
    D3D11_BLEND_DESC blend = {};
    FLOAT blend_factor[4] = {};
    UINT stencil_ref = 0;
    UINT sample_mask = D3D11_DEFAULT_SAMPLE_MASK;
    D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
};

/* Build all state required for the supported PGRAPH triangle-list subset.
 * On failure, output is left byte-for-byte unchanged. */
D3D11PgraphStateStatus
d3d11_build_draw_state(const PGRAPHState *pg,
                       const D3D11PgraphTargetDimensions *target,
                       const D3D11PgraphStateCapabilities *capabilities,
                       D3D11PgraphDrawState *output);

/* Short aliases make the adapter convenient to use beside clear-adapter.h. */
using D3D11DrawStateStatus = D3D11PgraphStateStatus;
using D3D11DrawTargetDimensions = D3D11PgraphTargetDimensions;
using D3D11DrawStateCapabilities = D3D11PgraphStateCapabilities;
using D3D11DrawState = D3D11PgraphDrawState;

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_STATE_ADAPTER_H
