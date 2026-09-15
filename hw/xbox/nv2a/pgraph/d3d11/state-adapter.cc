// xemu NV2A to D3D11 PGRAPH draw-state adapter

#include "state-adapter.h"

#ifdef _WIN32

#include "../../nv2a_regs.h"

#include <cfloat>
namespace xemu {
namespace {

uint32_t Read(const PGRAPHState *pg, unsigned int reg)
{
    assert(reg % 4 == 0);
    return pg->regs_[reg];
}

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
        value == NV_PGRAPH_BLEND_SFACTOR_CONSTANT_ALPHA ||
        value == NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_ALPHA) {
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
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_KEEP:
        *result = D3D11_STENCIL_OP_KEEP;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_ZERO:
        *result = D3D11_STENCIL_OP_ZERO;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_REPLACE:
        *result = D3D11_STENCIL_OP_REPLACE;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCRSAT:
        *result = D3D11_STENCIL_OP_INCR_SAT;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECRSAT:
        *result = D3D11_STENCIL_OP_DECR_SAT;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INVERT:
        *result = D3D11_STENCIL_OP_INVERT;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCR:
        *result = D3D11_STENCIL_OP_INCR;
        return true;
    case NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECR:
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

bool ValidInclusiveWindowClips(const PGRAPHState *pg,
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
        const uint32_t x = Read(pg, NV_PGRAPH_WINDOWCLIPX0 + i * 4);
        const uint32_t y = Read(pg, NV_PGRAPH_WINDOWCLIPY0 + i * 4);
        if ((x & ~(NV_PGRAPH_WINDOWCLIPX0_XMIN |
                   NV_PGRAPH_WINDOWCLIPX0_XMAX)) != 0 ||
            (y & ~(NV_PGRAPH_WINDOWCLIPY0_YMIN |
                   NV_PGRAPH_WINDOWCLIPY0_YMAX)) != 0) {
            return false;
        }
        full_cover |=
            GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN) == 0 &&
            GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN) == 0 &&
            GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX) >= expected_xmax &&
            GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX) >= expected_ymax;
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
    if (pg->primitive_mode != PRIM_TYPE_TRIANGLES ||
        pg->surface_shape.anti_aliasing !=
            NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1 ||
        capabilities->surface_scale_factor != 1 ||
        capabilities->anti_aliasing !=
            NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1 ||
        capabilities->shader_depth_convention !=
            D3D11ShaderDepthConvention::ZeroToOne) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    const uint32_t control0 = Read(pg, NV_PGRAPH_CONTROL_0);
    const uint32_t control1 = Read(pg, NV_PGRAPH_CONTROL_1);
    const uint32_t control2 = Read(pg, NV_PGRAPH_CONTROL_2);
    const uint32_t blend = Read(pg, NV_PGRAPH_BLEND);
    const uint32_t raster = Read(pg, NV_PGRAPH_SETUPRASTER);

    /* These modes need shader work or have no exact D3D11 state. */
    if ((control0 & (NV_PGRAPH_CONTROL_0_ALPHATESTENABLE |
                     NV_PGRAPH_CONTROL_0_DITHERENABLE)) ||
        (blend & NV_PGRAPH_BLEND_LOGICOP_ENABLE) ||
        (raster & (NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE |
                   NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE |
                   NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE |
                   NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE |
                   NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE)) ||
        (Read(pg, NV_PGRAPH_ANTIALIASING) & NV_PGRAPH_ANTIALIASING_ENABLE)) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    if (!ValidInclusiveWindowClips(pg, *target)) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    if ((GET_MASK(raster, NV_PGRAPH_SETUPRASTER_FRONTFACEMODE) !=
         NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_FILL) ||
        GET_MASK(raster, NV_PGRAPH_SETUPRASTER_BACKFACEMODE) != 0) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    const bool cull_enable = (raster & NV_PGRAPH_SETUPRASTER_CULLENABLE) != 0;
    const uint32_t cull_face = GET_MASK(raster, NV_PGRAPH_SETUPRASTER_CULLCTRL);
    if (cull_enable && cull_face != NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT &&
        cull_face != NV_PGRAPH_SETUPRASTER_CULLCTRL_BACK) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_BLEND src_blend = D3D11_BLEND_ZERO;
    D3D11_BLEND dst_blend = D3D11_BLEND_ZERO;
    D3D11_BLEND_OP blend_op = D3D11_BLEND_OP_ADD;
    if (!MapBlendFactor(GET_MASK(blend, NV_PGRAPH_BLEND_SFACTOR), &src_blend) ||
        !MapBlendFactor(GET_MASK(blend, NV_PGRAPH_BLEND_DFACTOR), &dst_blend) ||
        !MapBlendOp(GET_MASK(blend, NV_PGRAPH_BLEND_EQN), &blend_op)) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_COMPARISON_FUNC depth_func;
    if (!MapComparison(GET_MASK(control0, NV_PGRAPH_CONTROL_0_ZFUNC),
                       &depth_func)) {
        return D3D11PgraphStateStatus::Unsupported;
    }
    D3D11_COMPARISON_FUNC stencil_func;
    if (!MapComparison(GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_FUNC),
                       &stencil_func)) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    const bool stencil_enable =
        (control1 & NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE) != 0;
    D3D11_STENCIL_OP stencil_fail = D3D11_STENCIL_OP_KEEP;
    D3D11_STENCIL_OP stencil_zfail = D3D11_STENCIL_OP_KEEP;
    D3D11_STENCIL_OP stencil_zpass = D3D11_STENCIL_OP_KEEP;
    if (stencil_enable &&
        (!MapStencilOp(GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_FAIL),
                       &stencil_fail) ||
         !MapStencilOp(GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_ZFAIL),
                       &stencil_zfail) ||
         !MapStencilOp(GET_MASK(control2, NV_PGRAPH_CONTROL_2_STENCIL_OP_ZPASS),
                       &stencil_zpass))) {
        return D3D11PgraphStateStatus::Unsupported;
    }

    D3D11_RECT scissor;
    if (!ValidRect(pg->surface_shape.clip_x, pg->surface_shape.clip_y,
                   pg->surface_shape.clip_width, pg->surface_shape.clip_height,
                   *target, capabilities->clip_origin, &scissor)) {
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
        cull_enable ? (cull_face == NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT ?
                           D3D11_CULL_FRONT :
                           D3D11_CULL_BACK) :
                      D3D11_CULL_NONE;
    state.rasterizer.FrontCounterClockwise =
        (raster & NV_PGRAPH_SETUPRASTER_FRONTFACE) != 0;
    state.rasterizer.DepthBias = 0;
    state.rasterizer.DepthBiasClamp = 0.0f;
    state.rasterizer.SlopeScaledDepthBias = 0.0f;
    state.rasterizer.DepthClipEnable = TRUE;
    state.rasterizer.ScissorEnable = TRUE;
    state.rasterizer.MultisampleEnable = FALSE;
    state.rasterizer.AntialiasedLineEnable = FALSE;

    state.depth_stencil.DepthEnable =
        (control0 & NV_PGRAPH_CONTROL_0_ZENABLE) != 0;
    state.depth_stencil.DepthWriteMask =
        (control0 & NV_PGRAPH_CONTROL_0_ZWRITEENABLE) ?
            D3D11_DEPTH_WRITE_MASK_ALL :
            D3D11_DEPTH_WRITE_MASK_ZERO;
    state.depth_stencil.DepthFunc = depth_func;
    state.depth_stencil.StencilEnable = stencil_enable;
    state.depth_stencil.StencilReadMask = static_cast<UINT8>(
        GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_MASK_READ));
    state.depth_stencil.StencilWriteMask =
        (control0 & NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE) ?
            static_cast<UINT8>(
                GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_MASK_WRITE)) :
            0;
    state.depth_stencil.FrontFace = { stencil_fail, stencil_zfail,
                                      stencil_zpass, stencil_func };
    state.depth_stencil.BackFace = state.depth_stencil.FrontFace;
    state.stencil_ref = GET_MASK(control1, NV_PGRAPH_CONTROL_1_STENCIL_REF);

    state.blend.AlphaToCoverageEnable = FALSE;
    state.blend.IndependentBlendEnable = FALSE;
    auto &attachment = state.blend.RenderTarget[0];
    attachment.BlendEnable = (blend & NV_PGRAPH_BLEND_EN) != 0;
    attachment.SrcBlend = src_blend;
    attachment.DestBlend = dst_blend;
    attachment.BlendOp = blend_op;
    attachment.SrcBlendAlpha = src_blend;
    attachment.DestBlendAlpha = dst_blend;
    attachment.BlendOpAlpha = blend_op;
    attachment.RenderTargetWriteMask =
        ((control0 & NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE) ?
             D3D11_COLOR_WRITE_ENABLE_RED :
             0) |
        ((control0 & NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE) ?
             D3D11_COLOR_WRITE_ENABLE_GREEN :
             0) |
        ((control0 & NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE) ?
             D3D11_COLOR_WRITE_ENABLE_BLUE :
             0) |
        ((control0 & NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE) ?
             D3D11_COLOR_WRITE_ENABLE_ALPHA :
             0);
    const uint32_t blend_color = Read(pg, NV_PGRAPH_BLENDCOLOR);
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
