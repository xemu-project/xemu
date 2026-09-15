/* Renderer-neutral tests for the CPU-owned NV2A vertex adapter. */

#include "qemu/osdep.h"

#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/nv2a_regs.h"
#include "hw/xbox/nv2a/pgraph/pgraph.h"

#include "hw/xbox/nv2a/pgraph/d3d11/vertex-adapter.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

using xemu::D3D11VertexPlan;

DMAObject g_dma = {};
uint8_t *g_dma_data = nullptr;
unsigned g_dma_loads = 0;
unsigned g_dma_maps = 0;

extern "C" uint64_t memory_region_size(MemoryRegion *mr)
{
    return int128_get64(mr->size);
}

extern "C" DMAObject nv_dma_load(NV2AState *, hwaddr)
{
    ++g_dma_loads;
    return g_dma;
}

extern "C" void *nv_dma_map(NV2AState *, hwaddr, hwaddr *length)
{
    ++g_dma_maps;
    if (length != nullptr) {
        *length = g_dma.limit;
    }
    return g_dma_data;
}

struct Fixture {
    NV2AState d = {};
    PGRAPHState &pg = d.pgraph;
    MemoryRegion vram = {};
    std::vector<uint8_t> bytes = std::vector<uint8_t>(256, 0);

    Fixture()
    {
        vram.size = int128_make64(bytes.size());
        d.vram = &vram;
        d.vram_ptr = bytes.data();
        d.ramin_ptr = bytes.data();
        d.ramin.size = int128_make64(12);
        pg.primitive_mode = PRIM_TYPE_TRIANGLES;
    }
};

Fixture g_fixture;

void Reset(PGRAPHState *pg)
{
    std::memset(pg, 0, sizeof(*pg));
    pg->primitive_mode = PRIM_TYPE_TRIANGLES;
    g_dma = { NV_DMA_IN_MEMORY_CLASS, NV_DMA_TARGET_NVM, 0, 255 };
    g_dma_data = g_fixture.bytes.data();
    g_dma_loads = 0;
    g_dma_maps = 0;
}

void SetArrayAttrs(PGRAPHState *pg)
{
    VertexAttribute &position =
        pg->vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    position.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    position.count = 3;
    position.size = 4;
    position.stride = 12;
    position.offset = 0;

    VertexAttribute &color = pg->vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE];
    color.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    color.count = 4;
    color.size = 4;
    color.stride = 16;
    color.offset = 64;
}

bool Ready(const PGRAPHState *pg, D3D11VertexPlan *plan)
{
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, pg,
                                         g_fixture.bytes.size(), plan) ==
           xemu::D3D11VertexAdapterStatus::Ready;
}

void PutLe32(uint8_t *data, uint32_t value)
{
    data[0] = static_cast<uint8_t>(value);
    data[1] = static_cast<uint8_t>(value >> 8);
    data[2] = static_cast<uint8_t>(value >> 16);
    data[3] = static_cast<uint8_t>(value >> 24);
}

void PutFloat(uint8_t *data, float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    PutLe32(data, bits);
}

void PutInlineByte(uint32_t *words, size_t offset, uint8_t value)
{
    words[offset / 4] |= static_cast<uint32_t>(value) << ((offset % 4) * 8);
}

bool Nearly(float actual, float expected)
{
    return std::fabs(actual - expected) < 0.00001f;
}

bool TestInlineBufferPrecedence()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    VertexAttribute &position = pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    VertexAttribute &color = pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE];
    float positions[3][4] = { { 1, 2, 3, 1 }, { 4, 5, 6, 1 }, { 7, 8, 9, 1 } };
    float colors[3][4] = { { 1, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 } };
    position.inline_buffer = &positions[0][0];
    position.inline_buffer_populated = true;
    color.inline_buffer = &colors[0][0];
    color.inline_buffer_populated = true;
    pg.inline_buffer_length = 3;
    VertexAttribute position_before = position;
    VertexAttribute color_before = color;
    const unsigned inline_length_before = pg.inline_buffer_length;

    D3D11VertexPlan plan;
    if (!Ready(&pg, &plan) || plan.vertices.size() != 3 ||
        plan.indices != std::vector<uint32_t>({ 0, 1, 2 }) ||
        !Nearly(plan.vertices[1].position[0], 4.0f) ||
        !Nearly(plan.vertices[2].color[2], 1.0f) || g_dma_loads != 0 ||
        g_dma_maps != 0 ||
        std::memcmp(&position, &position_before, sizeof(position)) != 0 ||
        std::memcmp(&color, &color_before, sizeof(color)) != 0 ||
        pg.inline_buffer_length != inline_length_before) {
        return false;
    }

    Reset(&pg);
    position = {};
    color = {};
    position.count = 3;
    position.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    position.inline_value[0] = 11.0f;
    position.inline_value[1] = 12.0f;
    position.inline_value[2] = 13.0f;
    color.count = 4;
    color.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    color.inline_value[0] = 0.25f;
    color.inline_value[1] = 0.5f;
    color.inline_value[2] = 0.75f;
    color.inline_value[3] = 1.0f;
    pg.inline_buffer_length = 3;
    if (!Ready(&pg, &plan) || plan.vertices.size() != 3 ||
        !Nearly(plan.vertices[0].position[0], 11.0f) ||
        !Nearly(plan.vertices[2].color[1], 0.5f) || g_dma_loads != 0) {
        return false;
    }

    Reset(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_SPECULAR].inline_buffer_populated =
        true;
    pg.inline_buffer_length = 3;
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                         g_fixture.bytes.size(), &plan) ==
           xemu::D3D11VertexAdapterStatus::Unsupported;
}

