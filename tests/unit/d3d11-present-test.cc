#include "ui/xui/d3d11-present.h"

#ifdef _WIN32

#include <d3d11.h>
#include <string.h>
#include <stdio.h>

#include <array>
#include <limits>

using xemu::D3D11FrameFormat;
using xemu::D3D11FrameOrientation;
using xemu::D3D11FrameUploader;
using xemu::D3D11FrameView;

static bool check_pixels(ID3D11DeviceContext *context, ID3D11Device *device,
                         ID3D11Texture2D *source, const uint8_t *expected,
                         uint32_t width, uint32_t height)
{
    constexpr size_t bytes_per_pixel = 4;
    if (context == nullptr || device == nullptr || source == nullptr ||
        expected == nullptr || width == 0 || height == 0 ||
        width > std::numeric_limits<size_t>::max() / bytes_per_pixel ||
        (height > 1 &&
         width * bytes_per_pixel >
             (std::numeric_limits<size_t>::max() - width * bytes_per_pixel) /
                 (height - 1))) {
        return false;
    }
    const size_t row_bytes = static_cast<size_t>(width) * bytes_per_pixel;

    D3D11_TEXTURE2D_DESC source_desc = {};
    source->GetDesc(&source_desc);
    source_desc.Usage = D3D11_USAGE_STAGING;
    source_desc.BindFlags = 0;
    source_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    source_desc.MiscFlags = 0;

    ID3D11Texture2D *staging = nullptr;
    if (FAILED(device->CreateTexture2D(&source_desc, nullptr, &staging))) {
        return false;
    }
    context->CopyResource(staging, source);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    const bool map_succeeded =
        SUCCEEDED(context->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
    bool matches =
        map_succeeded && mapped.pData != nullptr &&
        mapped.RowPitch >= row_bytes &&
        (height == 1 ||
         mapped.RowPitch <=
             (std::numeric_limits<size_t>::max() - row_bytes) / (height - 1));
    if (map_succeeded && matches) {
        for (uint32_t y = 0; y < height && matches; ++y) {
            const uint8_t *actual = static_cast<const uint8_t *>(mapped.pData) +
                                    static_cast<size_t>(y) * mapped.RowPitch;
            matches =
                memcmp(actual, expected + static_cast<size_t>(y) * row_bytes,
                       row_bytes) == 0;
        }
    }
    if (map_succeeded) {
        context->Unmap(staging, 0);
    }
    staging->Release();
    return matches;
}

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

    constexpr uint32_t width = 2;
    constexpr uint32_t height = 2;
    constexpr uint32_t stride = 12;
    const std::array<uint8_t, stride * height> source = {
        1, 2,  3,  4,  5,  6,  7,  8,  0xaa, 0xbb, 0xcc, 0xdd,
        9, 10, 11, 12, 13, 14, 15, 16, 0xee, 0xff, 0x11, 0x22,
    };
    const std::array<uint8_t, width * height * 4> top_down = {
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    };
    const std::array<uint8_t, width * height * 4> bottom_up = {
        9, 10, 11, 12, 13, 14, 15, 16, 1, 2, 3, 4, 5, 6, 7, 8,
    };

    int result = 0;
    {
        D3D11FrameUploader uploader(device, context);
        D3D11FrameView frame = { source.data(),
                                 width,
                                 height,
                                 stride,
                                 D3D11FrameFormat::BGRA8_UNORM,
                                 D3D11FrameOrientation::TopDown };
        if (!uploader.Resize(width, height) || uploader.texture() == nullptr ||
            uploader.shader_resource_view() == nullptr ||
            !uploader.Upload(frame)) {
            result = 1;
        } else if (!check_pixels(context, device, uploader.texture(),
                                 top_down.data(), width, height)) {
            result = 1;
        }

        frame.orientation = D3D11FrameOrientation::BottomUp;
        if (result == 0 && (!uploader.Upload(frame) ||
                            !check_pixels(context, device, uploader.texture(),
                                          bottom_up.data(), width, height))) {
            result = 1;
        }

        frame.format = D3D11FrameFormat::RGBA8_UNORM;
        if (result == 0 && uploader.Upload(frame)) {
            result = 1;
        }

        frame.format = D3D11FrameFormat::BGRA8_UNORM;
        frame.stride = width * 4 - 1;
        if (result == 0 && uploader.Upload(frame)) {
            result = 1;
        }

        frame.stride = stride;
        frame.orientation = static_cast<D3D11FrameOrientation>(99);
        if (result == 0 && uploader.Upload(frame)) {
            result = 1;
        }

        if (result == 0 &&
            (!uploader.Resize(width + 1, height) ||
             uploader.width() != width + 1 || uploader.height() != height ||
             uploader.texture() == nullptr)) {
            result = 1;
        }
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
