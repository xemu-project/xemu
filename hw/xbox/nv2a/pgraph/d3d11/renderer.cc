// xemu NV2A D3D11 PGRAPH renderer implementation.

#include "renderer.h"

#ifdef _WIN32

#define NOMINMAX
#include <windows.h>

#include <d3d11.h>

#include <memory>

#include <wrl/client.h>

#include "clear-executor.h"
#include "context.h"
#include "draw-executor.h"

struct D3D11RendererState {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> immediate_context;
    std::unique_ptr<xemu::D3D11PgraphContext> pgraph_context;
    std::unique_ptr<xemu::D3D11ClearExecutor> clear;
    std::unique_ptr<xemu::D3D11DrawExecutor> draw;
};

namespace {

const char *kDeviceCreateFailure =
    "D3D11 renderer could not create a hardware or WARP device";

bool CreateDevice(Microsoft::WRL::ComPtr<ID3D11Device> *device,
                  Microsoft::WRL::ComPtr<ID3D11DeviceContext> *context)
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
    HRESULT result = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels,
        ARRAYSIZE(levels), D3D11_SDK_VERSION, device->GetAddressOf(), &selected,
        context->GetAddressOf());
    if (SUCCEEDED(result)) {
        return true;
    }

    /* WARP is a useful Windows/CI fallback.  It is also preferable to
     * silently selecting another PGRAPH backend after a hardware adapter
     * transiently fails, because it exercises exactly the same D3D11
     * command and surface contracts. */
    result = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags,
                               levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                               device->GetAddressOf(), &selected,
                               context->GetAddressOf());
    return SUCCEEDED(result);
}

} // namespace

extern "C" bool d3d11_renderer_state_init(NV2AState *state,
                                          D3D11RendererState **out_state,
                                          const char **reason)
{
    (void)state;
    if (out_state == nullptr || reason == nullptr) {
        return false;
    }
    *out_state = nullptr;
    *reason = kDeviceCreateFailure;

    auto renderer = std::make_unique<D3D11RendererState>();
    if (!CreateDevice(&renderer->device, &renderer->immediate_context)) {
        return false;
    }

    renderer->pgraph_context = std::make_unique<xemu::D3D11PgraphContext>(
        renderer->device.Get(), renderer->immediate_context.Get());
    renderer->clear =
        std::make_unique<xemu::D3D11ClearExecutor>(*renderer->pgraph_context);
    renderer->draw =
        std::make_unique<xemu::D3D11DrawExecutor>(*renderer->pgraph_context);
    *out_state = renderer.release();
    *reason = nullptr;
    return true;
}

extern "C" void d3d11_renderer_state_finalize(D3D11RendererState *renderer)
{
    delete renderer;
}

extern "C" void d3d11_renderer_clear_surface(D3D11RendererState *renderer,
                                             NV2AState *state,
                                             uint32_t parameter)
{
    if (renderer == nullptr || renderer->clear == nullptr) {
        return;
    }
    (void)renderer->clear->Execute(state, parameter);
}

extern "C" void d3d11_renderer_draw(D3D11RendererState *renderer,
                                    NV2AState *state)
{
    if (renderer == nullptr || renderer->draw == nullptr) {
        return;
    }
    (void)renderer->draw->Execute(state);
}

extern "C" void d3d11_renderer_flush(D3D11RendererState *renderer,
                                     NV2AState *state)
{
    if (renderer == nullptr || renderer->pgraph_context == nullptr) {
        return;
    }
    if (renderer->draw != nullptr) {
        (void)renderer->draw->Flush(state);
    }
    if (renderer->clear != nullptr) {
        (void)renderer->clear->Flush(state);
    }
}

extern "C" void d3d11_renderer_surface_update(D3D11RendererState *renderer,
                                              NV2AState *state, bool upload,
                                              bool color_write, bool zeta_write)
{
    (void)upload;
    (void)color_write;
    (void)zeta_write;
    /* Surface updates are the synchronization boundary used by PGRAPH before
     * CPU-visible VRAM accesses.  Both executors share one cache, so one
     * flush drains all current and retired attachments exactly once. */
    d3d11_renderer_flush(renderer, state);
}

extern "C" void d3d11_renderer_flip_stall(D3D11RendererState *renderer,
                                          NV2AState *state)
{
    d3d11_renderer_flush(renderer, state);
    if (renderer != nullptr && renderer->immediate_context != nullptr) {
        renderer->immediate_context->Flush();
    }
}

#else

/* The D3D11 directory is only added to the build on Windows.  Keep a
 * translation-unit definition for tools that syntax-check all sources. */
struct D3D11RendererState {};

#endif // _WIN32