bool TestInlineArrayAndSwizzle()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].format =
        NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D;
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].stride = 4;
    pg.inline_array_length = 12;
    const uint8_t bgra[3][4] = {
        { 10, 20, 30, 255 },
        { 40, 50, 60, 128 },
        { 70, 80, 90, 64 },
    };
    for (unsigned int vertex = 0; vertex < 3; ++vertex) {
        const size_t offset = vertex * 16;
        PutFloat(reinterpret_cast<uint8_t *>(pg.inline_array) + offset + 0,
                 static_cast<float>(vertex + 1));
        PutFloat(reinterpret_cast<uint8_t *>(pg.inline_array) + offset + 4,
                 static_cast<float>(vertex + 2));
        PutFloat(reinterpret_cast<uint8_t *>(pg.inline_array) + offset + 8,
                 static_cast<float>(vertex + 3));
        if (vertex == 0) {
            PutLe32(reinterpret_cast<uint8_t *>(pg.inline_array) + offset,
                    UINT32_C(0x3fc00000));
        }
        for (unsigned int byte = 0; byte < 4; ++byte) {
            PutInlineByte(pg.inline_array, offset + 12 + byte,
                          bgra[vertex][byte]);
        }
    }
    D3D11VertexPlan plan;
    if (!Ready(&pg, &plan) || plan.vertices.size() != 3 ||
        plan.indices != std::vector<uint32_t>({ 0, 1, 2 }) ||
        !Nearly(plan.vertices[0].position[0], 1.5f) ||
        !Nearly(plan.vertices[0].position[2], 3.0f) ||
        !Nearly(plan.vertices[0].color[0], 30.0f / 255.0f) ||
        !Nearly(plan.vertices[0].color[1], 20.0f / 255.0f) ||
        !Nearly(plan.vertices[0].color[2], 10.0f / 255.0f) ||
        !Nearly(plan.vertices[1].color[0], 60.0f / 255.0f) ||
        !Nearly(plan.vertices[1].color[1], 50.0f / 255.0f) ||
        !Nearly(plan.vertices[1].color[2], 40.0f / 255.0f) ||
        !Nearly(plan.vertices[1].color[3], 128.0f / 255.0f) ||
        !Nearly(plan.vertices[2].color[0], 90.0f / 255.0f) ||
        !Nearly(plan.vertices[2].color[1], 80.0f / 255.0f) ||
        !Nearly(plan.vertices[2].color[2], 70.0f / 255.0f) ||
        !Nearly(plan.vertices[2].color[3], 64.0f / 255.0f)) {
        return false;
    }
    pg.inline_array_length = 11;
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                         g_fixture.bytes.size(), &plan) ==
           xemu::D3D11VertexAdapterStatus::Invalid;
}

bool TestDmaRangesAndLimits()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].count = 0;
    pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION].stride = 12;
    for (unsigned int i = 0; i < 8; ++i) {
        PutFloat(g_fixture.bytes.data() + i * 12 + 0, static_cast<float>(i));
        PutFloat(g_fixture.bytes.data() + i * 12 + 4, 0.0f);
        PutFloat(g_fixture.bytes.data() + i * 12 + 8, 1.0f);
    }
    g_dma.limit = 8 * 12 - 1;
    pg.draw_arrays_length = 2;
    pg.draw_arrays_start[0] = 0;
    pg.draw_arrays_count[0] = 3;
    pg.draw_arrays_start[1] = 4;
    pg.draw_arrays_count[1] = 3;
    D3D11VertexPlan plan;
    if (!Ready(&pg, &plan) ||
        plan.indices != std::vector<uint32_t>({ 0, 1, 2, 3, 4, 5 }) ||
        plan.vertices.size() != 6) {
        return false;
    }
    pg.draw_arrays_count[1] = 2;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].count = 0;
    pg.inline_elements_length = 3;
    pg.inline_elements[0] = 2;
    pg.inline_elements[1] = 0;
    pg.inline_elements[2] = 2;
    g_dma.limit = 8 * 12 - 1;
    if (!Ready(&pg, &plan) ||
        plan.indices != std::vector<uint32_t>({ 0, 1, 0 }) ||
        plan.vertices.size() != 2) {
        return false;
    }
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].count = 0;
    pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION].stride = 0;
    pg.draw_arrays_length = 1;
    pg.draw_arrays_start[0] = 0;
    pg.draw_arrays_count[0] = 3;
    g_dma.limit = 11;
    if (!Ready(&pg, &plan) ||
        !Nearly(plan.vertices[0].position[0], plan.vertices[2].position[0])) {
        return false;
    }
    g_dma.limit = 10;
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                         g_fixture.bytes.size(), &plan) ==
           xemu::D3D11VertexAdapterStatus::Invalid;
}

