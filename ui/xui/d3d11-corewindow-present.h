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

#ifndef XEMU_D3D11_COREWINDOW_PRESENT_H
#define XEMU_D3D11_COREWINDOW_PRESENT_H

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <dxgi1_2.h>
#include <stdint.h>
#include <wrl/client.h>

namespace xemu {

/**
 * Owns the D3D11/DXGI objects needed to present into a Windows CoreWindow.
 *
 * The CoreWindow is deliberately passed as its ABI IUnknown.  This keeps the
 * presenter independent of C++/CX and lets callers obtain that pointer from
 * any WinRT projection.  The presenter retains the device, context, window,
 * swap chain, and current back buffer with ComPtr; the back_buffer() accessor
 * is a borrowed pointer for diagnostic readback and must not be released.
 * Calls using the immediate context must be serialized by the caller.
 */
class D3D11CoreWindowPresenter final {
public:
    D3D11CoreWindowPresenter(ID3D11Device *device,
                             ID3D11DeviceContext *context);
    ~D3D11CoreWindowPresenter();

    D3D11CoreWindowPresenter(const D3D11CoreWindowPresenter &) = delete;
    D3D11CoreWindowPresenter &
    operator=(const D3D11CoreWindowPresenter &) = delete;

    /** Create a two-buffer BGRA8 flip-sequential CoreWindow swap chain. */
    bool Initialize(IUnknown *core_window, uint32_t width, uint32_t height);

    /** Resize the existing swap chain and reacquire its back buffer. */
    bool Resize(uint32_t width, uint32_t height);

    /** Return whether source can be copied to the current back buffer. */
    bool IsTextureCompatible(ID3D11Texture2D *source) const;

    /** Copy a compatible source texture into the current back buffer. */
    bool CopyTexture(ID3D11Texture2D *source);

    /** Present the current back buffer. */
    bool Present(UINT sync_interval = 1, UINT flags = 0);

    /** HRESULT from the most recent Present call, including failure cases. */
    HRESULT last_present_hresult() const
    {
        return m_last_present_hresult;
    }

    /** True when Present reported a device-removal/reset/hang condition. */
    bool device_lost() const
    {
        return m_device_lost;
    }

    /** Resize failure requires a fresh Initialize call. */
    bool needs_reinitialize() const
    {
        return m_reinitialize_required;
    }

    /* Borrowed, read-only-by-convention diagnostic view; do not Release(). */
    ID3D11Texture2D *back_buffer() const
    {
        return m_back_buffer.Get();
    }

    IDXGISwapChain1 *swap_chain() const
    {
        return m_swap_chain.Get();
    }

    uint32_t width() const
    {
        return m_width;
    }
    uint32_t height() const
    {
        return m_height;
    }

private:
    bool AcquireBackBuffer();
    void ReleaseSwapChain();
    void EnterReinitializeRequired();

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IUnknown> m_core_window;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swap_chain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_back_buffer;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    HRESULT m_last_present_hresult = S_OK;
    bool m_device_lost = false;
    bool m_reinitialize_required = false;
};

} // namespace xemu

#endif // _WIN32

#endif // XEMU_D3D11_COREWINDOW_PRESENT_H
