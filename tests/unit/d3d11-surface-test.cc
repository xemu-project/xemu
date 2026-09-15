#include "hw/xbox/nv2a/pgraph/d3d11/clear.h"
#include "hw/xbox/nv2a/pgraph/d3d11/surface.h"

#ifdef _WIN32

#include <d3d11.h>

#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>

#include <wrl/client.h>

namespace {

using Microsoft::WRL::ComPtr;
using xemu::D3D11ClearParams;
using xemu::D3D11ClearPrimitive;
using xemu::D3D11ClearStatus;
using xemu::D3D11ClearSurfaceFormat;
using xemu::D3D11ClearSurfaceKind;
using xemu::D3D11SurfaceAntialias;
using xemu::D3D11SurfaceCache;
using xemu::D3D11SurfaceDescriptor;
using xemu::D3D11SurfaceFormat;
using xemu::D3D11SurfaceResource;
using xemu::D3D11SurfaceType;

struct DevicePair {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
};

bool CreateWarp(DevicePair *pair)
{
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                       0, nullptr, 0, D3D11_SDK_VERSION,
                                       &pair->device, &level, &pair->context));
}

bool Check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "d3d11-surface-test: %s\n", message);
    }
    return condition;
}

bool TestColorPitchAndClear(const DevicePair &pair)
{
    constexpr uint32_t width = 5;
    constexpr uint32_t height = 3;
    constexpr uint32_t pitch = 24;
    D3D11SurfaceDescriptor descriptor = {
        7,
        width,
        height,
        pitch,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    std::vector<uint8_t> vram(7 + pitch * height + 11, 0xcc);
    D3D11SurfaceCache cache(pair.device.Get(), pair.context.Get());
    auto surface = cache.Acquire(descriptor, vram.data(), vram.size());
    if (!Check(surface && surface->valid(), "color surface creation")) {
        return false;
    }
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width * 4; ++x) {
            vram[descriptor.offset + y * pitch + x] =
                static_cast<uint8_t>(x + y * 13);
        }
    }
    if (!Check(surface->Upload(vram.data(), vram.size()), "color upload")) {
        return false;
    }
    D3D11ClearParams clear = {};
    clear.surface_kind = D3D11ClearSurfaceKind::Color;
    clear.surface_format = D3D11ClearSurfaceFormat::ColorBgra8Unorm;
    clear.rect = { 0, 0, width - 1, height - 1 };
    clear.surface_width = width;
    clear.surface_height = height;
    clear.color[0] = 0.25f;
    clear.color[1] = 0.5f;
    clear.color[2] = 0.75f;
    clear.color[3] = 1.0f;
    D3D11ClearPrimitive primitive(pair.device.Get(), pair.context.Get());
    if (!Check(
            primitive.Clear(clear, surface->render_target(), nullptr).status ==
                D3D11ClearStatus::Cleared,
            "color clear")) {
        return false;
    }
    surface->MarkDrawDirty();
    const std::vector<uint8_t> before = vram;
    if (!Check(surface->Download(vram.data(), vram.size()), "color download")) {
        return false;
    }
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = width * 4; x < pitch; ++x) {
            if (!Check(vram[descriptor.offset + y * pitch + x] ==
                           before[descriptor.offset + y * pitch + x],
                       "pitch padding changed")) {
                return false;
            }
        }
    }
    return Check(vram[descriptor.offset + 0] == 191 &&
                     vram[descriptor.offset + 1] == 128 &&
                     vram[descriptor.offset + 2] == 64 &&
                     vram[descriptor.offset + 3] == 255,
                 "color clear roundtrip bytes");
}

