#include "hw/xbox/nv2a/pgraph/d3d11/clear-executor.h"

#ifdef _WIN32

#include "hw/xbox/nv2a/nv2a_int.h"

#include <d3d11.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <wrl/client.h>

extern "C" void pgraph_get_clear_color(PGRAPHState *, float rgba[4])
{
    rgba[0] = 0.25f;
    rgba[1] = 0.5f;
    rgba[2] = 0.75f;
    rgba[3] = 1.0f;
}

static DMAObject g_dma;
static hwaddr g_dirty_offset;
static hwaddr g_dirty_size;
static std::vector<unsigned> g_dirty_clients;

extern "C" DMAObject nv_dma_load(NV2AState *, hwaddr)
{
    return g_dma;
}

extern "C" uint64_t memory_region_size(MemoryRegion *mr)
{
    return int128_get64(mr->size);
}

extern "C" void memory_region_set_client_dirty(MemoryRegion *, hwaddr offset,
                                               hwaddr size, unsigned client)
{
    g_dirty_offset = offset;
    g_dirty_size = size;
    g_dirty_clients.push_back(client);
}

namespace {

using Microsoft::WRL::ComPtr;
using xemu::D3D11ClearExecutor;
using xemu::D3D11ClearExecutorStatus;
using xemu::D3D11ClearFlushReport;
using xemu::D3D11PgraphContext;
using xemu::D3D11SurfaceAntialias;
using xemu::D3D11SurfaceDescriptor;
using xemu::D3D11SurfaceFormat;
using xemu::D3D11SurfaceResource;
using xemu::D3D11SurfaceSnapshot;
using xemu::D3D11SurfaceStatus;
using xemu::D3D11SurfaceType;

static_assert(
    std::is_same_v<decltype(std::declval<D3D11SurfaceSnapshot &>().get()),
                   D3D11SurfaceResource *>);
static_assert(
    std::is_same_v<decltype(std::declval<const D3D11SurfaceSnapshot &>().get()),
                   const D3D11SurfaceResource *>);
static_assert(std::is_same_v<
              decltype(std::declval<D3D11SurfaceSnapshot &>().operator->()),
              D3D11SurfaceResource *>);
static_assert(
    std::is_same_v<
        decltype(std::declval<const D3D11SurfaceSnapshot &>().operator->()),
        const D3D11SurfaceResource *>);
static_assert(std::is_same_v<decltype(std::declval<D3D11SurfaceSnapshot &>()
                                          .shared_resource()),
                             std::shared_ptr<D3D11SurfaceResource>>);
static_assert(
    std::is_same_v<decltype(std::declval<const D3D11SurfaceSnapshot &>()
                                .shared_resource()),
                   std::shared_ptr<const D3D11SurfaceResource>>);
static_assert(std::is_same_v<decltype(std::declval<const D3D11ClearExecutor &>()
                                          .color_surface()),
                             const D3D11SurfaceSnapshot &>);
static_assert(std::is_same_v<decltype(std::declval<const D3D11ClearExecutor &>()
                                          .depth_stencil_surface()),
                             const D3D11SurfaceSnapshot &>);
static_assert(
    std::is_same_v<decltype(std::declval<D3D11PgraphContext &>().Upload(
                       std::declval<D3D11SurfaceSnapshot &>(), nullptr, 0)),
                   bool>);
static_assert(
    std::is_same_v<decltype(std::declval<D3D11PgraphContext &>().Download(
                       std::declval<D3D11SurfaceSnapshot &>(), nullptr, 0)),
                   bool>);

struct DevicePair {
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
};

bool Check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "d3d11-clear-executor-test: %s\n", message);
    }
    return condition;
}

bool CreateWarp(DevicePair *pair)
{
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                       0, nullptr, 0, D3D11_SDK_VERSION,
                                       &pair->device, &level, &pair->context));
}

