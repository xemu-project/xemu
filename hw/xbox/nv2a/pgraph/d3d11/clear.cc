//
// xemu NV2A D3D11 clear primitive
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#include "clear.h"

#ifdef _WIN32

#include <limits>

namespace xemu {

namespace {

constexpr HRESULT kUnsupported = DXGI_ERROR_UNSUPPORTED;

D3D11ClearResult Result(D3D11ClearStatus status, HRESULT hresult = S_OK)
{
    return { status, hresult };
}

bool SameComIdentity(IUnknown *a, IUnknown *b)
{
    if (a == nullptr || b == nullptr) {
        return false;
    }
    Microsoft::WRL::ComPtr<IUnknown> a_identity;
    Microsoft::WRL::ComPtr<IUnknown> b_identity;
    if (FAILED(a->QueryInterface(IID_PPV_ARGS(&a_identity))) ||
        FAILED(b->QueryInterface(IID_PPV_ARGS(&b_identity)))) {
        return false;
    }
    return a_identity.Get() == b_identity.Get();
}

bool GetResourceDevice(ID3D11View *view,
                       Microsoft::WRL::ComPtr<ID3D11Device> *device,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> *texture)
{
    if (view == nullptr || device == nullptr || texture == nullptr) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;
    view->GetResource(&resource);
    if (resource == nullptr || FAILED(resource.As(texture)) ||
        *texture == nullptr) {
        return false;
    }
    texture->Get()->GetDevice(device->ReleaseAndGetAddressOf());
    return *device != nullptr;
}

bool ValidColor(const D3D11ClearParams &params)
{
    if (params.surface_kind != D3D11ClearSurfaceKind::Color ||
        (params.surface_format != D3D11ClearSurfaceFormat::ColorRgba8Unorm &&
         params.surface_format != D3D11ClearSurfaceFormat::ColorBgra8Unorm)) {
        return false;
    }
    for (float channel : params.color) {
        if (!(channel >= 0.0f && channel <= 1.0f)) {
            return false;
        }
    }
    return true;
}

bool ValidDepthFormat(const D3D11ClearParams &params)
{
    return params.surface_kind == D3D11ClearSurfaceKind::DepthStencil &&
           (params.surface_format == D3D11ClearSurfaceFormat::Z16Fixed ||
            params.surface_format == D3D11ClearSurfaceFormat::Z24S8Fixed);
}

bool IsFloatDepthFormat(const D3D11ClearParams &params)
{
    return params.surface_format == D3D11ClearSurfaceFormat::Z16Float ||
           params.surface_format == D3D11ClearSurfaceFormat::Z24S8Float;
}

bool NormalizeRect(const D3D11ClearParams &params, D3D11ClearRect *rect)
{
    if (rect == nullptr ||
        params.surface_width >
            static_cast<uint32_t>((std::numeric_limits<LONG>::max)()) ||
        params.surface_height >
            static_cast<uint32_t>((std::numeric_limits<LONG>::max)())) {
        return false;
    }
    return pgraph_clear_rect_clamp(&params.rect, params.surface_width,
                                   params.surface_height, rect);
}

bool IsFullSurface(const D3D11ClearRect &rect, uint32_t width, uint32_t height)
{
    return pgraph_clear_rect_is_full(&rect, width, height);
}

DXGI_FORMAT ExpectedViewFormat(const D3D11ClearParams &params)
{
    switch (params.surface_format) {
    case D3D11ClearSurfaceFormat::ColorRgba8Unorm:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case D3D11ClearSurfaceFormat::ColorBgra8Unorm:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case D3D11ClearSurfaceFormat::Z16Fixed:
        return DXGI_FORMAT_D16_UNORM;
    case D3D11ClearSurfaceFormat::Z24S8Fixed:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case D3D11ClearSurfaceFormat::Z16Float:
    case D3D11ClearSurfaceFormat::Z24S8Float:
        return DXGI_FORMAT_UNKNOWN;
    }
    return DXGI_FORMAT_UNKNOWN;
}

bool ValidTarget(ID3D11View *view, const D3D11ClearParams &params,
                 ID3D11Device *device)
{
    if (view == nullptr || device == nullptr) {
        return false;
    }

    Microsoft::WRL::ComPtr<ID3D11Device> resource_device;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (!GetResourceDevice(view, &resource_device, &texture) ||
        !SameComIdentity(resource_device.Get(), device)) {
        return false;
    }

    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture->GetDesc(&texture_desc);
    if (texture_desc.Width != params.surface_width ||
        texture_desc.Height != params.surface_height ||
        texture_desc.MipLevels != 1 || texture_desc.ArraySize != 1 ||
        texture_desc.SampleDesc.Count != 1) {
        return false;
    }

    DXGI_FORMAT view_format = DXGI_FORMAT_UNKNOWN;
    if (params.surface_kind == D3D11ClearSurfaceKind::Color) {
        D3D11_RENDER_TARGET_VIEW_DESC desc = {};
        static_cast<ID3D11RenderTargetView *>(view)->GetDesc(&desc);
        if (desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D) {
            return false;
        }
        view_format = desc.Format;
    } else {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
        static_cast<ID3D11DepthStencilView *>(view)->GetDesc(&desc);
        if (desc.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D) {
            return false;
        }
        view_format = desc.Format;
    }
    return view_format == ExpectedViewFormat(params);
}

} // namespace

D3D11ClearPrimitive::D3D11ClearPrimitive(ID3D11Device *device,
                                         ID3D11DeviceContext *context)
{
    if (device != nullptr) {
        m_device = device;
    }
    if (context != nullptr) {
        m_context = context;
        context->QueryInterface(IID_PPV_ARGS(&m_context1));
    }
}

D3D11ClearResult
D3D11ClearPrimitive::Clear(const D3D11ClearParams &params,
                           ID3D11RenderTargetView *render_target,
                           ID3D11DepthStencilView *depth_stencil) const
{
    if (m_device == nullptr || m_context == nullptr) {
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }
    if (params.sample_count != 1 || params.swizzled || params.scaled) {
        return Result(D3D11ClearStatus::Unsupported, kUnsupported);
    }

    Microsoft::WRL::ComPtr<ID3D11Device> context_device;
    m_context->GetDevice(&context_device);
    if (!SameComIdentity(context_device.Get(), m_device.Get())) {
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }

    D3D11ClearRect rect = {};
    if (!NormalizeRect(params, &rect)) {
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }
    const bool full_surface =
        IsFullSurface(rect, params.surface_width, params.surface_height);

    if (params.surface_kind == D3D11ClearSurfaceKind::Color) {
        if (!ValidColor(params) || render_target == nullptr ||
            depth_stencil != nullptr ||
            params.color_mask != D3D11_CLEAR_ALL_COLOR_CHANNELS) {
            return Result(D3D11ClearStatus::Unsupported, kUnsupported);
        }
        if (!ValidTarget(render_target, params, m_device.Get())) {
            return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
        }
        if (full_surface) {
            m_context->ClearRenderTargetView(render_target, params.color);
            const HRESULT reason = m_device->GetDeviceRemovedReason();
            return FAILED(reason) ?
                       Result(D3D11ClearStatus::DeviceError, reason) :
                       Result(D3D11ClearStatus::Cleared);
        }
        if (!m_context1) {
            return Result(D3D11ClearStatus::Unsupported, kUnsupported);
        }
        D3D11_RECT d3d_rect = {
            static_cast<LONG>(rect.left),
            static_cast<LONG>(rect.top),
            static_cast<LONG>(rect.right + 1),
            static_cast<LONG>(rect.bottom + 1),
        };
        m_context1->ClearView(render_target, params.color, &d3d_rect, 1);
        const HRESULT reason = m_device->GetDeviceRemovedReason();
        return FAILED(reason) ? Result(D3D11ClearStatus::DeviceError, reason) :
                                Result(D3D11ClearStatus::Cleared);
    }

    if (render_target != nullptr || depth_stencil == nullptr ||
        !ValidDepthFormat(params)) {
        if (IsFloatDepthFormat(params)) {
            return Result(D3D11ClearStatus::Unsupported, kUnsupported);
        }
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }
    if (!full_surface) {
        return Result(D3D11ClearStatus::Unsupported, kUnsupported);
    }
    if (!params.clear_depth && !params.clear_stencil) {
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }
    if (params.surface_format == D3D11ClearSurfaceFormat::Z16Fixed &&
        params.clear_stencil) {
        return Result(D3D11ClearStatus::Unsupported, kUnsupported);
    }
    const uint32_t max_depth =
        params.surface_format == D3D11ClearSurfaceFormat::Z16Fixed ? 0xffffu :
                                                                     0xffffffu;
    if ((params.clear_depth && params.fixed_depth > max_depth) ||
        !ValidTarget(depth_stencil, params, m_device.Get())) {
        return Result(D3D11ClearStatus::Invalid, E_INVALIDARG);
    }

    UINT flags = 0;
    if (params.clear_depth) {
        flags |= D3D11_CLEAR_DEPTH;
    }
    if (params.clear_stencil) {
        flags |= D3D11_CLEAR_STENCIL;
    }
    const FLOAT depth = params.clear_depth ?
                            static_cast<FLOAT>(params.fixed_depth) /
                                static_cast<FLOAT>(max_depth) :
                            1.0f;
    m_context->ClearDepthStencilView(depth_stencil, flags, depth,
                                     params.stencil);
    const HRESULT reason = m_device->GetDeviceRemovedReason();
    return FAILED(reason) ? Result(D3D11ClearStatus::DeviceError, reason) :
                            Result(D3D11ClearStatus::Cleared);
}

} // namespace xemu

#endif // _WIN32
