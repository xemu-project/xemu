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

#include "win32-dxgi-present.h"

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <epoxy/gl.h>
#include <epoxy/wgl.h>
#include <stdio.h>
#include <atomic>

using Microsoft::WRL::ComPtr;

// Fallback constant definitions for MinGW-w64 compatibility
#ifndef WGL_ACCESS_READ_ONLY_NV
#define WGL_ACCESS_READ_ONLY_NV 0x00000000
#define WGL_ACCESS_READ_WRITE_NV 0x00000001
#define WGL_ACCESS_WRITE_DISCARD_NV 0x00000002
#endif

#ifndef DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 0x00000800UL
#endif

#ifndef DXGI_PRESENT_ALLOW_TEARING
#define DXGI_PRESENT_ALLOW_TEARING 0x00000200UL
#endif

#ifndef DXGI_FEATURE_PRESENT_ALLOW_TEARING
typedef enum DXGI_FEATURE {
    DXGI_FEATURE_PRESENT_ALLOW_TEARING = 0
} DXGI_FEATURE;
#endif

#if !defined(__IDXGIFactory5_INTERFACE_DEFINED__)
struct IDXGIFactory5 : public IDXGIFactory4 {
    virtual HRESULT STDMETHODCALLTYPE
    CheckFeatureSupport(DXGI_FEATURE feature, void *feature_support_data,
                        UINT feature_support_data_size) = 0;
};
#endif

DEFINE_GUID(IID_IDXGIFactory5, 0x7632e1f5, 0xee65, 0x4dca, 0x87, 0xfd, 0x84,
            0xcd, 0x75, 0xf8, 0x83, 0x8d);

typedef HANDLE(WINAPI *pfn_wgl_dx_open_device_nv)(void *dx_device);
typedef BOOL(WINAPI *pfn_wgl_dx_close_device_nv)(HANDLE device);
typedef HANDLE(WINAPI *pfn_wgl_dx_register_object_nv)(HANDLE device,
                                                      void *dx_object,
                                                      GLuint name, GLenum type,
                                                      GLenum access);
typedef BOOL(WINAPI *pfn_wgl_dx_unregister_object_nv)(HANDLE device,
                                                      HANDLE object);
typedef BOOL(WINAPI *pfn_wgl_dx_object_access_nv)(HANDLE object, GLenum access);
typedef BOOL(WINAPI *pfn_wgl_dx_lock_objects_nv)(HANDLE device, GLint count,
                                                 HANDLE *objects);
typedef BOOL(WINAPI *pfn_wgl_dx_unlock_objects_nv)(HANDLE device, GLint count,
                                                   HANDLE *objects);

class Win32DxgiPresenter {
public:
    Win32DxgiPresenter() = default;

    ~Win32DxgiPresenter()
    {
        Cleanup();
    }

    bool Init(SDL_Window *window);
    void Cleanup();
    bool IsActive() const
    {
        return m_active.load(std::memory_order_acquire);
    }
    void BeginFrame();
    void EndFrame(bool vsync);
    void Resize(int width, int height);

private:
    bool LoadWglInteropFunctions();
    bool CreateSharedResources(int width, int height);
    void ReleaseSharedResources();
    bool PerformResize(int width, int height);

    static inline uint64_t PackDimensions(int width, int height)
    {
        return (static_cast<uint64_t>(static_cast<uint32_t>(width)) << 32) |
               static_cast<uint64_t>(static_cast<uint32_t>(height));
    }

    static inline void UnpackDimensions(uint64_t packed, int &width,
                                        int &height)
    {
        width = static_cast<int>(static_cast<uint32_t>(packed >> 32));
        height = static_cast<int>(static_cast<uint32_t>(packed & 0xFFFFFFFF));
    }

    std::atomic<bool> m_active{ false };
    bool m_allow_tearing = false;
    std::atomic<uint64_t> m_pending_size{ 0 };
    int m_width = 0;
    int m_height = 0;

    SDL_Window *m_window = nullptr;

