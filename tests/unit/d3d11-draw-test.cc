#include "hw/xbox/nv2a/pgraph/d3d11/clear.h"
#include "hw/xbox/nv2a/pgraph/d3d11/draw.h"
#include "hw/xbox/nv2a/pgraph/d3d11/draw_shaders.h"

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
using xemu::D3D11DrawParams;
using xemu::D3D11DrawPrimitive;
using xemu::D3D11DrawStatus;
using xemu::D3D11DrawVertexBuffer;

struct DevicePair {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
};

struct ColorTarget {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
};

struct DepthTarget {
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11DepthStencilView> view;
};

struct Vertex {
    float position[3];
    float color[4];
};

bool Check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "d3d11-draw-test: %s\n", message);
    }
    return condition;
}

bool CreateWarp(DevicePair *pair)
{
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    return SUCCEEDED(D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, pair->device.ReleaseAndGetAddressOf(), &level,
        pair->context.ReleaseAndGetAddressOf()));
}

bool CreateColorTarget(ID3D11Device *device, UINT width, UINT height,
                       ColorTarget *target, UINT mip_levels = 1,
                       UINT view_mip = 0, UINT sample_count = 1,
                       UINT sample_quality = 0)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = sample_count > 1 ? 1 : mip_levels;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = sample_count;
    desc.SampleDesc.Quality = sample_quality;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(device->CreateTexture2D(
            &desc, nullptr, target->texture.ReleaseAndGetAddressOf()))) {
        return false;
    }
    D3D11_RENDER_TARGET_VIEW_DESC view_desc = {};
    view_desc.Format = desc.Format;
    view_desc.ViewDimension = sample_count > 1 ?
                                  D3D11_RTV_DIMENSION_TEXTURE2DMS :
                                  D3D11_RTV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipSlice = view_mip;
    return SUCCEEDED(
        device->CreateRenderTargetView(target->texture.Get(), &view_desc,
                                       target->view.ReleaseAndGetAddressOf()));
}

bool CreateDepthTarget(ID3D11Device *device, UINT width, UINT height,
                       DepthTarget *target, UINT mip_levels = 1,
                       UINT view_mip = 0, UINT sample_count = 1,
                       UINT sample_quality = 0)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = sample_count > 1 ? 1 : mip_levels;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.SampleDesc.Count = sample_count;
    desc.SampleDesc.Quality = sample_quality;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(device->CreateTexture2D(
            &desc, nullptr, target->texture.ReleaseAndGetAddressOf()))) {
        return false;
    }
    D3D11_DEPTH_STENCIL_VIEW_DESC view_desc = {};
    view_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    view_desc.ViewDimension = sample_count > 1 ?
                                  D3D11_DSV_DIMENSION_TEXTURE2DMS :
                                  D3D11_DSV_DIMENSION_TEXTURE2D;
    view_desc.Texture2D.MipSlice = view_mip;
    return SUCCEEDED(
        device->CreateDepthStencilView(target->texture.Get(), &view_desc,
                                       target->view.ReleaseAndGetAddressOf()));
}

bool ReadColor(ID3D11Device *device, ID3D11DeviceContext *context,
               ID3D11Texture2D *source, std::vector<uint8_t> *pixels)
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
    const size_t row_bytes = static_cast<size_t>(desc.Width) * 4;
    pixels->resize(row_bytes * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y) {
        memcpy(pixels->data() + y * row_bytes,
               static_cast<const uint8_t *>(mapped.pData) + y * mapped.RowPitch,
               row_bytes);
    }
    context->Unmap(staging.Get(), 0);
    return true;
}

bool PixelIs(const std::vector<uint8_t> &pixels, UINT width, UINT x, UINT y,
             uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t *pixel =
        pixels.data() + (static_cast<size_t>(y) * width + x) * 4;
    return pixel[0] == red && pixel[1] == green && pixel[2] == blue &&
           pixel[3] == 255;
}

bool ClearColor(D3D11ClearPrimitive *clear, ID3D11RenderTargetView *view,
                UINT width, UINT height, float red, float green, float blue)
{
    D3D11ClearParams params = {};
    params.surface_kind = D3D11ClearSurfaceKind::Color;
    params.surface_format = D3D11ClearSurfaceFormat::ColorRgba8Unorm;
    params.surface_width = width;
    params.surface_height = height;
    params.rect = { 0, 0, width - 1, height - 1 };
    params.color[0] = red;
    params.color[1] = green;
    params.color[2] = blue;
    params.color[3] = 1.0f;
    return clear->Clear(params, view, nullptr).status ==
           D3D11ClearStatus::Cleared;
}

