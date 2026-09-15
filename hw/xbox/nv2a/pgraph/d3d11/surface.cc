//
// xemu NV2A D3D11 surface resources
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "surface.h"

#ifdef _WIN32

#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#include <utility>

#include <windows.h>

namespace xemu {

namespace {

using Microsoft::WRL::ComPtr;

bool SameComIdentity(IUnknown *a, IUnknown *b)
{
    if (a == nullptr || b == nullptr) {
        return false;
    }
    ComPtr<IUnknown> a_identity;
    ComPtr<IUnknown> b_identity;
    if (FAILED(a->QueryInterface(IID_PPV_ARGS(&a_identity))) ||
        FAILED(b->QueryInterface(IID_PPV_ARGS(&b_identity)))) {
        return false;
    }
    return a_identity.Get() == b_identity.Get();
}

uint32_t BytesPerPixel(D3D11SurfaceFormat format)
{
    switch (format) {
    case D3D11SurfaceFormat::Bgr8A8:
    case D3D11SurfaceFormat::Z24S8:
        return 4;
    case D3D11SurfaceFormat::Z16:
        return 2;
    }
    return 0;
}

DXGI_FORMAT ResourceFormat(D3D11SurfaceFormat format)
{
    switch (format) {
    case D3D11SurfaceFormat::Bgr8A8:
        return DXGI_FORMAT_B8G8R8A8_TYPELESS;
    case D3D11SurfaceFormat::Z16:
        return DXGI_FORMAT_R16_TYPELESS;
    case D3D11SurfaceFormat::Z24S8:
        return DXGI_FORMAT_R24G8_TYPELESS;
    }
    return DXGI_FORMAT_UNKNOWN;
}

DXGI_FORMAT ViewFormat(D3D11SurfaceFormat format)
{
    switch (format) {
    case D3D11SurfaceFormat::Bgr8A8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3D11SurfaceFormat::Z16:
        return DXGI_FORMAT_D16_UNORM;
    case D3D11SurfaceFormat::Z24S8:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    }
    return DXGI_FORMAT_UNKNOWN;
}

bool IsDeviceError(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET ||
           hr == DXGI_ERROR_DEVICE_HUNG ||
           hr == DXGI_ERROR_DRIVER_INTERNAL_ERROR;
}

bool CheckedSurfaceValues(const D3D11SurfaceDescriptor &descriptor,
                          uint32_t *bytes_per_pixel, uint32_t *row_bytes,
                          uint64_t *byte_end)
{
    if (bytes_per_pixel == nullptr || row_bytes == nullptr ||
        byte_end == nullptr || descriptor.width == 0 ||
        descriptor.height == 0 ||
        descriptor.width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        descriptor.height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
        descriptor.type != D3D11SurfaceType::LinearPitch ||
        descriptor.scale != 1 ||
        descriptor.antialias != D3D11SurfaceAntialias::Center1) {
        return false;
    }
    const uint32_t bpp = BytesPerPixel(descriptor.format);
    if (bpp == 0 || descriptor.width > UINT32_MAX / bpp) {
        return false;
    }
    const uint32_t rows = descriptor.width * bpp;
    if (descriptor.pitch < rows) {
        return false;
    }
    const uint64_t surface_bytes =
        static_cast<uint64_t>(descriptor.pitch) * descriptor.height;
    if (surface_bytes > UINT64_MAX - descriptor.offset ||
        descriptor.offset + surface_bytes > SIZE_MAX) {
        return false;
    }
    *bytes_per_pixel = bpp;
    *row_bytes = rows;
    *byte_end = descriptor.offset + surface_bytes;
    return true;
}

bool CheckedRowOffset(uint64_t row, uint64_t stride, size_t *offset)
{
    if (offset == nullptr || (stride != 0 && row > SIZE_MAX / stride)) {
        return false;
    }
    const uint64_t result = row * stride;
    if (result > SIZE_MAX) {
        return false;
    }
    *offset = static_cast<size_t>(result);
    return true;
}

} // namespace

bool D3D11SurfaceDescriptor::operator==(
    const D3D11SurfaceDescriptor &other) const
{
    return offset == other.offset && width == other.width &&
           height == other.height && pitch == other.pitch &&
           format == other.format && type == other.type &&
           scale == other.scale && antialias == other.antialias;
}

void D3D11SurfaceResource::SetError(D3D11SurfaceStatus status, HRESULT hresult)
{
    if (m_ready && status != D3D11SurfaceStatus::DeviceLost &&
        status != D3D11SurfaceStatus::Timeout) {
        SetOperationError(status, hresult);
        return;
    }
    m_status = status;
    m_last_hresult = hresult;
    m_last_operation_status = status;
    m_last_operation_hresult = hresult;
}

void D3D11SurfaceResource::SetOperationError(D3D11SurfaceStatus status,
                                             HRESULT hresult)
{
    m_last_operation_status = status;
    m_last_operation_hresult = hresult;
    m_last_hresult = hresult;
}

D3D11SurfaceResource::AuthoritySnapshot
D3D11SurfaceResource::SnapshotAuthority() const
{
    return { m_draw_dirty,
             m_upload_dirty,
             m_download_dirty,
             m_status,
             m_last_hresult,
             m_last_query_hresult,
             m_last_operation_status,
             m_last_operation_hresult };
}

void D3D11SurfaceResource::RestoreAuthority(const AuthoritySnapshot &snapshot)
{
    m_draw_dirty = snapshot.draw_dirty;
    m_upload_dirty = snapshot.upload_dirty;
    m_download_dirty = snapshot.download_dirty;
    m_status = snapshot.status;
    m_last_hresult = snapshot.hresult;
    m_last_query_hresult = snapshot.query_hresult;
    m_last_operation_status = snapshot.operation_status;
    m_last_operation_hresult = snapshot.operation_hresult;
}

void D3D11SurfaceResource::RestoreTerminalFailure(
    D3D11SurfaceStatus status, HRESULT hresult, HRESULT query_hresult,
    D3D11SurfaceStatus operation_status, HRESULT operation_hresult)
{
    m_status = status;
    m_last_hresult = hresult;
    m_last_query_hresult = query_hresult;
    m_last_operation_status = operation_status;
    m_last_operation_hresult = operation_hresult;
}

D3D11SurfaceResource::D3D11SurfaceResource(
    ID3D11Device *device, ID3D11DeviceContext *context,
    const D3D11SurfaceDescriptor &descriptor, uint64_t generation,
    uint32_t gpu_timeout_ms)
    : m_descriptor(descriptor), m_generation(generation),
      m_gpu_timeout_ms(gpu_timeout_ms), m_owner_thread(GetCurrentThreadId())
{
    if (device == nullptr || context == nullptr) {
        SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return;
    }
    ComPtr<ID3D11Device> context_device;
    context->GetDevice(&context_device);
    if (!SameComIdentity(device, context_device.Get())) {
        SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return;
    }
    if (!CheckedSurfaceValues(descriptor, &m_bytes_per_pixel, &m_row_bytes,
                              &m_byte_end)) {
        SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
        return;
    }
    m_device = device;
    m_context = context;

    const DXGI_FORMAT resource_format = ResourceFormat(descriptor.format);
    const DXGI_FORMAT view_format = ViewFormat(descriptor.format);
    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = descriptor.width;
    texture_desc.Height = descriptor.height;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = resource_format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = descriptor.format == D3D11SurfaceFormat::Bgr8A8 ?
                                 D3D11_BIND_RENDER_TARGET :
                                 D3D11_BIND_DEPTH_STENCIL;
    HRESULT hr = m_device->CreateTexture2D(&texture_desc, nullptr,
                                           m_texture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return;
    }

    if (descriptor.format == D3D11SurfaceFormat::Bgr8A8) {
        D3D11_RENDER_TARGET_VIEW_DESC view_desc = {};
        view_desc.Format = view_format;
        view_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        hr = m_device->CreateRenderTargetView(
            m_texture.Get(), &view_desc,
            m_render_target.ReleaseAndGetAddressOf());
    } else {
        D3D11_DEPTH_STENCIL_VIEW_DESC view_desc = {};
        view_desc.Format = view_format;
        view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = m_device->CreateDepthStencilView(
            m_texture.Get(), &view_desc,
            m_depth_stencil.ReleaseAndGetAddressOf());
    }
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return;
    }

