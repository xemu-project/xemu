// xemu NV2A D3D11 draw executor

#ifndef XEMU_NV2A_PGRAPH_D3D11_DRAW_EXECUTOR_H
#define XEMU_NV2A_PGRAPH_D3D11_DRAW_EXECUTOR_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <cstdint>
#include <vector>

#include <wrl/client.h>

#include "context.h"
#include "draw.h"
#include "state-adapter.h"
#include "surface-adapter.h"
#include "vertex-adapter.h"
#include "vsh_cpu_transform.h"

struct NV2AState;

namespace xemu {

enum class D3D11DrawExecutorStatus : uint8_t {
    NotRequested,
    Drawn,
    Partial,
    Unsupported,
    Invalid,
    OutOfRange,
    DeviceError,
    WrongThread,
};

struct D3D11DrawExecutorAttachmentResult {
    D3D11DrawExecutorStatus status = D3D11DrawExecutorStatus::NotRequested;
    D3D11DrawResult draw = {};
    D3D11SurfaceStatus surface = D3D11SurfaceStatus::NotFound;

    bool succeeded() const
    {
        return status == D3D11DrawExecutorStatus::Drawn;
    }
};

struct D3D11DrawExecutorResult {
    D3D11DrawExecutorStatus status = D3D11DrawExecutorStatus::Invalid;
    D3D11DrawExecutorAttachmentResult color = {};
    D3D11DrawExecutorAttachmentResult depth_stencil = {};

    bool succeeded() const
    {
        return status == D3D11DrawExecutorStatus::Drawn;
    }
};

struct D3D11DrawFlushRange {
    uint64_t offset = 0;
    uint64_t size = 0;
    bool color = false;
    bool depth_stencil = false;
    bool implicit = false;
    bool retired = false;
    uint64_t generation = 0;
    D3D11SurfaceDescriptor descriptor = {};
};

struct D3D11DrawFlushReport {
    D3D11DrawExecutorStatus status = D3D11DrawExecutorStatus::Invalid;
    std::vector<D3D11DrawFlushRange> ranges;
    bool color_downloaded = false;
    bool depth_stencil_downloaded = false;
    uint64_t color_offset = 0;
    uint64_t color_size = 0;
    uint64_t depth_stencil_offset = 0;
    uint64_t depth_stencil_size = 0;
};

/* Executes the supported PGRAPH triangle-list subset using one owner-wide
 * D3D11PgraphContext.  The context-taking constructor is intentional: this
 * executor never creates a second surface cache.  Flush is the owner-thread
 * boundary for the context's authoritative download-event drain: every
 * queued successful download is applied to QEMU dirty tracking exactly once,
 * and PGRAPH dirty bits are cleared only for the current matching generation.
 * A terminal operation still drains events when the caller supplies a valid
 * owner-thread state; invalid state and wrong-thread calls preserve the queue.
 */
class D3D11DrawExecutor final {
public:
    explicit D3D11DrawExecutor(D3D11PgraphContext &context);
    ~D3D11DrawExecutor() = default;

    D3D11DrawExecutor(const D3D11DrawExecutor &) = delete;
    D3D11DrawExecutor &operator=(const D3D11DrawExecutor &) = delete;

    D3D11DrawExecutorResult Execute(NV2AState *state);
    bool Flush(NV2AState *state, D3D11DrawFlushReport *report = nullptr);

    const D3D11SurfaceSnapshot &color_surface() const
    {
        return m_color;
    }
    const D3D11SurfaceSnapshot &depth_stencil_surface() const
    {
        return m_depth_stencil;
    }
    D3D11PgraphContext *pgraph_context() const
    {
        return m_pgraph_context;
    }

private:
    D3D11DrawExecutorStatus SurfaceStatus(D3D11SurfaceStatus status) const;
    D3D11DrawExecutorStatus DrawStatus(D3D11DrawStatus status) const;
    D3D11DrawExecutorAttachmentResult
    AttachmentFailure(D3D11DrawExecutorStatus status,
                      D3D11SurfaceStatus surface) const;
    bool GetDimensions(const PGRAPHState *pg, uint32_t *width,
                       uint32_t *height) const;
    bool GetRasterizer(const D3D11_RASTERIZER_DESC &desc,
                       ID3D11RasterizerState **state);
    bool GetDepthStencil(const D3D11_DEPTH_STENCIL_DESC &desc,
                         ID3D11DepthStencilState **state);
    bool GetBlend(const D3D11_BLEND_DESC &desc, ID3D11BlendState **state);
    bool MarkDrawDirty(NV2AState *state, bool color, bool zeta,
                       bool color_writes, bool depth_writes,
                       bool stencil_writes);
    bool FlushAttachment(NV2AState *state, bool color);

    D3D11PgraphContext *m_pgraph_context = nullptr;
    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    D3D11DrawPrimitive m_draw;
    D3D11SurfaceSnapshot m_color;
    D3D11SurfaceSnapshot m_depth_stencil;
    uint32_t m_owner_thread = 0;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_rasterizer;
    D3D11_RASTERIZER_DESC m_rasterizer_desc = {};
    bool m_have_rasterizer = false;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depth_state;
    D3D11_DEPTH_STENCIL_DESC m_depth_desc = {};
    bool m_have_depth_state = false;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blend;
    D3D11_BLEND_DESC m_blend_desc = {};
    bool m_have_blend = false;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_DRAW_EXECUTOR_H
