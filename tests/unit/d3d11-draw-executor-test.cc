#include "hw/xbox/nv2a/pgraph/d3d11/draw-executor.h"

#ifdef _WIN32

#include "hw/xbox/nv2a/nv2a_int.h"

#include "nv2a_vsh_emulator_execution_state.h"

#include <d3d11.h>

#include <algorithm>
#include <cstdio>
#include <atomic>
#include <cstring>
#include <thread>
#include <utility>
#include <vector>

#include <wrl/client.h>

static DMAObject g_dma;
struct DirtyCall {
    hwaddr offset;
    hwaddr size;
    unsigned client;
};
static std::vector<DirtyCall> g_dirty_calls;

extern "C" DMAObject nv_dma_load(NV2AState *, hwaddr)
{
    return g_dma;
}

extern "C" uint64_t memory_region_size(MemoryRegion *region)
{
    return int128_get64(region->size);
}

extern "C" void memory_region_set_client_dirty(MemoryRegion *, hwaddr offset,
                                               hwaddr size, unsigned client)
{
    g_dirty_calls.push_back({ offset, size, client });
}

namespace {

using Microsoft::WRL::ComPtr;
using xemu::D3D11DrawExecutor;
using xemu::D3D11DrawExecutorStatus;
using xemu::D3D11PgraphContext;

bool Check(bool value, const char *message)
{
    if (!value) {
        std::fprintf(stderr, "d3d11-draw-executor-test: %s\n", message);
    }
    return value;
}

bool CreateWarp(ComPtr<ID3D11Device> *device,
                ComPtr<ID3D11DeviceContext> *context)
{
    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    return SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                       0, nullptr, 0, D3D11_SDK_VERSION,
                                       device->ReleaseAndGetAddressOf(), &level,
                                       context->ReleaseAndGetAddressOf()));
}

void InitState(NV2AState *state, MemoryRegion *vram,
               std::vector<uint8_t> *vram_bytes,
               std::vector<uint8_t> *ramin_bytes, float *positions,
               float *colors)
{
    *state = {};
    vram_bytes->assign(512, 0x55);
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
    state->pgraph.surface_shape.clip_width = 4;
    state->pgraph.surface_shape.clip_height = 4;
    state->pgraph.surface_color.pitch = 20;
    state->pgraph.surface_zeta.offset = 96;
    state->pgraph.surface_zeta.pitch = 20;
    state->pgraph.dma_color = 0;
    state->pgraph.dma_zeta = 16;
    state->pgraph.primitive_mode = PRIM_TYPE_TRIANGLES;
    for (unsigned int i = 0; i < 8; ++i) {
        state->pgraph.regs_[NV_PGRAPH_WINDOWCLIPX0 + i * 4] = 3u << 16;
        state->pgraph.regs_[NV_PGRAPH_WINDOWCLIPY0 + i * 4] = 3u << 16;
    }
    state->pgraph.regs_[NV_PGRAPH_CONTROL_0] =
        NV_PGRAPH_CONTROL_0_ZENABLE | NV_PGRAPH_CONTROL_0_ZWRITEENABLE |
        NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE |
        NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE |
        NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE |
        NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE;
    state->pgraph.regs_[NV_PGRAPH_CONTROL_0] |= NV_PGRAPH_CONTROL_0_ZFUNC_ALWAYS
                                                << 16;
    state->pgraph.regs_[NV_PGRAPH_CONTROL_1] = 0;
    state->pgraph.regs_[NV_PGRAPH_BLEND] = 2;
    state->pgraph.regs_[NV_PGRAPH_SETUPRASTER] = 0;
    auto &position = state->pgraph.vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    auto &color = state->pgraph.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE];
    position.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    position.count = 3;
    position.inline_buffer = positions;
    position.inline_buffer_populated = true;
    color.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    color.count = 4;
    color.inline_buffer = colors;
    color.inline_buffer_populated = true;
    state->pgraph.inline_buffer_length = 3;
    g_dma = { NV_DMA_IN_MEMORY_CLASS, 0, 8, 511 };
}