bool TestDepthBytesAndStencil(const DevicePair &pair)
{
    constexpr uint32_t width = 3;
    constexpr uint32_t height = 2;
    D3D11SurfaceCache cache(pair.device.Get(), pair.context.Get());
    D3D11SurfaceDescriptor z16_desc = {
        0,
        width,
        height,
        8,
        D3D11SurfaceFormat::Z16,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    std::vector<uint8_t> z16_vram(32, 0xee);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width * 2; ++x) {
            z16_vram[y * z16_desc.pitch + x] =
                static_cast<uint8_t>(0x10 + x + y * 7);
        }
    }
    const std::vector<uint8_t> z16_expected = z16_vram;
    auto z16 = cache.Acquire(z16_desc, z16_vram.data(), z16_vram.size());
    if (!Check(z16 && z16->Upload(z16_vram.data(), z16_vram.size()),
               "Z16 upload")) {
        return false;
    }
    if (!Check(z16->Download(z16_vram.data(), z16_vram.size()) &&
                   z16_vram == z16_expected,
               "Z16 exact bytes")) {
        return false;
    }

    D3D11SurfaceDescriptor z24_desc = {
        32,
        width,
        height,
        16,
        D3D11SurfaceFormat::Z24S8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    std::vector<uint8_t> vram(80, 0xaa);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            uint8_t *pixel =
                vram.data() + z24_desc.offset + y * z24_desc.pitch + x * 4;
            pixel[0] = 0x34;
            pixel[1] = 0x12;
            pixel[2] = 0x0f;
            pixel[3] = 0x5a;
        }
    }
    auto z24 = cache.Acquire(z24_desc, vram.data(), vram.size());
    if (!Check(z24 && z24->Upload(vram.data(), vram.size()), "Z24S8 upload")) {
        return false;
    }
    D3D11ClearParams clear = {};
    clear.surface_kind = D3D11ClearSurfaceKind::DepthStencil;
    clear.surface_format = D3D11ClearSurfaceFormat::Z24S8Fixed;
    clear.rect = { 0, 0, width - 1, height - 1 };
    clear.surface_width = width;
    clear.surface_height = height;
    clear.clear_depth = false;
    clear.clear_stencil = true;
    clear.stencil = 0xa7;
    D3D11ClearPrimitive primitive(pair.device.Get(), pair.context.Get());
    if (!Check(primitive.Clear(clear, nullptr, z24->depth_stencil()).status ==
                   D3D11ClearStatus::Cleared,
               "stencil-only clear")) {
        return false;
    }
    z24->MarkDrawDirty();
    if (!Check(z24->Download(vram.data(), vram.size()), "Z24S8 download")) {
        return false;
    }
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t *pixel =
                vram.data() + z24_desc.offset + y * z24_desc.pitch + x * 4;
            if (!Check(pixel[0] == 0x34 && pixel[1] == 0x12 &&
                           pixel[2] == 0x0f && pixel[3] == 0xa7,
                       "stencil-only clear changed depth bytes")) {
                return false;
            }
        }
    }
    return true;
}