    ComPtr<ID3D11Device> m_d3d_device;
    ComPtr<ID3D11DeviceContext> m_d3d_context;
    ComPtr<IDXGISwapChain1> m_swap_chain;
    ComPtr<ID3D11Texture2D> m_shared_texture;

    HANDLE m_wgl_device = nullptr;
    HANDLE m_wgl_object = nullptr;

    GLuint m_render_tex = 0;
    GLuint m_render_fbo = 0;

    GLuint m_interop_tex = 0;
    GLuint m_interop_fbo = 0;

    pfn_wgl_dx_open_device_nv m_wgl_dx_open_device_nv = nullptr;
    pfn_wgl_dx_close_device_nv m_wgl_dx_close_device_nv = nullptr;
    pfn_wgl_dx_register_object_nv m_wgl_dx_register_object_nv = nullptr;
    pfn_wgl_dx_unregister_object_nv m_wgl_dx_unregister_object_nv = nullptr;
    pfn_wgl_dx_lock_objects_nv m_wgl_dx_lock_objects_nv = nullptr;
    pfn_wgl_dx_unlock_objects_nv m_wgl_dx_unlock_objects_nv = nullptr;
};

bool Win32DxgiPresenter::LoadWglInteropFunctions()
{
    m_wgl_dx_open_device_nv =
        (pfn_wgl_dx_open_device_nv)SDL_GL_GetProcAddress("wglDXOpenDeviceNV");
    m_wgl_dx_close_device_nv =
        (pfn_wgl_dx_close_device_nv)SDL_GL_GetProcAddress("wglDXCloseDeviceNV");
    m_wgl_dx_register_object_nv =
        (pfn_wgl_dx_register_object_nv)SDL_GL_GetProcAddress(
            "wglDXRegisterObjectNV");
    m_wgl_dx_unregister_object_nv =
        (pfn_wgl_dx_unregister_object_nv)SDL_GL_GetProcAddress(
            "wglDXUnregisterObjectNV");
    m_wgl_dx_lock_objects_nv =
        (pfn_wgl_dx_lock_objects_nv)SDL_GL_GetProcAddress("wglDXLockObjectsNV");
    m_wgl_dx_unlock_objects_nv =
        (pfn_wgl_dx_unlock_objects_nv)SDL_GL_GetProcAddress(
            "wglDXUnlockObjectsNV");

    return m_wgl_dx_open_device_nv && m_wgl_dx_close_device_nv &&
           m_wgl_dx_register_object_nv && m_wgl_dx_unregister_object_nv &&
           m_wgl_dx_lock_objects_nv && m_wgl_dx_unlock_objects_nv;
}

void Win32DxgiPresenter::ReleaseSharedResources()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (m_render_fbo) {
        glDeleteFramebuffers(1, &m_render_fbo);
        m_render_fbo = 0;
    }
    if (m_render_tex) {
        glDeleteTextures(1, &m_render_tex);
        m_render_tex = 0;
    }

    if (m_interop_fbo) {
        glDeleteFramebuffers(1, &m_interop_fbo);
        m_interop_fbo = 0;
    }

    if (m_wgl_device && m_wgl_object) {
        m_wgl_dx_unregister_object_nv(m_wgl_device, m_wgl_object);
        m_wgl_object = nullptr;
    }

    if (m_interop_tex) {
        glDeleteTextures(1, &m_interop_tex);
        m_interop_tex = 0;
    }

    m_shared_texture.Reset();
}

