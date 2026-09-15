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

#ifndef XEMU_D3D11_PRESENT_H
#define XEMU_D3D11_PRESENT_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <stdint.h>

namespace xemu {

enum class D3D11FrameFormat {
    BGRA8_UNORM,
    RGBA8_UNORM,
};

enum class D3D11FrameOrientation {
    TopDown,
    BottomUp,
};

struct D3D11FrameView {
    const void *data = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t stride = 0;
    D3D11FrameFormat format = D3D11FrameFormat::BGRA8_UNORM;
    D3D11FrameOrientation orientation = D3D11FrameOrientation::TopDown;
};

/**
 * Uploads CPU-owned BGRA8 frames to a shader-readable D3D11 texture.
 *
 * The device and context are retained for the lifetime of this object. The
 * source frame remains owned by the caller and is only accessed by Upload().
 * Calls that use this object must be serialized with other uses of the
 * retained immediate context; this class does not provide synchronization.
 */
class D3D11FrameUploader {
public:
    D3D11FrameUploader(ID3D11Device *device, ID3D11DeviceContext *context);
    ~D3D11FrameUploader();

    D3D11FrameUploader(const D3D11FrameUploader &) = delete;
    D3D11FrameUploader &operator=(const D3D11FrameUploader &) = delete;

    bool Resize(uint32_t width, uint32_t height);
    bool Upload(const D3D11FrameView &frame);

    uint32_t width() const
    {
        return m_width;
    }
    uint32_t height() const
    {
        return m_height;
    }
    ID3D11Texture2D *texture() const
    {
        return m_texture;
    }
    ID3D11ShaderResourceView *shader_resource_view() const
    {
        return m_shader_resource_view;
    }

private:
    void ReleaseTexture();

    ID3D11Device *m_device = nullptr;
    ID3D11DeviceContext *m_context = nullptr;
    ID3D11Texture2D *m_texture = nullptr;
    ID3D11ShaderResourceView *m_shader_resource_view = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_D3D11_PRESENT_H