bool TestTerminalRollbackFailure(const DevicePair &pair,
                                 xemu::D3D11SurfaceStatus failure_status,
                                 HRESULT failure_hresult,
                                 const char *failure_name)
{
    D3D11SurfaceCache cache(pair.device.Get(), pair.context.Get());
    std::vector<uint8_t> vram(128, 0x6d);
    const D3D11SurfaceDescriptor first_descriptor = {
        0,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    const D3D11SurfaceDescriptor second_descriptor = {
        40,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    const D3D11SurfaceDescriptor replacement_descriptor = {
        20,
        4,
        3,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    auto first = cache.Acquire(first_descriptor, vram.data(), vram.size());
    auto second = cache.Acquire(second_descriptor, vram.data(), vram.size());
    if (!Check(first && second && first->Upload(vram.data(), vram.size()) &&
                   second->Upload(vram.data(), vram.size()) &&
                   first->MarkDrawDirty() && second->MarkDrawDirty(),
               failure_name)) {
        return false;
    }
    const std::vector<uint8_t> before = vram;
    second->InjectDownloadFailureForTesting(failure_status, failure_hresult);
    const bool rejected =
        !cache.Acquire(replacement_descriptor, vram.data(), vram.size());
    const auto second_snapshot = second->SnapshotAuthority();
    return Check(rejected && cache.size() == 2 && !cache.valid() &&
                     cache.status() == failure_status &&
                     cache.last_hresult() == failure_hresult &&
                     cache.last_operation_status() == failure_status &&
                     cache.last_operation_hresult() == failure_hresult &&
                     second->status() == failure_status && !second->valid() &&
                     second->last_hresult() == failure_hresult &&
                     second->last_query_hresult() == failure_hresult &&
                     second->last_operation_status() == failure_status &&
                     second->last_operation_hresult() == failure_hresult &&
                     second_snapshot.status == failure_status &&
                     first->status() == xemu::D3D11SurfaceStatus::Ok &&
                     first->valid() && first->draw_dirty() &&
                     first->download_dirty() && second->draw_dirty() &&
                     second->download_dirty() && vram == before,
                 failure_name);
}

bool TestCacheGenerationOverlapAndValidation(const DevicePair &pair)
{
    DevicePair other;
    if (!Check(CreateWarp(&other), "second WARP device")) {
        return false;
    }
    D3D11SurfaceCache mismatched_cache(pair.device.Get(), other.context.Get());
    if (!Check(!mismatched_cache.valid(), "mismatched device accepted")) {
        return false;
    }
    D3D11SurfaceCache cache(pair.device.Get(), pair.context.Get());
    D3D11SurfaceDescriptor descriptor = {
        4,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    std::vector<uint8_t> vram(128, 0);
    D3D11SurfaceResource mismatched_resource(pair.device.Get(),
                                             other.context.Get(), descriptor);
    if (!Check(!mismatched_resource.valid(),
               "mismatched resource device accepted")) {
        return false;
    }
    auto first = cache.Acquire(descriptor, vram.data(), vram.size());
    auto same = cache.Acquire(descriptor, vram.data(), vram.size());
    if (!Check(first && same && first.get() == same.get() &&
                   first.generation() == same.generation(),
               "exact descriptor was not reused")) {
        return false;
    }
    if (!Check(!first->Upload(nullptr, vram.size()), "null VRAM accepted") ||
        !Check(first->Upload(vram.data(), vram.size()),
               "valid VRAM retry after range failure")) {
        return false;
    }
    if (!Check(!cache.Acquire({ 0, 4, 2, 4, D3D11SurfaceFormat::Bgr8A8,
                                D3D11SurfaceType::LinearPitch, 1,
                                D3D11SurfaceAntialias::Center1 },
                              vram.data(), vram.size()),
               "invalid pitch accepted")) {
        return false;
    }
    for (uint32_t y = 0; y < descriptor.height; ++y) {
        for (uint32_t x = 0; x < descriptor.width * 4; ++x) {
            vram[descriptor.offset + y * descriptor.pitch + x] =
                static_cast<uint8_t>(0x80 + x + y * 17);
        }
    }
    const std::vector<uint8_t> old_bytes = vram;
    if (!Check(first->Upload(vram.data(), vram.size()),
               "overlap source upload")) {
        return false;
    }
    first->MarkDrawDirty();
    const auto old_generation = first.generation();
    ID3D11Texture2D *old_texture = first->texture();
    const D3D11SurfaceDescriptor replacement_descriptor = {
        8,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    if (!Check(!cache.Acquire(replacement_descriptor, vram.data(), 0) &&
                   cache.size() == 1 && first->draw_dirty(),
               "failed overlap replacement was not atomic")) {
        return false;
    }
    auto replacement = cache.Acquire({ 8, 4, 2, 20, D3D11SurfaceFormat::Bgr8A8,
                                       D3D11SurfaceType::LinearPitch, 1,
                                       D3D11SurfaceAntialias::Center1 },
                                     vram.data(), vram.size());
    if (!Check(replacement && replacement.generation() > old_generation &&
                   cache.size() == 1 && first->valid() &&
                   first->texture() == old_texture &&
                   std::memcmp(vram.data() + descriptor.offset,
                               old_bytes.data() + descriptor.offset,
                               descriptor.width * 4) == 0,
               "overlap replacement")) {
        return false;
    }
    if (!Check(replacement->Upload(vram.data(), vram.size()) &&
                   replacement->MarkDrawDirty(),
               "replacement dirty transition") ||
        !Check(cache.Flush(vram.data(), vram.size()), "cache flush")) {
        return false;
    }

    bool cross_thread_rejected = false;
    std::thread wrong_thread([&] {
        cross_thread_rejected =
            !cache.Acquire(descriptor, vram.data(), vram.size());
    });
    wrong_thread.join();
    if (!Check(cross_thread_rejected &&
                   cache.last_operation_status() ==
                       xemu::D3D11SurfaceStatus::InvalidDevice,
               "cross-thread cache access accepted")) {
        return false;
    }
    std::thread wrong_resource_thread([&] {
        cross_thread_rejected =
            !replacement->Download(vram.data(), vram.size());
    });
    wrong_resource_thread.join();
    if (!Check(cross_thread_rejected &&
                   replacement->last_operation_status() ==
                       xemu::D3D11SurfaceStatus::InvalidDevice,
               "cross-thread resource access accepted")) {
        return false;
    }

    auto conflict_surface = cache.Acquire(
        { 80, 4, 2, 20, D3D11SurfaceFormat::Bgr8A8,
          D3D11SurfaceType::LinearPitch, 1, D3D11SurfaceAntialias::Center1 },
        vram.data(), vram.size());
    if (!Check(conflict_surface && !conflict_surface->MarkDrawDirty() &&
                   conflict_surface->upload_dirty() &&
                   conflict_surface->last_operation_status() ==
                       xemu::D3D11SurfaceStatus::Conflict &&
                   conflict_surface->valid() &&
                   conflict_surface->Upload(vram.data(), vram.size()) &&
                   conflict_surface->MarkDrawDirty() &&
                   conflict_surface->Download(vram.data(), vram.size()),
               "dirty conflict was not recoverable")) {
        return false;
    }

    D3D11SurfaceCache rollback_cache(pair.device.Get(), pair.context.Get());
    std::vector<uint8_t> rollback_vram(128, 0x5c);
    const D3D11SurfaceDescriptor rollback_a = {
        0,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    const D3D11SurfaceDescriptor rollback_b = {
        40,
        4,
        2,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    auto rollback_first = rollback_cache.Acquire(
        rollback_a, rollback_vram.data(), rollback_vram.size());
    auto rollback_second = rollback_cache.Acquire(
        rollback_b, rollback_vram.data(), rollback_vram.size());
    if (!Check(rollback_first && rollback_second &&
                   rollback_first->Upload(rollback_vram.data(),
                                          rollback_vram.size()) &&
                   rollback_second->Upload(rollback_vram.data(),
                                           rollback_vram.size()) &&
                   rollback_first->MarkDrawDirty() &&
                   rollback_second->MarkDrawDirty(),
               "rollback setup")) {
        return false;
    }
    const std::vector<uint8_t> rollback_bytes = rollback_vram;
    const auto first_authority = rollback_first->SnapshotAuthority();
    const auto second_authority = rollback_second->SnapshotAuthority();
    rollback_second->InjectDownloadFailureForTesting();
    const D3D11SurfaceDescriptor rollback_replacement = {
        20,
        4,
        3,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    if (!Check(
            !rollback_cache.Acquire(rollback_replacement, rollback_vram.data(),
                                    rollback_vram.size()) &&
                rollback_cache.size() == 2 && rollback_first->draw_dirty() &&
                rollback_first->download_dirty() &&
                rollback_second->draw_dirty() &&
                rollback_second->download_dirty() &&
                rollback_cache.status() == xemu::D3D11SurfaceStatus::Ok &&
                rollback_cache.last_operation_status() ==
                    xemu::D3D11SurfaceStatus::Unsupported &&
                rollback_cache.last_operation_hresult() == E_FAIL &&
                rollback_second->status() == second_authority.status &&
                rollback_second->last_hresult() == second_authority.hresult &&
                rollback_first->status() == first_authority.status &&
                rollback_first->last_hresult() == first_authority.hresult &&
                rollback_first->last_operation_status() ==
                    first_authority.operation_status &&
                rollback_first->last_operation_hresult() ==
                    first_authority.operation_hresult &&
                rollback_vram == rollback_bytes,
            "overlap rollback did not restore authority")) {
        return false;
    }
    if (!Check(
            rollback_cache.Acquire(rollback_replacement, rollback_vram.data(),
                                   rollback_vram.size()) &&
                rollback_cache.size() == 1 && rollback_vram == rollback_bytes,
            "overlap retry after rollback")) {
        return false;
    }
    if (!TestTerminalRollbackFailure(pair, xemu::D3D11SurfaceStatus::DeviceLost,
                                     DXGI_ERROR_DEVICE_REMOVED,
                                     "device-lost rollback propagation") ||
        !TestTerminalRollbackFailure(pair, xemu::D3D11SurfaceStatus::Timeout,
                                     HRESULT_FROM_WIN32(WAIT_TIMEOUT),
                                     "timeout rollback propagation")) {
        return false;
    }

    D3D11SurfaceCache timeout_cache(pair.device.Get(), pair.context.Get(), 0);
    auto timeout_surface =
        timeout_cache.Acquire(descriptor, vram.data(), vram.size());
    if (!Check(
            timeout_surface && !timeout_cache.Flush(vram.data(), vram.size()) &&
                timeout_cache.status() == xemu::D3D11SurfaceStatus::Timeout &&
                !timeout_cache.valid(),
            "GPU wait timeout was not terminal")) {
        return false;
    }
    return Check(!timeout_cache.Acquire(descriptor, vram.data(), vram.size()) &&
                     timeout_cache.status() ==
                         xemu::D3D11SurfaceStatus::Timeout,
                 "terminal cache accepted acquisition");
}

} // namespace

int main()
{
    DevicePair pair;
    if (!CreateWarp(&pair)) {
        fprintf(stderr, "d3d11-surface-test: WARP unavailable\n");
        return 1;
    }
    return TestColorPitchAndClear(pair) && TestDepthBytesAndStencil(pair) &&
                   TestCacheGenerationOverlapAndValidation(pair) ?
               0 :
               1;
}

#else

int main()
{
    return 0;
}

#endif // _WIN32