    D3D11_TEXTURE2D_DESC staging_desc = texture_desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    hr = m_device->CreateTexture2D(&staging_desc, nullptr,
                                   m_read_staging.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return;
    }
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = m_device->CreateTexture2D(&staging_desc, nullptr,
                                   m_write_staging.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return;
    }

    D3D11_QUERY_DESC query_desc = {};
    query_desc.Query = D3D11_QUERY_EVENT;
    hr = m_device->CreateQuery(&query_desc,
                               m_event_query.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return;
    }
    m_status = D3D11SurfaceStatus::Ok;
    m_last_hresult = S_OK;
    m_ready = true;
}

bool D3D11SurfaceResource::CheckVramRange(size_t vram_size) const
{
    return valid() && m_descriptor.offset <= vram_size &&
           m_byte_end <= vram_size;
}

bool D3D11SurfaceResource::CheckOwnerThread()
{
    if (GetCurrentThreadId() == m_owner_thread) {
        return true;
    }
    SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_ACCESSDENIED);
    return false;
}

bool D3D11SurfaceResource::ValidateInternalResources()
{
    if (m_device == nullptr || m_context == nullptr || m_texture == nullptr ||
        m_read_staging == nullptr || m_write_staging == nullptr ||
        m_event_query == nullptr) {
        SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return false;
    }
    ComPtr<ID3D11Device> context_device;
    m_context->GetDevice(&context_device);
    if (!SameComIdentity(context_device.Get(), m_device.Get())) {
        SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return false;
    }

    D3D11_TEXTURE2D_DESC expected = {};
    expected.Width = m_descriptor.width;
    expected.Height = m_descriptor.height;
    expected.MipLevels = 1;
    expected.ArraySize = 1;
    expected.Format = ResourceFormat(m_descriptor.format);
    expected.SampleDesc.Count = 1;

    auto valid_texture = [&](ID3D11Texture2D *texture, D3D11_USAGE usage,
                             UINT bind_flags, UINT cpu_access) {
        if (texture == nullptr) {
            return false;
        }
        D3D11_TEXTURE2D_DESC actual = {};
        texture->GetDesc(&actual);
        ComPtr<ID3D11Device> texture_device;
        texture->GetDevice(&texture_device);
        return SameComIdentity(texture_device.Get(), m_device.Get()) &&
               actual.Width == expected.Width &&
               actual.Height == expected.Height &&
               actual.MipLevels == expected.MipLevels &&
               actual.ArraySize == expected.ArraySize &&
               actual.Format == expected.Format &&
               actual.SampleDesc.Count == expected.SampleDesc.Count &&
               actual.SampleDesc.Quality == expected.SampleDesc.Quality &&
               actual.Usage == usage && actual.BindFlags == bind_flags &&
               actual.CPUAccessFlags == cpu_access && actual.MiscFlags == 0;
    };
    const UINT resource_bind =
        m_descriptor.format == D3D11SurfaceFormat::Bgr8A8 ?
            D3D11_BIND_RENDER_TARGET :
            D3D11_BIND_DEPTH_STENCIL;
    if (!valid_texture(m_texture.Get(), D3D11_USAGE_DEFAULT, resource_bind,
                       0) ||
        !valid_texture(m_read_staging.Get(), D3D11_USAGE_STAGING, 0,
                       D3D11_CPU_ACCESS_READ) ||
        !valid_texture(m_write_staging.Get(), D3D11_USAGE_STAGING, 0,
                       D3D11_CPU_ACCESS_WRITE)) {
        SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return false;
    }
    if (m_descriptor.format == D3D11SurfaceFormat::Bgr8A8) {
        if (m_render_target == nullptr) {
            SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
            return false;
        }
        ComPtr<ID3D11Resource> view_resource;
        m_render_target->GetResource(&view_resource);
        if (!SameComIdentity(view_resource.Get(), m_texture.Get())) {
            SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
            return false;
        }
    } else {
        if (m_depth_stencil == nullptr) {
            SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
            return false;
        }
        ComPtr<ID3D11Resource> view_resource;
        m_depth_stencil->GetResource(&view_resource);
        if (!SameComIdentity(view_resource.Get(), m_texture.Get())) {
            SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
            return false;
        }
    }
    return true;
}

