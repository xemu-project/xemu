// xemu NV2A D3D11 draw executor

#include "draw-executor.h"

#ifdef _WIN32

#include "../../nv2a_int.h"
#include "../../nv2a_regs.h"
#include "draw_shaders.h"
#include "vsh_cpu_passthrough.h"

#include <windows.h>

#include <array>
#include <cstddef>
#include <cstring>

#include "nv2a_vsh_disassembler.h"

namespace xemu {
namespace {

using Microsoft::WRL::ComPtr;

static_assert(offsetof(D3D11CanonicalVertex, position) == 0);
static_assert(offsetof(D3D11CanonicalVertex, color) == sizeof(float) * 3);
static_assert(sizeof(D3D11CanonicalVertex) == sizeof(float) * 7);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, position) == 0);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, color) ==
              sizeof(float) * 3);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oPos) ==
              sizeof(float) * 7);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oD0) ==
              sizeof(float) * 11);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oD1) ==
              sizeof(float) * 15);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oFog) ==
              sizeof(float) * 19);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oPts) ==
              sizeof(float) * 23);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oB0) ==
              sizeof(float) * 27);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oB1) ==
              sizeof(float) * 31);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oT0) ==
              sizeof(float) * 35);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oT1) ==
              sizeof(float) * 39);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oT2) ==
              sizeof(float) * 43);
static_assert(offsetof(d3d11_vsh::VshCpuVertexOutput, oT3) ==
              sizeof(float) * 47);

uint32_t VshMode(const PGRAPHState *pg)
{
    return pg == nullptr ?
               UINT32_MAX :
               GET_MASK(pg->regs_[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_MODE);
}

bool GetProgrammableVsh(const PGRAPHState *pg,
                        std::array<uint32_t, NV2A_MAX_TRANSFORM_PROGRAM_LENGTH *
                                                 VSH_TOKEN_SIZE> *tokens,
                        uint32_t *token_count)
{
    if (pg == nullptr || tokens == nullptr || token_count == nullptr) {
        return false;
    }
    const uint32_t start = GET_MASK(pg->regs_[NV_PGRAPH_CSV0_C],
                                    NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);
    if (start >= NV2A_MAX_TRANSFORM_PROGRAM_LENGTH) {
        return false;
    }
    uint32_t count = 0;
    for (; start + count < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH; ++count) {
        const uint32_t *source = pg->program_data[start + count];
        std::memcpy(tokens->data() + count * VSH_TOKEN_SIZE, source,
                    sizeof(uint32_t) * VSH_TOKEN_SIZE);
        /* FLD_FINAL is token dword 3 bit 0.  Keep this scan local to the
         * bounded raw-token layout; the C disassembler is invoked only after
         * the range has been proven. */
        if ((source[3] & 1u) != 0) {
            *token_count = count + 1;
            return true;
        }
    }
    return false;
}

bool HasContextOutputs(const uint32_t *tokens, uint32_t token_count)
{
    if (tokens == nullptr || token_count == 0) {
        return false;
    }
    Nv2aVshProgram program = {};
    if (nv2a_vsh_parse_program(&program, tokens, token_count) !=
        NV2AVPR_SUCCESS) {
        return false;
    }
    bool context_output = false;
    for (uint32_t i = 0; i < token_count && !context_output; ++i) {
        const Nv2aVshOperation *operations[] = { &program.steps[i].mac,
                                                 &program.steps[i].ilu };
        for (const Nv2aVshOperation *operation : operations) {
            for (const Nv2aVshOutput &output : operation->outputs) {
                context_output |= output.type == NV2ART_CONTEXT;
            }
        }
    }
    nv2a_vsh_program_destroy(&program);
    return context_output;
}

bool ValidProgrammableTokens(const uint32_t *tokens, uint32_t token_count)
{
    if (tokens == nullptr || token_count == 0 ||
        token_count > NV2A_MAX_TRANSFORM_PROGRAM_LENGTH) {
        return false;
    }
    for (uint32_t i = 0; i < token_count; ++i) {
        const uint32_t *token = tokens + i * VSH_TOKEN_SIZE;
        if (((token[1] >> 21) & 0xfu) > 13u || ((token[1] >> 25) & 0x7u) > 7u) {
            return false;
        }
    }
    return true;
}

float VshClipRange(const PGRAPHState *pg)
{
    if (pg == nullptr) {
        return 1.0f;
    }
    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        return pg->surface_shape.z_format ? f16_max : 65535.0f;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        return pg->surface_shape.z_format ? f24_max : 16777215.0f;
    default:
        return 1.0f;
    }
}