void InitState(NV2AState *state, MemoryRegion *vram,
               std::vector<uint8_t> *vram_bytes,
               std::vector<uint8_t> *ramin_bytes)
{
    *state = {};
    vram_bytes->assign(256, 0x9b);
    ramin_bytes->assign(64, 0);
    vram->size = int128_make64(vram_bytes->size());
    state->ramin.size = int128_make64(ramin_bytes->size());
    state->vram = vram;
    state->vram_ptr = vram_bytes->data();
    state->ramin_ptr = ramin_bytes->data();
    state->pgraph.surface_type = NV097_SET_SURFACE_FORMAT_TYPE_PITCH;
    state->pgraph.surface_scale_factor = 1;
    state->pgraph.surface_shape.color_format =
        NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8;
    state->pgraph.surface_shape.zeta_format =
        NV097_SET_SURFACE_FORMAT_ZETA_Z24S8;
    state->pgraph.surface_shape.anti_aliasing =
        NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1;
    state->pgraph.surface_shape.clip_x = 1;
    state->pgraph.surface_shape.clip_y = 1;
    state->pgraph.surface_shape.clip_width = 4;
    state->pgraph.surface_shape.clip_height = 3;
    state->pgraph.surface_color.offset = 4;
    state->pgraph.surface_color.pitch = 20;
    state->pgraph.surface_zeta.offset = 96;
    state->pgraph.surface_zeta.pitch = 20;
    /* The bound dimensions are authoritative and leave four bytes of pitch
     * padding after each four-pixel color row. */
    state->pgraph.surface_binding_dim = { 0, 0, 0, 0, 4, 4 };
    state->pgraph.dma_color = 0;
    state->pgraph.dma_zeta = 16;
    state->pgraph.regs_[NV_PGRAPH_CLEARRECTX] = 1 | (4u << 16);
    state->pgraph.regs_[NV_PGRAPH_CLEARRECTY] = 1 | (3u << 16);
    state->pgraph.regs_[NV_PGRAPH_COLORCLEARVALUE] = 0xff804020;
    state->pgraph.regs_[NV_PGRAPH_ZSTENCILCLEARVALUE] = 0x12345678;
    state->pgraph.regs_[NV_PGRAPH_SETUPRASTER] = 0;
}

