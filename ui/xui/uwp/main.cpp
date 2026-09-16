/*
 * Minimal UWP entry point for xemu on Xbox Dev Mode.
 *
 * Copyright (C) 2026 xemu Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "corewindow-host.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wrl/client.h>

namespace {

struct AppState {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer_texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> render_target_view;
    uint32_t width = 0;
    uint32_t height = 0;
};

bool CreateBackingTexture(AppState *state, uint32_t width, uint32_t height)
{
    if (state == nullptr || state->device == nullptr || width == 0 ||
        height == 0) {
        return false;
    }

    state->width = width;
    state->height = height;
    state->render_target_view.Reset();
    state->back_buffer_texture.Reset();

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = state->device->CreateTexture2D(
        &desc, nullptr, state->back_buffer_texture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = state->device->CreateRenderTargetView(
        state->back_buffer_texture.Get(), nullptr,
        state->render_target_view.ReleaseAndGetAddressOf());
    return SUCCEEDED(hr);
}

bool OnInitialize(void *opaque, ID3D11Device *device,
                  ID3D11DeviceContext *context, IUnknown *core_window,
                  uint32_t width, uint32_t height)
{
    (void)core_window;
    auto *state = static_cast<AppState *>(opaque);
    if (state == nullptr || device == nullptr || context == nullptr) {
        return false;
    }
    state->device = device;
    state->context = context;
    return CreateBackingTexture(state, width, height);
}

bool OnResize(void *opaque, uint32_t width, uint32_t height)
{
    auto *state = static_cast<AppState *>(opaque);
    return CreateBackingTexture(state, width, height);
}

bool OnRender(void *opaque, ID3D11Texture2D **texture)
{
    auto *state = static_cast<AppState *>(opaque);
    if (state == nullptr || state->back_buffer_texture == nullptr ||
        texture == nullptr) {
        return false;
    }

    if (state->context != nullptr && state->render_target_view != nullptr) {
        const FLOAT clear_color[4] = { 0.05f, 0.05f, 0.05f, 1.0f };
        state->context->ClearRenderTargetView(state->render_target_view.Get(),
                                              clear_color);
    }

    *texture = state->back_buffer_texture.Get();
    return true;
}

void OnShutdown(void *opaque)
{
    auto *state = static_cast<AppState *>(opaque);
    if (state != nullptr) {
        state->render_target_view.Reset();
        state->back_buffer_texture.Reset();
        state->context.Reset();
        state->device.Reset();
    }
}

} // namespace

[Platform::MTAThread]
int main(Platform::Array<Platform::String ^> ^)
{
    AppState state = {};
    XemuUwpCoreCallbacks callbacks = {};
    callbacks.opaque = &state;
    callbacks.initialize = OnInitialize;
    callbacks.render = OnRender;
    callbacks.resize = OnResize;
    callbacks.shutdown = OnShutdown;

    return xemu_uwp_run(&callbacks);
}