bool D3D11SurfaceResource::MarkDrawDirty(bool dirty)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if (!dirty) {
        m_draw_dirty = false;
        m_download_dirty = false;
        return true;
    }
    if (m_upload_dirty) {
        SetOperationError(D3D11SurfaceStatus::Conflict, E_INVALIDARG);
        return false;
    }
    m_draw_dirty = true;
    m_download_dirty = true;
    return true;
}

bool D3D11SurfaceResource::MarkUploadDirty(bool dirty)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if (dirty && (m_draw_dirty || m_download_dirty)) {
        SetOperationError(D3D11SurfaceStatus::Conflict, E_INVALIDARG);
        return false;
    }
    m_upload_dirty = dirty;
    return true;
}

bool D3D11SurfaceResource::MarkDownloadDirty(bool dirty)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if (dirty && m_upload_dirty) {
        SetOperationError(D3D11SurfaceStatus::Conflict, E_INVALIDARG);
        return false;
    }
    m_download_dirty = dirty;
    return true;
}

bool D3D11SurfaceResource::CompleteGpuWork()
{
    if (!CheckOwnerThread() || !valid() || m_event_query == nullptr ||
        !ValidateInternalResources()) {
        return false;
    }
    const HRESULT initial_reason = m_device->GetDeviceRemovedReason();
    if (FAILED(initial_reason)) {
        SetError(D3D11SurfaceStatus::DeviceLost, initial_reason);
        return false;
    }
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(m_gpu_timeout_ms);
    if (m_gpu_timeout_ms == 0) {
        m_last_query_hresult = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        SetError(D3D11SurfaceStatus::Timeout, HRESULT_FROM_WIN32(WAIT_TIMEOUT));
        return false;
    }
    m_context->End(m_event_query.Get());
    m_context->Flush();
    HRESULT hr = S_OK;
    for (;;) {
        if (std::chrono::steady_clock::now() >= deadline) {
            m_last_query_hresult = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
            SetError(D3D11SurfaceStatus::Timeout,
                     HRESULT_FROM_WIN32(WAIT_TIMEOUT));
            return false;
        }
        hr = m_context->GetData(m_event_query.Get(), nullptr, 0, 0);
        m_last_query_hresult = hr;
        if (hr == S_OK) {
            break;
        }
        if (FAILED(hr)) {
            SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                         D3D11SurfaceStatus::Unsupported,
                     hr);
            return false;
        }
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        if (FAILED(reason)) {
            SetError(D3D11SurfaceStatus::DeviceLost, reason);
            return false;
        }
        Sleep(0);
    }
    const HRESULT reason = m_device->GetDeviceRemovedReason();
    if (FAILED(reason)) {
        SetError(D3D11SurfaceStatus::DeviceLost, reason);
        return false;
    }
    m_last_hresult = S_OK;
    return true;
}