bool SameRasterizer(const D3D11_RASTERIZER_DESC &a,
                    const D3D11_RASTERIZER_DESC &b)
{
    return a.FillMode == b.FillMode && a.CullMode == b.CullMode &&
           a.FrontCounterClockwise == b.FrontCounterClockwise &&
           a.DepthBias == b.DepthBias && a.DepthBiasClamp == b.DepthBiasClamp &&
           a.SlopeScaledDepthBias == b.SlopeScaledDepthBias &&
           a.DepthClipEnable == b.DepthClipEnable &&
           a.ScissorEnable == b.ScissorEnable &&
           a.MultisampleEnable == b.MultisampleEnable &&
           a.AntialiasedLineEnable == b.AntialiasedLineEnable;
}

bool SameDepth(const D3D11_DEPTH_STENCIL_DESC &a,
               const D3D11_DEPTH_STENCIL_DESC &b)
{
    return a.DepthEnable == b.DepthEnable &&
           a.DepthWriteMask == b.DepthWriteMask && a.DepthFunc == b.DepthFunc &&
           a.StencilEnable == b.StencilEnable &&
           a.StencilReadMask == b.StencilReadMask &&
           a.StencilWriteMask == b.StencilWriteMask &&
           a.FrontFace.StencilFailOp == b.FrontFace.StencilFailOp &&
           a.FrontFace.StencilDepthFailOp == b.FrontFace.StencilDepthFailOp &&
           a.FrontFace.StencilPassOp == b.FrontFace.StencilPassOp &&
           a.FrontFace.StencilFunc == b.FrontFace.StencilFunc &&
           a.BackFace.StencilFailOp == b.BackFace.StencilFailOp &&
           a.BackFace.StencilDepthFailOp == b.BackFace.StencilDepthFailOp &&
           a.BackFace.StencilPassOp == b.BackFace.StencilPassOp &&
           a.BackFace.StencilFunc == b.BackFace.StencilFunc;
}

bool SameBlend(const D3D11_BLEND_DESC &a, const D3D11_BLEND_DESC &b)
{
    if (a.AlphaToCoverageEnable != b.AlphaToCoverageEnable ||
        a.IndependentBlendEnable != b.IndependentBlendEnable) {
        return false;
    }
    for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        const D3D11_RENDER_TARGET_BLEND_DESC &x = a.RenderTarget[i];
        const D3D11_RENDER_TARGET_BLEND_DESC &y = b.RenderTarget[i];
        if (x.BlendEnable != y.BlendEnable || x.SrcBlend != y.SrcBlend ||
            x.DestBlend != y.DestBlend || x.BlendOp != y.BlendOp ||
            x.SrcBlendAlpha != y.SrcBlendAlpha ||
            x.DestBlendAlpha != y.DestBlendAlpha ||
            x.BlendOpAlpha != y.BlendOpAlpha ||
            x.RenderTargetWriteMask != y.RenderTargetWriteMask) {
            return false;
        }
    }
    return true;
}

D3D11DrawExecutorStatus
Aggregate(const D3D11DrawExecutorAttachmentResult &color,
          const D3D11DrawExecutorAttachmentResult &zeta)
{
    const bool color_requested =
        color.status != D3D11DrawExecutorStatus::NotRequested;
    const bool zeta_requested =
        zeta.status != D3D11DrawExecutorStatus::NotRequested;
    if (!color_requested && !zeta_requested) {
        return D3D11DrawExecutorStatus::NotRequested;
    }
    if (!zeta_requested) {
        return color.status;
    }
    if (color.status == D3D11DrawExecutorStatus::WrongThread ||
        zeta.status == D3D11DrawExecutorStatus::WrongThread) {
        return D3D11DrawExecutorStatus::WrongThread;
    }
    if (color.status == D3D11DrawExecutorStatus::DeviceError ||
        zeta.status == D3D11DrawExecutorStatus::DeviceError) {
        return D3D11DrawExecutorStatus::DeviceError;
    }
    if (color.status == D3D11DrawExecutorStatus::Drawn &&
        zeta.status == D3D11DrawExecutorStatus::Drawn) {
        return D3D11DrawExecutorStatus::Drawn;
    }
    if (color.status == D3D11DrawExecutorStatus::Drawn ||
        zeta.status == D3D11DrawExecutorStatus::Drawn) {
        return D3D11DrawExecutorStatus::Partial;
    }
    return color.status;
}

} // namespace