bool ClearDepth(D3D11ClearPrimitive *clear, ID3D11DepthStencilView *view,
                UINT width, UINT height, uint32_t depth)
{
    D3D11ClearParams params = {};
    params.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    params.surface_format = D3D11ClearSurfaceFormat::Z24S8Fixed;
    params.surface_width = width;
    params.surface_height = height;
    params.rect = { 0, 0, width - 1, height - 1 };
    params.fixed_depth = depth;
    params.clear_depth = true;
    return clear->Clear(params, nullptr, view).status ==
           D3D11ClearStatus::Cleared;
}

D3D11DrawParams BaseParams(ID3D11RenderTargetView *rtv,
                           ID3D11DepthStencilView *dsv, const Vertex *vertices,
                           size_t vertex_bytes)
{
    static const D3D11_INPUT_ELEMENT_DESC elements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    D3D11DrawParams params = {};
    params.render_target = rtv;
    params.depth_stencil = dsv;
    params.vertex_shader_bytecode = xemu::d3d11_draw_shaders::kVertexShader;
    params.vertex_shader_bytecode_size =
        xemu::d3d11_draw_shaders::kVertexShaderSize;
    params.pixel_shader_bytecode = xemu::d3d11_draw_shaders::kPixelShader;
    params.pixel_shader_bytecode_size =
        xemu::d3d11_draw_shaders::kPixelShaderSize;
    params.input_elements = elements;
    params.input_element_count = 2;
    static D3D11DrawVertexBuffer buffer;
    buffer.data = vertices;
    buffer.byte_size = vertex_bytes;
    buffer.stride = sizeof(Vertex);
    buffer.slot = 0;
    params.vertex_buffers = &buffer;
    params.vertex_buffer_count = 1;
    params.has_viewport = true;
    params.viewport = { 0.0f, 0.0f, 8.0f, 8.0f, 0.0f, 1.0f };
    params.has_scissor = true;
    params.scissor = { 0, 0, 8, 8 };
    return params;
}