bool TestDmaAddressAndRamin()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.vertex_attributes[NV2A_VERTEX_ATTR_DIFFUSE].count = 0;
    pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION].stride = 0;
    pg.draw_arrays_length = 1;
    pg.draw_arrays_count[0] = 3;
    g_dma.address = 0x80000000u;
    g_dma.limit = 11;
    D3D11VertexPlan plan;
    if (!Ready(&pg, &plan)) {
        return false;
    }
    g_dma.dma_target = NV_DMA_TARGET_PCI;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    g_dma.dma_target = NV_DMA_TARGET_NVM;
    g_fixture.d.ramin.size = int128_make64(11);
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    g_fixture.d.ramin.size = int128_make64(12);
    g_fixture.d.vram = nullptr;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    g_fixture.d.vram = &g_fixture.vram;
    g_fixture.d.vram_ptr = nullptr;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    g_fixture.d.vram_ptr = g_fixture.bytes.data();
    g_fixture.d.ramin_ptr = nullptr;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    g_fixture.d.ramin_ptr = g_fixture.bytes.data();
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                      g_fixture.bytes.size() + 1, &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    pg.dma_vertex_a = 4;
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
        xemu::D3D11VertexAdapterStatus::Invalid) {
        return false;
    }
    pg.dma_vertex_a = 0;
    pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION].offset =
        static_cast<hwaddr>(UINT32_MAX) + 1;
    g_dma.limit = 11;
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                         g_fixture.bytes.size(), &plan) ==
           xemu::D3D11VertexAdapterStatus::Invalid;
}

bool TestCompressedPosition()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    VertexAttribute &position = pg.vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    position.format = NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP;
    position.count = 1;
    pg.inline_array_length = 3;
    for (unsigned vertex = 0; vertex < 3; ++vertex) {
        pg.inline_array[vertex] = 511u | (1023u << 11) | (511u << 22);
    }
    D3D11VertexPlan plan;
    return Ready(&pg, &plan) && plan.vsh_inputs.size() == 3 &&
           Nearly(plan.vsh_inputs[0].values[NV2A_VERTEX_ATTR_POSITION][0],
                  511.0f / 1023.0f) &&
           Nearly(plan.vsh_inputs[0].values[NV2A_VERTEX_ATTR_POSITION][1],
                  1.0f) &&
           Nearly(plan.vsh_inputs[0].values[NV2A_VERTEX_ATTR_POSITION][2],
                  1.0f);
}

bool TestInvalidAndAtomic()
{
    PGRAPHState &pg = g_fixture.pg;
    Reset(&pg);
    SetArrayAttrs(&pg);
    pg.draw_arrays_length = 1;
    pg.draw_arrays_count[0] = INT32_MAX - 2;
    D3D11VertexPlan plan;
    plan.indices = { 99 };
    plan.vertices.resize(1);
    if (xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg, g_fixture.bytes.size(),
                                      &plan) !=
            xemu::D3D11VertexAdapterStatus::Invalid ||
        !plan.indices.empty() || !plan.vertices.empty()) {
        return false;
    }
    pg.draw_arrays_count[0] = 1;
    pg.primitive_mode = PRIM_TYPE_QUADS;
    return xemu::d3d11_build_vertex_plan(&g_fixture.d, &pg,
                                         g_fixture.bytes.size(), &plan) ==
           xemu::D3D11VertexAdapterStatus::Unsupported;
}

} // namespace

int main()
{
    return TestInlineBufferPrecedence() && TestInlineArrayAndSwizzle() &&
                   TestDmaRangesAndLimits() && TestDmaAddressAndRamin() &&
                   TestCompressedPosition() && TestInvalidAndAtomic() ?
               0 :
               1;
}