D3D11DrawExecutor::D3D11DrawExecutor(D3D11PgraphContext &context)
    : m_pgraph_context(&context), m_device(context.device()),
      m_context(context.context()), m_draw(m_device, m_context),
      m_owner_thread(context.owner_thread_id())
{
}

D3D11DrawExecutorStatus
D3D11DrawExecutor::SurfaceStatus(D3D11SurfaceStatus status) const
{
    switch (status) {
    case D3D11SurfaceStatus::Ok:
        return D3D11DrawExecutorStatus::Drawn;
    case D3D11SurfaceStatus::OutOfRange:
        return D3D11DrawExecutorStatus::OutOfRange;
    case D3D11SurfaceStatus::DeviceLost:
    case D3D11SurfaceStatus::Timeout:
        return D3D11DrawExecutorStatus::DeviceError;
    case D3D11SurfaceStatus::Unsupported:
        return D3D11DrawExecutorStatus::Unsupported;
    default:
        return D3D11DrawExecutorStatus::Invalid;
    }
}

D3D11DrawExecutorStatus
D3D11DrawExecutor::DrawStatus(D3D11DrawStatus status) const
{
    switch (status) {
    case D3D11DrawStatus::Drawn:
        return D3D11DrawExecutorStatus::Drawn;
    case D3D11DrawStatus::Unsupported:
        return D3D11DrawExecutorStatus::Unsupported;
    case D3D11DrawStatus::DeviceError:
        return D3D11DrawExecutorStatus::DeviceError;
    case D3D11DrawStatus::WrongThread:
        return D3D11DrawExecutorStatus::WrongThread;
    default:
        return D3D11DrawExecutorStatus::Invalid;
    }
}

D3D11DrawExecutorAttachmentResult
D3D11DrawExecutor::AttachmentFailure(D3D11DrawExecutorStatus status,
                                     D3D11SurfaceStatus surface) const
{
    D3D11DrawExecutorAttachmentResult result = {};
    result.status = status;
    result.surface = surface;
    return result;
}

bool D3D11DrawExecutor::GetDimensions(const PGRAPHState *pg, uint32_t *width,
                                      uint32_t *height) const
{
    return d3d11_get_surface_dimensions(pg, width, height);
}

bool D3D11DrawExecutor::GetRasterizer(const D3D11_RASTERIZER_DESC &desc,
                                      ID3D11RasterizerState **state)
{
    if (state == nullptr || m_device == nullptr) {
        return false;
    }
    if (!m_have_rasterizer || !SameRasterizer(m_rasterizer_desc, desc)) {
        ComPtr<ID3D11RasterizerState> created;
        if (FAILED(m_device->CreateRasterizerState(&desc,
                                                   created.GetAddressOf()))) {
            return false;
        }
        m_rasterizer = created;
        m_rasterizer_desc = desc;
        m_have_rasterizer = true;
    }
    *state = m_rasterizer.Get();
    return true;
}

bool D3D11DrawExecutor::GetDepthStencil(const D3D11_DEPTH_STENCIL_DESC &desc,
                                        ID3D11DepthStencilState **state)
{
    if (state == nullptr || m_device == nullptr) {
        return false;
    }
    if (!m_have_depth_state || !SameDepth(m_depth_desc, desc)) {
        ComPtr<ID3D11DepthStencilState> created;
        if (FAILED(m_device->CreateDepthStencilState(&desc,
                                                     created.GetAddressOf()))) {
            return false;
        }
        m_depth_state = created;
        m_depth_desc = desc;
        m_have_depth_state = true;
    }
    *state = m_depth_state.Get();
    return true;
}

