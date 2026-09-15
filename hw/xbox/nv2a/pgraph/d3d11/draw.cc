//
// xemu NV2A D3D11 view-based triangle draw primitive
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include "draw.h"

#ifdef _WIN32

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace xemu {

namespace {

using Microsoft::WRL::ComPtr;

D3D11DrawResult Result(D3D11DrawStatus status, HRESULT hresult = S_OK,
                       HRESULT removed_reason = S_OK)
{
    return { status, hresult, removed_reason };
}

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

bool ResourceAndDevice(ID3D11View *view, ID3D11Device *expected_device,
                       ComPtr<ID3D11Texture2D> *texture)
{
    if (view == nullptr || expected_device == nullptr || texture == nullptr) {
        return false;
    }
    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    if (resource == nullptr || FAILED(resource.As(texture)) ||
        *texture == nullptr) {
        return false;
    }
    ComPtr<ID3D11Device> resource_device;
    texture->Get()->GetDevice(resource_device.GetAddressOf());
    return SameComIdentity(resource_device.Get(), expected_device);
}

bool StateOnDevice(IUnknown *state, ID3D11Device *device)
{
    if (state == nullptr) {
        return true;
    }
    ComPtr<ID3D11DeviceChild> child;
    if (FAILED(state->QueryInterface(IID_PPV_ARGS(&child))) ||
        child == nullptr) {
        return false;
    }
    ComPtr<ID3D11Device> state_device;
    child->GetDevice(state_device.GetAddressOf());
    return SameComIdentity(state_device.Get(), device);
}

bool IsColorFormat(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return true;
    default:
        return false;
    }
}

bool IsDepthFormat(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_D16_UNORM:
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
    case DXGI_FORMAT_D32_FLOAT:
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return true;
    default:
        return false;
    }
}

UINT MipDimension(UINT base, UINT mip)
{
    while (mip != 0 && base > 1) {
        base >>= 1;
        --mip;
    }
    return (std::max)(base, 1u);
}

UINT AvailableMipLevels(const D3D11_TEXTURE2D_DESC &desc)
{
    if (desc.MipLevels != 0) {
        return desc.MipLevels;
    }
    UINT largest = (std::max)(desc.Width, desc.Height);
    UINT levels = 1;
    while (largest > 1) {
        largest >>= 1;
        ++levels;
    }
    return levels;
}

bool ValidateTargets(const D3D11DrawParams &params, ID3D11Device *device,
                     UINT *width, UINT *height)
{
    if (params.render_target == nullptr || device == nullptr ||
        width == nullptr || height == nullptr) {
        return false;
    }

    ComPtr<ID3D11Texture2D> color_texture;
    if (!ResourceAndDevice(params.render_target, device, &color_texture)) {
        return false;
    }
    D3D11_TEXTURE2D_DESC color_desc = {};
    color_texture->GetDesc(&color_desc);
    D3D11_RENDER_TARGET_VIEW_DESC color_view = {};
    params.render_target->GetDesc(&color_view);
    const UINT color_mip_levels = AvailableMipLevels(color_desc);
    const bool color_multisample =
        color_view.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DMS;
    if (!IsColorFormat(color_view.Format) || color_desc.Width == 0 ||
        color_desc.Height == 0 ||
        (color_view.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D &&
         !color_multisample) ||
        (color_multisample && color_desc.MipLevels != 1) ||
        (!color_multisample &&
         color_view.Texture2D.MipSlice >= color_mip_levels)) {
        return false;
    }
    const UINT color_width =
        color_multisample ?
            color_desc.Width :
            MipDimension(color_desc.Width, color_view.Texture2D.MipSlice);
    const UINT color_height =
        color_multisample ?
            color_desc.Height :
            MipDimension(color_desc.Height, color_view.Texture2D.MipSlice);

    if (params.depth_stencil != nullptr) {
        ComPtr<ID3D11Texture2D> depth_texture;
        if (!ResourceAndDevice(params.depth_stencil, device, &depth_texture)) {
            return false;
        }
        D3D11_TEXTURE2D_DESC depth_desc = {};
        depth_texture->GetDesc(&depth_desc);
        D3D11_DEPTH_STENCIL_VIEW_DESC depth_view = {};
        params.depth_stencil->GetDesc(&depth_view);
        const bool depth_multisample =
            depth_view.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DMS;
        if (!IsDepthFormat(depth_view.Format) || depth_desc.Width == 0 ||
            depth_desc.Height == 0 ||
            (depth_view.ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D &&
             !depth_multisample) ||
            depth_multisample != color_multisample ||
            (depth_multisample && depth_desc.MipLevels != 1) ||
            (!depth_multisample &&
             depth_view.Texture2D.MipSlice >= AvailableMipLevels(depth_desc)) ||
            (depth_multisample ?
                 depth_desc.Width :
                 MipDimension(depth_desc.Width,
                              depth_view.Texture2D.MipSlice)) != color_width ||
            (depth_multisample ?
                 depth_desc.Height :
                 MipDimension(depth_desc.Height,
                              depth_view.Texture2D.MipSlice)) != color_height ||
            depth_desc.SampleDesc.Count != color_desc.SampleDesc.Count) {
            return false;
        }
        if (depth_desc.SampleDesc.Quality != color_desc.SampleDesc.Quality) {
            return false;
        }
    }

    *width = color_width;
    *height = color_height;
    return true;
}