bool D3D11SurfaceResource::Upload(const uint8_t *vram, size_t vram_size)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if (m_draw_dirty || m_download_dirty) {
        SetError(D3D11SurfaceStatus::Conflict, E_INVALIDARG);
        return false;
    }
    if (vram == nullptr || !CheckVramRange(vram_size)) {
        SetError(D3D11SurfaceStatus::OutOfRange, E_INVALIDARG);
        return false;
    }
    if (!ValidateInternalResources()) {
        return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr =
        m_context->Map(m_write_staging.Get(), 0, D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return false;
    }
    if (mapped.pData == nullptr || mapped.RowPitch < m_row_bytes) {
        m_context->Unmap(m_write_staging.Get(), 0);
        SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
        return false;
    }
    const uint8_t *source = vram + static_cast<size_t>(m_descriptor.offset);
    uint8_t *destination = static_cast<uint8_t *>(mapped.pData);
    for (uint32_t y = 0; y < m_descriptor.height; ++y) {
        size_t source_offset = 0;
        size_t destination_offset = 0;
        if (!CheckedRowOffset(y, m_descriptor.pitch, &source_offset) ||
            !CheckedRowOffset(y, mapped.RowPitch, &destination_offset)) {
            m_context->Unmap(m_write_staging.Get(), 0);
            SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
            return false;
        }
        std::memcpy(destination + destination_offset, source + source_offset,
                    m_row_bytes);
    }
    m_context->Unmap(m_write_staging.Get(), 0);
    if (!ValidateInternalResources()) {
        return false;
    }
    m_context->CopyResource(m_texture.Get(), m_write_staging.Get());
    if (!CompleteGpuWork()) {
        return false;
    }
    m_upload_dirty = false;
    m_status = D3D11SurfaceStatus::Ok;
    m_last_hresult = S_OK;
    m_last_operation_status = D3D11SurfaceStatus::Ok;
    m_last_operation_hresult = S_OK;
    return true;
}

