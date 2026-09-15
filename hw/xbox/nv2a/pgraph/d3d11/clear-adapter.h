//
// xemu NV2A to D3D11 CLEAR_SURFACE plan adapter
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#ifndef XEMU_NV2A_PGRAPH_D3D11_CLEAR_ADAPTER_H
#define XEMU_NV2A_PGRAPH_D3D11_CLEAR_ADAPTER_H

#ifdef _WIN32

#include "d3d11_bridge.h"
#include "clear.h"

namespace xemu {

enum class D3D11ClearPlanStatus {
    NotRequested,
    Ready,
    Unsupported,
    Invalid,
};

/* Dimensions are borrowed from the renderer's currently bound views.  The
 * adapter does not retain the pointer or any native D3D11 object. */
struct D3D11ClearBindingDimensions {
    bool color_bound = false;
    uint32_t color_width = 0;
    uint32_t color_height = 0;
    bool zeta_bound = false;
    uint32_t zeta_width = 0;
    uint32_t zeta_height = 0;
};

/* Capabilities belong to the adapter call rather than PGRAPHState.  This
 * keeps the plan builder independent of renderer initialization order (the
 * GL/VK surface-scale cache is not authoritative for a D3D11 caller). */
struct D3D11ClearCapabilities {
    uint32_t surface_scale_factor = 1;
};

/* A CLEAR_SURFACE command may address both attachments.  Each member is a
 * complete, independently executable D3D11ClearParams value; callers can
 * submit the color and depth/stencil members to the primitive in order. */
struct D3D11ClearPlan {
    bool clear_color = false;
    bool clear_depth_stencil = false;
    D3D11ClearParams color = {};
    D3D11ClearParams depth_stencil = {};
};

/* Build a plan without changing PGRAPHState, dirty bits, or VRAM.  On every
 * status other than Ready, output is left byte-for-byte untouched. */
D3D11ClearPlanStatus
d3d11_build_clear_plan(const PGRAPHState *pg, uint32_t parameter,
                       const D3D11ClearBindingDimensions *bindings,
                       const D3D11ClearCapabilities *capabilities,
                       D3D11ClearPlan *output);

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_CLEAR_ADAPTER_H