bool TestSharedContext(const DevicePair &pair)
{
    D3D11PgraphContext context(pair.device.Get(), pair.context.Get());
    if (!Check(context.valid(), "shared context valid")) {
        return false;
    }
    std::atomic<bool> wrong_thread_valid = true;
    std::thread wrong_thread([&] { wrong_thread_valid = context.valid(); });
    wrong_thread.join();
    if (!Check(!wrong_thread_valid, "context rejects wrong thread")) {
        return false;
    }
    std::atomic<bool> wrong_thread_terminal_valid = true;
    std::thread wrong_thread_terminal_call([&] {
        context.MarkTerminalFailure(D3D11SurfaceStatus::DeviceLost, E_FAIL);
        wrong_thread_terminal_valid = context.valid();
    });
    wrong_thread_terminal_call.join();
    if (!Check(!wrong_thread_terminal_valid && context.valid(),
               "wrong thread cannot mutate terminal context")) {
        return false;
    }

    NV2AState state = {};
    MemoryRegion vram = {};
    std::vector<uint8_t> vram_bytes;
    std::vector<uint8_t> ramin_bytes;
    InitState(&state, &vram, &vram_bytes, &ramin_bytes);
    g_dma = { NV_DMA_IN_MEMORY_CLASS, 0, 8, 220 };

    /* The logical PGRAPH binding, rather than either executor's local
     * snapshot, owns dirty-state attribution across shared executors. */
    D3D11PgraphContext overlap_context(pair.device.Get(), pair.context.Get());
    D3D11ClearExecutor zeta_executor(overlap_context);
    D3D11ClearExecutor color_executor(overlap_context);
    NV2AState overlap_state = {};
    MemoryRegion overlap_vram = {};
    std::vector<uint8_t> overlap_vram_bytes;
    std::vector<uint8_t> overlap_ramin_bytes;
    InitState(&overlap_state, &overlap_vram, &overlap_vram_bytes,
              &overlap_ramin_bytes);
    overlap_state.pgraph.regs_[NV_PGRAPH_CLEARRECTX] = 0 | (4u << 16);
    overlap_state.pgraph.regs_[NV_PGRAPH_CLEARRECTY] = 0 | (4u << 16);
    g_dma = { NV_DMA_IN_MEMORY_CLASS, 0, 8, 220 };
    if (!Check(zeta_executor.Execute(&overlap_state, NV097_CLEAR_SURFACE_Z)
                   .depth_stencil.succeeded(),
               "shared executor zeta setup")) {
        return false;
    }
    overlap_state.pgraph.surface_color.offset = 92;
    if (!Check(color_executor.Execute(&overlap_state, NV097_CLEAR_SURFACE_COLOR)
                       .color.succeeded() &&
                   overlap_state.pgraph.surface_zeta.draw_dirty,
               "cross-executor color eviction preserves zeta dirty")) {
        return false;
    }
    D3D11ClearFlushReport cross_executor_report = {};
    if (!Check(color_executor.Flush(&overlap_state, &cross_executor_report) &&
                   cross_executor_report.implicit_depth_stencil_downloaded &&
                   !overlap_state.pgraph.surface_zeta.draw_dirty,
               "shared drain clears evicted zeta from logical binding")) {
        return false;
    }
    if (!Check(zeta_executor.Execute(&overlap_state, NV097_CLEAR_SURFACE_Z)
                       .depth_stencil.succeeded() &&
                   overlap_state.pgraph.surface_zeta.draw_dirty &&
                   color_executor
                       .Execute(&overlap_state, NV097_CLEAR_SURFACE_COLOR)
                       .color.succeeded() &&
                   zeta_executor.Execute(&overlap_state, NV097_CLEAR_SURFACE_Z)
                       .depth_stencil.succeeded(),
               "new zeta generation becomes dirty before old drain")) {
        return false;
    }
    D3D11ClearFlushReport old_generation_report = {};
    if (!Check(color_executor.Flush(&overlap_state, &old_generation_report) &&
                   overlap_state.pgraph.surface_zeta.draw_dirty,
               "old evicted event cannot clear newer zeta generation")) {
        return false;
    }
    if (!Check(zeta_executor.Flush(&overlap_state) &&
                   !overlap_state.pgraph.surface_zeta.draw_dirty,
               "new zeta generation drains independently")) {
        return false;
    }

    D3D11ClearExecutor first(context);
    D3D11ClearExecutor second(context);
    if (!Check(first.pgraph_context() == &context &&
                   second.pgraph_context() == &context,
               "executors borrow shared context")) {
        return false;
    }
    if (!Check(
            first.Execute(&state, NV097_CLEAR_SURFACE_COLOR).color.succeeded(),
            "injected executor clear")) {
        return false;
    }

    const D3D11SurfaceDescriptor descriptor = {
        12,
        4,
        4,
        20,
        D3D11SurfaceFormat::Bgr8A8,
        D3D11SurfaceType::LinearPitch,
        1,
        D3D11SurfaceAntialias::Center1,
    };
    const D3D11SurfaceSnapshot cached =
        context.surface_cache().Find(descriptor);
    if (!Check(cached && first.color_surface() &&
                   cached.get() == first.color_surface().get() &&
                   cached.generation() == first.color_surface().generation() &&
                   context.surface_cache().size() == 1,
               "context and injected executor share snapshot")) {
        return false;
    }

    if (!Check(second.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                       .color.succeeded() &&
                   second.color_surface().get() == cached.get() &&
                   second.color_surface().generation() == cached.generation() &&
                   context.surface_cache().size() == 1,
               "shared exact descriptor is not duplicated")) {
        return false;
    }

    state.pgraph.surface_color.offset = 32;
    if (!Check(second.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                       .color.succeeded() &&
                   !cached->draw_dirty() && context.surface_cache().size() == 1,
               "overlap download is context-owned")) {
        return false;
    }
    if (!Check(second.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                       .color.succeeded() &&
                   state.pgraph.surface_color.draw_dirty,
               "old pending event cannot clear new generation")) {
        return false;
    }
    std::atomic<bool> wrong_thread_drained = false;
    std::thread wrong_thread_drain(
        [&] { wrong_thread_drained = !context.TakeDownloadEvents().empty(); });
    wrong_thread_drain.join();
    if (!Check(!wrong_thread_drained,
               "wrong thread cannot drain context events")) {
        return false;
    }
    D3D11ClearFlushReport second_report = {};
    if (!Check(second.Flush(&state, &second_report) &&
                   second_report.implicit_color_downloaded &&
                   second_report.ranges.size() == 2 &&
                   second_report.ranges[0].generation == cached.generation() &&
                   !state.pgraph.surface_color.draw_dirty,
               "shared overlap event drains exactly once")) {
        return false;
    }
    D3D11ClearFlushReport first_report = {};
    return Check(first.Flush(&state, &first_report) &&
                     first_report.ranges.empty(),
                 "second executor does not redrain overlap event");
}