UINT FormatBytes(DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R32_FLOAT:
        return 4;
    case DXGI_FORMAT_R32G32_FLOAT:
        return 8;
    case DXGI_FORMAT_R32G32B32_FLOAT:
        return 12;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        return 16;
    case DXGI_FORMAT_R16_FLOAT:
        return 2;
    case DXGI_FORMAT_R16G16_FLOAT:
    case DXGI_FORMAT_R16G16_UNORM:
        return 4;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM:
        return 8;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_R10G10B10A2_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UINT:
        return 4;
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_UINT:
        return 2;
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_UINT:
        return 1;
    default:
        return 0;
    }
}

struct SlotRequirement {
    UINT slot = 0;
    UINT bytes_per_vertex = 0;
};

bool ValidateInputAndBuffers(const D3D11DrawParams &params,
                             std::vector<SlotRequirement> *requirements)
{
    if (requirements == nullptr || params.input_elements == nullptr ||
        params.input_element_count == 0 ||
        params.input_element_count >
            D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT ||
        params.vertex_buffers == nullptr || params.vertex_buffer_count == 0 ||
        params.vertex_buffer_count >
            D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
        return false;
    }
    requirements->clear();
    for (UINT i = 0; i < params.input_element_count; ++i) {
        const D3D11_INPUT_ELEMENT_DESC &element = params.input_elements[i];
        if (element.SemanticName == nullptr ||
            element.Format == DXGI_FORMAT_UNKNOWN ||
            element.InputSlot >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT ||
            element.InputSlotClass != D3D11_INPUT_PER_VERTEX_DATA ||
            element.InstanceDataStepRate != 0) {
            return false;
        }
        const UINT bytes = FormatBytes(element.Format);
        if (bytes == 0) {
            return false;
        }
        auto found =
            std::find_if(requirements->begin(), requirements->end(),
                         [&element](const SlotRequirement &requirement) {
                             return requirement.slot == element.InputSlot;
                         });
        UINT offset = element.AlignedByteOffset;
        if (offset == D3D11_APPEND_ALIGNED_ELEMENT) {
            offset = found == requirements->end() ? 0 : found->bytes_per_vertex;
        }
        if (offset > UINT_MAX - bytes) {
            return false;
        }
        const UINT end = offset + bytes;
        if (found == requirements->end()) {
            requirements->push_back({ element.InputSlot, end });
        } else {
            found->bytes_per_vertex = (std::max)(found->bytes_per_vertex, end);
        }
    }

    for (UINT i = 0; i < params.vertex_buffer_count; ++i) {
        const D3D11DrawVertexBuffer &buffer = params.vertex_buffers[i];
        if (buffer.data == nullptr || buffer.byte_size == 0 ||
            buffer.stride == 0 ||
            buffer.slot >= D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT ||
            buffer.byte_size > UINT_MAX) {
            return false;
        }
        for (UINT j = 0; j < i; ++j) {
            if (params.vertex_buffers[j].slot == buffer.slot) {
                return false;
            }
        }
        for (const SlotRequirement &requirement : *requirements) {
            if (requirement.slot == buffer.slot &&
                requirement.bytes_per_vertex > buffer.stride) {
                return false;
            }
        }
    }
    for (const SlotRequirement &requirement : *requirements) {
        bool supplied = false;
        for (UINT i = 0; i < params.vertex_buffer_count; ++i) {
            supplied |= params.vertex_buffers[i].slot == requirement.slot;
        }
        if (!supplied) {
            return false;
        }
    }
    return true;
}