bool D3D11SurfaceResource::Download(uint8_t *vram, size_t vram_size)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if (m_upload_dirty) {
        SetError(D3D11SurfaceStatus::Conflict, E_INVALIDARG);
        return false;
    }
    if (vram == nullptr || !CheckVramRange(vram_size)) {
        SetError(D3D11SurfaceStatus::OutOfRange, E_INVALIDARG);
        return false;
    }
    if (m_fail_download_for_testing) {
        m_fail_download_for_testing = false;
        m_last_query_hresult = m_injected_download_hresult;
        if (m_injected_download_status == D3D11SurfaceStatus::DeviceLost ||
            m_injected_download_status == D3D11SurfaceStatus::Timeout) {
            SetError(m_injected_download_status, m_injected_download_hresult);
        } else {
            SetOperationError(m_injected_download_status,
                              m_injected_download_hresult);
        }
        return false;
    }
    if (!ValidateInternalResources()) {
        return false;
    }
    m_context->CopyResource(m_read_staging.Get(), m_texture.Get());
    if (!CompleteGpuWork()) {
        return false;
    }
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr =
        m_context->Map(m_read_staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        SetError(IsDeviceError(hr) ? D3D11SurfaceStatus::DeviceLost :
                                     D3D11SurfaceStatus::Unsupported,
                 hr);
        return false;
    }
    if (mapped.pData == nullptr || mapped.RowPitch < m_row_bytes) {
        m_context->Unmap(m_read_staging.Get(), 0);
        SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
        return false;
    }
    uint8_t *destination = vram + static_cast<size_t>(m_descriptor.offset);
    const uint8_t *source = static_cast<const uint8_t *>(mapped.pData);
    for (uint32_t y = 0; y < m_descriptor.height; ++y) {
        size_t source_offset = 0;
        size_t destination_offset = 0;
        if (!CheckedRowOffset(y, mapped.RowPitch, &source_offset) ||
            !CheckedRowOffset(y, m_descriptor.pitch, &destination_offset)) {
            m_context->Unmap(m_read_staging.Get(), 0);
            SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
            return false;
        }
        std::memcpy(destination + destination_offset, source + source_offset,
                    m_row_bytes);
    }
    m_context->Unmap(m_read_staging.Get(), 0);
    m_draw_dirty = false;
    m_download_dirty = false;
    m_status = D3D11SurfaceStatus::Ok;
    m_last_hresult = S_OK;
    m_last_operation_status = D3D11SurfaceStatus::Ok;
    m_last_operation_hresult = S_OK;
    return true;
}