bool D3D11DrawExecutor::GetBlend(const D3D11_BLEND_DESC &desc,
                                 ID3D11BlendState **state)
{
    if (state == nullptr || m_device == nullptr) {
        return false;
    }
    if (!m_have_blend || !SameBlend(m_blend_desc, desc)) {
        ComPtr<ID3D11BlendState> created;
        if (FAILED(m_device->CreateBlendState(&desc, created.GetAddressOf()))) {
            return false;
        }
        m_blend = created;
        m_blend_desc = desc;
        m_have_blend = true;
    }
    *state = m_blend.Get();
    return true;
}

bool D3D11DrawExecutor::MarkDrawDirty(NV2AState *state, bool color, bool zeta,
                                      bool color_writes, bool depth_writes,
                                      bool stencil_writes)
{
    if (!color_writes && !depth_writes && !stencil_writes) {
        return true;
    }
    D3D11SurfaceSnapshot *color_snapshot = color ? &m_color : nullptr;
    D3D11SurfaceSnapshot *zeta_snapshot = zeta ? &m_depth_stencil : nullptr;
    if (color_snapshot != nullptr && zeta_snapshot != nullptr &&
        *color_snapshot && *zeta_snapshot &&
        color_snapshot->get() == zeta_snapshot->get()) {
        if (!(*color_snapshot)->MarkDrawDirty()) {
            return false;
        }
    } else {
        if (color_writes && color_snapshot != nullptr && *color_snapshot &&
            !(*color_snapshot)->MarkDrawDirty()) {
            return false;
        }
        if ((depth_writes || stencil_writes) && zeta_snapshot != nullptr &&
            *zeta_snapshot && !(*zeta_snapshot)->MarkDrawDirty()) {
            return false;
        }
    }
    if (state != nullptr) {
        if (color_writes) {
            state->pgraph.surface_color.draw_dirty = true;
        }
        if (zeta && (depth_writes || stencil_writes)) {
            state->pgraph.surface_zeta.draw_dirty = true;
        }
    }
    return true;
}

