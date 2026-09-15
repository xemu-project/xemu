//
// xemu NV2A D3D11 clear primitive
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

#ifndef XEMU_NV2A_PGRAPH_D3D11_CLEAR_H
#define XEMU_NV2A_PGRAPH_D3D11_CLEAR_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <d3d11_1.h>

#include <stdint.h>

#include <wrl/client.h>

#include "../clear.h"

namespace xemu {

/* Keep the D3D11 primitive's public spelling while sharing the exact
 * renderer-neutral rectangle contract with all backends. */
using D3D11ClearRect = PGRAPHClearRect;

enum class D3D11ClearSurfaceKind {
    Color,
    DepthStencil,
};

/* The fixed formats are the integer encodings used by the NV2A clear value.
 * The float variants are named explicitly so an adapter cannot accidentally
 * reinterpret a floating-point NV2A zeta value as fixed point. */
enum class D3D11ClearSurfaceFormat {
    ColorRgba8Unorm,
    ColorBgra8Unorm,
    Z16Fixed,
    Z24S8Fixed,
    Z16Float,
    Z24S8Float,
};

enum D3D11ClearColorChannel : uint8_t {
    D3D11_CLEAR_RED = 1u << 0,
    D3D11_CLEAR_GREEN = 1u << 1,
    D3D11_CLEAR_BLUE = 1u << 2,
    D3D11_CLEAR_ALPHA = 1u << 3,
    D3D11_CLEAR_ALL_COLOR_CHANNELS = D3D11_CLEAR_RED | D3D11_CLEAR_GREEN |
                                     D3D11_CLEAR_BLUE | D3D11_CLEAR_ALPHA,
};

enum class D3D11ClearStatus {
    Cleared,
    Unsupported,
    Invalid,
    DeviceError,
};

struct D3D11ClearResult {
    D3D11ClearStatus status = D3D11ClearStatus::Invalid;
    HRESULT hresult = E_INVALIDARG;

    bool succeeded() const
    {
        return status == D3D11ClearStatus::Cleared;
    }
};

/**
 * Normalized input for one NV2A clear target.
 *
 * rect is inclusive, matching NV097_CLEAR_SURFACE.  The right and bottom
 * edges are clipped to the target dimensions before deciding whether this is
 * a full-surface clear; a rectangle with no overlap is invalid.  fixed_depth
 * is in the native integer range of the selected fixed format (0..0xffff for
 * Z16 and 0..0xffffff for Z24S8).
 * color contains normalized [0, 1] RGBA values.  The caller must identify
 * swizzled, scaled, and multisampled surfaces; this primitive rejects those
 * cases rather than silently applying a different clear operation.
 */
struct D3D11ClearParams {
    D3D11ClearSurfaceKind surface_kind = D3D11ClearSurfaceKind::Color;
    D3D11ClearSurfaceFormat surface_format =
        D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    D3D11ClearRect rect = {};
    uint32_t surface_width = 0;
    uint32_t surface_height = 0;

    uint8_t color_mask = D3D11_CLEAR_ALL_COLOR_CHANNELS;
    float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    uint32_t fixed_depth = 0;
    uint8_t stencil = 0;
    bool clear_depth = true;
    bool clear_stencil = false;

    bool swizzled = false;
    bool scaled = false;
    uint32_t sample_count = 1;
};

/**
 * Executes the normalized direct-D3D11 subset of NV2A CLEAR_SURFACE; it is
 * not a complete implementation of all NV2A color formats or clear modes.
 * This class deliberately has no renderer registration or PGRAPH
 * dependencies; a later adapter owns NV2A register normalization.
 *
 * Calls using the retained immediate context must be externally serialized
 * with all other users of that context.  The D3D11 clear methods are void, so
 * Clear() checks GetDeviceRemovedReason() after submission.  That catches
 * synchronous device loss, but cannot report an asynchronous GPU fault that
 * occurs after the check.
 */
class D3D11ClearPrimitive final {
public:
    D3D11ClearPrimitive(ID3D11Device *device, ID3D11DeviceContext *context);
    ~D3D11ClearPrimitive() = default;

    D3D11ClearPrimitive(const D3D11ClearPrimitive &) = delete;
    D3D11ClearPrimitive &operator=(const D3D11ClearPrimitive &) = delete;

    D3D11ClearResult Clear(const D3D11ClearParams &params,
                           ID3D11RenderTargetView *render_target,
                           ID3D11DepthStencilView *depth_stencil) const;

    bool has_context1() const
    {
        return m_context1 != nullptr;
    }

private:
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_context1;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_CLEAR_H