void WriteProgrammableToken(PGRAPHState *pg, uint32_t slot, uint32_t input,
                            uint32_t output, bool final,
                            bool context_output = false)
{
    uint32_t token[4] = { 0x00000000, 0x0020161B, 0x0836106C, 0x2070F859 };
    token[1] = (token[1] & ~(UINT32_C(0xF) << 9)) | (input << 9);
    token[3] = (token[3] & ~(UINT32_C(0xFF) << 3)) | (output << 3);
    token[3] = context_output ? token[3] & ~(UINT32_C(1) << 11) :
                                token[3] | (UINT32_C(1) << 11);
    token[3] = final ? token[3] | 1u : token[3] & ~1u;
    std::memcpy(pg->program_data[slot], token, sizeof(token));
}

bool TestProgrammableDraw()
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (!Check(CreateWarp(&device, &context), "WARP unavailable")) {
        return true;
    }
    D3D11PgraphContext owner(device.Get(), context.Get());
    D3D11DrawExecutor executor(owner);
    NV2AState state = {};
    MemoryRegion vram = {};
    std::vector<uint8_t> vram_bytes;
    std::vector<uint8_t> ramin_bytes;
    float positions[] = { 0.0f, 0.0f, 0.5f, 1.0f, 4.0f, 0.0f,
                          0.5f, 1.0f, 2.0f, 4.0f, 0.5f, 1.0f };
    float legacy_colors[] = { 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
                              0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f };
    float v1_colors[] = { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                          0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
    InitState(&state, &vram, &vram_bytes, &ramin_bytes, positions,
              legacy_colors);
    /* Use two identical indexed ranges so the adapter must deduplicate the
     * three source vertices while retaining six triangle-list indices. */
    constexpr size_t position_offset = 200;
    constexpr size_t legacy_color_offset = 248;
    constexpr size_t v1_color_offset = 296;
    std::memcpy(vram_bytes.data() + position_offset, positions,
                sizeof(positions));
    std::memcpy(vram_bytes.data() + legacy_color_offset, legacy_colors,
                sizeof(legacy_colors));
    std::memcpy(vram_bytes.data() + v1_color_offset, v1_colors,
                sizeof(v1_colors));
    state.pgraph.inline_buffer_length = 0;
    state.pgraph.draw_arrays_length = 2;
    state.pgraph.draw_arrays_start[0] = 0;
    state.pgraph.draw_arrays_count[0] = 3;
    state.pgraph.draw_arrays_start[1] = 0;
    state.pgraph.draw_arrays_count[1] = 3;
    auto &position = state.pgraph.vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    position.offset = position_offset;
    position.stride = 16;
    auto &legacy_color =
        state.pgraph.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE];
    legacy_color.offset = legacy_color_offset;
    legacy_color.stride = 16;
    auto &v1 = state.pgraph.vertex_attributes[1];
    v1.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    v1.count = 4;
    v1.offset = v1_color_offset;
    v1.stride = 16;
    xemu::D3D11VertexPlan plan = {};
    if (!Check(xemu::d3d11_build_vertex_plan(
                   &state, &state.pgraph, memory_region_size(state.vram),
                   &plan) == xemu::D3D11VertexAdapterStatus::Ready &&
                   plan.vertices.size() == 3 && plan.indices.size() == 6,
               "programmable indexed vertex deduplication")) {
        return false;
    }
    state.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(2) << 30;
    constexpr uint32_t start = 7;
    state.pgraph.regs_[NV_PGRAPH_CSV0_C] = start << 8;
    WriteProgrammableToken(&state.pgraph, start, 0, NV2AOR_POS, false);
    WriteProgrammableToken(&state.pgraph, start + 1, 1, NV2AOR_DIFFUSE, true);
    const auto drawn = executor.Execute(&state);
    if (!Check(drawn.status == D3D11DrawExecutorStatus::Drawn,
               "programmable VSH draw")) {
        return false;
    }
    if (!Check(state.pgraph.surface_color.draw_dirty,
               "programmable color becomes dirty")) {
        return false;
    }
    g_dirty_calls.clear();
    if (!Check(executor.Flush(&state), "programmable framebuffer readback")) {
        return false;
    }
    bool exact_coverage = true;
    for (size_t y = 0; y < 4; ++y) {
        for (size_t x = 0; x < 4; ++x) {
            const size_t offset = 8 + y * 20 + x * 4;
            const bool changed = vram_bytes[offset] != 0x55 ||
                                 vram_bytes[offset + 1] != 0x55 ||
                                 vram_bytes[offset + 2] != 0x55 ||
                                 vram_bytes[offset + 3] != 0x55;
            const bool expected_covered =
                y == 0 || ((y == 1 || y == 2) && (x == 1 || x == 2));
            exact_coverage &= changed == expected_covered;
            if (expected_covered) {
                exact_coverage &= vram_bytes[offset + 1] == 0xff;
            }
        }
    }
    if (!Check(exact_coverage,
               "programmable output has exact coordinate-sensitive coverage")) {
        return false;
    }

    NV2AState missing_final = {};
    InitState(&missing_final, &vram, &vram_bytes, &ramin_bytes, positions,
              legacy_colors);
    missing_final.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(2) << 30;
    missing_final.pgraph.regs_[NV_PGRAPH_CSV0_C] = start << 8;
    WriteProgrammableToken(&missing_final.pgraph, start, 0, NV2AOR_POS, false);
    const size_t cache_before_missing_final = owner.surface_cache().size();
    const auto rejected = executor.Execute(&missing_final);
    if (!Check(rejected.status == D3D11DrawExecutorStatus::Unsupported &&
                   owner.surface_cache().size() == cache_before_missing_final &&
                   !missing_final.pgraph.surface_color.draw_dirty,
               "missing FINAL rejects without dirty draw")) {
        return false;
    }

    NV2AState invalid_mode = {};
    InitState(&invalid_mode, &vram, &vram_bytes, &ramin_bytes, positions,
              legacy_colors);
    invalid_mode.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(1) << 30;
    const size_t cache_before_mode = owner.surface_cache().size();
    const auto mode_one = executor.Execute(&invalid_mode);
    if (!Check(mode_one.status == D3D11DrawExecutorStatus::Unsupported &&
                   owner.surface_cache().size() == cache_before_mode &&
                   !invalid_mode.pgraph.surface_color.draw_dirty,
               "mode one rejected without cache mutation")) {
        return false;
    }
    invalid_mode.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(3) << 30;
    const auto mode_three = executor.Execute(&invalid_mode);
    if (!Check(mode_three.status == D3D11DrawExecutorStatus::Unsupported &&
                   owner.surface_cache().size() == cache_before_mode &&
                   !invalid_mode.pgraph.surface_color.draw_dirty,
               "mode three rejected without cache mutation")) {
        return false;
    }

    NV2AState context_output = {};
    InitState(&context_output, &vram, &vram_bytes, &ramin_bytes, positions,
              legacy_colors);
    context_output.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(2) << 30;
    context_output.pgraph.regs_[NV_PGRAPH_CSV0_C] = start << 8;
    WriteProgrammableToken(&context_output.pgraph, start, 0, 2, true, true);
    const size_t cache_before_context = owner.surface_cache().size();
    const auto context_token_rejected = executor.Execute(&context_output);
    if (!Check(context_token_rejected.status ==
                       D3D11DrawExecutorStatus::Unsupported &&
                   owner.surface_cache().size() == cache_before_context &&
                   !context_output.pgraph.surface_color.draw_dirty,
               "context output token rejects without dirty draw")) {
        return false;
    }

    NV2AState malformed = {};
    InitState(&malformed, &vram, &vram_bytes, &ramin_bytes, positions,
              legacy_colors);
    malformed.pgraph.regs_[NV_PGRAPH_CSV0_D] = UINT32_C(2) << 30;
    malformed.pgraph.regs_[NV_PGRAPH_CSV0_C] = start << 8;
    WriteProgrammableToken(&malformed.pgraph, start, 0, NV2AOR_POS, true);
    malformed.pgraph.program_data[start][1] =
        (malformed.pgraph.program_data[start][1] & ~(UINT32_C(0xF) << 21)) |
        (UINT32_C(14) << 21);
    const size_t cache_before_malformed = owner.surface_cache().size();
    const auto malformed_rejected = executor.Execute(&malformed);
    if (!Check(malformed_rejected.status ==
                       D3D11DrawExecutorStatus::Unsupported &&
                   owner.surface_cache().size() == cache_before_malformed &&
                   !malformed.pgraph.surface_color.draw_dirty,
               "malformed opcode rejects without dirty draw")) {
        return false;
    }

    state.pgraph.surface_color.draw_dirty = false;
    state.pgraph.enable_vertex_program_write = true;
    const size_t cache_before_writeback = owner.surface_cache().size();
    const auto context_rejected = executor.Execute(&state);
    return Check(context_rejected.status ==
                         D3D11DrawExecutorStatus::Unsupported &&
                     owner.surface_cache().size() == cache_before_writeback &&
                     !state.pgraph.surface_color.draw_dirty,
                 "context writeback rejects without dirty draw");
}