D3D11DrawExecutorResult D3D11DrawExecutor::Execute(NV2AState *state)
{
    D3D11DrawExecutorResult result = {};
    if (GetCurrentThreadId() != m_owner_thread || m_pgraph_context == nullptr ||
        !m_pgraph_context->owner_thread()) {
        result.status = D3D11DrawExecutorStatus::WrongThread;
        result.color.status = D3D11DrawExecutorStatus::WrongThread;
        return result;
    }
    if (state == nullptr || state->vram == nullptr ||
        state->vram_ptr == nullptr || m_device == nullptr ||
        m_context == nullptr || !m_pgraph_context->valid()) {
        result.status =
            m_pgraph_context != nullptr && !m_pgraph_context->valid() ?
                D3D11DrawExecutorStatus::DeviceError :
                D3D11DrawExecutorStatus::Invalid;
        result.color.status = result.status;
        return result;
    }
    const uint32_t vsh_mode = VshMode(&state->pgraph);
    if (vsh_mode != 0 && vsh_mode != 2) {
        result.status = D3D11DrawExecutorStatus::Unsupported;
        result.color.status = result.status;
        return result;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (!GetDimensions(&state->pgraph, &width, &height)) {
        result.status = D3D11DrawExecutorStatus::Invalid;
        result.color.status = result.status;
        return result;
    }

    D3D11VertexPlan vertex_plan;
    if (d3d11_build_vertex_plan(
            state, &state->pgraph, memory_region_size(state->vram),
            &vertex_plan) != D3D11VertexAdapterStatus::Ready) {
        result.color.status = D3D11DrawExecutorStatus::Unsupported;
        result.status = result.color.status;
        return result;
    }
    const bool programmable_vsh = vsh_mode == 2;
    std::array<uint32_t, NV2A_MAX_TRANSFORM_PROGRAM_LENGTH * VSH_TOKEN_SIZE>
        programmable_tokens = {};
    uint32_t programmable_token_count = 0;
    if (programmable_vsh &&
        (!GetProgrammableVsh(&state->pgraph, &programmable_tokens,
                             &programmable_token_count) ||
         !ValidProgrammableTokens(programmable_tokens.data(),
                                  programmable_token_count) ||
         state->pgraph.enable_vertex_program_write ||
         HasContextOutputs(programmable_tokens.data(),
                           programmable_token_count))) {
        /* The CPU fallback has no state writeback boundary.  Reject context
         * output programs before touching the surface cache or dirty bits. */
        result.color.status = D3D11DrawExecutorStatus::Unsupported;
        result.status = result.color.status;
        return result;
    }
    D3D11PgraphDrawState draw_state = {};
    const D3D11PgraphTargetDimensions target = { width, height };
    const D3D11PgraphStateCapabilities capabilities = {
        state->pgraph.surface_scale_factor,
        NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1,
        D3D11ShaderDepthConvention::ZeroToOne,
        D3D11PgraphClipOrigin::BottomLeft,
    };
    if (d3d11_build_draw_state(&state->pgraph, &target, &capabilities,
                               &draw_state) != D3D11PgraphStateStatus::Ready) {
        result.color.status = D3D11DrawExecutorStatus::Unsupported;
        result.status = result.color.status;
        return result;
    }

    std::vector<d3d11_vsh::VshCpuVertexOutput> programmable_vertices;
    if (programmable_vsh) {
        d3d11_vsh::VshCpuTransformOptions options = {};
        options.constants =
            reinterpret_cast<const float (*)[4]>(state->pgraph.vsh_constants);
        options.postprocess = d3d11_vsh::DefaultVshCpuPostprocess();
        options.postprocess.surface_size[0] = static_cast<float>(width);
        options.postprocess.surface_size[1] = static_cast<float>(height);
        /* GLSL derives clipRange from the configured zeta format even when
         * the D3D11 color-only path cannot bind a usable zeta attachment. */
        options.postprocess.clip_range[1] = VshClipRange(&state->pgraph);
        options.serialize_context_writes = false;
        char error[128] = {};
        if (d3d11_vsh::TransformPlan(
                programmable_tokens.data(), programmable_token_count,
                vertex_plan, options, &programmable_vertices, error,
                sizeof(error)) != d3d11_vsh::VshCpuTransformStatus::Ready) {
            result.color.status = D3D11DrawExecutorStatus::Unsupported;
            result.status = result.color.status;
            return result;
        }
    }

    D3D11SurfaceDescriptor color_descriptor = {};
    D3D11SurfaceStatus color_surface_status = D3D11SurfaceStatus::Ok;
    if (!d3d11_build_surface_descriptor(state, true, width, height,
                                        &color_descriptor,
                                        &color_surface_status)) {
        result.color = AttachmentFailure(SurfaceStatus(color_surface_status),
                                         color_surface_status);
        result.status = result.color.status;
        return result;
    }

    /* A zero-sized/unbound zeta is the normal color-only case.  If a zeta is
     * configured, its failure is reported but never prevents color drawing. */
    const bool zeta_requested = state->pgraph.surface_zeta.pitch != 0;
    D3D11SurfaceDescriptor zeta_descriptor = {};
    D3D11SurfaceStatus zeta_surface_status = D3D11SurfaceStatus::Ok;
    bool zeta_usable = false;
    if (zeta_requested && !d3d11_build_surface_descriptor(
                              state, false, width, height, &zeta_descriptor,
                              &zeta_surface_status)) {
        result.depth_stencil = AttachmentFailure(
            SurfaceStatus(zeta_surface_status), zeta_surface_status);
    } else if (!zeta_requested) {
        if (m_depth_stencil && (m_depth_stencil->draw_dirty() ||
                                m_depth_stencil->download_dirty())) {
            m_pgraph_context->Retire(m_depth_stencil,
                                     D3D11SurfaceAttachment::DepthStencil);
        }
        m_depth_stencil = {};
    } else {
        zeta_usable = true;
    }

    if (zeta_usable &&
        d3d11_surface_descriptors_overlap(color_descriptor, zeta_descriptor)) {
        result.color.status = D3D11DrawExecutorStatus::Invalid;
        result.depth_stencil.status = D3D11DrawExecutorStatus::Invalid;
        result.status = D3D11DrawExecutorStatus::Invalid;
        return result;
    }

    if (!zeta_usable && m_depth_stencil) {
        if (m_depth_stencil->draw_dirty() ||
            m_depth_stencil->download_dirty()) {
            m_pgraph_context->Retire(m_depth_stencil,
                                     D3D11SurfaceAttachment::DepthStencil);
        }
        m_depth_stencil = {};
    }

    D3D11SurfaceSnapshot old_color = m_color;
    D3D11SurfaceSnapshot old_zeta = m_depth_stencil;
    D3D11SurfaceSnapshot acquired_color = m_pgraph_context->Acquire(
        color_descriptor, state->vram_ptr, memory_region_size(state->vram),
        D3D11SurfaceAttachment::Color);
    if (!acquired_color) {
        result.color = AttachmentFailure(
            SurfaceStatus(m_pgraph_context->last_operation_status()),
            m_pgraph_context->last_operation_status());
        result.status = result.color.status;
        return result;
    }
    m_color = acquired_color;
    if (old_color && old_color.get() != m_color.get() &&
        (old_color->draw_dirty() || old_color->download_dirty())) {
        m_pgraph_context->Retire(old_color, D3D11SurfaceAttachment::Color);
    }
    if (zeta_usable &&
        result.depth_stencil.status == D3D11DrawExecutorStatus::NotRequested) {
        D3D11SurfaceSnapshot acquired_zeta = m_pgraph_context->Acquire(
            zeta_descriptor, state->vram_ptr, memory_region_size(state->vram),
            D3D11SurfaceAttachment::DepthStencil);
        if (!acquired_zeta) {
            const D3D11SurfaceStatus status =
                m_pgraph_context->last_operation_status();
            result.depth_stencil =
                AttachmentFailure(SurfaceStatus(status), status);
            zeta_usable = false;
            if (old_zeta &&
                (old_zeta->draw_dirty() || old_zeta->download_dirty())) {
                m_pgraph_context->Retire(old_zeta,
                                         D3D11SurfaceAttachment::DepthStencil);
            }
            m_depth_stencil = {};
        } else {
            m_depth_stencil = acquired_zeta;
            if (old_zeta && old_zeta.get() != m_depth_stencil.get() &&
                (old_zeta->draw_dirty() || old_zeta->download_dirty())) {
                m_pgraph_context->Retire(old_zeta,
                                         D3D11SurfaceAttachment::DepthStencil);
            }
        }
    }

    if (m_color->upload_dirty() &&
        !m_pgraph_context->Upload(m_color, state->vram_ptr,
                                  memory_region_size(state->vram))) {
        result.color = AttachmentFailure(
            SurfaceStatus(m_pgraph_context->last_operation_status()),
            m_pgraph_context->last_operation_status());
        result.status = result.color.status;
        return result;
    }
    if (zeta_usable && m_depth_stencil && m_depth_stencil->upload_dirty() &&
        !m_pgraph_context->Upload(m_depth_stencil, state->vram_ptr,
                                  memory_region_size(state->vram))) {
        const D3D11SurfaceStatus status =
            m_pgraph_context->last_operation_status();
        result.depth_stencil = AttachmentFailure(SurfaceStatus(status), status);
        zeta_usable = false;
    }

    ID3D11RasterizerState *rasterizer = nullptr;
    ID3D11DepthStencilState *depth_state = nullptr;
    ID3D11BlendState *blend = nullptr;
    if (!GetRasterizer(draw_state.rasterizer, &rasterizer) ||
        !GetDepthStencil(draw_state.depth_stencil, &depth_state) ||
        !GetBlend(draw_state.blend, &blend)) {
        result.color.status = D3D11DrawExecutorStatus::DeviceError;
        result.status = result.color.status;
        return result;
    }

    static const D3D11_INPUT_ELEMENT_DESC kInput[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(D3D11CanonicalVertex, color)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    static const D3D11_INPUT_ELEMENT_DESC kVshInput[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oPos)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oD0)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oD1)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oB0)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oB1)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "FOG", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oFog)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "PSIZE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oPts)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oT0)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oT1)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oT2)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
          static_cast<UINT>(offsetof(d3d11_vsh::VshCpuVertexOutput, oT3)),
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    const void *vertex_data =
        programmable_vsh ?
            static_cast<const void *>(programmable_vertices.data()) :
            static_cast<const void *>(vertex_plan.vertices.data());
    const size_t vertex_stride = programmable_vsh ?
                                     sizeof(d3d11_vsh::VshCpuVertexOutput) :
                                     sizeof(D3D11CanonicalVertex);
    const D3D11DrawVertexBuffer vertex_buffer = {
        vertex_data, vertex_plan.vertices.size() * vertex_stride,
        static_cast<UINT>(vertex_stride), 0
    };
    const D3D11DrawIndexBuffer index_buffer = { vertex_plan.indices.data(),
                                                vertex_plan.indices.size() };
    D3D11DrawParams params = {};
    params.render_target = m_color->render_target();
    params.depth_stencil = zeta_usable && m_depth_stencil ?
                               m_depth_stencil->depth_stencil() :
                               nullptr;
    params.vertex_shader_bytecode =
        programmable_vsh ? d3d11_vsh_cpu_passthrough::kVertexShader :
                           d3d11_draw_shaders::kVertexShader;
    params.vertex_shader_bytecode_size =
        programmable_vsh ? d3d11_vsh_cpu_passthrough::kVertexShaderSize :
                           d3d11_draw_shaders::kVertexShaderSize;
    params.pixel_shader_bytecode = d3d11_draw_shaders::kPixelShader;
    params.pixel_shader_bytecode_size = d3d11_draw_shaders::kPixelShaderSize;
    params.input_elements = programmable_vsh ? kVshInput : kInput;
    params.input_element_count = programmable_vsh ?
                                     sizeof(kVshInput) / sizeof(kVshInput[0]) :
                                     sizeof(kInput) / sizeof(kInput[0]);
    params.vertex_buffers = &vertex_buffer;
    params.vertex_buffer_count = 1;
    params.index_buffer = index_buffer;
    params.index_count = static_cast<UINT>(vertex_plan.indices.size());
    params.has_viewport = draw_state.has_viewport;
    params.viewport = draw_state.viewport;
    params.has_scissor = draw_state.has_scissor;
    params.scissor = draw_state.scissor;
    params.rasterizer_state = rasterizer;
    params.depth_stencil_state = depth_state;
    params.stencil_ref = draw_state.stencil_ref;
    params.blend_state = blend;
    std::memcpy(params.blend_factor, draw_state.blend_factor,
                sizeof(params.blend_factor));
    params.sample_mask = draw_state.sample_mask;

    const D3D11DrawResult draw_result = m_draw.Draw(params);
    if (draw_result.status == D3D11DrawStatus::DeviceError) {
        m_pgraph_context->MarkTerminalFailure(
            D3D11SurfaceStatus::DeviceLost,
            draw_result.device_removed_reason != S_OK ?
                draw_result.device_removed_reason :
                draw_result.hresult);
    }
    result.color.draw = draw_result;
    result.color.status = DrawStatus(draw_result.status);
    if (result.color.succeeded()) {
        result.color.surface = D3D11SurfaceStatus::Ok;
        const bool color_writes =
            (draw_state.blend.RenderTarget[0].RenderTargetWriteMask &
             D3D11_COLOR_WRITE_ENABLE_ALL) != 0;
        const bool depth_writes = draw_state.depth_stencil.DepthEnable &&
                                  draw_state.depth_stencil.DepthWriteMask !=
                                      D3D11_DEPTH_WRITE_MASK_ZERO;
        const bool stencil_writes =
            draw_state.depth_stencil.StencilEnable &&
            draw_state.depth_stencil.StencilWriteMask != 0;
        if (!MarkDrawDirty(state, true,
                           zeta_usable && static_cast<bool>(m_depth_stencil),
                           color_writes, depth_writes, stencil_writes)) {
            result.color.status = D3D11DrawExecutorStatus::DeviceError;
        }
        if (zeta_usable && m_depth_stencil &&
            result.depth_stencil.status ==
                D3D11DrawExecutorStatus::NotRequested) {
            result.depth_stencil.status = D3D11DrawExecutorStatus::Drawn;
            result.depth_stencil.surface = D3D11SurfaceStatus::Ok;
        }
    }
    result.status = Aggregate(result.color, result.depth_stencil);
    return result;
}