bool D3D11SurfaceResource::Flush(uint8_t *vram, size_t vram_size)
{
    if (!CheckOwnerThread() || !valid()) {
        return false;
    }
    if ((m_draw_dirty || m_download_dirty) && !Download(vram, vram_size)) {
        return false;
    }
    if (m_upload_dirty && !Upload(vram, vram_size)) {
        return false;
    }
    return true;
}

D3D11SurfaceCache::D3D11SurfaceCache(ID3D11Device *device,
                                     ID3D11DeviceContext *context,
                                     uint32_t gpu_timeout_ms)
    : m_gpu_timeout_ms(gpu_timeout_ms), m_owner_thread(GetCurrentThreadId())
{
    if (device == nullptr || context == nullptr) {
        SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return;
    }
    ComPtr<ID3D11Device> context_device;
    context->GetDevice(&context_device);
    if (!SameComIdentity(device, context_device.Get())) {
        SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        return;
    }
    m_device = device;
    m_context = context;
    m_valid_device = true;
    m_status = D3D11SurfaceStatus::Ok;
    m_last_hresult = S_OK;
}

void D3D11SurfaceCache::SetError(D3D11SurfaceStatus status, HRESULT hresult)
{
    if (m_valid_device && status != D3D11SurfaceStatus::DeviceLost &&
        status != D3D11SurfaceStatus::Timeout) {
        SetOperationError(status, hresult);
        return;
    }
    m_status = status;
    m_last_hresult = hresult;
    m_last_operation_status = status;
    m_last_operation_hresult = hresult;
    if (status == D3D11SurfaceStatus::DeviceLost ||
        status == D3D11SurfaceStatus::Timeout) {
        m_terminal = true;
    }
}

void D3D11SurfaceCache::SetOperationError(D3D11SurfaceStatus status,
                                          HRESULT hresult)
{
    m_last_operation_status = status;
    m_last_operation_hresult = hresult;
    m_last_hresult = hresult;
}

bool D3D11SurfaceCache::CheckOwnerThread() const
{
    return GetCurrentThreadId() == m_owner_thread;
}

bool D3D11SurfaceCache::ValidateDescriptor(
    const D3D11SurfaceDescriptor &descriptor, uint32_t *bytes_per_pixel,
    uint32_t *row_bytes, uint64_t *byte_end) const
{
    return CheckedSurfaceValues(descriptor, bytes_per_pixel, row_bytes,
                                byte_end);
}

D3D11SurfaceSnapshot
D3D11SurfaceCache::Find(const D3D11SurfaceDescriptor &descriptor) const
{
    if (!CheckOwnerThread() || m_terminal) {
        return {};
    }
    for (const Entry &entry : m_entries) {
        if (entry.descriptor == descriptor) {
            return D3D11SurfaceSnapshot(entry.resource);
        }
    }
    return {};
}

D3D11SurfaceSnapshot D3D11SurfaceCache::FindForInspection(
    const D3D11SurfaceDescriptor &descriptor) const
{
    if (!CheckOwnerThread()) {
        return {};
    }
    for (const Entry &entry : m_entries) {
        if (entry.descriptor == descriptor) {
            return D3D11SurfaceSnapshot(entry.resource);
        }
    }
    return {};
}

