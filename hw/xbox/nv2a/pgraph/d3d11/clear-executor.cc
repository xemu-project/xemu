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

#include "clear-executor.h"

#ifdef _WIN32

#include <windows.h>


namespace xemu {

namespace {

constexpr uint32_t kColorBit = D3D11_BRIDGE_CLEAR_COLOR;
constexpr uint32_t kZetaBits =
    D3D11_BRIDGE_CLEAR_Z | D3D11_BRIDGE_CLEAR_STENCIL;
constexpr uint32_t kKnownBits = kColorBit | kZetaBits;

D3D11ClearExecutorStatus SurfaceStatusToExecutor(D3D11SurfaceStatus status)
{
    switch (status) {
    case D3D11SurfaceStatus::Ok:
        return D3D11ClearExecutorStatus::Cleared;
    case D3D11SurfaceStatus::OutOfRange:
        return D3D11ClearExecutorStatus::OutOfRange;
    case D3D11SurfaceStatus::DeviceLost:
    case D3D11SurfaceStatus::Timeout:
        return D3D11ClearExecutorStatus::DeviceError;
    case D3D11SurfaceStatus::Unsupported:
        return D3D11ClearExecutorStatus::Unsupported;
    case D3D11SurfaceStatus::InvalidDevice:
    case D3D11SurfaceStatus::InvalidDescriptor:
    case D3D11SurfaceStatus::Conflict:
    case D3D11SurfaceStatus::NotFound:
        return D3D11ClearExecutorStatus::Invalid;
    }
    return D3D11ClearExecutorStatus::Invalid;
}

D3D11ClearExecutorStatus ClearStatusToExecutor(D3D11ClearStatus status)
{
    switch (status) {
    case D3D11ClearStatus::Cleared:
        return D3D11ClearExecutorStatus::Cleared;
    case D3D11ClearStatus::Unsupported:
        return D3D11ClearExecutorStatus::Unsupported;
    case D3D11ClearStatus::DeviceError:
        return D3D11ClearExecutorStatus::DeviceError;
    case D3D11ClearStatus::Invalid:
        return D3D11ClearExecutorStatus::Invalid;
    }
    return D3D11ClearExecutorStatus::Invalid;
}

D3D11ClearExecutorStatus PlanStatusToExecutor(D3D11ClearPlanStatus status)
{
    switch (status) {
    case D3D11ClearPlanStatus::Ready:
        return D3D11ClearExecutorStatus::Cleared;
    case D3D11ClearPlanStatus::Unsupported:
        return D3D11ClearExecutorStatus::Unsupported;
    case D3D11ClearPlanStatus::Invalid:
        return D3D11ClearExecutorStatus::Invalid;
    case D3D11ClearPlanStatus::NotRequested:
        return D3D11ClearExecutorStatus::NotRequested;
    }
    return D3D11ClearExecutorStatus::Invalid;
}

D3D11ClearExecutorStatus
AggregateStatus(const D3D11ClearAttachmentResult &color,
                const D3D11ClearAttachmentResult &depth_stencil)
{
    const bool color_requested =
        color.status != D3D11ClearExecutorStatus::NotRequested;
    const bool zeta_requested =
        depth_stencil.status != D3D11ClearExecutorStatus::NotRequested;
    const bool color_ok = color.status == D3D11ClearExecutorStatus::Cleared;
    const bool zeta_ok =
        depth_stencil.status == D3D11ClearExecutorStatus::Cleared;

    if (!color_requested && !zeta_requested) {
        return D3D11ClearExecutorStatus::NotRequested;
    }
    if (!color_requested || !zeta_requested) {
        return color_requested ? color.status : depth_stencil.status;
    }
    if (color.status == D3D11ClearExecutorStatus::WrongThread ||
        depth_stencil.status == D3D11ClearExecutorStatus::WrongThread) {
        return D3D11ClearExecutorStatus::WrongThread;
    }
    if (color.status == D3D11ClearExecutorStatus::DeviceError ||
        depth_stencil.status == D3D11ClearExecutorStatus::DeviceError) {
        return D3D11ClearExecutorStatus::DeviceError;
    }
    if (color_ok && zeta_ok) {
        return D3D11ClearExecutorStatus::Cleared;
    }
    if (color_ok || zeta_ok) {
        return D3D11ClearExecutorStatus::Partial;
    }
    return color.status;
}

void SetAttachmentError(D3D11ClearAttachmentResult *result,
                        D3D11ClearExecutorStatus status)
{
    result->status = status;
}

bool CheckedDimensions(const PGRAPHState *pg, uint32_t *width, uint32_t *height)
{
    return d3d11_get_surface_dimensions(pg, width, height);
}

bool Vram(const NV2AState *state, uint8_t **data, size_t *size)
{
    uint64_t bridge_size = 0;
    if (!d3d11_bridge_vram_view(state, data, &bridge_size) ||
        bridge_size > SIZE_MAX) {
        return false;
    }
    *size = static_cast<size_t>(bridge_size);
    return true;
}

} // namespace

D3D11ClearExecutor::D3D11ClearExecutor(D3D11PgraphContext &context)
    : m_pgraph_context(&context), m_device(context.device()),
      m_context(context.context()), m_clear(m_device, m_context),
      m_owner_thread(context.owner_thread_id())
{
}

D3D11ClearExecutor::D3D11ClearExecutor(ID3D11Device *device,
                                       ID3D11DeviceContext *context,
                                       uint32_t gpu_timeout_ms)
    : m_owned_context(std::make_unique<D3D11PgraphContext>(device, context,
                                                           gpu_timeout_ms)),
      m_pgraph_context(m_owned_context.get()),
      m_device(m_pgraph_context->device()),
      m_context(m_pgraph_context->context()), m_clear(m_device, m_context),
      m_owner_thread(m_pgraph_context->owner_thread_id())
{
}

void D3D11ClearExecutor::AppendEvent(
    D3D11ClearFlushReport *report, const D3D11PgraphDownloadEvent &event) const
{
    if (report == nullptr) {
        return;
    }
    const bool color =
        HasSurfaceAttachment(event.attachment, D3D11SurfaceAttachment::Color);
    const bool depth = HasSurfaceAttachment(
        event.attachment, D3D11SurfaceAttachment::DepthStencil);
    const D3D11ClearFlushRange record = {
        event.descriptor.offset,
        event.size,
        color,
        depth,
        event.implicit,
        event.retired,
        event.generation,
        event.descriptor,
    };
    report->ranges.push_back(record);
    if (event.implicit) {
        if (color) {
            report->implicit_color_downloaded = true;
            report->implicit_color_offset = event.descriptor.offset;
            report->implicit_color_size = event.size;
        }
        if (depth) {
            report->implicit_depth_stencil_downloaded = true;
            report->implicit_depth_stencil_offset = event.descriptor.offset;
            report->implicit_depth_stencil_size = event.size;
        }
    } else if (!event.retired) {
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
    if (event.retired) {
        report->retired_downloaded = true;
        report->retired_offset = event.descriptor.offset;
        report->retired_size = event.size;
    }
}

D3D11ClearAttachmentResult
D3D11ClearExecutor::ExecuteAttachment(NV2AState *state, bool color,
                                      const D3D11ClearPlan &plan,
                                      const D3D11SurfaceDescriptor &descriptor)
{
    D3D11ClearAttachmentResult result = {};
    result.status = D3D11ClearExecutorStatus::Invalid;
    D3D11SurfaceSnapshot previous = color ? m_color : m_depth_stencil;
    const D3D11SurfaceAttachment attachment =
        color ? D3D11SurfaceAttachment::Color :
                D3D11SurfaceAttachment::DepthStencil;
    uint8_t *vram = nullptr;
    size_t vram_size = 0;
    if (!Vram(state, &vram, &vram_size)) {
        return result;
    }
    D3D11SurfaceSnapshot snapshot =
        m_pgraph_context->Acquire(descriptor, vram, vram_size, attachment);
    if (!snapshot) {
        result.surface = m_pgraph_context->last_operation_status();
        result.status = SurfaceStatusToExecutor(result.surface);
        return result;
    }
    if (previous && previous.get() != snapshot.get() &&
        (previous->draw_dirty() || previous->download_dirty())) {
        m_pgraph_context->Retire(previous, attachment);
    }
    if (color) {
        m_color = snapshot;
    } else {
        m_depth_stencil = snapshot;
    }
    result.surface = D3D11SurfaceStatus::Ok;
    D3D11SurfaceResource *resource = snapshot.get();
    if (resource->upload_dirty() &&
        !m_pgraph_context->Upload(snapshot, vram, vram_size)) {
        result.surface = !m_pgraph_context->valid() ?
                             m_pgraph_context->status() :
                             resource->last_operation_status();
        result.status = SurfaceStatusToExecutor(result.surface);
        return result;
    }

    result.clear = m_clear.Clear(color ? plan.color : plan.depth_stencil,
                                 color ? resource->render_target() : nullptr,
                                 color ? nullptr : resource->depth_stencil());
    result.status = ClearStatusToExecutor(result.clear.status);
    if (!result.clear.succeeded()) {
        if (result.clear.status == D3D11ClearStatus::DeviceError) {
            m_pgraph_context->MarkTerminalFailure(
                D3D11SurfaceStatus::DeviceLost, result.clear.hresult);
        }
        return result;
    }
    if (!resource->MarkDrawDirty()) {
        if (resource->status() == D3D11SurfaceStatus::DeviceLost ||
            resource->status() == D3D11SurfaceStatus::Timeout) {
            m_pgraph_context->MarkTerminalFailure(resource->status(),
                                                  resource->last_hresult());
        }
        result.surface = resource->last_operation_status();
        result.status = SurfaceStatusToExecutor(result.surface);
        return result;
    }
    d3d11_bridge_mark_surface_draw_dirty(state, color, true);
    result.surface = D3D11SurfaceStatus::Ok;
    result.status = D3D11ClearExecutorStatus::Cleared;
    return result;
}

D3D11ClearExecutorResult D3D11ClearExecutor::Execute(NV2AState *state,
                                                     uint32_t parameter)
{
    D3D11ClearExecutorResult result = {};
    if (GetCurrentThreadId() != m_owner_thread) {
        result.status = D3D11ClearExecutorStatus::WrongThread;
        if (parameter & kColorBit) {
            result.color.status = D3D11ClearExecutorStatus::WrongThread;
        }
        if (parameter & kZetaBits) {
            result.depth_stencil.status = D3D11ClearExecutorStatus::WrongThread;
        }
        return result;
    }
    uint8_t *vram = nullptr;
    size_t vram_size = 0;
    const PGRAPHState *pg = d3d11_bridge_pgraph(state);
    D3D11BridgePgraphSnapshot pg_snapshot = {};
    if (state == nullptr || !Vram(state, &vram, &vram_size) ||
        !d3d11_bridge_snapshot_pgraph(pg, &pg_snapshot) ||
        m_device == nullptr || m_context == nullptr ||
        m_pgraph_context == nullptr) {
        result.status = D3D11ClearExecutorStatus::Invalid;
        return result;
    }
    if ((parameter & ~kKnownBits) != 0) {
        result.status = D3D11ClearExecutorStatus::Unsupported;
        if (parameter & kColorBit) {
            result.color.status = D3D11ClearExecutorStatus::Unsupported;
        }
        if (parameter & kZetaBits) {
            result.depth_stencil.status = D3D11ClearExecutorStatus::Unsupported;
        }
        return result;
    }
    if ((parameter & kKnownBits) == 0) {
        result.status = D3D11ClearExecutorStatus::NotRequested;
        return result;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (!CheckedDimensions(pg, &width, &height)) {
        result.status = D3D11ClearExecutorStatus::Invalid;
        if (parameter & kColorBit) {
            result.color.status = D3D11ClearExecutorStatus::Invalid;
        }
        if (parameter & kZetaBits) {
            result.depth_stencil.status = D3D11ClearExecutorStatus::Invalid;
        }
        return result;
    }

    D3D11ClearCapabilities capabilities = {
        pg_snapshot.surface_scale_factor,
    };
    D3D11ClearBindingDimensions bindings = {
        (parameter & kColorBit) != 0, width, height,
        (parameter & kZetaBits) != 0, width, height,
    };

    D3D11ClearPlan color_plan = {};
    D3D11ClearPlan zeta_plan = {};
    D3D11SurfaceDescriptor color_descriptor = {};
    D3D11SurfaceDescriptor zeta_descriptor = {};
    D3D11SurfaceStatus color_surface_status = D3D11SurfaceStatus::Ok;
    D3D11SurfaceStatus zeta_surface_status = D3D11SurfaceStatus::Ok;
    const bool color_plan_ready =
        !(parameter & kColorBit) ||
        d3d11_build_clear_plan(pg, parameter & kColorBit, &bindings,
                               &capabilities,
                               &color_plan) == D3D11ClearPlanStatus::Ready;
    const bool zeta_plan_ready =
        !(parameter & kZetaBits) ||
        d3d11_build_clear_plan(pg, parameter & kZetaBits, &bindings,
                               &capabilities,
                               &zeta_plan) == D3D11ClearPlanStatus::Ready;
    const bool color_descriptor_ready =
        !(parameter & kColorBit) ||
        d3d11_build_surface_descriptor(state, true, width, height,
                                       &color_descriptor,
                                       &color_surface_status);
    const bool zeta_descriptor_ready =
        !(parameter & kZetaBits) ||
        d3d11_build_surface_descriptor(state, false, width, height,
                                       &zeta_descriptor, &zeta_surface_status);

    /* Both descriptors are validated before either attachment can acquire or
     * retire a cache resource.  An overlap is never representable as two
     * independent D3D11 views, including exact aliasing. */
    if ((parameter & kColorBit) && (parameter & kZetaBits) &&
        color_descriptor_ready && zeta_descriptor_ready &&
        d3d11_surface_descriptors_overlap(color_descriptor, zeta_descriptor)) {
        result.color.status = D3D11ClearExecutorStatus::Invalid;
        result.depth_stencil.status = D3D11ClearExecutorStatus::Invalid;
        result.status = D3D11ClearExecutorStatus::Invalid;
        return result;
    }

    if (parameter & kColorBit) {
        if (!color_plan_ready) {
            D3D11ClearPlan ignored = {};
            const D3D11ClearPlanStatus status = d3d11_build_clear_plan(
                pg, parameter & kColorBit, &bindings, &capabilities, &ignored);
            result.color.status = PlanStatusToExecutor(status);
        } else if (color_descriptor_ready) {
            result.color =
                ExecuteAttachment(state, true, color_plan, color_descriptor);
        } else {
            result.color.status = SurfaceStatusToExecutor(color_surface_status);
        }
    }
    if (parameter & kZetaBits) {
        if (!zeta_plan_ready) {
            D3D11ClearPlan ignored = {};
            const D3D11ClearPlanStatus status = d3d11_build_clear_plan(
                pg, parameter & kZetaBits, &bindings, &capabilities, &ignored);
            result.depth_stencil.status = PlanStatusToExecutor(status);
        } else if (zeta_descriptor_ready) {
            result.depth_stencil =
                ExecuteAttachment(state, false, zeta_plan, zeta_descriptor);
        } else {
            result.depth_stencil.status =
                SurfaceStatusToExecutor(zeta_surface_status);
        }
    }
    result.status = AggregateStatus(result.color, result.depth_stencil);
    return result;
}

bool D3D11ClearExecutor::FlushAttachment(NV2AState *state, bool color,
                                         D3D11ClearFlushReport *report)
{
    D3D11SurfaceSnapshot &snapshot = color ? m_color : m_depth_stencil;
    if (!snapshot) {
        return true;
    }
    const D3D11SurfaceResource *resource = snapshot.get();
    if (!resource->draw_dirty() && !resource->download_dirty()) {
        return true;
    }
    uint8_t *vram = nullptr;
    size_t vram_size = 0;
    if (!Vram(state, &vram, &vram_size)) {
        return false;
    }
    if (!m_pgraph_context->Download(snapshot, vram, vram_size,
                                    color ?
                                        D3D11SurfaceAttachment::Color :
                                        D3D11SurfaceAttachment::DepthStencil)) {
        return false;
    }
    (void)report;
    return true;
}

bool D3D11ClearExecutor::Flush(NV2AState *state, D3D11ClearFlushReport *report)
{
    D3D11ClearFlushReport local_report = {};
    if (report == nullptr) {
        report = &local_report;
    }
    if (GetCurrentThreadId() != m_owner_thread) {
        *report = {};
        report->status = D3D11ClearExecutorStatus::WrongThread;
        return false;
    }
    uint8_t *vram = nullptr;
    size_t vram_size = 0;
    if (state == nullptr || !Vram(state, &vram, &vram_size)) {
        *report = {};
        report->status = D3D11ClearExecutorStatus::Invalid;
        return false;
    }
    *report = {};
    bool retired_ok = false;
    bool color_ok = false;
    bool zeta_ok = false;
    if (m_pgraph_context->valid()) {
        retired_ok = m_pgraph_context->FlushRetired(vram, vram_size);
        color_ok = FlushAttachment(state, true, report);
        zeta_ok = FlushAttachment(state, false, report);
    } else {
        report->status = SurfaceStatusToExecutor(m_pgraph_context->status());
    }
    if (!color_ok) {
        const D3D11SurfaceSnapshot &snapshot = m_color;
        report->status =
            !m_pgraph_context->valid() ?
                SurfaceStatusToExecutor(m_pgraph_context->status()) :
            snapshot ?
                SurfaceStatusToExecutor(snapshot->last_operation_status()) :
                D3D11ClearExecutorStatus::Invalid;
    }
    if (!zeta_ok) {
        const D3D11SurfaceSnapshot &snapshot = m_depth_stencil;
        report->status =
            !m_pgraph_context->valid() ?
                SurfaceStatusToExecutor(m_pgraph_context->status()) :
            snapshot ?
                SurfaceStatusToExecutor(snapshot->last_operation_status()) :
                D3D11ClearExecutorStatus::Invalid;
    }
    const std::vector<D3D11PgraphDownloadEvent> events =
        m_pgraph_context->DrainDownloadEvents(state, m_color, m_depth_stencil);
    for (const D3D11PgraphDownloadEvent &event : events) {
        AppendEvent(report, event);
    }
    if ((!retired_ok || !color_ok || !zeta_ok) &&
        report->status == D3D11ClearExecutorStatus::Invalid) {
        report->status = SurfaceStatusToExecutor(
            m_pgraph_context->status() != D3D11SurfaceStatus::Ok ?
                m_pgraph_context->status() :
                m_pgraph_context->last_operation_status());
    }
    if (color_ok && zeta_ok && retired_ok) {
        report->status = D3D11ClearExecutorStatus::Cleared;
    }
    return color_ok && zeta_ok && retired_ok;
}

} // namespace xemu

#endif // _WIN32
