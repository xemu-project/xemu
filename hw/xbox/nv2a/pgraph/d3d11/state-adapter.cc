// xemu NV2A to D3D11 PGRAPH draw-state adapter

#include "state-adapter.h"

#ifdef _WIN32

#include <cfloat>
namespace xemu {
namespace {

bool MapBlendFactor(uint32_t value, D3D11_BLEND *result)
{
    static const D3D11_BLEND table[] = {
        D3D11_BLEND_ZERO,
        D3D11_BLEND_ONE,
        D3D11_BLEND_SRC_COLOR,
        D3D11_BLEND_INV_SRC_COLOR,
        D3D11_BLEND_SRC_ALPHA,
        D3D11_BLEND_INV_SRC_ALPHA,
        D3D11_BLEND_DEST_ALPHA,
        D3D11_BLEND_INV_DEST_ALPHA,
        D3D11_BLEND_DEST_COLOR,
        D3D11_BLEND_INV_DEST_COLOR,
        D3D11_BLEND_SRC_ALPHA_SAT,
        D3D11_BLEND_ZERO, /* internal value 11 is a table hole */
        D3D11_BLEND_BLEND_FACTOR,
        D3D11_BLEND_INV_BLEND_FACTOR,
        D3D11_BLEND_BLEND_FACTOR,
        D3D11_BLEND_INV_BLEND_FACTOR,
    };
    if (value >= sizeof(table) / sizeof(table[0]) || value == 11 ||
        value == 14 || value == 15) {
        return false;
    }
    *result = table[value];
    return true;
}

bool MapBlendOp(uint32_t value, D3D11_BLEND_OP *result)
{
    switch (value) {
    case 0:
        *result = D3D11_BLEND_OP_SUBTRACT;
        return true;
    case 1:
        *result = D3D11_BLEND_OP_REV_SUBTRACT;
        return true;
    case 2:
        *result = D3D11_BLEND_OP_ADD;
        return true;
    case 3:
        *result = D3D11_BLEND_OP_MIN;
        return true;
    case 4:
        *result = D3D11_BLEND_OP_MAX;
        return true;
    default:
        /* The signed NV2A equations (5 and 6) have no D3D11 equivalent. */
        return false;
    }
}

bool MapComparison(uint32_t value, D3D11_COMPARISON_FUNC *result)
{
    static const D3D11_COMPARISON_FUNC table[] = {
        D3D11_COMPARISON_NEVER,         D3D11_COMPARISON_LESS,
        D3D11_COMPARISON_EQUAL,         D3D11_COMPARISON_LESS_EQUAL,
        D3D11_COMPARISON_GREATER,       D3D11_COMPARISON_NOT_EQUAL,
        D3D11_COMPARISON_GREATER_EQUAL, D3D11_COMPARISON_ALWAYS,
    };
    if (value >= sizeof(table) / sizeof(table[0])) {
        return false;
    }
    *result = table[value];
    return true;
}

bool MapStencilOp(uint32_t value, D3D11_STENCIL_OP *result)
{
    switch (value) {
    case 1:
        *result = D3D11_STENCIL_OP_KEEP;
        return true;
    case 2:
        *result = D3D11_STENCIL_OP_ZERO;
        return true;
    case 3:
        *result = D3D11_STENCIL_OP_REPLACE;
        return true;
    case 4:
        *result = D3D11_STENCIL_OP_INCR_SAT;
        return true;
    case 5:
        *result = D3D11_STENCIL_OP_DECR_SAT;
        return true;
    case 6:
        *result = D3D11_STENCIL_OP_INVERT;
        return true;
    case 7:
        *result = D3D11_STENCIL_OP_INCR;
        return true;
    case 8:
        *result = D3D11_STENCIL_OP_DECR;
        return true;
    default:
        return false;
    }
}

bool ValidRect(uint32_t left, uint32_t top, uint32_t width, uint32_t height,
               const D3D11PgraphTargetDimensions &target,
               D3D11PgraphClipOrigin origin, D3D11_RECT *rect)
{
    const uint64_t right = static_cast<uint64_t>(left) + width;
    const uint64_t bottom = static_cast<uint64_t>(top) + height;
    if (width == 0 || height == 0 || right > target.width ||
        bottom > target.height || right > INT32_MAX || bottom > INT32_MAX ||
        left > INT32_MAX || top > INT32_MAX) {
        return false;
    }
    rect->left = static_cast<LONG>(left);
    rect->right = static_cast<LONG>(right);
    if (origin == D3D11PgraphClipOrigin::TopLeft) {
        rect->top = static_cast<LONG>(top);
        rect->bottom = static_cast<LONG>(bottom);
    } else {
        rect->top = static_cast<LONG>(target.height - bottom);
        rect->bottom = static_cast<LONG>(target.height - top);
    }
    return true;
}

bool ValidInclusiveWindowClips(const D3D11BridgePgraphSnapshot &pg,
                               const D3D11PgraphTargetDimensions &target)
{
    constexpr uint32_t kMaxCoordinate = 0xFFF;
    if (target.width > kMaxCoordinate + 1 ||
        target.height > kMaxCoordinate + 1) {
        return false;
    }
    const uint32_t expected_xmax = target.width - 1;
    const uint32_t expected_ymax = target.height - 1;
    bool full_cover = false;
    for (unsigned int i = 0; i < 8; ++i) {
        full_cover |= pg.window_clip_x_min[i] == 0 &&
                      pg.window_clip_y_min[i] == 0 &&
                      pg.window_clip_x_max[i] >= expected_xmax &&
                      pg.window_clip_y_max[i] >= expected_ymax;
    }
    return full_cover;
}

} // namespace

D3D11PgraphStateStatus
d3d11_build_draw_state(const PGRAPHState *pg,
                       const D3D11PgraphTargetDimensions *target,
                       const D3D11PgraphStateCapabilities *capabilities,
                       D3D11PgraphDrawState *output)
{
    if (!pg || !target || !capabilities || !output || target->width == 0 ||
        target->height == 0 || target->width > FLT_MAX ||
        target->height > FLT_MAX) {
        return D3D11PgraphStateStatus::Invalid;
    }
    D3D11BridgePgraphSnapshot snapshot = {};
    if (!d3d11_bridge_snapshot_pgraph(pg, &snapshot)) {
        return D3D11PgraphStateStatus::Invalid;
    }
    if (snapshot.primitive_mode != 5 ||
        snapshot.anti_aliasing != D3D11_BRIDGE_ANTIALIAS_CENTER_1 ||
        capabilities->surface_scale_factor != 1 ||
        capabilities->anti_aliasing != D3D11_BRIDGE_ANTIALIAS_CENTER_1 ||
        capabilities->shader_depth_convention !=
            D3D11ShaderDepthConvention::ZeroToOne) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    /* These modes need shader work or have no exact D3D11 state. */
    if (snapshot.alpha_test_enable || snapshot.dither_enable ||
        snapshot.logic_op_enable || snapshot.polygon_offset_point_enable ||
        snapshot.polygon_offset_line_enable ||
        snapshot.polygon_offset_fill_enable || snapshot.polygon_smooth_enable ||
        snapshot.window_clip_type || snapshot.antialiasing_enable) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    if (!snapshot.window_clip_values_valid ||
        !ValidInclusiveWindowClips(snapshot, *target)) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    if (snapshot.front_face_mode != 0 || snapshot.back_face_mode != 0) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    const bool cull_enable = snapshot.cull_enable;
    const uint32_t cull_face = snapshot.cull_face;
    if (cull_enable && cull_face != 1 && cull_face != 2) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_BLEND src_blend = D3D11_BLEND_ZERO;
    D3D11_BLEND dst_blend = D3D11_BLEND_ZERO;
    D3D11_BLEND_OP blend_op = D3D11_BLEND_OP_ADD;
    if (!MapBlendFactor(snapshot.blend_src_factor, &src_blend) ||
        !MapBlendFactor(snapshot.blend_dst_factor, &dst_blend) ||
        !MapBlendOp(snapshot.blend_equation, &blend_op)) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_COMPARISON_FUNC depth_func;
    if (!MapComparison(snapshot.z_func, &depth_func)) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    D3D11_COMPARISON_FUNC stencil_func;
    if (!MapComparison(snapshot.stencil_func, &stencil_func)) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    const bool stencil_enable = snapshot.stencil_test_enable;
    D3D11_STENCIL_OP stencil_fail = D3D11_STENCIL_OP_KEEP;
    D3D11_STENCIL_OP stencil_zfail = D3D11_STENCIL_OP_KEEP;
    D3D11_STENCIL_OP stencil_zpass = D3D11_STENCIL_OP_KEEP;
    if (stencil_enable &&
        (!MapStencilOp(snapshot.stencil_op_fail, &stencil_fail) ||
         !MapStencilOp(snapshot.stencil_op_zfail, &stencil_zfail) ||
         !MapStencilOp(snapshot.stencil_op_zpass, &stencil_zpass))) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_RECT scissor;
    if (!ValidRect(snapshot.clip_x, snapshot.clip_y, snapshot.clip_width,
                   snapshot.clip_height, *target, capabilities->clip_origin,
                   &scissor)) {
        return D3D11PgraphStateStatus::Invalid;
    }

    D3D11PgraphDrawState state = {};
    state.has_viewport = true;
    state.viewport.TopLeftX = 0.0f;
    state.viewport.TopLeftY = 0.0f;
    state.viewport.Width = static_cast<FLOAT>(target->width);
    state.viewport.Height = static_cast<FLOAT>(target->height);
    state.viewport.MinDepth = 0.0f;
    state.viewport.MaxDepth = 1.0f;
    state.has_scissor = true;
    state.scissor = scissor;

    state.rasterizer.FillMode = D3D11_FILL_SOLID;
    state.rasterizer.CullMode =
        cull_enable ? (cull_face == 1 ? D3D11_CULL_FRONT : D3D11_CULL_BACK) :
                      D3D11_CULL_NONE;
    state.rasterizer.FrontCounterClockwise = snapshot.front_face;
    state.rasterizer.DepthBias = 0;
    state.rasterizer.DepthBiasClamp = 0.0f;
    state.rasterizer.SlopeScaledDepthBias = 0.0f;
    state.rasterizer.DepthClipEnable = TRUE;
    state.rasterizer.ScissorEnable = TRUE;
    state.rasterizer.MultisampleEnable = FALSE;
    state.rasterizer.AntialiasedLineEnable = FALSE;

    state.depth_stencil.DepthEnable = snapshot.z_enable;
    state.depth_stencil.DepthWriteMask = snapshot.z_write_enable ?
                                             D3D11_DEPTH_WRITE_MASK_ALL :
                                             D3D11_DEPTH_WRITE_MASK_ZERO;
    state.depth_stencil.DepthFunc = depth_func;
    state.depth_stencil.StencilEnable = stencil_enable;
    state.depth_stencil.StencilReadMask =
        static_cast<UINT8>(snapshot.stencil_read_mask);
    state.depth_stencil.StencilWriteMask =
        snapshot.stencil_write_enable ?
            static_cast<UINT8>(snapshot.stencil_write_mask) :
            0;
    state.depth_stencil.FrontFace = { stencil_fail, stencil_zfail,
                                      stencil_zpass, stencil_func };
    state.depth_stencil.BackFace = state.depth_stencil.FrontFace;
    state.stencil_ref = snapshot.stencil_ref;

    state.blend.AlphaToCoverageEnable = FALSE;
    state.blend.IndependentBlendEnable = FALSE;
    auto &attachment = state.blend.RenderTarget[0];
    attachment.BlendEnable = snapshot.blend_enable;
    attachment.SrcBlend = src_blend;
    attachment.DestBlend = dst_blend;
    attachment.BlendOp = blend_op;
    attachment.SrcBlendAlpha = src_blend;
    attachment.DestBlendAlpha = dst_blend;
    attachment.BlendOpAlpha = blend_op;
    attachment.RenderTargetWriteMask =
        (snapshot.red_write_enable ? D3D11_COLOR_WRITE_ENABLE_RED : 0) |
        (snapshot.green_write_enable ? D3D11_COLOR_WRITE_ENABLE_GREEN : 0) |
        (snapshot.blue_write_enable ? D3D11_COLOR_WRITE_ENABLE_BLUE : 0) |
        (snapshot.alpha_write_enable ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0);
    const uint32_t blend_color = snapshot.blend_color;
    state.blend_factor[0] = ((blend_color >> 16) & 0xff) / 255.0f;
    state.blend_factor[1] = ((blend_color >> 8) & 0xff) / 255.0f;
    state.blend_factor[2] = (blend_color & 0xff) / 255.0f;
    state.blend_factor[3] = ((blend_color >> 24) & 0xff) / 255.0f;
    state.sample_mask = D3D11_DEFAULT_SAMPLE_MASK;
    state.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    *output = state;
    return D3D11PgraphStateStatus::Ready;
}

} // namespace xemu

#endif // _WIN32