bool Win32DxgiPresenter::CreateSharedResources(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    ReleaseSharedResources();

    auto create_texture = [](GLuint &texture) {
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };

    auto create_fbo = [this, width, height](GLuint &fbo,
                                            GLuint framebuffer_tex) {
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, framebuffer_tex, 0);
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr,
                    "win32_dxgi_present: FBO incomplete (status=0x%X, "
                    "GLError=%u, w=%d, h=%d)\n",
                    status, glGetError(), width, height);
            ReleaseSharedResources();
            return false;
        }

        return true;
    };

    create_texture(m_render_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!create_fbo(m_render_fbo, m_render_tex)) {
        return false;
    }

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = static_cast<UINT>(width);
    tex_desc.Height = static_cast<UINT>(height);
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    tex_desc.SampleDesc.Quality = 0;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    HRESULT res =
        m_d3d_device->CreateTexture2D(&tex_desc, nullptr, &m_shared_texture);
    if (FAILED(res)) {
        fprintf(stderr,
                "win32_dxgi_present: Failed to create D3D11 shared texture "
                "(hr=0x%08lX)\n",
                (unsigned long)res);
        ReleaseSharedResources();
        return false;
    }

    create_texture(m_interop_tex);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_wgl_object = m_wgl_dx_register_object_nv(
        m_wgl_device, m_shared_texture.Get(), m_interop_tex, GL_TEXTURE_2D,
        WGL_ACCESS_READ_WRITE_NV);
    if (!m_wgl_object) {
        fprintf(stderr,
                "win32_dxgi_present: wglDXRegisterObjectNV failed (GLError=%u, "
                "WinError=0x%X)\n",
                glGetError(), static_cast<unsigned int>(GetLastError()));
        ReleaseSharedResources();
        return false;
    }

    if (!create_fbo(m_interop_fbo, m_interop_tex)) {
        return false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_width = width;
    m_height = height;

    return true;
}

bool Win32DxgiPresenter::PerformResize(int width, int height)
{
    if (!m_active.load(std::memory_order_acquire) || width <= 0 ||
        height <= 0 ||
        (width == m_width && height == m_height && m_render_fbo)) {
        return true;
    }

    ReleaseSharedResources();

    if (m_d3d_context) {
        m_d3d_context->ClearState();
        m_d3d_context->Flush();
    }

    UINT flags = m_allow_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT res = m_swap_chain->ResizeBuffers(0, static_cast<UINT>(width),
                                              static_cast<UINT>(height),
                                              DXGI_FORMAT_UNKNOWN, flags);
    if (FAILED(res)) {
        fprintf(stderr,
                "win32_dxgi_present: ResizeBuffers failed (hr=0x%08lX, w=%d, "
                "h=%d, flags=0x%X)\n",
                (unsigned long)res, width, height, flags);
        Cleanup();
        return false;
    }

    if (!CreateSharedResources(width, height)) {
        fprintf(stderr, "win32_dxgi_present: Failed to recreate shared "
                        "resources after resize\n");
        Cleanup();
        return false;
    }

    return true;
}

