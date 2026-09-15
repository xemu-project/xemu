#include "hw/xbox/nv2a/pgraph/d3d11/clear.h"

#ifdef _WIN32

#include <d3d11.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;
using xemu::D3D11ClearParams;
using xemu::D3D11ClearPrimitive;
using xemu::D3D11ClearStatus;
using xemu::D3D11ClearSurfaceFormat;
using xemu::D3D11ClearSurfaceKind;

struct DevicePair {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
};

bool CreateWarpDevice(DevicePair *pair)
{
    D3D_FEATURE_LEVEL feature_level = D3D_FEATURE_LEVEL_11_0;
    return SUCCEEDED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &pair->device, &feature_level, &pair->context));
}

bool CreateColorTarget(ID3D11Device *device, uint32_t width, uint32_t height,
                       DXGI_FORMAT format, ComPtr<ID3D11Texture2D> *texture,
                       ComPtr<ID3D11RenderTargetView> *view)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    return SUCCEEDED(device->CreateTexture2D(
               &desc, nullptr, texture->ReleaseAndGetAddressOf())) &&
           SUCCEEDED(device->CreateRenderTargetView(
               texture->Get(), nullptr, view->ReleaseAndGetAddressOf()));
}

bool CreateDepthTarget(ID3D11Device *device, uint32_t width, uint32_t height,
                       DXGI_FORMAT resource_format, DXGI_FORMAT view_format,
                       ComPtr<ID3D11Texture2D> *texture,
                       ComPtr<ID3D11DepthStencilView> *view)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = resource_format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(&desc, nullptr,
                                       texture->ReleaseAndGetAddressOf()))) {
        return false;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc = {};
    view_desc.Format = view_format;
    view_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    return SUCCEEDED(device->CreateDepthStencilView(
        texture->Get(), &view_desc, view->ReleaseAndGetAddressOf()));
}

bool Readback(ID3D11Device *device, ID3D11DeviceContext *context,
              ID3D11Texture2D *source, std::vector<uint8_t> *bytes,
              uint32_t *row_pitch)
{
    D3D11_TEXTURE2D_DESC desc = {};
    source->GetDesc(&desc);
    D3D11_TEXTURE2D_DESC staging_desc = desc;
    staging_desc.Usage = D3D11_USAGE_STAGING;
    staging_desc.BindFlags = 0;
    staging_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    staging_desc.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&staging_desc, nullptr,
                                       staging.ReleaseAndGetAddressOf()))) {
        return false;
    }
    context->CopyResource(staging.Get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        return false;
    }
    const size_t row_bytes = static_cast<size_t>(desc.Width) *
                             (desc.Format == DXGI_FORMAT_D16_UNORM ? 2 : 4);
    bytes->resize(row_bytes * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y) {
        memcpy(bytes->data() + y * row_bytes,
               static_cast<const uint8_t *>(mapped.pData) + y * mapped.RowPitch,
               row_bytes);
    }
    *row_pitch = static_cast<uint32_t>(row_bytes);
    context->Unmap(staging.Get(), 0);
    return true;
}

bool NearByte(uint8_t actual, uint8_t expected)
{
    return actual >= expected - (expected > 1 ? 1 : 0) &&
           actual <= expected + (expected < 254 ? 1 : 0);
}