bool TestDraw()
{
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (!Check(CreateWarp(&device, &context), "WARP unavailable")) {
        return true;
    }
    D3D11PgraphContext owner(device.Get(), context.Get());
    D3D11DrawExecutor executor(owner);
    NV2AState state = {};
    MemoryRegion vram = {};
    std::vector<uint8_t> vram_bytes;
    std::vector<uint8_t> ramin_bytes;
    float positions[] = { -0.8f, -0.8f, 0.5f, 1.0f,  0.0f, 0.8f,
                          0.5f,  1.0f,  0.8f, -0.8f, 0.5f, 1.0f };
    float colors[] = { 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f,
                       0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f };
    InitState(&state, &vram, &vram_bytes, &ramin_bytes, positions, colors);
    state.pgraph.surface_binding_dim = { 1, 2, 123, 456, 4, 4 };
    const std::vector<uint32_t> regs_before(
        state.pgraph.regs_,
        state.pgraph.regs_ +
            sizeof(state.pgraph.regs_) / sizeof(state.pgraph.regs_[0]));
    const Surface color_before = state.pgraph.surface_color;
    const Surface zeta_before = state.pgraph.surface_zeta;
    const auto drawn = executor.Execute(&state);
    if (!Check(drawn.status == D3D11DrawExecutorStatus::Drawn,
               "inline triangle draw")) {
        return false;
    }
    if (!Check(state.pgraph.surface_color.draw_dirty &&
                   state.pgraph.surface_zeta.draw_dirty,
               "only written attachments become dirty")) {
        return false;
    }
    if (!Check(std::memcmp(regs_before.data(), state.pgraph.regs_,
                           regs_before.size() * sizeof(uint32_t)) == 0 &&
                   state.pgraph.surface_color.offset == color_before.offset &&
                   state.pgraph.surface_zeta.offset == zeta_before.offset,
               "input registers and surface bindings remain immutable")) {
        return false;
    }
    const uint8_t color_padding = vram_bytes[8 + 16];
    const uint8_t zeta_padding = vram_bytes[104 + 16];
    const uint8_t before_color_payload = vram_bytes[8 - 1];
    const uint8_t after_color_payload = vram_bytes[8 + 80];
    const xemu::D3D11SurfaceDescriptor color_descriptor =
        executor.color_surface()->descriptor();
    const xemu::D3D11SurfaceDescriptor zeta_descriptor =
        executor.depth_stencil_surface()->descriptor();
    const uint64_t color_generation = executor.color_surface().generation();
    const uint64_t zeta_generation =
        executor.depth_stencil_surface().generation();
    g_dirty_calls.clear();
    xemu::D3D11DrawFlushReport flush_report = {};
    if (!Check(executor.Flush(&state, &flush_report), "shared owner flush")) {
        return false;
    }
    if (!Check(flush_report.ranges.size() == 2 &&
                   flush_report.ranges[0].offset == 8 &&
                   flush_report.ranges[0].size == 80 &&
                   flush_report.ranges[0].color &&
                   !flush_report.ranges[0].depth_stencil &&
                   flush_report.ranges[0].descriptor == color_descriptor &&
                   flush_report.ranges[0].generation == color_generation &&
                   flush_report.ranges[1].offset == 104 &&
                   flush_report.ranges[1].size == 80 &&
                   !flush_report.ranges[1].color &&
                   flush_report.ranges[1].depth_stencil &&
                   flush_report.ranges[1].descriptor == zeta_descriptor &&
                   flush_report.ranges[1].generation == zeta_generation,
               "flush report preserves ordered attachment ranges")) {
        return false;
    }
    if (!Check(
            g_dirty_calls.size() == 4 && g_dirty_calls[0].offset == 8 &&
                g_dirty_calls[0].size == 80 &&
                g_dirty_calls[0].client == DIRTY_MEMORY_VGA &&
                g_dirty_calls[1].offset == 8 && g_dirty_calls[1].size == 80 &&
                g_dirty_calls[1].client == DIRTY_MEMORY_NV2A_TEX &&
                g_dirty_calls[2].offset == 104 && g_dirty_calls[2].size == 80 &&
                g_dirty_calls[2].client == DIRTY_MEMORY_VGA &&
                g_dirty_calls[3].offset == 104 && g_dirty_calls[3].size == 80 &&
                g_dirty_calls[3].client == DIRTY_MEMORY_NV2A_TEX &&
                std::count_if(g_dirty_calls.begin(), g_dirty_calls.end(),
                              [](const DirtyCall &call) {
                                  return call.client == DIRTY_MEMORY_VGA;
                              }) == 2 &&
                std::count_if(g_dirty_calls.begin(), g_dirty_calls.end(),
                              [](const DirtyCall &call) {
                                  return call.client == DIRTY_MEMORY_NV2A_TEX;
                              }) == 2,
            "dirty ranges and clients are exact and ordered")) {
        return false;
    }
    for (size_t row = 0; row < 4; ++row) {
        if (!Check(vram_bytes[8 + row * 20 + 16] == color_padding &&
                       vram_bytes[104 + row * 20 + 16] == zeta_padding,
                   "pitch padding unchanged")) {
            return false;
        }
    }
    bool color_changed = false;
    for (size_t row = 1; row < 4; ++row) {
        for (size_t pixel = 0; pixel < 4; ++pixel) {
            color_changed |= vram_bytes[8 + row * 20 + pixel * 4] != 0x55 ||
                             vram_bytes[8 + row * 20 + pixel * 4 + 1] != 0x55 ||
                             vram_bytes[8 + row * 20 + pixel * 4 + 2] != 0x55 ||
                             vram_bytes[8 + row * 20 + pixel * 4 + 3] != 0x55;
        }
    }
    bool zeta_changed = false;
    for (size_t row = 1; row < 4; ++row) {
        for (size_t pixel = 0; pixel < 4; ++pixel) {
            zeta_changed |=
                vram_bytes[104 + row * 20 + pixel * 4] != 0x55 ||
                vram_bytes[104 + row * 20 + pixel * 4 + 1] != 0x55 ||
                vram_bytes[104 + row * 20 + pixel * 4 + 2] != 0x55 ||
                vram_bytes[104 + row * 20 + pixel * 4 + 3] != 0x55;
        }
    }
    if (!Check(
            color_changed && zeta_changed &&
                vram_bytes[8 - 1] == before_color_payload &&
                vram_bytes[8 + 80] == after_color_payload &&
                vram_bytes[8] == 0x55 && vram_bytes[8 + 1] == 0x55 &&
                vram_bytes[8 + 2] == 0x55 && vram_bytes[8 + 3] == 0x55 &&
                vram_bytes[8 + 16] == 0x55,
            "triangle readback covers rows 1-3 and preserves outside pixel")) {
        return false;
    }
    if (!Check(owner.pending_download_events().empty(),
               "successful flush drains shared event queue")) {
        return false;
    }

    xemu::D3D11SurfaceDescriptor descriptor = {};
    xemu::D3D11SurfaceStatus descriptor_status = xemu::D3D11SurfaceStatus::Ok;
    uint32_t bound_width = 0;
    uint32_t bound_height = 0;
    if (!Check(xemu::d3d11_get_surface_dimensions(&state.pgraph, &bound_width,
                                                  &bound_height) &&
                   bound_width == 4 && bound_height == 4,
               "binding dimensions are already expanded")) {
        return false;
    }
    state.pgraph.surface_binding_dim = {};
    state.pgraph.surface_shape.clip_x = 1;
    state.pgraph.surface_shape.clip_y = 2;
    state.pgraph.surface_shape.clip_width = 3;
    state.pgraph.surface_shape.clip_height = 2;
    if (!Check(xemu::d3d11_get_surface_dimensions(&state.pgraph, &bound_width,
                                                  &bound_height) &&
                   bound_width == 4 && bound_height == 4,
               "clip fallback dimensions expand origin")) {
        return false;
    }
    state.pgraph.surface_binding_dim = { 1, 2, 123, 456, 4, 4 };
    state.pgraph.surface_shape.clip_x = 0;
    state.pgraph.surface_shape.clip_y = 0;
    state.pgraph.surface_shape.clip_width = 4;
    state.pgraph.surface_shape.clip_height = 4;
    if (!Check(xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status) &&
                   descriptor.offset == 8 && descriptor.width == 4 &&
                   descriptor.height == 4,
               "surface descriptor uses direct binding dimensions")) {
        return false;
    }
    const hwaddr saved_dma_color = state.pgraph.dma_color;
    state.pgraph.dma_color = 2;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "unaligned DMA object rejected")) {
        return false;
    }
    state.pgraph.dma_color = 60;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "RAMIN descriptor bound rejected")) {
        return false;
    }
    state.pgraph.dma_color = saved_dma_color;
    state.ramin_ptr = nullptr;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "null RAMIN rejected")) {
        return false;
    }
    state.ramin_ptr = ramin_bytes.data();
    g_dma.dma_target = NV_DMA_TARGET_PCI;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "non-NVM DMA target rejected")) {
        return false;
    }
    g_dma.dma_target = NV_DMA_TARGET_NVM;
    g_dma.limit = 79;
    if (!Check(xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "inclusive DMA limit accepted")) {
        return false;
    }
    g_dma.limit = 78;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, true, 4, 4, &descriptor, &descriptor_status),
               "DMA limit one byte short rejected")) {
        return false;
    }
    g_dma.limit = 511;
    state.pgraph.surface_shape.z_format = 1;
    if (!Check(!xemu::d3d11_build_surface_descriptor(
                   &state, false, 4, 4, &descriptor, &descriptor_status),
               "float zeta descriptor rejected")) {
        return false;
    }
    state.pgraph.surface_shape.z_format = 0;

    /* Overlapping attachments are rejected before the shared cache is
     * touched, including exact aliases. */
    state.pgraph.surface_color.draw_dirty = false;
    state.pgraph.surface_zeta.draw_dirty = false;
    state.pgraph.surface_zeta.offset = 0;
    state.pgraph.surface_shape.zeta_format = NV097_SET_SURFACE_FORMAT_ZETA_Z16;
    const size_t cache_size = owner.surface_cache().size();
    const auto overlap = executor.Execute(&state);
    if (!Check(overlap.status == D3D11DrawExecutorStatus::Invalid &&
                   owner.surface_cache().size() == cache_size &&
                   !state.pgraph.surface_color.draw_dirty &&
                   !state.pgraph.surface_zeta.draw_dirty,
               "overlap rejected without cache mutation")) {
        return false;
    }
    state.pgraph.surface_zeta.offset = 96;
    state.pgraph.surface_shape.zeta_format =
        NV097_SET_SURFACE_FORMAT_ZETA_Z24S8;
    state.pgraph.surface_shape.z_format = 1;
    const auto float_zeta = executor.Execute(&state);
    if (!Check(float_zeta.status == D3D11DrawExecutorStatus::Partial &&
                   float_zeta.color.succeeded(),
               "float zeta is optional and reports partial color draw")) {
        return false;
    }
    state.pgraph.surface_shape.z_format = 0;
    std::atomic<D3D11DrawExecutorStatus> wrong_thread =
        D3D11DrawExecutorStatus::Drawn;
    std::thread thread([&] { wrong_thread = executor.Execute(&state).status; });
    thread.join();
    return Check(wrong_thread == D3D11DrawExecutorStatus::WrongThread,
                 "wrong thread rejected");
}

} // namespace

int main()
{
    return TestDraw() && TestProgrammableDraw() ? 0 : 1;
}

#else

int main()
{
    return 0;
}

#endif // _WIN32