D3D11SurfaceSnapshot
D3D11SurfaceCache::Acquire(const D3D11SurfaceDescriptor &descriptor,
                           uint8_t *vram, size_t vram_size,
                           D3D11SurfaceAttachment attachment)
{
    if (!CheckOwnerThread()) {
        SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_ACCESSDENIED);
        return {};
    }
    if (!valid()) {
        if (!m_terminal) {
            SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        }
        return {};
    }
    uint32_t bytes_per_pixel = 0;
    uint32_t row_bytes = 0;
    uint64_t byte_end = 0;
    if (!ValidateDescriptor(descriptor, &bytes_per_pixel, &row_bytes,
                            &byte_end)) {
        SetError(D3D11SurfaceStatus::InvalidDescriptor, E_INVALIDARG);
        return {};
    }
    if (vram == nullptr || descriptor.offset > vram_size ||
        byte_end > vram_size) {
        SetError(D3D11SurfaceStatus::OutOfRange, E_INVALIDARG);
        return {};
    }
    for (Entry &entry : m_entries) {
        if (entry.descriptor == descriptor) {
            entry.attachment |= attachment;
            SetError(D3D11SurfaceStatus::Ok, S_OK);
            return D3D11SurfaceSnapshot(entry.resource);
        }
    }

    /* Construct the replacement and reserve its entry before touching the
     * cache.  A failed transfer therefore leaves the old entries installed. */
    auto resource = std::make_shared<D3D11SurfaceResource>(
        m_device.Get(), m_context.Get(), descriptor, m_next_generation++,
        m_gpu_timeout_ms);
    if (!resource->valid() || !resource->MarkUploadDirty()) {
        SetError(resource->status(), resource->last_hresult());
        return {};
    }
    m_entries.reserve(m_entries.size() + 1);

    struct Backup {
        D3D11SurfaceResource *resource;
        size_t offset;
        std::vector<uint8_t> bytes;
        D3D11SurfaceResource::AuthoritySnapshot authority;
    };
    std::vector<Entry *> overlaps;
    std::vector<Backup> backups;
    std::vector<D3D11SurfaceDownloadEvent> downloaded_events;
    overlaps.reserve(m_entries.size());
    backups.reserve(m_entries.size());

    for (Entry &entry : m_entries) {
        const uint64_t old_begin = entry.descriptor.offset;
        const uint64_t old_end = entry.resource->byte_end();
        if (descriptor.offset >= old_end || old_begin >= byte_end) {
            continue;
        }
        overlaps.push_back(&entry);
        D3D11SurfaceResource *old = entry.resource.get();
        if (old->status() == D3D11SurfaceStatus::DeviceLost ||
            old->status() == D3D11SurfaceStatus::Timeout) {
            SetError(old->status(), old->last_hresult());
            return {};
        }
        if (old->draw_dirty() || old->download_dirty()) {
            if (vram == nullptr || old->descriptor().offset > vram_size ||
                old->byte_end() > vram_size) {
                SetError(D3D11SurfaceStatus::OutOfRange, E_INVALIDARG);
                return {};
            }
            const size_t offset = static_cast<size_t>(old->descriptor().offset);
            const size_t length =
                static_cast<size_t>(old->byte_end() - old->descriptor().offset);
            backups.push_back(
                { old, offset,
                  std::vector<uint8_t>(vram + offset, vram + offset + length),
                  old->SnapshotAuthority() });
        }
    }

    /* Download every dirty overlapping surface before erasing it.  If a later
     * download fails, restore all VRAM bytes written by earlier downloads. */
    for (Entry *entry : overlaps) {
        D3D11SurfaceResource *old = entry->resource.get();
        if (!(old->draw_dirty() || old->download_dirty())) {
            continue;
        }
        if (!old->Download(vram, vram_size)) {
            const D3D11SurfaceStatus failure_status = old->status();
            const HRESULT failure_hresult = old->last_hresult();
            const HRESULT failure_query_hresult = old->last_query_hresult();
            const D3D11SurfaceStatus failure_operation_status =
                old->last_operation_status();
            const HRESULT failure_operation_hresult =
                old->last_operation_hresult();
            for (const Backup &backup : backups) {
                std::memcpy(vram + backup.offset, backup.bytes.data(),
                            backup.bytes.size());
                backup.resource->RestoreAuthority(backup.authority);
            }
            if (failure_status == D3D11SurfaceStatus::DeviceLost ||
                failure_status == D3D11SurfaceStatus::Timeout) {
                old->RestoreTerminalFailure(
                    failure_status, failure_hresult, failure_query_hresult,
                    failure_operation_status, failure_operation_hresult);
                SetError(failure_status, failure_hresult);
            } else {
                SetOperationError(failure_operation_status,
                                  failure_operation_hresult);
            }
            return {};
        }
        downloaded_events.push_back(
            { old->descriptor(), old->generation(),
              old->byte_end() - old->descriptor().offset, entry->attachment });
    }
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&](const Entry &entry) {
                                       return descriptor.offset <
                                                  entry.resource->byte_end() &&
                                              entry.descriptor.offset <
                                                  byte_end;
                                   }),
                    m_entries.end());
    /* New resources start with the guest bytes authoritative.  The eventual
     * PGRAPH adapter decides when to call Upload; no TCG memory callback is
     * coupled to this cache. */
    m_entries.push_back({ descriptor, resource, attachment });
    m_download_events.insert(m_download_events.end(), downloaded_events.begin(),
                             downloaded_events.end());
    SetError(D3D11SurfaceStatus::Ok, S_OK);
    return D3D11SurfaceSnapshot(std::move(resource));
}

