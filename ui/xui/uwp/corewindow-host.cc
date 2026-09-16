/*
 * SDL-free Windows UWP CoreWindow host.
 *
 * Copyright (C) 2026 xemu Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "corewindow-host.h"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <d3d11.h>
#include <collection.h>
#include <roapi.h>
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.applicationmodel.core.h>
#include <windows.foundation.h>
#include <windows.ui.core.h>

#include <cstdint>
#include <memory>

#include "../d3d11-corewindow-present.h"

using Microsoft::WRL::ComPtr;
using namespace Platform;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation;
using namespace Windows::UI::Core;

namespace xemu_uwp {

namespace {

bool ToPixels(float value, uint32_t *out)
{
    if (out == nullptr || value <= 0.0f || value > UINT32_MAX) {
        return false;
    }
    *out = static_cast<uint32_t>(value + 0.5f);
    return *out != 0;
}

bool WindowSize(CoreWindow ^window, uint32_t *width, uint32_t *height)
{
    if (window == nullptr || width == nullptr || height == nullptr) {
        return false;
    }
    const Rect bounds = window->Bounds;
    return ToPixels(bounds.Width, width) && ToPixels(bounds.Height, height);
}

bool CreateDevice(ComPtr<ID3D11Device> *device,
                 ComPtr<ID3D11DeviceContext> *context)
{
    if (device == nullptr || context == nullptr) {
        return false;
    }

    const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL selected = D3D_FEATURE_LEVEL_11_0;
    const UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels,
        ARRAYSIZE(levels), D3D11_SDK_VERSION, device->ReleaseAndGetAddressOf(),
        &selected, context->ReleaseAndGetAddressOf());
    if (SUCCEEDED(hr)) {
        return true;
    }

    /* WARP keeps Windows CI useful when no hardware adapter is exposed. The
     * Xbox package normally takes the hardware path and does not depend on
     * WARP being present on the console. */
    hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, levels,
        ARRAYSIZE(levels), D3D11_SDK_VERSION, device->ReleaseAndGetAddressOf(),
        &selected, context->ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

} // namespace

ref class CoreWindowSource;

ref class CoreWindowView sealed : IFrameworkView {
    Agile<CoreWindow> window_;
    XemuUwpCoreCallbacks callbacks_ = {};
    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    std::unique_ptr<xemu::D3D11CoreWindowPresenter> presenter_;
    Windows::Foundation::EventRegistrationToken size_changed_token_ = {};
    Windows::Foundation::EventRegistrationToken closed_token_ = {};
    bool running_ = true;
    bool callbacks_initialized_ = false;

    void StopCallbacks()
    {
        if (callbacks_initialized_ && callbacks_.shutdown != nullptr) {
            callbacks_.shutdown(callbacks_.opaque);
        }
        callbacks_initialized_ = false;
    }

    void OnSizeChanged(CoreWindow ^sender, WindowSizeChangedEventArgs ^args)
    {
        (void)sender;
        if (presenter_ == nullptr || args == nullptr) {
            return;
        }

        uint32_t width = 0;
        uint32_t height = 0;
        if (!ToPixels(args->Size.Width, &width) ||
            !ToPixels(args->Size.Height, &height)) {
            return;
        }
        if (!presenter_->Resize(width, height)) {
            running_ = false;
            return;
        }
        if (callbacks_.resize != nullptr &&
            !callbacks_.resize(callbacks_.opaque, width, height)) {
            running_ = false;
        }
    }

    void OnClosed(CoreWindow ^sender, CoreWindowEventArgs ^args)
    {
        (void)sender;
        (void)args;
        running_ = false;
    }

    bool InitializeGraphics(uint32_t width, uint32_t height)
    {
        if (!CreateDevice(&device_, &context_)) {
            return false;
        }

        presenter_ = std::make_unique<xemu::D3D11CoreWindowPresenter>(
            device_.Get(), context_.Get());
        IUnknown *window_unknown = reinterpret_cast<IUnknown *>(window_.Get());
        if (!presenter_->Initialize(window_unknown, width, height)) {
            presenter_.reset();
            return false;
        }

        if (callbacks_.initialize == nullptr ||
            !callbacks_.initialize(callbacks_.opaque, device_.Get(),
                                    context_.Get(), window_unknown,
                                    presenter_->width(), presenter_->height())) {
            presenter_.reset();
            return false;
        }
        callbacks_initialized_ = true;
        return true;
    }

    void RenderFrame()
    {
        if (presenter_ == nullptr || callbacks_.render == nullptr) {
            running_ = false;
            return;
        }
        ID3D11Texture2D *texture = nullptr;
        if (!callbacks_.render(callbacks_.opaque, &texture) || texture == nullptr ||
            !presenter_->CopyTexture(texture) || !presenter_->Present(1, 0)) {
            running_ = false;
        }
    }

    friend ref class CoreWindowSource;

    explicit CoreWindowView(const XemuUwpCoreCallbacks &callbacks)
        : callbacks_(callbacks)
    {
    }

public:

    virtual void Initialize(CoreApplicationView ^application_view)
    {
        (void)application_view;
    }

    virtual void SetWindow(CoreWindow ^window)
    {
        window_ = window;
        if (window != nullptr) {
            size_changed_token_ = window->SizeChanged +=
                ref new TypedEventHandler<CoreWindow ^,
                                          WindowSizeChangedEventArgs ^>(
                    this, &CoreWindowView::OnSizeChanged);
            closed_token_ = window->Closed +=
                ref new TypedEventHandler<CoreWindow ^, CoreWindowEventArgs ^>(
                    this, &CoreWindowView::OnClosed);
        }
    }

    virtual void Load(String ^entry_point)
    {
        (void)entry_point;
    }

    virtual void Uninitialize()
    {
        StopCallbacks();
        presenter_.reset();
        context_.Reset();
        device_.Reset();
    }

    virtual void Run()
    {
        CoreWindow ^window = window_.Get();
        uint32_t width = 0;
        uint32_t height = 0;
        if (window == nullptr || !WindowSize(window, &width, &height) ||
            !InitializeGraphics(width, height)) {
            StopCallbacks();
            running_ = false;
            return;
        }

        window->Activate();
        while (running_) {
            window->Dispatcher->ProcessEvents(
                CoreProcessEventsOption::ProcessAllIfPresent);
            if (running_) {
                RenderFrame();
            }
            SwitchToThread();
        }
        StopCallbacks();
    }
};

ref class CoreWindowSource sealed : IFrameworkViewSource {
    XemuUwpCoreCallbacks callbacks_ = {};

public:
    internal:
    explicit CoreWindowSource(const XemuUwpCoreCallbacks &callbacks)
        : callbacks_(callbacks)
    {
    }

public:
    virtual IFrameworkView ^CreateView()
    {
        return ref new CoreWindowView(callbacks_);
    }
};

} // namespace xemu_uwp

extern "C" int xemu_uwp_run(const XemuUwpCoreCallbacks *callbacks)
{
    if (callbacks == nullptr || callbacks->initialize == nullptr ||
        callbacks->render == nullptr || callbacks->shutdown == nullptr) {
        return 1;
    }

    const XemuUwpCoreCallbacks copy = *callbacks;
    Microsoft::WRL::Wrappers::RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
    if (FAILED(initialize)) {
        return 1;
    }
    try {
        CoreApplication::Run(ref new xemu_uwp::CoreWindowSource(copy));
    } catch (Exception ^) {
        return 1;
    }
    return 0;
}

#else

extern "C" int xemu_uwp_run(const XemuUwpCoreCallbacks *callbacks)
{
    (void)callbacks;
    return 1;
}

#endif /* _WIN32 */