bool ValidateCounts(const D3D11DrawParams &params)
{
    uint64_t max_vertex = 0;
    if (params.index_buffer.data != nullptr) {
        if (params.index_count == 0 ||
            params.start_index > params.index_buffer.index_count ||
            params.index_count >
                params.index_buffer.index_count - params.start_index) {
            return false;
        }
        /* Validate every index consumed by DrawIndexed. */
        const size_t index_count = params.index_count;
        if (index_count == 0 ||
            params.index_buffer.index_count > SIZE_MAX / sizeof(uint32_t)) {
            return false;
        }
        for (size_t i = 0; i < index_count; ++i) {
            const uint32_t index =
                params.index_buffer.data[params.start_index + i];
            const int64_t adjusted =
                static_cast<int64_t>(index) + params.base_vertex;
            if (adjusted < 0) {
                return false;
            }
            max_vertex =
                (std::max)(max_vertex, static_cast<uint64_t>(adjusted));
        }
    } else {
        if (params.vertex_count == 0 ||
            static_cast<uint64_t>(params.start_vertex) + params.vertex_count >
                UINT_MAX) {
            return false;
        }
        max_vertex = static_cast<uint64_t>(params.start_vertex) +
                     params.vertex_count - 1;
    }

    for (UINT i = 0; i < params.vertex_buffer_count; ++i) {
        const D3D11DrawVertexBuffer &buffer = params.vertex_buffers[i];
        if (max_vertex == UINT64_MAX ||
            (buffer.stride != 0 &&
             max_vertex + 1 > UINT64_MAX / buffer.stride)) {
            return false;
        }
        const uint64_t bytes_needed = (max_vertex + 1) * buffer.stride;
        if (bytes_needed > buffer.byte_size) {
            return false;
        }
    }
    return true;
}

bool ValidateViewportAndScissor(const D3D11DrawParams &params, UINT width,
                                UINT height)
{
    if (params.has_viewport) {
        const D3D11_VIEWPORT &viewport = params.viewport;
        if (!std::isfinite(viewport.TopLeftX) ||
            !std::isfinite(viewport.TopLeftY) ||
            !std::isfinite(viewport.Width) || !std::isfinite(viewport.Height) ||
            !std::isfinite(viewport.MinDepth) ||
            !std::isfinite(viewport.MaxDepth) || !(viewport.Width > 0.0f) ||
            !(viewport.Height > 0.0f) || viewport.TopLeftX < 0.0f ||
            viewport.TopLeftY < 0.0f ||
            viewport.TopLeftX + viewport.Width > static_cast<float>(width) ||
            viewport.TopLeftY + viewport.Height > static_cast<float>(height) ||
            viewport.MinDepth < 0.0f || viewport.MinDepth > 1.0f ||
            viewport.MaxDepth < 0.0f || viewport.MaxDepth > 1.0f ||
            viewport.MinDepth > viewport.MaxDepth) {
            return false;
        }
    }
    if (params.has_scissor) {
        const D3D11_RECT &scissor = params.scissor;
        if (scissor.left < 0 || scissor.top < 0 ||
            scissor.right <= scissor.left || scissor.bottom <= scissor.top ||
            scissor.right > static_cast<LONG>(width) ||
            scissor.bottom > static_cast<LONG>(height)) {
            return false;
        }
    }
    return true;
}