std::vector<D3D11SurfaceDownloadEvent> D3D11SurfaceCache::TakeDownloadEvents()
{
    if (!CheckOwnerThread()) {
        return {};
    }
    std::vector<D3D11SurfaceDownloadEvent> events;
    events.swap(m_download_events);
    return events;
}

void D3D11SurfaceCache::MarkTerminalFailure(D3D11SurfaceStatus status,
                                            HRESULT hresult)
{
    if (!CheckOwnerThread()) {
        return;
    }
    if (status == D3D11SurfaceStatus::DeviceLost ||
        status == D3D11SurfaceStatus::Timeout) {
        SetError(status, hresult);
    }
}

void D3D11SurfaceCache::RecordOperationFailure(D3D11SurfaceStatus status,
                                               HRESULT hresult)
{
    if (!CheckOwnerThread()) {
        return;
    }
    SetOperationError(status, hresult);
}

bool D3D11SurfaceCache::Flush(uint8_t *vram, size_t vram_size)
{
    if (!CheckOwnerThread()) {
        SetOperationError(D3D11SurfaceStatus::InvalidDevice, E_ACCESSDENIED);
        return false;
    }
    if (!valid()) {
        if (!m_terminal) {
            SetError(D3D11SurfaceStatus::InvalidDevice, E_INVALIDARG);
        }
        return false;
    }
    for (const Entry &entry : m_entries) {
        const bool was_download_dirty =
            entry.resource->draw_dirty() || entry.resource->download_dirty();
        if (!entry.resource->Flush(vram, vram_size)) {
            if (entry.resource->status() == D3D11SurfaceStatus::DeviceLost ||
                entry.resource->status() == D3D11SurfaceStatus::Timeout) {
                SetError(entry.resource->status(),
                         entry.resource->last_hresult());
            } else {
                SetOperationError(entry.resource->last_operation_status(),
                                  entry.resource->last_operation_hresult());
            }
            return false;
        }
        if (was_download_dirty && !entry.resource->draw_dirty() &&
            !entry.resource->download_dirty()) {
            m_download_events.push_back(
                { entry.descriptor, entry.resource->generation(),
                  entry.resource->byte_end() - entry.descriptor.offset,
                  entry.attachment });
        }
    }
    SetError(D3D11SurfaceStatus::Ok, S_OK);
    return true;
}

} // namespace xemu

#endif // _WIN32