bool Win32DxgiPresenter::Init(SDL_Window *window)
{
    if (m_active.load(std::memory_order_acquire)) {
        return true;
    }

    m_window = window;

    auto hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                               SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (!hwnd) {
        fprintf(stderr,
                "win32_dxgi_present: Failed to obtain HWND from SDL_Window\n");
        return false;
    }

    if (!LoadWglInteropFunctions()) {
        fprintf(stderr, "win32_dxgi_present: WGL_NV_DX_interop2 extensions not "
                        "available\n");
        return false;
    }

    ComPtr<IDXGIFactory2> dxgi_factory;
    HRESULT res = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));
    if (FAILED(res)) {
        fprintf(stderr,
                "win32_dxgi_present: CreateDXGIFactory1 failed (hr=0x%08lX)\n",
                (unsigned long)res);
        return false;
    }

    HMONITOR target_monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    ComPtr<IDXGIAdapter1> matched_adapter;
    UINT adapter_idx = 0;
    ComPtr<IDXGIAdapter1> adapter;
    while (dxgi_factory->EnumAdapters1(adapter_idx, &adapter) !=
           DXGI_ERROR_NOT_FOUND) {
        UINT output_idx = 0;
        ComPtr<IDXGIOutput> output;
        while (adapter->EnumOutputs(output_idx, &output) !=
               DXGI_ERROR_NOT_FOUND) {
            DXGI_OUTPUT_DESC out_desc = {};
            if (SUCCEEDED(output->GetDesc(&out_desc)) &&
                out_desc.Monitor == target_monitor) {
                matched_adapter = adapter;
                break;
            }
            output_idx++;
        }
        if (matched_adapter) {
            break;
        }
        adapter_idx++;
    }

    ComPtr<IDXGIAdapter1> selected_adapter = matched_adapter;
    if (!selected_adapter) {
        dxgi_factory->EnumAdapters1(0, &selected_adapter);
        fprintf(stderr, "win32_dxgi_present: Target monitor output not matched "
                        "to specific adapter, fallback to Adapter[0]\n");
    }

    UINT create_device_flags = 0;

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    res = D3D11CreateDevice(
        selected_adapter ? selected_adapter.Get() : nullptr,
        selected_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE,
        nullptr, create_device_flags, feature_levels, ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION, &m_d3d_device, &feature_level, &m_d3d_context);
    if (FAILED(res)) {
        fprintf(stderr,
                "win32_dxgi_present: D3D11CreateDevice failed (hr=0x%08lX)\n",
                (unsigned long)res);
        return false;
    }

    selected_adapter.Reset();
    matched_adapter.Reset();
    adapter.Reset();

    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    if (width <= 0)
        width = 640;
    if (height <= 0)
        height = 480;

    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = static_cast<UINT>(width);
    swap_chain_desc.Height = static_cast<UINT>(height);
    swap_chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = 2;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.Flags = 0;

    m_allow_tearing = false;
    ComPtr<IDXGIFactory5> factory5;
    if (SUCCEEDED(dxgi_factory->QueryInterface(IID_IDXGIFactory5,
                                               (void **)&factory5))) {
        BOOL allow_tearing = FALSE;
        if (SUCCEEDED(factory5->CheckFeatureSupport(
                DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing,
                sizeof(allow_tearing))) &&
            allow_tearing) {
            swap_chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
            m_allow_tearing = true;
        }
    } else {
        fprintf(
            stderr,
            "win32_dxgi_present: QueryInterface(IDXGIFactory5) unsupported\n");
    }

    res = dxgi_factory->CreateSwapChainForHwnd(m_d3d_device.Get(), hwnd,
                                               &swap_chain_desc, nullptr,
                                               nullptr, &m_swap_chain);
    if (FAILED(res)) {
        fprintf(stderr,
                "win32_dxgi_present: CreateSwapChainForHwnd (flip discard) "
                "failed (hr=0x%08lX) - falling back to legacy discard\n",
                (unsigned long)res);

        swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        swap_chain_desc.Flags &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        m_allow_tearing = false;

        res = dxgi_factory->CreateSwapChainForHwnd(m_d3d_device.Get(), hwnd,
                                                   &swap_chain_desc, nullptr,
                                                   nullptr, &m_swap_chain);
        if (FAILED(res)) {
            fprintf(stderr,
                    "win32_dxgi_present: CreateSwapChainForHwnd failed "
                    "(hr=0x%08lX)\n",
                    (unsigned long)res);
            Cleanup();
            return false;
        }
    }

    dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    m_wgl_device = m_wgl_dx_open_device_nv(m_d3d_device.Get());
    if (!m_wgl_device) {
        fprintf(stderr,
                "win32_dxgi_present: wglDXOpenDeviceNV failed (GLError=%u, "
                "WinError=0x%X)\n",
                glGetError(), static_cast<unsigned int>(GetLastError()));
        Cleanup();
        return false;
    }

    if (!CreateSharedResources(width, height)) {
        Cleanup();
        return false;
    }

    m_active.store(true, std::memory_order_release);
    fprintf(stderr,
            "win32_dxgi_present: DXGI Presentation Helper successfully "
            "initialized (mode=%s, tearing=%s)\n",
            swap_chain_desc.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD ?
                "flip" :
                "legacy",
            m_allow_tearing ? "supported" : "unsupported");
    return true;
}

void Win32DxgiPresenter::Cleanup()
{
    m_active.store(false, std::memory_order_release);
    m_pending_size.store(0, std::memory_order_release);
    ReleaseSharedResources();

    if (m_d3d_context) {
        m_d3d_context->ClearState();
        m_d3d_context->Flush();
    }

    if (m_wgl_device) {
        if (m_wgl_dx_close_device_nv) {
            m_wgl_dx_close_device_nv(m_wgl_device);
        }
        m_wgl_device = nullptr;
    }

    m_swap_chain.Reset();
    m_d3d_context.Reset();
    m_d3d_device.Reset();
    m_window = nullptr;
    m_width = 0;
    m_height = 0;
}

