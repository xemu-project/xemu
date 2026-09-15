//
// xemu NV2A D3D11 PGRAPH context
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef XEMU_NV2A_PGRAPH_D3D11_CONTEXT_H
#define XEMU_NV2A_PGRAPH_D3D11_CONTEXT_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "surface.h"

struct NV2AState;

namespace xemu {

struct D3D11PgraphDownloadEvent {
    D3D11SurfaceDescriptor descriptor;
    uint64_t generation = 0;
    uint64_t size = 0;
    D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown;
    bool implicit = false;
    bool retired = false;
};

/*
 * The PGRAPH D3D11 context is the owner of the one surface cache associated
 * with an immediate D3D11 context.  Device and immediate-context accessors
 * are borrowed: the context keeps no additional COM ownership for them, so
 * callers must keep this object alive while using the returned pointers.
 * SurfaceCache itself retains the native objects needed by its resources.
 *
 * The context is single-threaded.  valid() is false from another thread and
 * after the cache reaches its terminal DeviceLost or Timeout state.  Those
 * states are deliberately sticky; this milestone has no reset/recreate path.
 * Destroy the context after the device has been replaced and construct a new
 * one on the replacement device/context.  Acquire, Upload, Download, and
 * Retire are the owner-wide entry points: overlap and retired downloads are
 * queued here and can be drained exactly once by any cooperating caller.
 */
class D3D11PgraphContext final {
public:
    D3D11PgraphContext(ID3D11Device *device, ID3D11DeviceContext *context,
                       uint32_t gpu_timeout_ms = 5000);
    ~D3D11PgraphContext() = default;

    D3D11PgraphContext(const D3D11PgraphContext &) = delete;
    D3D11PgraphContext &operator=(const D3D11PgraphContext &) = delete;

    ID3D11Device *device() const
    {
        return m_device;
    }
    ID3D11DeviceContext *context() const
    {
        return m_context;
    }
    ID3D11DeviceContext *immediate_context() const
    {
        return context();
    }

    /* Read-only inspection for diagnostics/tests.  Mutating cache operations
     * go through Acquire/Upload/Download/Retire so events remain owner-wide. */
    const D3D11SurfaceCache &surface_cache() const
    {
        return m_surface_cache;
    }

    bool owner_thread() const;
    bool on_owner_thread() const
    {
        return owner_thread();
    }
    uint32_t owner_thread_id() const
    {
        return m_owner_thread;
    }
    bool valid() const;

    /* Resource-mutating operations require a mutable snapshot handle. */
    D3D11SurfaceSnapshot Acquire(
        const D3D11SurfaceDescriptor &descriptor, uint8_t *vram,
        size_t vram_size,
        D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown);
    bool Upload(D3D11SurfaceSnapshot &snapshot, uint8_t *vram,
                size_t vram_size);
    bool Download(
        D3D11SurfaceSnapshot &snapshot, uint8_t *vram, size_t vram_size,
        D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown);
    void
    Retire(D3D11SurfaceSnapshot &snapshot,
           D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown);
    bool FlushRetired(uint8_t *vram, size_t vram_size);
    const std::vector<D3D11PgraphDownloadEvent> &pending_download_events();
    /* The single authoritative owner-thread event drain.  It applies each
     * event's QEMU dirty ranges once and clears PGRAPH dirty state only when
     * the descriptor and generation still match the supplied current
     * bindings from the logical PGRAPH state.  A terminal context may still
     * drain with a valid state; invalid state and wrong-thread calls leave the
     * queue untouched. */
    std::vector<D3D11PgraphDownloadEvent>
    DrainDownloadEvents(NV2AState *state, const D3D11SurfaceSnapshot &color,
                        const D3D11SurfaceSnapshot &depth_stencil);
    /* Compatibility inspection hook; executors must use DrainDownloadEvents.
     */
    std::vector<D3D11PgraphDownloadEvent> TakeDownloadEvents();
    size_t retired_surface_count() const
    {
        return m_retired.size();
    }
    void MarkTerminalFailure(D3D11SurfaceStatus status, HRESULT hresult);

    D3D11SurfaceStatus status() const
    {
        return m_surface_cache.status();
    }
    HRESULT last_hresult() const
    {
        return m_surface_cache.last_hresult();
    }
    D3D11SurfaceStatus last_operation_status() const
    {
        return m_surface_cache.last_operation_status();
    }

private:
    bool DownloadInternal(D3D11SurfaceSnapshot &snapshot, uint8_t *vram,
                          size_t vram_size, D3D11SurfaceAttachment attachment,
                          bool retired);
    void HarvestCacheEvents(
        D3D11SurfaceAttachment fallback = D3D11SurfaceAttachment::Unknown);

    struct RetiredSurface {
        D3D11SurfaceSnapshot snapshot;
        D3D11SurfaceAttachment attachment = D3D11SurfaceAttachment::Unknown;
    };

    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    D3D11SurfaceCache m_surface_cache;
    std::vector<D3D11PgraphDownloadEvent> m_download_events;
    std::vector<RetiredSurface> m_retired;
    uint32_t m_owner_thread = 0;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_CONTEXT_H