bool TestTerminalContext(const DevicePair &pair)
{
    NV2AState state = {};
    MemoryRegion vram = {};
    std::vector<uint8_t> vram_bytes;
    std::vector<uint8_t> ramin_bytes;
    InitState(&state, &vram, &vram_bytes, &ramin_bytes);
    g_dma = { NV_DMA_IN_MEMORY_CLASS, 0, 8, 220 };

    D3D11PgraphContext timeout_context(pair.device.Get(), pair.context.Get(),
                                       0);
    D3D11ClearExecutor timeout_executor(timeout_context);
    if (!Check(timeout_executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                           .status == D3D11ClearExecutorStatus::DeviceError &&
                   !timeout_context.valid(),
               "upload timeout makes context terminal")) {
        return false;
    }
    if (!Check(timeout_executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                       .status == D3D11ClearExecutorStatus::DeviceError,
               "terminal context rejects reuse")) {
        return false;
    }

    D3D11PgraphContext download_context(pair.device.Get(), pair.context.Get());
    D3D11ClearExecutor download_executor(download_context);
    if (!Check(download_executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR)
                   .color.succeeded(),
               "download terminal setup")) {
        return false;
    }
    state.pgraph.regs_[NV_PGRAPH_CLEARRECTX] = 0 | (4u << 16);
    state.pgraph.regs_[NV_PGRAPH_CLEARRECTY] = 0 | (4u << 16);
    if (!Check(download_executor.Execute(&state, NV097_CLEAR_SURFACE_Z)
                   .depth_stencil.succeeded(),
               "download terminal zeta setup")) {
        return false;
    }
    D3D11SurfaceSnapshot injection_snapshot =
        download_executor.depth_stencil_surface();
    injection_snapshot->InjectDownloadFailureForTesting(
        D3D11SurfaceStatus::DeviceLost, E_FAIL);
    g_dirty_clients.clear();
    D3D11ClearFlushReport report = {};
    if (!Check(!download_executor.Flush(&state, &report) &&
                   report.status == D3D11ClearExecutorStatus::DeviceError &&
                   report.color_downloaded && report.ranges.size() == 1 &&
                   g_dirty_clients.size() == 2 && !download_context.valid() &&
                   !state.pgraph.surface_color.draw_dirty &&
                   state.pgraph.surface_zeta.draw_dirty &&
                   injection_snapshot->status() ==
                       D3D11SurfaceStatus::DeviceLost,
               "successful color event drains before terminal zeta failure")) {
        return false;
    }
    if (!Check(download_context.pending_download_events().empty(),
               "terminal flush drains all pending events")) {
        return false;
    }
    return Check(
        download_executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR).status ==
            D3D11ClearExecutorStatus::DeviceError,
        "download terminal context rejects reuse");
}

