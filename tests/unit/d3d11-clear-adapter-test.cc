#include "hw/xbox/nv2a/pgraph/d3d11/clear-adapter.h"

#include "hw/xbox/nv2a/pgraph/pgraph.h"

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

DMAObject g_bridge_dma = {};
uint64_t g_bridge_dirty_offset[2] = {};
uint64_t g_bridge_dirty_size[2] = {};
unsigned g_bridge_dirty_count = 0;

extern "C" uint64_t memory_region_size(MemoryRegion *mr)
{
    return int128_get64(mr->size);
}

extern "C" DMAObject nv_dma_load(NV2AState *, hwaddr)
{
    return g_bridge_dma;
}

extern "C" void memory_region_set_client_dirty(MemoryRegion *, hwaddr offset,
                                               hwaddr size, unsigned)
{
    if (g_bridge_dirty_count < 2) {
        g_bridge_dirty_offset[g_bridge_dirty_count] = offset;
        g_bridge_dirty_size[g_bridge_dirty_count] = size;
    }
    ++g_bridge_dirty_count;
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

bool TestBridgeSnapshotsAndBounds()
{
    PGRAPHState pg = {};
    InitState(&pg);
    pg.primitive_mode = PRIM_TYPE_TRIANGLES;
    pg.regs_[NV_PGRAPH_CSV0_C] = 3u << 8;
    pg.regs_[NV_PGRAPH_CSV0_D] = 2u << 30;
    pg.program_data[3][0] = 0x12345678;
    pg.program_data[3][3] = 1;
    pg.vsh_constants[4][0] = 0x3f800000;
    pg.inline_array_length = 2;
    pg.inline_array[0] = 11;
    pg.inline_array[1] = 22;

    D3D11BridgePgraphSnapshot snapshot = {};
    if (!d3d11_bridge_snapshot_pgraph(&pg, &snapshot) ||
        snapshot.primitive_mode != PRIM_TYPE_TRIANGLES ||
        snapshot.vsh_program_start != 3 || snapshot.vsh_mode != 2 ||
        snapshot.clear_color[0] != 0.25f) {
        return false;
    }
    uint32_t program[VSH_TOKEN_SIZE] = {};
    size_t token_count = 0;
    if (!d3d11_bridge_copy_program(&pg, program, VSH_TOKEN_SIZE,
                                   &token_count) ||
        token_count != 1 || program[0] != 0x12345678) {
        return false;
    }
    uint32_t constants[D3D11_BRIDGE_VSH_CONSTANTS * 4] = {};
    size_t constant_words = 0;
    if (!d3d11_bridge_copy_vsh_constants(
            &pg, constants, D3D11_BRIDGE_VSH_CONSTANTS * 4, &constant_words) ||
        constant_words != D3D11_BRIDGE_VSH_CONSTANTS * 4 ||
        constants[16] != 0x3f800000) {
        return false;
    }
    uint32_t inline_words[2] = {};
    size_t inline_count = 0;
    if (!d3d11_bridge_copy_inline_array(&pg, inline_words, 2, &inline_count) ||
        inline_count != 2 || inline_words[1] != 22) {
        return false;
    }
    pg.inline_array_length = D3D11_BRIDGE_MAX_BATCH_WORDS + 1;
    inline_count = 0;
    if (d3d11_bridge_copy_inline_array(&pg, inline_words, 2, &inline_count) ||
        inline_count != D3D11_BRIDGE_MAX_BATCH_WORDS + 1) {
        return false;
    }
    return true;
}

bool TestBridgeDmaAndDirtyBounds()
{
    NV2AState state = {};
    MemoryRegion vram = {};
    uint8_t bytes[256] = {};
    for (unsigned i = 0; i < sizeof(bytes); ++i) {
        bytes[i] = static_cast<uint8_t>(i);
    }
    vram.size = int128_make64(sizeof(bytes));
    state.vram = &vram;
    state.vram_ptr = bytes;
    state.ramin.size = int128_make64(12);
    state.ramin_ptr = bytes;
    g_bridge_dma = { NV_DMA_IN_MEMORY_CLASS, NV_DMA_TARGET_NVM, 16, 64 };
    uint8_t copied[4] = {};
    if (!d3d11_bridge_copy_dma(&state, 0, 4, sizeof(copied), copied,
                               sizeof(copied)) ||
        copied[0] != 20 || copied[3] != 23) {
        return false;
    }
    g_bridge_dma.address = UINT64_C(0x08000000);
    if (!d3d11_bridge_copy_dma(&state, 0, 0, 1, copied, sizeof(copied)) ||
        copied[0] != bytes[0]) {
        return false;
    }
    g_bridge_dma.address = 16;
    if (d3d11_bridge_copy_dma(&state, 0, 64, 1, copied, sizeof(copied)) ||
        d3d11_bridge_copy_dma(&state, 16, 0, 1, copied, sizeof(copied))) {
        return false;
    }
    state.pgraph.surface_type = D3D11_BRIDGE_SURFACE_TYPE_PITCH;
    state.pgraph.surface_scale_factor = 1;
    state.pgraph.surface_shape.anti_aliasing = D3D11_BRIDGE_ANTIALIAS_CENTER_1;
    state.pgraph.surface_shape.color_format =
        NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8;
    state.pgraph.surface_color.offset = 0;
    state.pgraph.surface_color.pitch = 16;
    state.pgraph.dma_color = 0;
    D3D11BridgeSurfaceDescriptor descriptor = {};
    uint32_t surface_status = D3D11_BRIDGE_SURFACE_INVALID;
    g_bridge_dma.address = UINT64_C(0x08000000);
    if (d3d11_bridge_describe_surface(&state, true, 4, 4, &descriptor,
                                      &surface_status) ||
        surface_status != D3D11_BRIDGE_SURFACE_OUT_OF_RANGE) {
        return false;
    }
    g_bridge_dma.address = 0;
    g_bridge_dma.dma_class = 0;
    surface_status = D3D11_BRIDGE_SURFACE_INVALID;
    if (d3d11_bridge_describe_surface(&state, true, 4, 4, &descriptor,
                                      &surface_status) ||
        surface_status != D3D11_BRIDGE_SURFACE_UNSUPPORTED) {
        return false;
    }
    g_bridge_dirty_count = 0;
    if (!d3d11_bridge_mark_vram_dirty(&state, 8, 12) ||
        g_bridge_dirty_count != 2 || g_bridge_dirty_offset[0] != 8 ||
        g_bridge_dirty_size[1] != 12) {
        return false;
    }
    state.vram_ptr = nullptr;
    return !d3d11_bridge_mark_vram_dirty(&state, 8, 12);
}

} // namespace

int main()
{
    if (!TestReadyAndValues() || !TestGatesAndAtomicity() ||
        !TestFormatAndLayoutGates() || !TestBridgeSnapshotsAndBounds() ||
        !TestBridgeDmaAndDirtyBounds()) {
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