bool TestDraws(const DevicePair &pair)
{
    constexpr UINT width = 8;
    constexpr UINT height = 8;
    ColorTarget color;
    DepthTarget depth;
    if (!Check(CreateColorTarget(pair.device.Get(), width, height, &color) &&
                   CreateDepthTarget(pair.device.Get(), width, height, &depth),
               "create WARP targets")) {
        return false;
    }

    D3D11ClearPrimitive clear(pair.device.Get(), pair.context.Get());
    D3D11DrawPrimitive draw(pair.device.Get(), pair.context.Get());
    if (!Check(ClearColor(&clear, color.view.Get(), width, height, 1.0f, 0.0f,
                          0.0f),
               "clear red")) {
        return false;
    }

    const Vertex green_vertices[] = {
        { { -0.75f, -0.75f, 0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { 0.0f, 0.75f, 0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { 0.75f, -0.75f, 0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
    };
    ColorTarget mip_color;
    DepthTarget mip_depth0;
    DepthTarget mip_depth1;
    if (!Check(CreateColorTarget(pair.device.Get(), width, height, &mip_color,
                                 2, 1) &&
                   CreateDepthTarget(pair.device.Get(), width, height,
                                     &mip_depth0, 2, 0) &&
                   CreateDepthTarget(pair.device.Get(), width, height,
                                     &mip_depth1, 2, 1),
               "create mip targets")) {
        return false;
    }
    D3D11DrawParams mip_mismatch =
        BaseParams(mip_color.view.Get(), mip_depth0.view.Get(), green_vertices,
                   sizeof(green_vertices));
    mip_mismatch.vertex_count = 3;
    if (!Check(draw.Draw(mip_mismatch).status == D3D11DrawStatus::Invalid,
               "mip dimension mismatch")) {
        return false;
    }
    D3D11DrawParams mip_match =
        BaseParams(mip_color.view.Get(), mip_depth1.view.Get(), green_vertices,
                   sizeof(green_vertices));
    mip_match.vertex_count = 3;
    mip_match.viewport = { 0.0f, 0.0f, 4.0f, 4.0f, 0.0f, 1.0f };
    mip_match.scissor = { 0, 0, 4, 4 };
    if (!Check(draw.Draw(mip_match).succeeded(),
               "mip dimensions and viewport")) {
        return false;
    }
    UINT quality_levels = 0;
    UINT depth_quality_levels = 0;
    if (SUCCEEDED(pair.device->CheckMultisampleQualityLevels(
            DXGI_FORMAT_R8G8B8A8_UNORM, 2, &quality_levels)) &&
        SUCCEEDED(pair.device->CheckMultisampleQualityLevels(
            DXGI_FORMAT_D24_UNORM_S8_UINT, 2, &depth_quality_levels)) &&
        quality_levels >= 2 && depth_quality_levels >= 2) {
        ColorTarget quality_color;
        DepthTarget quality_depth;
        if (!Check(CreateColorTarget(pair.device.Get(), width, height,
                                     &quality_color, 1, 0, 2, 0) &&
                       CreateDepthTarget(pair.device.Get(), width, height,
                                         &quality_depth, 1, 0, 2, 1),
                   "create quality mismatch targets")) {
            return false;
        }
        D3D11DrawParams quality_mismatch =
            BaseParams(quality_color.view.Get(), quality_depth.view.Get(),
                       green_vertices, sizeof(green_vertices));
        quality_mismatch.vertex_count = 3;
        if (!Check(draw.Draw(quality_mismatch).status ==
                       D3D11DrawStatus::Invalid,
                   "sample quality mismatch")) {
            return false;
        }
    }
    const uint32_t indices[] = { 0, 1, 2 };
    D3D11DrawParams indexed = BaseParams(
        color.view.Get(), nullptr, green_vertices, sizeof(green_vertices));
    indexed.index_buffer = { indices, 3 };
    indexed.index_count = 3;
    indexed.vertex_count = 1;
    if (!Check(draw.Draw(indexed).status == D3D11DrawStatus::Drawn,
               "indexed green triangle")) {
        return false;
    }
    std::vector<uint8_t> pixels;
    if (!Check(ReadColor(pair.device.Get(), pair.context.Get(),
                         color.texture.Get(), &pixels) &&
                   PixelIs(pixels, width, 4, 4, 0, 255, 0) &&
                   PixelIs(pixels, width, 0, 0, 255, 0, 0),
               "indexed staging readback")) {
        return false;
    }

    if (!Check(ClearColor(&clear, color.view.Get(), width, height, 1.0f, 0.0f,
                          0.0f),
               "clear before non-indexed draw")) {
        return false;
    }
    const Vertex blue_vertices[] = {
        { { -0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { 0.0f, 0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { 0.5f, -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
    };
    D3D11DrawParams non_indexed = BaseParams(
        color.view.Get(), nullptr, blue_vertices, sizeof(blue_vertices));
    non_indexed.vertex_count = 3;
    if (!Check(draw.Draw(non_indexed).status == D3D11DrawStatus::Drawn,
               "non-indexed blue triangle") ||
        !Check(ReadColor(pair.device.Get(), pair.context.Get(),
                         color.texture.Get(), &pixels),
               "non-indexed staging readback")) {
        return false;
    }
    bool found_blue = false;
    for (UINT y = 0; y < height && !found_blue; ++y) {
        for (UINT x = 0; x < width; ++x) {
            found_blue |= PixelIs(pixels, width, x, y, 0, 0, 255);
        }
    }
    if (!Check(found_blue, "non-indexed pixels")) {
        return false;
    }

    if (!Check(
            ClearColor(&clear, color.view.Get(), width, height, 1.0f, 0.0f,
                       0.0f) &&
                ClearDepth(&clear, depth.view.Get(), width, height, 0xffffffu),
            "clear depth")) {
        return false;
    }
    D3D11_DEPTH_STENCIL_DESC depth_desc = {};
    depth_desc.DepthEnable = TRUE;
    depth_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depth_desc.DepthFunc = D3D11_COMPARISON_LESS;
    ComPtr<ID3D11DepthStencilState> depth_state;
    if (!Check(SUCCEEDED(pair.device->CreateDepthStencilState(
                   &depth_desc, depth_state.ReleaseAndGetAddressOf())),
               "create depth state")) {
        return false;
    }
    const Vertex far_vertices[] = {
        { { -0.75f, -0.75f, 0.75f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { 0.0f, 0.75f, 0.75f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
        { { 0.75f, -0.75f, 0.75f }, { 0.0f, 0.0f, 1.0f, 1.0f } },
    };
    const Vertex near_vertices[] = {
        { { -0.75f, -0.75f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { 0.0f, 0.75f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
        { { 0.75f, -0.75f, 0.25f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
    };
    D3D11DrawParams far_draw = BaseParams(color.view.Get(), depth.view.Get(),
                                          far_vertices, sizeof(far_vertices));
    far_draw.vertex_count = 3;
    far_draw.depth_stencil_state = depth_state.Get();
    D3D11DrawParams near_draw =
        BaseParams(color.view.Get(), depth.view.Get(), near_vertices,
                   sizeof(near_vertices));
    near_draw.vertex_count = 3;
    near_draw.depth_stencil_state = depth_state.Get();
    if (!Check(draw.Draw(far_draw).succeeded() &&
                   draw.Draw(near_draw).succeeded(),
               "depth draws") ||
        !Check(ReadColor(pair.device.Get(), pair.context.Get(),
                         color.texture.Get(), &pixels) &&
                   PixelIs(pixels, width, 4, 4, 0, 255, 0),
               "depth write/test")) {
        return false;
    }

    uint8_t bad_bytecode[] = { 0, 1, 2, 3 };
    D3D11DrawParams bad = non_indexed;
    bad.vertex_shader_bytecode = bad_bytecode;
    bad.vertex_shader_bytecode_size = sizeof(bad_bytecode);
    if (!Check(draw.Draw(bad).status == D3D11DrawStatus::ShaderError,
               "invalid bytecode")) {
        return false;
    }
    D3D11DrawParams overflow = non_indexed;
    overflow.vertex_count = UINT_MAX;
    overflow.start_vertex = 1;
    if (!Check(draw.Draw(overflow).status == D3D11DrawStatus::Invalid,
               "count overflow")) {
        return false;
    }
    D3D11DrawParams oversize = non_indexed;
    D3D11DrawVertexBuffer oversize_buffer = *oversize.vertex_buffers;
    oversize_buffer.byte_size = SIZE_MAX;
    oversize.vertex_buffers = &oversize_buffer;
    if (!Check(draw.Draw(oversize).status == D3D11DrawStatus::Invalid,
               "vertex buffer size overflow")) {
        return false;
    }
    D3D11DrawParams stride_overflow = non_indexed;
    D3D11DrawVertexBuffer extreme_buffer = *stride_overflow.vertex_buffers;
    extreme_buffer.byte_size = UINT_MAX;
    extreme_buffer.stride = UINT_MAX;
    stride_overflow.vertex_buffers = &extreme_buffer;
    stride_overflow.vertex_count = 2;
    if (!Check(draw.Draw(stride_overflow).status == D3D11DrawStatus::Invalid,
               "vertex byte multiplication overflow")) {
        return false;
    }
    D3D11DrawParams missing_rtv = non_indexed;
    missing_rtv.render_target = nullptr;
    if (!Check(draw.Draw(missing_rtv).status == D3D11DrawStatus::Invalid,
               "missing RTV")) {
        return false;
    }
    return true;
}

bool TestWrongDevice(const DevicePair &pair)
{
    DevicePair other;
    if (!Check(CreateWarp(&other), "create second WARP device")) {
        return false;
    }
    ColorTarget target;
    if (!Check(CreateColorTarget(other.device.Get(), 8, 8, &target),
               "create wrong-device target")) {
        return false;
    }
    const Vertex vertices[] = {
        { { -0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 0.5f, -0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
        { { 0.0f, 0.5f, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
    };
    D3D11DrawPrimitive draw(pair.device.Get(), pair.context.Get());
    D3D11DrawParams params =
        BaseParams(target.view.Get(), nullptr, vertices, sizeof(vertices));
    params.vertex_count = 3;
    return Check(draw.Draw(params).status == D3D11DrawStatus::Invalid,
                 "wrong-device view");
}

} // namespace

int main()
{
    DevicePair pair;
    if (!Check(CreateWarp(&pair), "create WARP device")) {
        return 1;
    }
    return TestDraws(pair) && TestWrongDevice(pair) ? 0 : 1;
}

#else

int main()
{
    return 0;
}

#endif // _WIN32
