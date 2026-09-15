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

#include "context.h"

#ifdef _WIN32

#include "../../nv2a_int.h"
#include "surface-adapter.h"

#include <windows.h>

namespace xemu {

D3D11PgraphContext::D3D11PgraphContext(ID3D11Device *device,
                                       ID3D11DeviceContext *context,
                                       uint32_t gpu_timeout_ms)
    : m_device(device), m_context(context),
      m_surface_cache(device, context, gpu_timeout_ms),
      m_owner_thread(GetCurrentThreadId())
{
}

bool D3D11PgraphContext::owner_thread() const
{
    return GetCurrentThreadId() == m_owner_thread;
}

bool D3D11PgraphContext::valid() const
{
    return owner_thread() && m_device != nullptr && m_context != nullptr &&
           m_surface_cache.valid();
}

void D3D11PgraphContext::HarvestCacheEvents(D3D11SurfaceAttachment fallback)
{
    if (!owner_thread()) {
        return;
    }
    std::vector<D3D11SurfaceDownloadEvent> events =
        m_surface_cache.TakeDownloadEvents();
    for (const D3D11SurfaceDownloadEvent &event : events) {
        bool retired = false;
        D3D11SurfaceAttachment attachment_hint = event.attachment;
        for (auto entry = m_retired.begin(); entry != m_retired.end();) {
            if (entry->snapshot &&
                entry->snapshot->descriptor() == event.descriptor &&
                entry->snapshot->generation() == event.generation) {
                retired = true;
                attachment_hint |= entry->attachment;
                entry = m_retired.erase(entry);
                break;
            }
            ++entry;
        }
        const D3D11SurfaceAttachment attachment =
            attachment_hint == D3D11SurfaceAttachment::Unknown ?
                fallback :
                attachment_hint;
        m_download_events.push_back({ event.descriptor, event.generation,
                                      event.size, attachment, true, retired });
    }
}

D3D11SurfaceSnapshot
D3D11PgraphContext::Acquire(const D3D11SurfaceDescriptor &descriptor,
                            uint8_t *vram, size_t vram_size,
                            D3D11SurfaceAttachment attachment)
{
    if (!owner_thread()) {
        return {};
    }
    D3D11SurfaceSnapshot snapshot =
        m_surface_cache.Acquire(descriptor, vram, vram_size, attachment);
    HarvestCacheEvents(attachment);
    return snapshot;
}

bool D3D11PgraphContext::Upload(D3D11SurfaceSnapshot &snapshot, uint8_t *vram,
                                size_t vram_size)
{
    if (!owner_thread() || !valid() || !snapshot) {
        return false;
    }
    if (!snapshot->Upload(vram, vram_size)) {
        if (snapshot->status() == D3D11SurfaceStatus::DeviceLost ||
            snapshot->status() == D3D11SurfaceStatus::Timeout) {
            MarkTerminalFailure(snapshot->status(), snapshot->last_hresult());
        } else {
            m_surface_cache.RecordOperationFailure(
                snapshot->last_operation_status(),
                snapshot->last_operation_hresult());
        }
        return false;
    }
    return true;
}

bool D3D11PgraphContext::DownloadInternal(D3D11SurfaceSnapshot &snapshot,
                                          uint8_t *vram, size_t vram_size,
                                          D3D11SurfaceAttachment attachment,
                                          bool retired)
{
    if (!owner_thread() || !valid() || !snapshot) {
        return false;
    }
    if (!snapshot->Download(vram, vram_size)) {
        if (snapshot->status() == D3D11SurfaceStatus::DeviceLost ||
            snapshot->status() == D3D11SurfaceStatus::Timeout) {
            MarkTerminalFailure(snapshot->status(), snapshot->last_hresult());
        } else {
            m_surface_cache.RecordOperationFailure(
                snapshot->last_operation_status(),
                snapshot->last_operation_hresult());
        }
        return false;
    }
    const D3D11SurfaceDescriptor &descriptor = snapshot->descriptor();
    m_download_events.push_back({ descriptor, snapshot->generation(),
                                  snapshot->byte_end() - descriptor.offset,
                                  attachment, false, retired });
    return true;
}

bool D3D11PgraphContext::Download(D3D11SurfaceSnapshot &snapshot, uint8_t *vram,
                                  size_t vram_size,
                                  D3D11SurfaceAttachment attachment)
{
    return DownloadInternal(snapshot, vram, vram_size, attachment, false);
}

void D3D11PgraphContext::Retire(D3D11SurfaceSnapshot &snapshot,
                                D3D11SurfaceAttachment attachment)
{
    if (!snapshot || !owner_thread() || !valid() ||
        (!snapshot->draw_dirty() && !snapshot->download_dirty())) {
        return;
    }
    for (RetiredSurface &entry : m_retired) {
        if (entry.snapshot.get() == snapshot.get()) {
            entry.attachment |= attachment;
            return;
        }
    }
    m_retired.push_back({ snapshot, attachment });
}

bool D3D11PgraphContext::FlushRetired(uint8_t *vram, size_t vram_size)
{
    if (!owner_thread() || !valid()) {
        return false;
    }
    HarvestCacheEvents();
    for (auto entry = m_retired.begin(); entry != m_retired.end();) {
        if (!entry->snapshot) {
            entry = m_retired.erase(entry);
            continue;
        }
        if (entry->snapshot->status() == D3D11SurfaceStatus::DeviceLost ||
            entry->snapshot->status() == D3D11SurfaceStatus::Timeout) {
            MarkTerminalFailure(entry->snapshot->status(),
                                entry->snapshot->last_hresult());
            return false;
        }
        if (entry->snapshot->draw_dirty() ||
            entry->snapshot->download_dirty()) {
            if (!DownloadInternal(entry->snapshot, vram, vram_size,
                                  entry->attachment, true)) {
                return false;
            }
        }
        entry = m_retired.erase(entry);
    }
    return true;
}

const std::vector<D3D11PgraphDownloadEvent> &
D3D11PgraphContext::pending_download_events()
{
    static const std::vector<D3D11PgraphDownloadEvent> empty_events;
    if (!owner_thread()) {
        return empty_events;
    }
    HarvestCacheEvents();
    return m_download_events;
}

std::vector<D3D11PgraphDownloadEvent> D3D11PgraphContext::DrainDownloadEvents(
    NV2AState *state, const D3D11SurfaceSnapshot &color,
    const D3D11SurfaceSnapshot &depth_stencil)
{
    if (!owner_thread() || state == nullptr || state->vram == nullptr ||
        state->vram_ptr == nullptr) {
        return {};
    }
    HarvestCacheEvents();
    std::vector<D3D11PgraphDownloadEvent> events;
    events.swap(m_download_events);
    for (const D3D11PgraphDownloadEvent &event : events) {
        if (event.size != 0) {
            const hwaddr range = static_cast<hwaddr>(event.size);
            memory_region_set_client_dirty(state->vram, event.descriptor.offset,
                                           range, DIRTY_MEMORY_VGA);
            memory_region_set_client_dirty(state->vram, event.descriptor.offset,
                                           range, DIRTY_MEMORY_NV2A_TEX);
        }
        uint32_t width = 0;
        uint32_t height = 0;
        const bool have_dimensions =
            d3d11_get_surface_dimensions(&state->pgraph, &width, &height);
        D3D11SurfaceDescriptor color_descriptor = {};
        D3D11SurfaceDescriptor zeta_descriptor = {};
        D3D11SurfaceStatus color_status = D3D11SurfaceStatus::InvalidDescriptor;
        D3D11SurfaceStatus zeta_status = D3D11SurfaceStatus::InvalidDescriptor;
        const bool have_color =
            have_dimensions &&
            d3d11_build_surface_descriptor(state, true, width, height,
                                           &color_descriptor, &color_status);
        const bool have_zeta =
            have_dimensions &&
            d3d11_build_surface_descriptor(state, false, width, height,
                                           &zeta_descriptor, &zeta_status);
        const bool logical_color =
            have_color && color_descriptor == event.descriptor &&
            HasSurfaceAttachment(event.attachment,
                                 D3D11SurfaceAttachment::Color);
        const bool logical_zeta =
            have_zeta && zeta_descriptor == event.descriptor &&
            HasSurfaceAttachment(event.attachment,
                                 D3D11SurfaceAttachment::DepthStencil);
        const D3D11SurfaceSnapshot current =
            m_surface_cache.FindForInspection(event.descriptor);
        const bool current_generation =
            current && current.generation() == event.generation &&
            !current->draw_dirty() && !current->download_dirty();
        if (logical_color && (current_generation || !current) &&
            (current_generation || !color ||
             (color->descriptor() == event.descriptor &&
              color.generation() == event.generation))) {
            state->pgraph.surface_color.draw_dirty = false;
        }
        if (logical_zeta && (current_generation || !current) &&
            (current_generation || !depth_stencil ||
             (depth_stencil->descriptor() == event.descriptor &&
              depth_stencil.generation() == event.generation))) {
            state->pgraph.surface_zeta.draw_dirty = false;
        }
    }
    return events;
}

std::vector<D3D11PgraphDownloadEvent> D3D11PgraphContext::TakeDownloadEvents()
{
    if (!owner_thread()) {
        return {};
    }
    HarvestCacheEvents();
    std::vector<D3D11PgraphDownloadEvent> events;
    events.swap(m_download_events);
    return events;
}

void D3D11PgraphContext::MarkTerminalFailure(D3D11SurfaceStatus status,
                                             HRESULT hresult)
{
    if (!owner_thread()) {
        return;
    }
    if (status == D3D11SurfaceStatus::DeviceLost ||
        status == D3D11SurfaceStatus::Timeout) {
        m_surface_cache.MarkTerminalFailure(status, hresult);
    }
}

} // namespace xemu

#endif // _WIN32
