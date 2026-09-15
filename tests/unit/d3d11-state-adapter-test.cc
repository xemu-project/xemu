#include "hw/xbox/nv2a/pgraph/d3d11/state-adapter.h"

#ifdef _WIN32

#include "hw/xbox/nv2a/nv2a_regs.h"

#include <cstring>

namespace {

using xemu::D3D11PgraphDrawState;
using xemu::D3D11PgraphStateCapabilities;
using xemu::D3D11PgraphStateStatus;
using xemu::D3D11PgraphTargetDimensions;

void SetFullWindowClips(PGRAPHState *pg, uint32_t width, uint32_t height)
{
    for (unsigned int i = 0; i < 8; ++i) {
        pg->regs_[NV_PGRAPH_WINDOWCLIPX0 + i * 4] = (width - 1u) << 16u;
        pg->regs_[NV_PGRAPH_WINDOWCLIPY0 + i * 4] = (height - 1u) << 16u;
    }
}

void Init(PGRAPHState *pg)
{
    *pg = {};
    pg->primitive_mode = PRIM_TYPE_TRIANGLES;
    pg->surface_shape.anti_aliasing =
        NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1;
    pg->surface_shape.clip_y = 5u;
    pg->surface_shape.clip_width = 64u;
    pg->surface_shape.clip_height = 10u;
    pg->regs_[NV_PGRAPH_CONTROL_0] = NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE |
                                     NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE |
                                     NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE |
                                     NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE;
    SetFullWindowClips(pg, 64u, 32u);
}

bool Build(const PGRAPHState *pg, D3D11PgraphDrawState *state)
{
    const D3D11PgraphTargetDimensions target = { 64u, 32u };
    const D3D11PgraphStateCapabilities capabilities = {};
    return xemu::d3d11_build_draw_state(pg, &target, &capabilities, state) ==
           D3D11PgraphStateStatus::Ready;
}

bool TestDefaultAndNoMutation()
{
    PGRAPHState pg = {};
    Init(&pg);
    D3D11PgraphDrawState state = {};
    const PGRAPHState before = pg;
    if (!Build(&pg, &state) || !state.has_viewport || !state.has_scissor ||
        state.viewport.Width != 64.0f || state.viewport.Height != 32.0f ||
        state.scissor.left != static_cast<LONG>(0) ||
        state.scissor.right != static_cast<LONG>(64) ||
        state.scissor.top != static_cast<LONG>(17) ||
        state.scissor.bottom != static_cast<LONG>(27) ||
        state.rasterizer.FillMode != D3D11_FILL_SOLID ||
        state.rasterizer.CullMode != D3D11_CULL_NONE ||
        state.depth_stencil.DepthEnable ||
        state.blend.RenderTarget[0].BlendEnable ||
        state.blend.RenderTarget[0].RenderTargetWriteMask !=
            (D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN |
             D3D11_COLOR_WRITE_ENABLE_BLUE | D3D11_COLOR_WRITE_ENABLE_ALPHA) ||
        state.topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST ||
        std::memcmp(&pg, &before, sizeof(pg)) != 0) {
        return false;
    }
    return true;
}

bool TestEnums()
{
    PGRAPHState pg = {};
    Init(&pg);
    pg.regs_[NV_PGRAPH_SETUPRASTER] =
        NV_PGRAPH_SETUPRASTER_CULLENABLE |
        (NV_PGRAPH_SETUPRASTER_CULLCTRL_BACK << 21u) |
        NV_PGRAPH_SETUPRASTER_FRONTFACE;
    pg.regs_[NV_PGRAPH_CONTROL_0] |=
        NV_PGRAPH_CONTROL_0_ZENABLE | NV_PGRAPH_CONTROL_0_ZWRITEENABLE;
    pg.regs_[NV_PGRAPH_CONTROL_0] |= (NV_PGRAPH_CONTROL_0_ZFUNC_GEQUAL << 16u);
    pg.regs_[NV_PGRAPH_CONTROL_1] =
        NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE |
        (NV_PGRAPH_CONTROL_1_STENCIL_FUNC_GREATER << 4u) | (0x5au << 8u) |
        (0x3cu << 16u) | (0xa5u << 24u);
    pg.regs_[NV_PGRAPH_CONTROL_2] =
        (NV_PGRAPH_CONTROL_2_STENCIL_OP_V_REPLACE << 0u) |
        (NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCRSAT << 4u) |
        (NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECR << 8u);
    pg.regs_[NV_PGRAPH_BLEND] =
        NV_PGRAPH_BLEND_EN | (NV_PGRAPH_BLEND_SFACTOR_SRC_ALPHA << 4u) |
        (NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_SRC_ALPHA << 8u) | (2u << 0u);
    pg.regs_[NV_PGRAPH_BLENDCOLOR] = 0x80402010;

    D3D11PgraphDrawState state = {};
    if (!Build(&pg, &state) || state.rasterizer.CullMode != D3D11_CULL_BACK ||
        !state.rasterizer.FrontCounterClockwise ||
        state.depth_stencil.DepthFunc != D3D11_COMPARISON_GREATER_EQUAL ||
        state.depth_stencil.FrontFace.StencilFunc != D3D11_COMPARISON_GREATER ||
        state.depth_stencil.FrontFace.StencilFailOp !=
            D3D11_STENCIL_OP_REPLACE ||
        state.depth_stencil.FrontFace.StencilDepthFailOp !=
            D3D11_STENCIL_OP_INCR_SAT ||
        state.depth_stencil.FrontFace.StencilPassOp != D3D11_STENCIL_OP_DECR ||
        state.stencil_ref != 0x5au ||
        state.blend.RenderTarget[0].SrcBlend != D3D11_BLEND_SRC_ALPHA ||
        state.blend.RenderTarget[0].DestBlend != D3D11_BLEND_INV_SRC_ALPHA ||
        state.blend.RenderTarget[0].BlendOp != D3D11_BLEND_OP_ADD ||
        state.blend_factor[0] != 0x40 / 255.0f ||
        state.blend_factor[3] != 0x80 / 255.0f ||
        state.depth_stencil.StencilWriteMask != 0) {
        return false;
    }
    pg.regs_[NV_PGRAPH_CONTROL_0] |= NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE;
    if (!Build(&pg, &state) ||
        state.depth_stencil.StencilWriteMask != static_cast<UINT8>(0xa5u)) {
        return false;
    }
    pg.regs_[NV_PGRAPH_BLEND] =
        NV_PGRAPH_BLEND_EN | (NV_PGRAPH_BLEND_SFACTOR_CONSTANT_COLOR << 4) |
        (NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_CONSTANT_COLOR << 8u) | (2u << 0u);
    return Build(&pg, &state);
}

bool TestUnsupportedAndBounds()
{
    PGRAPHState pg = {};
    Init(&pg);
    D3D11PgraphDrawState sentinel = {};
    sentinel.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    D3D11PgraphDrawState state = sentinel;
    const D3D11PgraphTargetDimensions target = { 64u, 32u };
    const D3D11PgraphStateCapabilities capabilities = {};

    pg.primitive_mode = PRIM_TYPE_TRIANGLE_STRIP;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    pg.surface_shape.clip_x = 60;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Invalid ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    pg.regs_[NV_PGRAPH_BLEND] = NV_PGRAPH_BLEND_LOGICOP_ENABLE;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    for (uint32_t factor :
         { NV_PGRAPH_BLEND_SFACTOR_CONSTANT_ALPHA,
           NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_ALPHA }) {
        pg.regs_[NV_PGRAPH_BLEND] = factor << 4;
        if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
                D3D11PgraphStateStatus::Unsupported ||
            std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
            return false;
        }
        state = sentinel;
    }
    Init(&pg);
    state = sentinel;
    pg.regs_[NV_PGRAPH_SETUPRASTER] = NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    for (unsigned int i = 0; i < 8; ++i) {
        pg.regs_[NV_PGRAPH_WINDOWCLIPX0 + i * 4] = 0u;
        pg.regs_[NV_PGRAPH_WINDOWCLIPY0 + i * 4] = 0u;
    }
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    for (unsigned int i = 0; i < 8; ++i) {
        pg.regs_[NV_PGRAPH_WINDOWCLIPX0 + i * 4] = 0u;
        pg.regs_[NV_PGRAPH_WINDOWCLIPY0 + i * 4] = 0u;
    }
    pg.regs_[NV_PGRAPH_WINDOWCLIPX0] = 63u << 16u;
    pg.regs_[NV_PGRAPH_WINDOWCLIPY0] = 31u << 16u;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Ready ||
        state.scissor.right != static_cast<LONG>(64) ||
        state.scissor.bottom != static_cast<LONG>(27)) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    for (unsigned int i = 1; i < 8; ++i) {
        pg.regs_[NV_PGRAPH_WINDOWCLIPX0 + i * 4] = 1u | (4u << 16u);
        pg.regs_[NV_PGRAPH_WINDOWCLIPY0 + i * 4] = 1u | (4u << 16u);
    }
    pg.regs_[NV_PGRAPH_WINDOWCLIPX0] = 0xFFFu << 16u;
    pg.regs_[NV_PGRAPH_WINDOWCLIPY0] = 0xFFFu << 16u;
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
        D3D11PgraphStateStatus::Ready) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    pg.regs_[NV_PGRAPH_SETUPRASTER] =
        static_cast<uint32_t>(NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE);
    if (xemu::d3d11_build_draw_state(&pg, &target, &capabilities, &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    D3D11PgraphStateCapabilities top_left = capabilities;
    top_left.clip_origin = xemu::D3D11PgraphClipOrigin::TopLeft;
    if (xemu::d3d11_build_draw_state(&pg, &target, &top_left, &state) !=
            D3D11PgraphStateStatus::Ready ||
        state.scissor.top != static_cast<LONG>(5) ||
        state.scissor.bottom != static_cast<LONG>(15)) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    const D3D11PgraphTargetDimensions asymmetric_target = { 7u, 5u };
    pg.surface_shape.clip_y = 1;
    pg.surface_shape.clip_width = 7;
    pg.surface_shape.clip_height = 2;
    SetFullWindowClips(&pg, asymmetric_target.width, asymmetric_target.height);
    if (xemu::d3d11_build_draw_state(&pg, &asymmetric_target, &capabilities,
                                     &state) != D3D11PgraphStateStatus::Ready ||
        state.scissor.top != static_cast<LONG>(2) ||
        state.scissor.bottom != static_cast<LONG>(4)) {
        return false;
    }
    Init(&pg);
    state = sentinel;
    const D3D11PgraphTargetDimensions overflow_target = { 4097u, 32u };
    if (xemu::d3d11_build_draw_state(&pg, &overflow_target, &capabilities,
                                     &state) !=
            D3D11PgraphStateStatus::Unsupported ||
        std::memcmp(&state, &sentinel, sizeof(state)) != 0) {
        return false;
    }
    return true;
}

} // namespace

int main()
{
    return TestDefaultAndNoMutation() && TestEnums() &&
                   TestUnsupportedAndBounds() ?
               0 :
               1;
}

#else

int main()
{
    return 0;
}

#endif
