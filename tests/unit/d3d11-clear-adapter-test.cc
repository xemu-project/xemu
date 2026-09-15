#include "hw/xbox/nv2a/pgraph/d3d11/clear-adapter.h"

#ifdef _WIN32

#include "hw/xbox/nv2a/nv2a_regs.h"

#include <cmath>
#include <cstring>

extern "C" void pgraph_get_clear_color(PGRAPHState *, float rgba[4])
{
    rgba[0] = 0.25f;
    rgba[1] = 0.5f;
    rgba[2] = 0.75f;
    rgba[3] = 1.0f;
}

namespace {

using xemu::D3D11ClearBindingDimensions;
using xemu::D3D11ClearCapabilities;
using xemu::D3D11ClearPlan;
using xemu::D3D11ClearPlanStatus;

void InitState(PGRAPHState *pg)
{
    *pg = {};
    pg->surface_type = NV097_SET_SURFACE_FORMAT_TYPE_PITCH;
    pg->surface_scale_factor = 17;
    pg->surface_shape.anti_aliasing =
        NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1;
    pg->surface_shape.color_format = NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8;
    pg->surface_shape.zeta_format = NV097_SET_SURFACE_FORMAT_ZETA_Z24S8;
    pg->surface_color.pitch = 32;
    pg->surface_zeta.pitch = 32;
    pg->regs_[NV_PGRAPH_CLEARRECTX] = 0 | (7u << 16);
    pg->regs_[NV_PGRAPH_CLEARRECTY] = 0 | (5u << 16);
    pg->regs_[NV_PGRAPH_ZSTENCILCLEARVALUE] = 0x12345678;
    pg->regs_[NV_PGRAPH_SETUPRASTER] = 0;
}

D3D11ClearBindingDimensions MakeBindings()
{
    return { true, 8, 6, true, 8, 6 };
}

D3D11ClearCapabilities MakeCapabilities()
{
    return { 1 };
}

bool Same(const D3D11ClearPlan &a, const D3D11ClearPlan &b)
{
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

bool TestReadyAndValues()
{
    PGRAPHState pg = {};
    InitState(&pg);
    D3D11ClearBindingDimensions bindings = MakeBindings();
    D3D11ClearCapabilities capabilities = MakeCapabilities();
    D3D11ClearPlan plan = {};
    const auto status = xemu::d3d11_build_clear_plan(
        &pg,
        NV097_CLEAR_SURFACE_COLOR | NV097_CLEAR_SURFACE_Z |
            NV097_CLEAR_SURFACE_STENCIL,
        &bindings, &capabilities, &plan);
    return status == D3D11ClearPlanStatus::Ready && plan.clear_color &&
           plan.clear_depth_stencil && plan.color.surface_width == 8 &&
           plan.color.surface_height == 6 && plan.color.rect.right == 7 &&
           std::fabs(plan.color.color[0] - 0.25f) < 0.0001f &&
           plan.depth_stencil.fixed_depth == 0x123456 &&
           plan.depth_stencil.stencil == 0x78 &&
           plan.depth_stencil.clear_depth && plan.depth_stencil.clear_stencil;
}

bool TestGatesAndAtomicity()
{
    PGRAPHState pg = {};
    InitState(&pg);
    D3D11ClearBindingDimensions bindings = MakeBindings();
    D3D11ClearCapabilities capabilities = MakeCapabilities();
    D3D11ClearPlan sentinel = {};
    sentinel.clear_color = true;
    sentinel.color.color[0] = 0.125f;

    D3D11ClearPlan output = sentinel;
    if (xemu::d3d11_build_clear_plan(&pg, 0, &bindings, &capabilities,
                                     &output) !=
            D3D11ClearPlanStatus::NotRequested ||
        !Same(output, sentinel)) {
        return false;
    }

    output = sentinel;
    if (xemu::d3d11_build_clear_plan(
            &pg, NV097_CLEAR_SURFACE_R | NV097_CLEAR_SURFACE_G, &bindings,
            &capabilities, &output) != D3D11ClearPlanStatus::Unsupported ||
        !Same(output, sentinel)) {
        return false;
    }

    output = sentinel;
    bindings.color_bound = false;
    if (xemu::d3d11_build_clear_plan(&pg, NV097_CLEAR_SURFACE_COLOR, &bindings,
                                     &capabilities, &output) !=
            D3D11ClearPlanStatus::Invalid ||
        !Same(output, sentinel)) {
        return false;
    }
    bindings = MakeBindings();

    output = sentinel;
    bindings.zeta_width = 7;
    if (xemu::d3d11_build_clear_plan(
            &pg, NV097_CLEAR_SURFACE_COLOR | NV097_CLEAR_SURFACE_Z, &bindings,
            &capabilities, &output) != D3D11ClearPlanStatus::Invalid ||
        !Same(output, sentinel)) {
        return false;
    }
    bindings = MakeBindings();

    output = sentinel;
    pg.regs_[NV_PGRAPH_CLEARRECTX] = 1 | (7u << 16);
    if (xemu::d3d11_build_clear_plan(&pg, NV097_CLEAR_SURFACE_Z, &bindings,
                                     &capabilities, &output) !=
            D3D11ClearPlanStatus::Unsupported ||
        !Same(output, sentinel)) {
        return false;
    }
    return true;
}

bool TestFormatAndLayoutGates()
{
    PGRAPHState pg = {};
    InitState(&pg);
    D3D11ClearBindingDimensions bindings = MakeBindings();
    D3D11ClearCapabilities capabilities = MakeCapabilities();
    D3D11ClearPlan output = {};
    pg.regs_[NV_PGRAPH_SETUPRASTER] = NV_PGRAPH_SETUPRASTER_Z_FORMAT;
    pg.surface_shape.z_format = 0;
    if (xemu::d3d11_build_clear_plan(&pg, NV097_CLEAR_SURFACE_Z, &bindings,
                                     &capabilities, &output) !=
        D3D11ClearPlanStatus::Unsupported) {
        return false;
    }
    InitState(&pg);
    pg.surface_type = NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE;
    if (xemu::d3d11_build_clear_plan(&pg, NV097_CLEAR_SURFACE_COLOR, &bindings,
                                     &capabilities, &output) !=
        D3D11ClearPlanStatus::Unsupported) {
        return false;
    }
    InitState(&pg);
    capabilities.surface_scale_factor = 2;
    if (xemu::d3d11_build_clear_plan(&pg, NV097_CLEAR_SURFACE_COLOR, &bindings,
                                     &capabilities, &output) !=
        D3D11ClearPlanStatus::Unsupported) {
        return false;
    }
    return true;
}

} // namespace

int main()
{
    if (!TestReadyAndValues() || !TestGatesAndAtomicity() ||
        !TestFormatAndLayoutGates()) {
        return 1;
    }
    return 0;
}

#else

int main()
{
    return 0;
}

#endif // _WIN32