bool TestExecutor()
{
    DevicePair pair;
    if (!Check(CreateWarp(&pair), "WARP unavailable")) {
        return false;
    }
    NV2AState state = {};
    MemoryRegion vram = {};
    std::vector<uint8_t> vram_bytes;
    std::vector<uint8_t> ramin_bytes;
    InitState(&state, &vram, &vram_bytes, &ramin_bytes);
    g_dma = { NV_DMA_IN_MEMORY_CLASS, 0, 8, 220 };

    D3D11ClearExecutor executor(pair.device.Get(), pair.context.Get());
    if (!Check(executor.pgraph_context() != nullptr,
               "legacy constructor owns fallback context")) {
        return false;
    }
    auto color = executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(color.status == D3D11ClearExecutorStatus::Cleared &&
                   color.color.succeeded() &&
                   state.pgraph.surface_color.draw_dirty,
               "color clear")) {
        return false;
    }
    const std::vector<uint8_t> padding_before = {
        vram_bytes[12 + 16],
        vram_bytes[12 + 36],
        vram_bytes[12 + 56],
        vram_bytes[12 + 76],
    };
    g_dirty_clients.clear();
    D3D11ClearFlushReport report = {};
    if (!Check(executor.Flush(&state, &report) && report.color_downloaded &&
                   !state.pgraph.surface_color.draw_dirty &&
                   report.color_offset == 12 && report.color_size == 80,
               "color flush")) {
        return false;
    }
    if (!Check(vram_bytes[12 + 16] == padding_before[0] &&
                   vram_bytes[12 + 36] == padding_before[1] &&
                   vram_bytes[12 + 56] == padding_before[2] &&
                   vram_bytes[12 + 76] == padding_before[3] &&
                   vram_bytes[12 + 20 + 4] == 0xbf &&
                   vram_bytes[12 + 20 + 5] == 0x80 &&
                   vram_bytes[12 + 20 + 6] == 0x40 &&
                   vram_bytes[12 + 20 + 7] == 0xff,
               "pitch padding and clear pixel bytes")) {
        return false;
    }
    if (!Check(g_dirty_offset == 12 && g_dirty_size == 80 &&
                   g_dirty_clients.size() == 2 &&
                   g_dirty_clients[0] == DIRTY_MEMORY_VGA &&
                   g_dirty_clients[1] == DIRTY_MEMORY_NV2A_TEX,
               "flush range was not exact")) {
        return false;
    }

    state.pgraph.regs_[NV_PGRAPH_CLEARRECTX] = 0 | (4u << 16);
    state.pgraph.regs_[NV_PGRAPH_CLEARRECTY] = 0 | (3u << 16);
    g_dma.address = 8;
    g_dma.limit = 220;
    const auto combined = executor.Execute(
        &state, NV097_CLEAR_SURFACE_COLOR | NV097_CLEAR_SURFACE_Z |
                    NV097_CLEAR_SURFACE_STENCIL);
    if (!Check(combined.status == D3D11ClearExecutorStatus::Cleared &&
                   combined.color.succeeded() &&
                   combined.depth_stencil.succeeded() &&
                   state.pgraph.surface_color.draw_dirty &&
                   state.pgraph.surface_zeta.draw_dirty,
               "combined color/zeta clear")) {
        return false;
    }

    state.pgraph.surface_shape.zeta_format = 0xff;
    const auto independent = executor.Execute(
        &state, NV097_CLEAR_SURFACE_COLOR | NV097_CLEAR_SURFACE_Z);
    if (!Check(independent.status == D3D11ClearExecutorStatus::Partial &&
                   independent.color.succeeded() &&
                   independent.depth_stencil.status ==
                       D3D11ClearExecutorStatus::Unsupported &&
                   state.pgraph.surface_color.draw_dirty,
               "color/zeta failure independence")) {
        return false;
    }
    state.pgraph.surface_shape.zeta_format =
        NV097_SET_SURFACE_FORMAT_ZETA_Z24S8;
    D3D11ClearFlushReport combined_report = {};
    if (!Check(executor.Flush(&state, &combined_report) &&
                   combined_report.color_downloaded &&
                   combined_report.depth_stencil_downloaded &&
                   !state.pgraph.surface_color.draw_dirty &&
                   !state.pgraph.surface_zeta.draw_dirty &&
                   combined_report.color_offset == 12 &&
                   combined_report.color_size == 80 &&
                   combined_report.depth_stencil_offset == 104 &&
                   combined_report.depth_stencil_size == 80 &&
                   g_dirty_offset == 104 && g_dirty_size == 80,
               "combined flush")) {
        return false;
    }

    /* dma.limit is inclusive: the final byte is surface.offset + size - 1. */
    g_dma.limit = 83;
    const auto inclusive_limit =
        executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(inclusive_limit.status == D3D11ClearExecutorStatus::Cleared &&
                   inclusive_limit.color.succeeded(),
               "inclusive DMA limit accepted")) {
        return false;
    }
    g_dma.limit = 82;
    const auto exclusive_limit =
        executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(exclusive_limit.color.status ==
                   D3D11ClearExecutorStatus::OutOfRange,
               "exclusive DMA limit rejected")) {
        return false;
    }
    g_dma.limit = 220;

    const auto zeta_again = executor.Execute(&state, NV097_CLEAR_SURFACE_Z);
    if (!Check(zeta_again.status == D3D11ClearExecutorStatus::Cleared &&
                   zeta_again.depth_stencil.succeeded() &&
                   state.pgraph.surface_zeta.draw_dirty,
               "zeta overlap setup")) {
        return false;
    }
    state.pgraph.surface_color.offset = 96;
    const auto overlapping_color =
        executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(overlapping_color.color.succeeded() &&
                   state.pgraph.surface_zeta.draw_dirty,
               "overlap download remains dirty until shared flush")) {
        return false;
    }
    D3D11ClearFlushReport overlap_report = {};
    if (!Check(executor.Flush(&state, &overlap_report) &&
                   overlap_report.color_downloaded &&
                   overlap_report.implicit_depth_stencil_downloaded &&
                   overlap_report.implicit_depth_stencil_offset == 104 &&
                   overlap_report.implicit_depth_stencil_size == 80 &&
                   overlap_report.retired_downloaded &&
                   overlap_report.retired_offset == 12 &&
                   overlap_report.retired_size == 80 &&
                   overlap_report.ranges.size() == 3 &&
                   !state.pgraph.surface_zeta.draw_dirty &&
                   executor.retired_surface_count() == 0,
               "overlap color flush")) {
        return false;
    }
    state.pgraph.surface_color.offset = 4;

    /* A non-overlapping replacement retires A.  A later overlapping acquire
     * downloads both A and the current B before evicting them.  Both implicit
     * writes must be reported exactly once, even when Flush first sees an
     * invalid state. */
    g_dma.address = 0;
    g_dma.limit = 255;
    state.pgraph.surface_color.offset = 0;
    const auto retired_a = executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(retired_a.color.succeeded(), "retired A setup")) {
        return false;
    }
    state.pgraph.surface_color.offset = 80;
    const auto retired_b = executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(retired_b.color.succeeded() &&
                   executor.retired_surface_count() == 1,
               "retired B setup")) {
        return false;
    }
    state.pgraph.surface_color.offset = 160;
    const auto retired_c = executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(retired_c.color.succeeded() &&
                   executor.retired_surface_count() == 2,
               "retired C setup")) {
        return false;
    }
    state.pgraph.surface_color.offset = 40;
    const auto retired_overlap =
        executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(retired_overlap.color.succeeded() &&
                   executor.retired_surface_count() == 1,
               "retired overlap acquire")) {
        return false;
    }
    NV2AState invalid_state = state;
    invalid_state.vram = nullptr;
    D3D11ClearFlushReport invalid_report = {};
    if (!Check(!executor.Flush(&invalid_state, &invalid_report) &&
                   invalid_report.status == D3D11ClearExecutorStatus::Invalid,
               "invalid flush preserves pending report")) {
        return false;
    }
    D3D11ClearFlushReport retired_report = {};
    if (!Check(executor.Flush(&state, &retired_report) &&
                   retired_report.ranges.size() == 4,
               "retired report drain")) {
        return false;
    }
    size_t retired_a_count = 0;
    size_t retired_b_count = 0;
    for (const auto &range : retired_report.ranges) {
        retired_a_count += range.offset == 0 && range.implicit && range.retired;
        retired_b_count +=
            range.offset == 80 && range.implicit && range.retired;
    }
    if (!Check(retired_a_count == 1 && retired_b_count == 1,
               "retired ranges reported exactly once")) {
        return false;
    }
    D3D11ClearFlushReport drained_report = {};
    if (!Check(executor.Flush(&state, &drained_report) &&
                   drained_report.ranges.empty() &&
                   executor.retired_surface_count() == 0,
               "retired report drained once")) {
        return false;
    }
    g_dma.address = 8;
    state.pgraph.surface_color.offset = 4;

    g_dma.limit = 70;
    const auto dma_limit = executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(dma_limit.color.status == D3D11ClearExecutorStatus::OutOfRange,
               "DMA limit bounds")) {
        return false;
    }
    g_dma.limit = 220;
    g_dma.address = 240;
    const auto out_of_range =
        executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR);
    if (!Check(out_of_range.color.status ==
                   D3D11ClearExecutorStatus::OutOfRange,
               "DMA/VRAM bounds")) {
        return false;
    }

    std::atomic<D3D11ClearExecutorStatus> wrong_thread_status =
        D3D11ClearExecutorStatus::Cleared;
    std::thread wrong_thread([&] {
        wrong_thread_status =
            executor.Execute(&state, NV097_CLEAR_SURFACE_COLOR).status;
    });
    wrong_thread.join();
    return Check(wrong_thread_status == D3D11ClearExecutorStatus::WrongThread,
                 "wrong thread accepted");
}

} // namespace

int main()
{
    DevicePair pair;
    if (!Check(CreateWarp(&pair), "WARP unavailable")) {
        return 0;
    }
    return TestSharedContext(pair) && TestTerminalContext(pair) &&
                   TestExecutor() ?
               0 :
               1;
}

#else

int main()
{
    return 0;
}

#endif // _WIN32