void Win32DxgiPresenter::BeginFrame()
{
    if (!m_active.load(std::memory_order_acquire)) {
        return;
    }

    uint64_t packed_size =
        m_pending_size.exchange(0, std::memory_order_acq_rel);
    if (packed_size) {
        int target_width = 0;
        int target_height = 0;
        UnpackDimensions(packed_size, target_width, target_height);
        if (target_width > 0 && target_height > 0) {
            PerformResize(target_width, target_height);
        }
    }

    if (!m_render_fbo) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, m_render_fbo);
    glViewport(0, 0, m_width, m_height);
}

void Win32DxgiPresenter::EndFrame(bool vsync)
{
    if (!m_active.load(std::memory_order_acquire) || !m_wgl_object ||
        !m_render_fbo || !m_interop_fbo) {
        return;
    }

    if (!m_wgl_dx_lock_objects_nv(m_wgl_device, 1, &m_wgl_object)) {
        fprintf(stderr,
                "Win32DxgiPresenter::EndFrame: wglDXLockObjectsNV failed "
                "(WinError=0x%X)\n",
                static_cast<unsigned int>(GetLastError()));
        Cleanup();
        return;
    }
    m_wgl_dx_lock_objects_nv(m_wgl_device, 1, &m_wgl_object);

    // Blit from render FBO to interop FBO with vertical flip to convert OpenGL
    // (Y-up) to D3D11 (Y-down)
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_render_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_interop_fbo);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, m_height, m_width, 0,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_wgl_dx_unlock_objects_nv(m_wgl_device, 1, &m_wgl_object);

    ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT res = m_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (SUCCEEDED(res)) {
        m_d3d_context->CopyResource(back_buffer.Get(), m_shared_texture.Get());
    }

    UINT sync_interval = vsync ? 1 : 0;
    UINT present_flags =
        (!vsync && m_allow_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    res = m_swap_chain->Present(sync_interval, present_flags);
    if (FAILED(res)) {
        fprintf(stderr,
                "Win32DxgiPresenter::EndFrame: Present failed (hr=0x%08lX)\n",
                (unsigned long)res);
        Cleanup();
    }
}

void Win32DxgiPresenter::Resize(int width, int height)
{
    if (!m_active.load(std::memory_order_acquire) || width <= 0 ||
        height <= 0) {
        return;
    }

    m_pending_size.store(PackDimensions(width, height),
                         std::memory_order_release);
}

static Win32DxgiPresenter g_dxgi_presenter;

extern "C" {

bool win32_dxgi_present_init(SDL_Window *window)
{
    return g_dxgi_presenter.Init(window);
}

void win32_dxgi_present_cleanup(void)
{
    g_dxgi_presenter.Cleanup();
}

bool win32_dxgi_present_is_active(void)
{
    return g_dxgi_presenter.IsActive();
}

void win32_dxgi_present_begin_frame(void)
{
    g_dxgi_presenter.BeginFrame();
}

void win32_dxgi_present_end_frame(bool vsync)
{
    g_dxgi_presenter.EndFrame(vsync);
}

void win32_dxgi_present_resize(int width, int height)
{
    g_dxgi_presenter.Resize(width, height);
}

} // extern "C"

#else // !_WIN32

extern "C" {

bool win32_dxgi_present_init(SDL_Window *window)
{
    (void)window;
    return false;
}
void win32_dxgi_present_cleanup(void)
{
}
bool win32_dxgi_present_is_active(void)
{
    return false;
}
void win32_dxgi_present_begin_frame(void)
{
}
void win32_dxgi_present_end_frame(bool vsync)
{
    (void)vsync;
}
void win32_dxgi_present_resize(int width, int height)
{
    (void)width;
    (void)height;
}

} // extern "C"

#endif // _WIN32