bool TestColorFullAndRect(D3D11ClearPrimitive *clear, ID3D11Device *device,
                          ID3D11DeviceContext *context)
{
    constexpr uint32_t width = 8;
    constexpr uint32_t height = 6;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    if (!CreateColorTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                           &texture, &view)) {
        return false;
    }

    D3D11ClearParams params = {};
    params.surface_kind = D3D11ClearSurfaceKind::Color;
    params.surface_format = D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    params.surface_width = width;
    params.surface_height = height;
    params.rect = { 0, 0, width - 1, height - 1 };
    params.color[0] = 0.2f;
    params.color[1] = 0.4f;
    params.color[2] = 0.6f;
    params.color[3] = 0.8f;
    const auto initial_result = clear->Clear(params, view.Get(), nullptr);
    if (initial_result.status != D3D11ClearStatus::Cleared) {
        return false;
    }
    std::vector<uint8_t> bytes;
    uint32_t pitch = 0;
    if (!Readback(device, context, texture.Get(), &bytes, &pitch) ||
        !NearByte(bytes[0], 51) || !NearByte(bytes[1], 102) ||
        !NearByte(bytes[2], 153) || !NearByte(bytes[3], 204)) {
        return false;
    }

    // Right/bottom are inclusive and clipped.  This rectangle must clear only
    // the bottom-right pixel even though its far edge is outside the target.
    params.color[0] = 0.1f;
    params.color[1] = 0.2f;
    params.color[2] = 0.3f;
    params.color[3] = 0.4f;
    params.rect = { width - 1, height - 1, width + 20, height + 20 };
    if (clear->Clear(params, view.Get(), nullptr).status !=
            D3D11ClearStatus::Cleared ||
        !Readback(device, context, texture.Get(), &bytes, &pitch)) {
        return false;
    }
    const uint8_t *edge = bytes.data() + (height - 1) * pitch + (width - 1) * 4;
    if (!NearByte(edge[0], 26) || !NearByte(edge[1], 51) ||
        !NearByte(edge[2], 77) || !NearByte(edge[3], 102)) {
        return false;
    }
    params.rect = { width, 0, width, 0 };
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }
    params.rect = { 2, 2, 1, 1 };
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }

    params.color[0] = 0.2f;
    params.color[1] = 0.4f;
    params.color[2] = 0.6f;
    params.color[3] = 0.8f;
    params.rect = { 0, 0, width - 1, height - 1 };
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Cleared) {
        return false;
    }
    memset(params.color, 0, sizeof(params.color));
    params.color[3] = 1.0f;
    params.rect = { 1, 2, 3, 4 };
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Cleared) {
        return false;
    }
    if (!Readback(device, context, texture.Get(), &bytes, &pitch)) {
        return false;
    }
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const bool in_rect = x >= 1 && x <= 3 && y >= 2 && y <= 4;
            const uint8_t *pixel = bytes.data() + y * pitch + x * 4;
            if (in_rect) {
                if (pixel[0] != 0 || pixel[1] != 0 || pixel[2] != 0 ||
                    pixel[3] != 255) {
                    return false;
                }
            } else if (!NearByte(pixel[0], 51) || !NearByte(pixel[1], 102) ||
                       !NearByte(pixel[2], 153) || !NearByte(pixel[3], 204)) {
                return false;
            }
        }
    }

    // The format declaration is part of the normalized contract; a BGRA
    // request cannot target an RGBA view.
    params.rect = { 0, 0, width - 1, height - 1 };
    params.surface_format = D3D11ClearSurfaceFormat::ColorBgra8Unorm;
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }

    ComPtr<ID3D11Texture2D> bgra_texture;
    ComPtr<ID3D11RenderTargetView> bgra_view;
    if (!CreateColorTarget(device, width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                           &bgra_texture, &bgra_view)) {
        return false;
    }
    params.surface_format = D3D11ClearSurfaceFormat::ColorBgra8Unorm;
    params.color[0] = 0.2f;
    params.color[1] = 0.4f;
    params.color[2] = 0.6f;
    params.color[3] = 0.8f;
    if (clear->Clear(params, bgra_view.Get(), nullptr).status !=
            D3D11ClearStatus::Cleared ||
        !Readback(device, context, bgra_texture.Get(), &bytes, &pitch)) {
        return false;
    }
    return NearByte(bytes[0], 153) && NearByte(bytes[1], 102) &&
           NearByte(bytes[2], 51) && NearByte(bytes[3], 204);
}

