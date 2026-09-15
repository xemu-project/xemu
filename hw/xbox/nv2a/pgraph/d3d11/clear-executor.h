//
// xemu NV2A D3D11 CLEAR_SURFACE executor
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef XEMU_NV2A_PGRAPH_D3D11_CLEAR_EXECUTOR_H
#define XEMU_NV2A_PGRAPH_D3D11_CLEAR_EXECUTOR_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "clear-adapter.h"
#include "context.h"
#include "surface-adapter.h"
#include "surface.h"

struct NV2AState;

namespace xemu {

enum class D3D11ClearExecutorStatus : uint8_t {
    NotRequested,
    Cleared,
    Partial,
    Unsupported,
    Invalid,
    OutOfRange,
    DeviceError,
    WrongThread,
};

struct D3D11ClearAttachmentResult {
    D3D11ClearExecutorStatus status = D3D11ClearExecutorStatus::NotRequested;
    D3D11ClearResult clear = {};
    D3D11SurfaceStatus surface = D3D11SurfaceStatus::NotFound;

    bool succeeded() const
    {
        return status == D3D11ClearExecutorStatus::Cleared;
    }
};

struct D3D11ClearFlushRange {
    /* One successful D3D11-to-VRAM write.  The vector is authoritative; the
     * legacy per-attachment fields below are compatibility summaries. */
    uint64_t offset = 0;
    uint64_t size = 0;
    bool color = false;
    bool depth_stencil = false;
    bool implicit = false;
    bool retired = false;
    uint64_t generation = 0;
    D3D11SurfaceDescriptor descriptor = {};
};

struct D3D11ClearExecutorResult {
    D3D11ClearExecutorStatus status = D3D11ClearExecutorStatus::Invalid;
    D3D11ClearAttachmentResult color = {};
    D3D11ClearAttachmentResult depth_stencil = {};

    bool succeeded() const
    {
        return status == D3D11ClearExecutorStatus::Cleared;
    }
};

struct D3D11ClearFlushReport {
    D3D11ClearExecutorStatus status = D3D11ClearExecutorStatus::Invalid;
    std::vector<D3D11ClearFlushRange> ranges;
    bool color_downloaded = false;
    bool depth_stencil_downloaded = false;
    bool implicit_color_downloaded = false;
    bool implicit_depth_stencil_downloaded = false;
    bool retired_downloaded = false;
    uint64_t color_offset = 0;
    uint64_t color_size = 0;
    uint64_t depth_stencil_offset = 0;
    uint64_t depth_stencil_size = 0;
    uint64_t implicit_color_offset = 0;
    uint64_t implicit_color_size = 0;
    uint64_t implicit_depth_stencil_offset = 0;
    uint64_t implicit_depth_stencil_size = 0;
    uint64_t retired_offset = 0;
    uint64_t retired_size = 0;
};

/**
 * Executes the supported, linear-pitch NV2A CLEAR_SURFACE subset against a
 * D3D11 surface cache.  The context-taking constructor borrows the context's
 * device, immediate context, and cache; it does not create another cache.
 * The legacy device/context constructor creates an owned context fallback for
 * source compatibility.  This class is deliberately an internal adapter: it
 * does not register a PGRAPH renderer and does not install memory callbacks.
 * The caller owns serialization with the immediate D3D11 context.
 */
class D3D11ClearExecutor final {
public:
    explicit D3D11ClearExecutor(D3D11PgraphContext &context);
    D3D11ClearExecutor(ID3D11Device *device, ID3D11DeviceContext *context,
                       uint32_t gpu_timeout_ms = 5000);
    ~D3D11ClearExecutor() = default;

    D3D11ClearExecutor(const D3D11ClearExecutor &) = delete;
    D3D11ClearExecutor &operator=(const D3D11ClearExecutor &) = delete;

    D3D11ClearExecutorResult Execute(NV2AState *state, uint32_t parameter);

    /* Download only resources made dirty by a successful clear.  A failed
     * color download does not prevent the zeta download from being attempted
     * and vice versa.  On the owner thread, every queued successful download
     * is then drained through the shared context path, including when an
     * attachment or retired download made the context terminal. */
    bool Flush(NV2AState *state, D3D11ClearFlushReport *report = nullptr);

    const D3D11SurfaceSnapshot &color_surface() const
    {
        return m_color;
    }
    const D3D11SurfaceSnapshot &depth_stencil_surface() const
    {
        return m_depth_stencil;
    }
    size_t retired_surface_count() const
    {
        return m_pgraph_context != nullptr ?
                   m_pgraph_context->retired_surface_count() :
                   0;
    }

    D3D11PgraphContext *pgraph_context() const
    {
        return m_pgraph_context;
    }

private:
    D3D11ClearAttachmentResult
    ExecuteAttachment(NV2AState *state, bool color, const D3D11ClearPlan &plan,
                      const D3D11SurfaceDescriptor &descriptor);
    void AppendEvent(D3D11ClearFlushReport *report,
                     const D3D11PgraphDownloadEvent &event) const;
    bool FlushAttachment(NV2AState *state, bool color,
                         D3D11ClearFlushReport *report);

    struct RetiredSurface {
        D3D11SurfaceSnapshot snapshot;
        bool color = false;
    };

    /* Declared first so the legacy fallback outlives all borrowed members and
     * the clear/snapshot objects that may retain native context references. */
    std::unique_ptr<D3D11PgraphContext> m_owned_context;
    D3D11PgraphContext *m_pgraph_context = nullptr;
    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    D3D11ClearPrimitive m_clear;
    D3D11SurfaceSnapshot m_color;
    D3D11SurfaceSnapshot m_depth_stencil;
    uint32_t m_owner_thread = 0;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_CLEAR_EXECUTOR_H
