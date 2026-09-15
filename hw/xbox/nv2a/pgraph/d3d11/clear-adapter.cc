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

#include "clear-adapter.h"

#ifdef _WIN32

#include "../clear.h"
#include "../../nv2a_regs.h"

namespace xemu {

namespace {

constexpr uint32_t kColorBits = NV097_CLEAR_SURFACE_COLOR;
constexpr uint32_t kZetaBits =
    NV097_CLEAR_SURFACE_Z | NV097_CLEAR_SURFACE_STENCIL;
constexpr uint32_t kKnownBits = kColorBits | kZetaBits;
constexpr uint8_t kAllColorChannels = D3D11_CLEAR_ALL_COLOR_CHANNELS;

D3D11ClearPlanStatus Unsupported()
{
    return D3D11ClearPlanStatus::Unsupported;
}

bool HasColor(uint32_t parameter)
{
    return (parameter & kColorBits) != 0;
}

bool HasZeta(uint32_t parameter)
{
    return (parameter & kZetaBits) != 0;
}

bool ValidDimensions(uint32_t width, uint32_t height)
{
    return width != 0 && height != 0;
}

bool ValidPitch(const Surface &surface, uint32_t width, unsigned int bytes)
{
    const uint64_t minimum = static_cast<uint64_t>(width) * bytes;
    return surface.pitch >= minimum && (surface.pitch % bytes) == 0;
}

bool DimensionsMatch(const D3D11ClearBindingDimensions &bindings)
{
    return !bindings.color_bound || !bindings.zeta_bound ||
           (bindings.color_width == bindings.zeta_width &&
            bindings.color_height == bindings.zeta_height);
}

uint32_t ReadRegister(const PGRAPHState *pg, unsigned int reg)
{
    assert(reg % 4 == 0);
    return pg->regs_[reg];
}

bool BuildRect(const PGRAPHState *pg, uint32_t width, uint32_t height,
               PGRAPHClearRect *rect)
{
    const uint32_t clearrectx = ReadRegister(pg, NV_PGRAPH_CLEARRECTX);
    const uint32_t clearrecty = ReadRegister(pg, NV_PGRAPH_CLEARRECTY);
    const PGRAPHClearRect requested = {
        GET_MASK(clearrectx, NV_PGRAPH_CLEARRECTX_XMIN),
        GET_MASK(clearrecty, NV_PGRAPH_CLEARRECTY_YMIN),
        GET_MASK(clearrectx, NV_PGRAPH_CLEARRECTX_XMAX),
        GET_MASK(clearrecty, NV_PGRAPH_CLEARRECTY_YMAX),
    };
    return pgraph_clear_rect_clamp(&requested, width, height, rect);
}

} // namespace

D3D11ClearPlanStatus
d3d11_build_clear_plan(const PGRAPHState *pg, uint32_t parameter,
                       const D3D11ClearBindingDimensions *bindings,
                       const D3D11ClearCapabilities *capabilities,
                       D3D11ClearPlan *output)
{
    if (!output) {
        return D3D11ClearPlanStatus::Invalid;
    }
    if ((parameter & ~kKnownBits) != 0) {
        return Unsupported();
    }
    if ((parameter & kKnownBits) == 0) {
        return D3D11ClearPlanStatus::NotRequested;
    }
    if (!pg || !bindings || !capabilities) {
        return D3D11ClearPlanStatus::Invalid;
    }

    const bool clear_color = HasColor(parameter);
    const bool clear_zeta = HasZeta(parameter);
    if (HasColor(parameter) && (parameter & kColorBits) != kColorBits) {
        return Unsupported();
    }
    if (pg->surface_type != NV097_SET_SURFACE_FORMAT_TYPE_PITCH ||
        capabilities->surface_scale_factor != 1 ||
        pg->surface_shape.anti_aliasing !=
            NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1) {
        return Unsupported();
    }
    if (!DimensionsMatch(*bindings)) {
        return D3D11ClearPlanStatus::Invalid;
    }

    if (clear_color &&
        pgraph_clear_classify_color_storage(pg->surface_shape.color_format) !=
            PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8) {
        return Unsupported();
    }
    if (clear_zeta &&
        pg->surface_shape.zeta_format != NV097_SET_SURFACE_FORMAT_ZETA_Z16 &&
        pg->surface_shape.zeta_format != NV097_SET_SURFACE_FORMAT_ZETA_Z24S8) {
        return Unsupported();
    }
    if (clear_zeta &&
        pg->surface_shape.zeta_format == NV097_SET_SURFACE_FORMAT_ZETA_Z16 &&
        (parameter & NV097_CLEAR_SURFACE_STENCIL)) {
        return Unsupported();
    }

    uint32_t fixed_depth = 0;
    uint8_t stencil = 0;
    const unsigned int z_format =
        GET_MASK(ReadRegister(pg, NV_PGRAPH_SETUPRASTER),
                 NV_PGRAPH_SETUPRASTER_Z_FORMAT);
    if (clear_zeta && !pgraph_clear_extract_fixed_depth_stencil(
                          pg->surface_shape.zeta_format, z_format,
                          ReadRegister(pg, NV_PGRAPH_ZSTENCILCLEARVALUE),
                          &fixed_depth, &stencil)) {
        return Unsupported();
    }

    const uint32_t width =
        clear_color ? bindings->color_width : bindings->zeta_width;
    const uint32_t height =
        clear_color ? bindings->color_height : bindings->zeta_height;
    if ((clear_color && !bindings->color_bound) ||
        (clear_zeta && !bindings->zeta_bound) ||
        !ValidDimensions(width, height)) {
        return D3D11ClearPlanStatus::Invalid;
    }
    if (clear_color && !ValidPitch(pg->surface_color, width, 4)) {
        return D3D11ClearPlanStatus::Invalid;
    }
    const unsigned int zeta_bytes =
        pg->surface_shape.zeta_format == NV097_SET_SURFACE_FORMAT_ZETA_Z16 ? 2 :
                                                                             4;
    if (clear_zeta && !ValidPitch(pg->surface_zeta, width, zeta_bytes)) {
        return D3D11ClearPlanStatus::Invalid;
    }

    PGRAPHClearRect rect = {};
    if (!BuildRect(pg, width, height, &rect)) {
        return D3D11ClearPlanStatus::Invalid;
    }
    if (clear_zeta && !pgraph_clear_rect_is_full(&rect, width, height)) {
        return Unsupported();
    }

    D3D11ClearPlan plan = {};
    plan.clear_color = clear_color;
    plan.clear_depth_stencil = clear_zeta;

    if (clear_color) {
        plan.color.surface_kind = D3D11ClearSurfaceKind::Color;
        plan.color.surface_format = D3D11ClearSurfaceFormat::ColorBgra8Unorm;
        plan.color.rect = rect;
        plan.color.surface_width = width;
        plan.color.surface_height = height;
        plan.color.color_mask = kAllColorChannels;
        /* The legacy helper predates const-correct renderer adapters; it only
         * reads the state and does not modify registers or dirty bits. */
        pgraph_get_clear_color(const_cast<PGRAPHState *>(pg), plan.color.color);
    }

    if (clear_zeta) {
        plan.depth_stencil.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
        plan.depth_stencil.surface_format =
            pg->surface_shape.zeta_format == NV097_SET_SURFACE_FORMAT_ZETA_Z16 ?
                D3D11ClearSurfaceFormat::Z16Fixed :
                D3D11ClearSurfaceFormat::Z24S8Fixed;
        plan.depth_stencil.rect = rect;
        plan.depth_stencil.surface_width = width;
        plan.depth_stencil.surface_height = height;
        plan.depth_stencil.fixed_depth = fixed_depth;
        plan.depth_stencil.stencil = stencil;
        plan.depth_stencil.clear_depth =
            (parameter & NV097_CLEAR_SURFACE_Z) != 0;
        plan.depth_stencil.clear_stencil =
            (parameter & NV097_CLEAR_SURFACE_STENCIL) != 0;
    }

    *output = plan;
    return D3D11ClearPlanStatus::Ready;
}

} // namespace xemu

#endif // _WIN32