bool TestDepthAndStencil(D3D11ClearPrimitive *clear, ID3D11Device *device,
                         ID3D11DeviceContext *context)
{
    constexpr uint32_t width = 4;
    constexpr uint32_t height = 3;

    ComPtr<ID3D11Texture2D> z16_texture;
    ComPtr<ID3D11DepthStencilView> z16_view;
    if (!CreateDepthTarget(device, width, height, DXGI_FORMAT_D16_UNORM,
                           DXGI_FORMAT_D16_UNORM, &z16_texture, &z16_view)) {
        return false;
    }
    D3D11ClearParams z16 = {};
    z16.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    z16.surface_format = D3D11ClearSurfaceFormat::Z16Fixed;
    z16.surface_width = width;
    z16.surface_height = height;
    z16.rect = { 0, 0, width - 1, height - 1 };
    z16.fixed_depth = 0x8000;
    z16.clear_depth = true;
    z16.clear_stencil = false;
    if (clear->Clear(z16, nullptr, z16_view.Get()).status !=
        D3D11ClearStatus::Cleared) {
        return false;
    }
    std::vector<uint8_t> bytes;
    uint32_t pitch = 0;
    if (!Readback(device, context, z16_texture.Get(), &bytes, &pitch)) {
        return false;
    }
    const uint16_t expected_z16 = 0x8000;
    if (bytes[0] != (expected_z16 & 0xff) || bytes[1] != (expected_z16 >> 8)) {
        return false;
    }

    ComPtr<ID3D11Texture2D> z24_texture;
    ComPtr<ID3D11DepthStencilView> z24_view;
    if (!CreateDepthTarget(device, width, height, DXGI_FORMAT_R24G8_TYPELESS,
                           DXGI_FORMAT_D24_UNORM_S8_UINT, &z24_texture,
                           &z24_view)) {
        return false;
    }
    D3D11ClearParams z24 = {};
    z24.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    z24.surface_format = D3D11ClearSurfaceFormat::Z24S8Fixed;
    z24.surface_width = width;
    z24.surface_height = height;
    z24.rect = { 0, 0, width - 1, height - 1 };
    z24.fixed_depth = 0x804020;
    z24.stencil = 0xA5;
    z24.clear_depth = true;
    z24.clear_stencil = true;
    if (clear->Clear(z24, nullptr, z24_view.Get()).status !=
            D3D11ClearStatus::Cleared ||
        !Readback(device, context, z24_texture.Get(), &bytes, &pitch)) {
        return false;
    }
    if (bytes[0] != 0x20 || bytes[1] != 0x40 || bytes[2] != 0x80 ||
        bytes[3] != 0xA5) {
        return false;
    }

    // Depth-only preserves stencil, and stencil-only does not need a valid
    // fixed depth value because the depth flag is clear.
    z24.fixed_depth = 0x102030;
    z24.stencil = 0x11;
    z24.clear_depth = true;
    z24.clear_stencil = false;
    if (clear->Clear(z24, nullptr, z24_view.Get()).status !=
            D3D11ClearStatus::Cleared ||
        !Readback(device, context, z24_texture.Get(), &bytes, &pitch) ||
        bytes[0] != 0x30 || bytes[1] != 0x20 || bytes[2] != 0x10 ||
        bytes[3] != 0xA5) {
        return false;
    }
    z24.fixed_depth = UINT32_MAX;
    z24.stencil = 0x5A;
    z24.clear_depth = false;
    z24.clear_stencil = true;
    if (clear->Clear(z24, nullptr, z24_view.Get()).status !=
            D3D11ClearStatus::Cleared ||
        !Readback(device, context, z24_texture.Get(), &bytes, &pitch)) {
        return false;
    }
    return bytes[0] == 0x30 && bytes[1] == 0x20 && bytes[2] == 0x10 &&
           bytes[3] == 0x5A;
}

