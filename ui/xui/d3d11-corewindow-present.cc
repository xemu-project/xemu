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

#include "d3d11-corewindow-present.h"

#ifdef _WIN32

#include <limits>

namespace xemu {

namespace {

bool ValidSize(uint32_t width, uint32_t height)
{
    return width != 0 && height != 0 &&
           width <= std::numeric_limits<UINT>::max() &&
           height <= std::numeric_limits<UINT>::max();
}

bool SameTextureShape(const D3D11_TEXTURE2D_DESC &a,
                      const D3D11_TEXTURE2D_DESC &b)
{
    return a.Width == b.Width && a.Height == b.Height &&
           a.MipLevels == b.MipLevels && a.ArraySize == b.ArraySize &&
           a.Format == b.Format && a.SampleDesc.Count == b.SampleDesc.Count &&
           a.SampleDesc.Quality == b.SampleDesc.Quality;
}

bool SameComIdentity(IUnknown *a, IUnknown *b)
{
    if (a == nullptr || b == nullptr) {
        return false;
    }
    Microsoft::WRL::ComPtr<IUnknown> a_identity;
    Microsoft::WRL::ComPtr<IUnknown> b_identity;
    return SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&a_identity))) &&
           SUCCEEDED(b->QueryInterface(IID_PPV_ARGS(&b_identity))) &&
           a_identity.Get() == b_identity.Get();
}

bool IsDeviceLost(HRESULT hr)
{
    return hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_HUNG ||
           hr == DXGI_ERROR_DEVICE_RESET;
}

} // namespace

D3D11CoreWindowPresenter::D3D11CoreWindowPresenter(ID3D11Device *device,
                                                   ID3D11DeviceContext *context)
{
    if (device != nullptr) {
        m_device = device;
    }
    if (context != nullptr) {
        m_context = context;
    }
}

D3D11CoreWindowPresenter::~D3D11CoreWindowPresenter()
{
    ReleaseSwapChain();
}

void D3D11CoreWindowPresenter::ReleaseSwapChain()
{
    m_back_buffer.Reset();
    m_swap_chain.Reset();
    m_core_window.Reset();
    m_width = 0;
    m_height = 0;
    m_last_present_hresult = S_OK;
    m_device_lost = false;
    m_reinitialize_required = false;
}

void D3D11CoreWindowPresenter::EnterReinitializeRequired()
{
    ReleaseSwapChain();
    m_reinitialize_required = true;
}

bool D3D11CoreWindowPresenter::AcquireBackBuffer()
{
    m_back_buffer.Reset();
    if (!m_swap_chain ||
        FAILED(m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&m_back_buffer)))) {
        m_back_buffer.Reset();
        return false;
    }
    return true;
}

bool D3D11CoreWindowPresenter::Initialize(IUnknown *core_window, uint32_t width,
                                          uint32_t height)
{
    ReleaseSwapChain();
    if (!m_device || !m_context || core_window == nullptr ||
        !ValidSize(width, height)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    if (FAILED(m_device.As(&dxgi_device)) ||
        FAILED(dxgi_device->GetAdapter(&adapter)) ||
        FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = static_cast<UINT>(width);
    desc.Height = static_cast<UINT>(height);
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;
    if (FAILED(factory->CreateSwapChainForCoreWindow(
            m_device.Get(), core_window, &desc, nullptr, &swap_chain))) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 actual_desc = {};
    if (FAILED(swap_chain->GetDesc1(&actual_desc)) ||
        actual_desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM ||
        actual_desc.BufferCount != 2 ||
        (actual_desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0 ||
        actual_desc.SwapEffect != DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL ||
        actual_desc.SampleDesc.Count != 1) {
        return false;
    }

    m_core_window = core_window;
    m_swap_chain = swap_chain;
    m_width = actual_desc.Width;
    m_height = actual_desc.Height;
    if (m_width == 0 || m_height == 0 || !AcquireBackBuffer()) {
        ReleaseSwapChain();
        return false;
    }
    return true;
}

bool D3D11CoreWindowPresenter::Resize(uint32_t width, uint32_t height)
{
    if (!m_swap_chain || !m_context || !ValidSize(width, height)) {
        return false;
    }

    m_back_buffer.Reset();
    const HRESULT hr = m_swap_chain->ResizeBuffers(
        2, static_cast<UINT>(width), static_cast<UINT>(height),
        DXGI_FORMAT_B8G8R8A8_UNORM, 0);
    if (FAILED(hr)) {
        EnterReinitializeRequired();
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    if (FAILED(m_swap_chain->GetDesc1(&desc)) ||
        desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM || desc.BufferCount != 2 ||
        (desc.BufferUsage & DXGI_USAGE_RENDER_TARGET_OUTPUT) == 0 ||
        desc.SwapEffect != DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL ||
        desc.SampleDesc.Count != 1 || !AcquireBackBuffer()) {
        EnterReinitializeRequired();
        return false;
    }
    m_width = desc.Width;
    m_height = desc.Height;
    if (m_width != width || m_height != height) {
        EnterReinitializeRequired();
        return false;
    }
    return true;
}

bool D3D11CoreWindowPresenter::IsTextureCompatible(
    ID3D11Texture2D *source) const
{
    if (source == nullptr || m_context == nullptr || m_device == nullptr) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> source_device;
    Microsoft::WRL::ComPtr<ID3D11Device> context_device;
    source->GetDevice(&source_device);
    m_context->GetDevice(&context_device);
    if (!SameComIdentity(source_device.Get(), m_device.Get()) ||
        !SameComIdentity(context_device.Get(), m_device.Get())) {
        return false;
    }
    if (m_back_buffer == nullptr) {
        return false;
    }
    Microsoft::WRL::ComPtr<ID3D11Device> back_buffer_device;
    m_back_buffer->GetDevice(&back_buffer_device);
    if (!SameComIdentity(back_buffer_device.Get(), m_device.Get())) {
        return false;
    }
    D3D11_TEXTURE2D_DESC source_desc = {};
    D3D11_TEXTURE2D_DESC back_buffer_desc = {};
    source->GetDesc(&source_desc);
    m_back_buffer->GetDesc(&back_buffer_desc);
    return SameTextureShape(source_desc, back_buffer_desc);
}

bool D3D11CoreWindowPresenter::CopyTexture(ID3D11Texture2D *source)
{
    if (!m_context || !IsTextureCompatible(source)) {
        return false;
    }
    m_context->CopyResource(m_back_buffer.Get(), source);
    return true;
}

bool D3D11CoreWindowPresenter::Present(UINT sync_interval, UINT flags)
{
    if (m_swap_chain == nullptr) {
        m_last_present_hresult = DXGI_ERROR_INVALID_CALL;
        m_device_lost = false;
        return false;
    }
    m_last_present_hresult = m_swap_chain->Present(sync_interval, flags);
    m_device_lost = IsDeviceLost(m_last_present_hresult);
    return m_last_present_hresult == S_OK;
}

} // namespace xemu

#endif // _WIN32