bool DeviceIsRemoved(ID3D11Device *device, HRESULT *reason)
{
    if (device == nullptr || reason == nullptr) {
        return true;
    }
    *reason = device->GetDeviceRemovedReason();
    return FAILED(*reason);
}

} // namespace

D3D11DrawPrimitive::D3D11DrawPrimitive(ID3D11Device *device,
                                       ID3D11DeviceContext *context)
    : m_device(device), m_context(context), m_owner_thread(GetCurrentThreadId())
{
}

D3D11DrawResult D3D11DrawPrimitive::Draw(const D3D11DrawParams &params) const
{
    if (m_device == nullptr || m_context == nullptr) {
        return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
    }
    if (GetCurrentThreadId() != m_owner_thread) {
        return Result(D3D11DrawStatus::WrongThread, RPC_E_WRONG_THREAD);
    }

    ComPtr<ID3D11Device> context_device;
    m_context->GetDevice(context_device.GetAddressOf());
    if (!SameComIdentity(context_device.Get(), m_device.Get())) {
        return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
    }

    UINT target_width = 0;
    UINT target_height = 0;
    if (!ValidateTargets(params, m_device.Get(), &target_width,
                         &target_height)) {
        return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
    }
    if (!ValidateViewportAndScissor(params, target_width, target_height) ||
        params.vertex_shader_bytecode == nullptr ||
        params.pixel_shader_bytecode == nullptr ||
        params.vertex_shader_bytecode_size == 0 ||
        params.pixel_shader_bytecode_size == 0 ||
        params.vertex_shader_bytecode_size > UINT_MAX ||
        params.pixel_shader_bytecode_size > UINT_MAX ||
        !StateOnDevice(reinterpret_cast<IUnknown *>(params.rasterizer_state),
                       m_device.Get()) ||
        !StateOnDevice(reinterpret_cast<IUnknown *>(params.depth_stencil_state),
                       m_device.Get()) ||
        !StateOnDevice(reinterpret_cast<IUnknown *>(params.blend_state),
                       m_device.Get())) {
        return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
    }

    std::vector<SlotRequirement> requirements;
    if (!ValidateInputAndBuffers(params, &requirements) ||
        !ValidateCounts(params)) {
        return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
    }
    HRESULT removed_reason = S_OK;
    if (DeviceIsRemoved(m_device.Get(), &removed_reason)) {
        return Result(D3D11DrawStatus::DeviceError, removed_reason,
                      removed_reason);
    }

    ComPtr<ID3D11VertexShader> vertex_shader;
    HRESULT hr = m_device->CreateVertexShader(
        params.vertex_shader_bytecode, params.vertex_shader_bytecode_size,
        nullptr, vertex_shader.GetAddressOf());
    if (FAILED(hr)) {
        if (DeviceIsRemoved(m_device.Get(), &removed_reason)) {
            return Result(D3D11DrawStatus::DeviceError, hr, removed_reason);
        }
        return Result(D3D11DrawStatus::ShaderError, hr);
    }
    ComPtr<ID3D11PixelShader> pixel_shader;
    hr = m_device->CreatePixelShader(params.pixel_shader_bytecode,
                                     params.pixel_shader_bytecode_size, nullptr,
                                     pixel_shader.GetAddressOf());
    if (FAILED(hr)) {
        if (DeviceIsRemoved(m_device.Get(), &removed_reason)) {
            return Result(D3D11DrawStatus::DeviceError, hr, removed_reason);
        }
        return Result(D3D11DrawStatus::ShaderError, hr);
    }
    ComPtr<ID3D11InputLayout> input_layout;
    hr = m_device->CreateInputLayout(
        params.input_elements, params.input_element_count,
        params.vertex_shader_bytecode, params.vertex_shader_bytecode_size,
        input_layout.GetAddressOf());
    if (FAILED(hr)) {
        if (DeviceIsRemoved(m_device.Get(), &removed_reason)) {
            return Result(D3D11DrawStatus::DeviceError, hr, removed_reason);
        }
        return Result(D3D11DrawStatus::ShaderError, hr);
    }

    std::vector<ComPtr<ID3D11Buffer>> vertex_buffers;
    std::vector<UINT> vertex_strides;
    std::vector<UINT> vertex_offsets;
    vertex_buffers.resize(params.vertex_buffer_count);
    vertex_strides.resize(params.vertex_buffer_count);
    vertex_offsets.resize(params.vertex_buffer_count);
    for (UINT i = 0; i < params.vertex_buffer_count; ++i) {
        const D3D11DrawVertexBuffer &source = params.vertex_buffers[i];
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(source.byte_size);
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initial = {};
        initial.pSysMem = source.data;
        hr = m_device->CreateBuffer(&desc, &initial,
                                    vertex_buffers[i].GetAddressOf());
        if (FAILED(hr)) {
            return Result(DeviceIsRemoved(m_device.Get(), &removed_reason) ?
                              D3D11DrawStatus::DeviceError :
                              D3D11DrawStatus::Invalid,
                          hr, removed_reason);
        }
        vertex_strides[i] = source.stride;
    }

    ComPtr<ID3D11Buffer> index_buffer;
    if (params.index_buffer.data != nullptr) {
        if (params.index_buffer.index_count > UINT_MAX / sizeof(uint32_t) ||
            params.index_count == 0) {
            return Result(D3D11DrawStatus::Invalid, E_INVALIDARG);
        }
        D3D11_BUFFER_DESC desc = {};
        desc.ByteWidth = static_cast<UINT>(params.index_buffer.index_count *
                                           sizeof(uint32_t));
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initial = {};
        initial.pSysMem = params.index_buffer.data;
        hr = m_device->CreateBuffer(&desc, &initial,
                                    index_buffer.GetAddressOf());
        if (FAILED(hr)) {
            return Result(DeviceIsRemoved(m_device.Get(), &removed_reason) ?
                              D3D11DrawStatus::DeviceError :
                              D3D11DrawStatus::Invalid,
                          hr, removed_reason);
        }
    }

    ID3D11Buffer *buffer_ptrs[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT] = {};
    for (UINT i = 0; i < params.vertex_buffer_count; ++i) {
        const UINT slot = params.vertex_buffers[i].slot;
        buffer_ptrs[slot] = vertex_buffers[i].Get();
        strides[slot] = vertex_strides[i];
    }

    m_context->OMSetRenderTargets(1, &params.render_target,
                                  params.depth_stencil);
    m_context->RSSetState(params.rasterizer_state);
    m_context->OMSetDepthStencilState(params.depth_stencil_state,
                                      params.stencil_ref);
    m_context->OMSetBlendState(params.blend_state, params.blend_factor,
                               params.sample_mask);
    m_context->IASetInputLayout(input_layout.Get());
    m_context->IASetVertexBuffers(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
                                  buffer_ptrs, strides, offsets);
    m_context->IASetIndexBuffer(index_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->VSSetShader(vertex_shader.Get(), nullptr, 0);
    m_context->PSSetShader(pixel_shader.Get(), nullptr, 0);
    if (params.has_viewport) {
        m_context->RSSetViewports(1, &params.viewport);
    }
    if (params.has_scissor) {
        m_context->RSSetScissorRects(1, &params.scissor);
    }
    if (index_buffer != nullptr) {
        m_context->DrawIndexed(params.index_count, params.start_index,
                               params.base_vertex);
    } else {
        m_context->Draw(params.vertex_count, params.start_vertex);
    }

    if (DeviceIsRemoved(m_device.Get(), &removed_reason)) {
        return Result(D3D11DrawStatus::DeviceError, removed_reason,
                      removed_reason);
    }
    return Result(D3D11DrawStatus::Drawn, S_OK, S_OK);
}

} // namespace xemu

#endif // _WIN32