bool TestUnsupportedAndSecondDevice(D3D11ClearPrimitive *clear,
                                    ID3D11Device *device,
                                    ID3D11DeviceContext *context)
{
    constexpr uint32_t width = 4;
    constexpr uint32_t height = 4;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    if (!CreateColorTarget(device, width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
                           &texture, &view)) {
        return false;
    }
    D3D11ClearParams params = {};
    params.surface_kind = D3D11ClearSurfaceKind::Color;
    params.surface_format = D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    params.surface_width = width;
    params.surface_height = height;
    params.rect = { 0, 0, width - 1, height - 1 };
    params.color_mask = xemu::D3D11_CLEAR_RED;
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Unsupported) {
        return false;
    }
    params.color_mask = xemu::D3D11_CLEAR_ALL_COLOR_CHANNELS;
    params.surface_format = D3D11ClearSurfaceFormat::Z16Float;
    params.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    if (clear->Clear(params, nullptr, nullptr).status !=
        D3D11ClearStatus::Unsupported) {
        return false;
    }
    params.surface_kind = D3D11ClearSurfaceKind::Color;
    params.surface_format = D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    params.swizzled = true;
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Unsupported) {
        return false;
    }
    params.swizzled = false;
    params.scaled = true;
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Unsupported) {
        return false;
    }
    params.scaled = false;
    params.sample_count = 2;
    if (clear->Clear(params, view.Get(), nullptr).status !=
        D3D11ClearStatus::Unsupported) {
        return false;
    }
    params.sample_count = 1;
    params.rect = { 1, 1, 2, 2 };
    // A partial zeta clear is unsupported even when the format is fixed.
    params.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    params.surface_format = D3D11ClearSurfaceFormat::Z24S8Fixed;
    ComPtr<ID3D11Texture2D> z24_texture;
    ComPtr<ID3D11DepthStencilView> z24_view;
    if (!CreateDepthTarget(device, width, height, DXGI_FORMAT_R24G8_TYPELESS,
                           DXGI_FORMAT_D24_UNORM_S8_UINT, &z24_texture,
                           &z24_view) ||
        clear->Clear(params, nullptr, z24_view.Get()).status !=
            D3D11ClearStatus::Unsupported) {
        return false;
    }

    DevicePair second;
    if (!CreateWarpDevice(&second)) {
        return false;
    }
    ComPtr<ID3D11Texture2D> second_texture;
    ComPtr<ID3D11RenderTargetView> second_view;
    if (!CreateColorTarget(second.device.Get(), width, height,
                           DXGI_FORMAT_R8G8B8A8_UNORM, &second_texture,
                           &second_view)) {
        return false;
    }
    params.surface_kind = D3D11ClearSurfaceKind::Color;
    params.surface_format = D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    params.rect = { 0, 0, width - 1, height - 1 };
    if (clear->Clear(params, second_view.Get(), nullptr).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }
    D3D11ClearPrimitive mismatched_context(device, second.context.Get());
    if (mismatched_context.Clear(params, second_view.Get(), nullptr).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }

    ComPtr<ID3D11Texture2D> z16_texture;
    ComPtr<ID3D11DepthStencilView> z16_view;
    if (!CreateDepthTarget(device, width, height, DXGI_FORMAT_D16_UNORM,
                           DXGI_FORMAT_D16_UNORM, &z16_texture, &z16_view)) {
        return false;
    }
    params.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    params.surface_format = D3D11ClearSurfaceFormat::Z16Fixed;
    params.rect = { 0, 0, width - 1, height - 1 };
    params.fixed_depth = 0x10000;
    params.clear_depth = true;
    params.clear_stencil = false;
    if (clear->Clear(params, nullptr, z16_view.Get()).status !=
        D3D11ClearStatus::Invalid) {
        return false;
    }
    (void)context;
    return true;
}

} // namespace

int main()
{
    DevicePair pair;
    if (!CreateWarpDevice(&pair)) {
        fprintf(stderr, "D3D11 WARP device creation failed\n");
        return 1;
    }
    D3D11ClearPrimitive clear(pair.device.Get(), pair.context.Get());
    if (!clear.has_context1()) {
        fprintf(stderr, "D3D11.1 context is required for rectangular clears\n");
        return 1;
    }
    if (!TestColorFullAndRect(&clear, pair.device.Get(), pair.context.Get())) {
        return 1;
    }
    if (!TestDepthAndStencil(&clear, pair.device.Get(), pair.context.Get())) {
        return 1;
    }
    if (!TestUnsupportedAndSecondDevice(&clear, pair.device.Get(),
                                        pair.context.Get())) {
        fprintf(stderr, "D3D11 clear primitive test failed\n");
        return 1;
    }
    return 0;
}

#else

int main()
{
    return 0;
}

#endif
