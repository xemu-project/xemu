#include "ui/xui/d3d11-corewindow-present.h"

#ifdef _WIN32

#include <d3d11.h>
#include <stdio.h>

#include <wrl/client.h>

int main()
{
    ID3D11Device *device = nullptr;
    ID3D11DeviceContext *context = nullptr;
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                 nullptr, 0, D3D11_SDK_VERSION, &device,
                                 &feature_level, &context))) {
        fprintf(stderr, "D3D11 WARP device creation failed\n");
        return 1;
    }

    int result = 0;
    xemu::D3D11CoreWindowPresenter presenter(device, context);
    if (presenter.Resize(1, 1) || presenter.Present() ||
        presenter.IsTextureCompatible(nullptr) ||
        presenter.CopyTexture(nullptr)) {
        fprintf(stderr, "uninitialized presenter accepted an operation\n");
        result = 1;
    }
    if (presenter.Initialize(nullptr, 1, 1) ||
        presenter.back_buffer() != nullptr || presenter.width() != 0 ||
        presenter.height() != 0 || presenter.needs_reinitialize()) {
        fprintf(stderr, "invalid CoreWindow initialization was accepted\n");
        result = 1;
    }
    if (presenter.Resize(0, 1) || presenter.Resize(1, 0)) {
        fprintf(stderr, "invalid resize was accepted\n");
        result = 1;
    }

    // A source from a second D3D11 device must never be accepted for a copy.
    ID3D11Device *second_device = nullptr;
    ID3D11DeviceContext *second_context = nullptr;
    if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                    nullptr, 0, D3D11_SDK_VERSION,
                                    &second_device, &feature_level,
                                    &second_context))) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> second_texture;
        if (FAILED(second_device->CreateTexture2D(&desc, nullptr,
                                                  &second_texture))) {
            fprintf(stderr, "second-device texture creation failed\n");
            result = 1;
        } else if (presenter.IsTextureCompatible(second_texture.Get()) ||
                   presenter.CopyTexture(second_texture.Get())) {
            fprintf(stderr, "second-device texture was accepted\n");
            result = 1;
        }
        second_context->Release();
        second_device->Release();
    } else {
        fprintf(stderr, "second D3D11 WARP device creation failed\n");
        result = 1;
    }

    context->Release();
    device->Release();
    return result;
}

#else

int main()
{
    return 0;
}

#endif