bool D3D11DrawExecutor::FlushAttachment(NV2AState *state, bool color)
{
    D3D11SurfaceSnapshot &snapshot = color ? m_color : m_depth_stencil;
    if (!snapshot || (!snapshot->draw_dirty() && !snapshot->download_dirty())) {
        return true;
    }
    const D3D11SurfaceAttachment attachment =
        color ? D3D11SurfaceAttachment::Color :
                D3D11SurfaceAttachment::DepthStencil;
    if (!m_pgraph_context->Download(snapshot, state->vram_ptr,
                                    memory_region_size(state->vram),
                                    attachment)) {
        return false;
    }
    return true;
}

bool D3D11DrawExecutor::Flush(NV2AState *state, D3D11DrawFlushReport *report)
{
    D3D11DrawFlushReport local = {};
    if (report == nullptr) {
        report = &local;
    }
    *report = {};
    if (GetCurrentThreadId() != m_owner_thread) {
        report->status = D3D11DrawExecutorStatus::WrongThread;
        return false;
    }
    if (state == nullptr || state->vram == nullptr ||
        state->vram_ptr == nullptr || m_pgraph_context == nullptr) {
        report->status = D3D11DrawExecutorStatus::Invalid;
        return false;
    }
    bool retired_ok = false;
    bool color_ok = false;
    bool zeta_ok = false;
    if (m_pgraph_context->valid()) {
        retired_ok = m_pgraph_context->FlushRetired(
            state->vram_ptr, memory_region_size(state->vram));
        if (m_color && m_depth_stencil &&
            m_color.get() == m_depth_stencil.get() &&
            (m_color->draw_dirty() || m_color->download_dirty())) {
            const bool alias_ok = m_pgraph_context->Download(
                m_color, state->vram_ptr, memory_region_size(state->vram),
                D3D11SurfaceAttachment::Color |
                    D3D11SurfaceAttachment::DepthStencil);
            color_ok = alias_ok;
            zeta_ok = alias_ok;
        } else {
            color_ok = FlushAttachment(state, true);
            zeta_ok = FlushAttachment(state, false);
        }
    } else {
        report->status = SurfaceStatus(m_pgraph_context->status());
    }
    const std::vector<D3D11PgraphDownloadEvent> events =
        m_pgraph_context->DrainDownloadEvents(state, m_color, m_depth_stencil);
    for (const D3D11PgraphDownloadEvent &event : events) {
        const bool color = HasSurfaceAttachment(event.attachment,
                                                D3D11SurfaceAttachment::Color);
        const bool depth = HasSurfaceAttachment(
            event.attachment, D3D11SurfaceAttachment::DepthStencil);
        report->ranges.push_back({ event.descriptor.offset, event.size, color,
                                   depth, event.implicit, event.retired,
                                   event.generation, event.descriptor });
        if (event.implicit || event.retired) {
            continue;
        }
        if (color) {
            report->color_downloaded = true;
            report->color_offset = event.descriptor.offset;
            report->color_size = event.size;
        }
        if (depth) {
            report->depth_stencil_downloaded = true;
            report->depth_stencil_offset = event.descriptor.offset;
            report->depth_stencil_size = event.size;
        }
    }
    if (retired_ok && color_ok && zeta_ok) {
        report->status = D3D11DrawExecutorStatus::Drawn;
        return true;
    }
    if (!m_pgraph_context->valid()) {
        report->status = SurfaceStatus(m_pgraph_context->status());
    } else if (!retired_ok) {
        report->status =
            SurfaceStatus(m_pgraph_context->last_operation_status());
    } else if (!color_ok && m_color) {
        report->status = SurfaceStatus(m_color->last_operation_status());
    } else if (!zeta_ok && m_depth_stencil) {
        report->status =
            SurfaceStatus(m_depth_stencil->last_operation_status());
    } else {
        report->status = D3D11DrawExecutorStatus::Invalid;
    }
    return false;
}

} // namespace xemu

#endif // _WIN32
