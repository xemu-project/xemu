//
// xemu User Interface
//
// Copyright (C) 2026 Matt Borgerson
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
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "d3d11-present.h"

#ifdef _WIN32

#include <algorithm>
#include <limits>

namespace xemu {

D3D11FrameUploader::D3D11FrameUploader(ID3D11Device *device,
                                       ID3D11DeviceContext *context)
    : m_device(device), m_context(context)
{
    if (m_device != nullptr) {
        m_device->AddRef();
    }
    if (m_context != nullptr) {
        m_context->AddRef();
    }
}

D3D11FrameUploader::~D3D11FrameUploader()
{
    ReleaseTexture();
    if (m_context != nullptr) {
        m_context->Release();
    }
    if (m_device != nullptr) {
        m_device->Release();
    }
}

void D3D11FrameUploader::ReleaseTexture()
{
    if (m_shader_resource_view != nullptr) {
        m_shader_resource_view->Release();
        m_shader_resource_view = nullptr;
    }
    if (m_texture != nullptr) {
        m_texture->Release();
        m_texture = nullptr;
    }
    m_width = 0;
    m_height = 0;
}

bool D3D11FrameUploader::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        ReleaseTexture();
        return true;
    }
    if (m_device == nullptr || m_context == nullptr ||
        (width == m_width && height == m_height && m_texture != nullptr)) {
        return m_texture != nullptr && width == m_width && height == m_height;
    }

    if (width > std::numeric_limits<UINT>::max() ||
        height > std::numeric_limits<UINT>::max()) {
        return false;
    }

    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = static_cast<UINT>(width);
    texture_desc.Height = static_cast<UINT>(height);
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 1;
    texture_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DYNAMIC;
    texture_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    texture_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Texture2D *texture = nullptr;
    HRESULT hr = m_device->CreateTexture2D(&texture_desc, nullptr, &texture);
    if (FAILED(hr)) {
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC view_desc = {};
    view_desc.Format = texture_desc.Format;
    view_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView *shader_resource_view = nullptr;
    hr = m_device->CreateShaderResourceView(texture, &view_desc,
                                            &shader_resource_view);
    if (FAILED(hr)) {
        texture->Release();
        return false;
    }

    ReleaseTexture();
    m_texture = texture;
    m_shader_resource_view = shader_resource_view;
    m_width = width;
    m_height = height;
    return true;
}

bool D3D11FrameUploader::Upload(const D3D11FrameView &frame)
{
    if (frame.data == nullptr ||
        frame.format != D3D11FrameFormat::BGRA8_UNORM || frame.width == 0 ||
        frame.height == 0 || frame.width != m_width ||
        frame.height != m_height || m_texture == nullptr ||
        m_context == nullptr) {
        return false;
    }

    constexpr uint32_t bytes_per_pixel = 4;
    if (frame.width > std::numeric_limits<uint32_t>::max() / bytes_per_pixel) {
        return false;
    }
    const uint32_t row_bytes = frame.width * bytes_per_pixel;
    if (frame.stride < row_bytes ||
        (frame.height > 1 &&
         frame.stride > (std::numeric_limits<size_t>::max() - row_bytes) /
                            (frame.height - 1))) {
        return false;
    }
    if (frame.orientation != D3D11FrameOrientation::TopDown &&
        frame.orientation != D3D11FrameOrientation::BottomUp) {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr =
        m_context->Map(m_texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        return false;
    }
    if (mapped.pData == nullptr || mapped.RowPitch < row_bytes ||
        (frame.height > 1 &&
         mapped.RowPitch > (std::numeric_limits<size_t>::max() - row_bytes) /
                               (frame.height - 1))) {
        m_context->Unmap(m_texture, 0);
        return false;
    }

    const uint8_t *source = static_cast<const uint8_t *>(frame.data);
    uint8_t *destination = static_cast<uint8_t *>(mapped.pData);
    for (uint32_t y = 0; y < frame.height; ++y) {
        const uint32_t source_y =
            frame.orientation == D3D11FrameOrientation::TopDown ?
                y :
                frame.height - 1 - y;
        std::copy_n(source + static_cast<size_t>(source_y) * frame.stride,
                    row_bytes,
                    destination + static_cast<size_t>(y) * mapped.RowPitch);
    }
    m_context->Unmap(m_texture, 0);
    return true;
}

} // namespace xemu

#endif // _WIN32
