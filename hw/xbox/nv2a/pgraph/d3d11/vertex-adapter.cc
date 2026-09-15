// xemu NV2A to D3D11 CPU-owned vertex/index adapter

#include "vertex-adapter.h"

#include "../../nv2a_int.h"
#include "../../nv2a_regs.h"
#include "../pgraph.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xemu {
namespace {

constexpr unsigned int kMaxDrawArrayRanges = 1250;
constexpr size_t kMaxPlanIndices = NV2A_MAX_BATCH_LENGTH;
constexpr hwaddr kDmaAddressMask = 0x07FFFFFF;
constexpr hwaddr kDmaDescriptorBytes = 3 * sizeof(uint32_t);

struct MappedAttribute {
    const uint8_t *data = nullptr;
    hwaddr dma_address = 0;
    hwaddr dma_limit = 0;
    hwaddr offset = 0;
    uint32_t stride = 0;
    uint32_t element_size = 0;
    unsigned int count = 0;
    unsigned int format = 0;
    bool bgra = false;
};

struct SourceIndexList {
    std::vector<uint32_t> values;
    bool packed_inline_array = false;
    unsigned int packed_vertex_count = 0;
};

bool AddOverflow(uint64_t a, uint64_t b, uint64_t *result)
{
    if (result == nullptr || a > std::numeric_limits<uint64_t>::max() - b) {
        return true;
    }
    *result = a + b;
    return false;
}

bool MultiplyOverflow(uint64_t a, uint64_t b, uint64_t *result)
{
    if (result == nullptr ||
        (a != 0 && b > std::numeric_limits<uint64_t>::max() / a)) {
        return true;
    }
    *result = a * b;
    return false;
}

uint32_t ReadLe32(const uint8_t *data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

float FloatFromLe32(uint32_t bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

int16_t ReadLe16(const uint8_t *data)
{
    return static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                (static_cast<uint16_t>(data[1]) << 8));
}

uint8_t ReadInlineByte(const uint32_t *words, size_t byte_offset)
{
    const uint32_t word = words[byte_offset / sizeof(uint32_t)];
    const unsigned int shift = (byte_offset % sizeof(uint32_t)) * 8;
    return static_cast<uint8_t>(word >> shift);
}

uint32_t ReadInlineWord(const uint32_t *words, size_t byte_offset)
{
    return static_cast<uint32_t>(ReadInlineByte(words, byte_offset)) |
           (static_cast<uint32_t>(ReadInlineByte(words, byte_offset + 1))
            << 8) |
           (static_cast<uint32_t>(ReadInlineByte(words, byte_offset + 2))
            << 16) |
           (static_cast<uint32_t>(ReadInlineByte(words, byte_offset + 3))
            << 24);
}

bool RoundUp(size_t value, size_t alignment, size_t *result)
{
    if (result == nullptr || alignment == 0) {
        return false;
    }
    const size_t remainder = value % alignment;
    if (remainder == 0) {
        *result = value;
        return true;
    }
    const size_t padding = alignment - remainder;
    if (value > std::numeric_limits<size_t>::max() - padding) {
        return false;
    }
    *result = value + padding;
    return true;
}

bool IsSupportedAttribute(const VertexAttribute &attribute, bool position)
{
    if (attribute.count == 0) {
        return true;
    }
    if (attribute.count > 4 ||
        (position &&
         attribute.format != NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP &&
         attribute.count != 3 && attribute.count != 4)) {
        return false;
    }
    if (attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D &&
        attribute.count != 4) {
        return false;
    }
    if (attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP) {
        return attribute.count == 1;
    }
    return attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F ||
           attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D ||
           attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL ||
           attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1 ||
           attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K;
}

bool AttributeLayout(const VertexAttribute &attribute, bool position,
                     size_t *size, bool *bgra)
{
    if (size == nullptr || bgra == nullptr ||
        !IsSupportedAttribute(attribute, position) || attribute.count == 0) {
        return false;
    }
    switch (attribute.format) {
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
        *size = sizeof(float);
        *bgra = false;
        return true;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        *size = sizeof(uint8_t);
        *bgra = attribute.count == 4;
        return true;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
        *size = sizeof(uint8_t);
        *bgra = false;
        return true;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1:
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K:
        *size = sizeof(int16_t);
        *bgra = false;
        return true;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP:
        *size = sizeof(uint32_t);
        *bgra = false;
        return true;
    default:
        return false;
    }
}

bool ValidateDmaObject(const NV2AState *d, hwaddr dma_object,
                       uint64_t vram_size, DMAObject *dma)
{
    const hwaddr ramin_size =
        d == nullptr ? 0 :
                       memory_region_size(&const_cast<NV2AState *>(d)->ramin);
    if (d == nullptr || d->vram == nullptr || d->vram_ptr == nullptr ||
        d->ramin_ptr == nullptr || dma == nullptr || vram_size == 0 ||
        memory_region_size(d->vram) < vram_size || (dma_object & 3) != 0 ||
        ramin_size < kDmaDescriptorBytes ||
        dma_object > ramin_size - kDmaDescriptorBytes) {
        return false;
    }
    *dma = nv_dma_load(const_cast<NV2AState *>(d), dma_object);
    const hwaddr address = dma->address & kDmaAddressMask;
    if (dma->dma_class != NV_DMA_IN_MEMORY_CLASS ||
        dma->dma_target != NV_DMA_TARGET_NVM || address >= vram_size ||
        dma->limit >= vram_size - address) {
        return false;
    }
    return true;
}

bool MapAttribute(const NV2AState *d, const PGRAPHState &pg,
                  const VertexAttribute &attribute, bool position,
                  uint64_t vram_size, MappedAttribute *mapped)
{
    if (mapped == nullptr) {
        return false;
    }
    size_t scalar_size = 0;
    bool bgra = false;
    if (!AttributeLayout(attribute, position, &scalar_size, &bgra) ||
        scalar_size > std::numeric_limits<uint32_t>::max() / attribute.count) {
        return false;
    }

    const hwaddr dma_object =
        attribute.dma_select ? pg.dma_vertex_b : pg.dma_vertex_a;
    DMAObject dma = {};
    if (!ValidateDmaObject(d, dma_object, vram_size, &dma)) {
        return false;
    }

    hwaddr mapped_length = 0;
    void *data =
        nv_dma_map(const_cast<NV2AState *>(d), dma_object, &mapped_length);
    if (data == nullptr) {
        return false;
    }
    /* DMA limit is an inclusive byte address, as used by the existing
     * surface adapters.  nv_dma_map historically reports the limit itself as
     * its length, so the descriptor and VRAM checks above are authoritative. */
    (void)mapped_length;
    mapped->data = static_cast<const uint8_t *>(data);
    mapped->dma_address = dma.address & kDmaAddressMask;
    mapped->dma_limit = dma.limit;
    mapped->offset = attribute.offset;
    mapped->stride = attribute.stride;
    mapped->element_size =
        attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP ?
            sizeof(uint32_t) :
            static_cast<uint32_t>(scalar_size * attribute.count);
    mapped->count = attribute.count;
    mapped->format = attribute.format;
    mapped->bgra = bgra;
    return true;
}

bool ValidateRead(const MappedAttribute &mapped, uint32_t source_index,
                  uint64_t vram_size, const uint8_t **entry)
{
    if (entry == nullptr || mapped.data == nullptr ||
        mapped.element_size == 0 || mapped.dma_address >= vram_size) {
        return false;
    }
    uint64_t offset = mapped.offset;
    if (mapped.stride != 0 &&
        (static_cast<uint64_t>(source_index) * mapped.stride >
         std::numeric_limits<uint64_t>::max() - offset)) {
        return false;
    }
    if (mapped.stride != 0) {
        offset += static_cast<uint64_t>(source_index) * mapped.stride;
    }
    uint64_t end = 0;
    if (AddOverflow(offset, mapped.element_size, &end) || end == 0 ||
        end - 1 > mapped.dma_limit || end > vram_size - mapped.dma_address ||
        end >
            static_cast<uint64_t>(std::numeric_limits<std::ptrdiff_t>::max())) {
        return false;
    }
    *entry = mapped.data + static_cast<std::ptrdiff_t>(offset);
    return true;
}

bool DecodeAttribute(const MappedAttribute &mapped, uint32_t source_index,
                     uint64_t vram_size, float output[4])
{
    const uint8_t *entry = nullptr;
    if (!ValidateRead(mapped, source_index, vram_size, &entry) ||
        output == nullptr) {
        return false;
    }
    for (unsigned int component = 0; component < 4; ++component) {
        output[component] = component == 3 ? 1.0f : 0.0f;
    }
    if (mapped.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP) {
        const uint32_t packed = ReadLe32(entry);
        int32_t x = static_cast<int32_t>(packed & 0x7ffu);
        int32_t y = static_cast<int32_t>((packed >> 11) & 0x7ffu);
        int32_t z = static_cast<int32_t>((packed >> 22) & 0x3ffu);
        if ((x & 0x400) != 0)
            x |= ~0x7ff;
        if ((y & 0x400) != 0)
            y |= ~0x7ff;
        if ((z & 0x200) != 0)
            z |= ~0x3ff;
        output[0] = (std::max)(-1.0f, static_cast<float>(x) / 1023.0f);
        output[1] = (std::max)(-1.0f, static_cast<float>(y) / 1023.0f);
        output[2] = (std::max)(-1.0f, static_cast<float>(z) / 511.0f);
        return true;
    }
    if (mapped.bgra) {
        /* This also applies when stride is zero: the DMA stream is still
         * raw D3D/BGRA bytes.  The canonical VSH stream is post-conversion
         * RGBA, matching GL/VK's later .bgra handling of the raw attribute. */
        output[0] = static_cast<float>(entry[2]) / 255.0f;
        output[1] = static_cast<float>(entry[1]) / 255.0f;
        output[2] = static_cast<float>(entry[0]) / 255.0f;
        output[3] = static_cast<float>(entry[3]) / 255.0f;
        return true;
    }
    for (unsigned int component = 0; component < mapped.count; ++component) {
        switch (mapped.format) {
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
            output[component] = FloatFromLe32(ReadLe32(entry + component * 4));
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
            output[component] = static_cast<float>(entry[component]) / 255.0f;
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1:
            output[component] =
                (std::max)(-1.0f,
                           static_cast<float>(ReadLe16(entry + component * 2)) /
                               32767.0f);
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K:
            output[component] =
                static_cast<float>(ReadLe16(entry + component * 2));
            break;
        default:
            return false;
        }
    }
    return true;
}

bool DecodeInlineArrayAttribute(const PGRAPHState &pg,
                                const VertexAttribute &attribute,
                                size_t byte_offset, size_t vertex_size,
                                unsigned int vertex, bool position,
                                float output[4])
{
    size_t scalar_size = 0;
    bool bgra = false;
    if (output == nullptr ||
        !AttributeLayout(attribute, position, &scalar_size, &bgra) ||
        vertex_size == 0 || pg.inline_array_length > NV2A_MAX_BATCH_LENGTH) {
        return false;
    }
    uint64_t total_bytes64 = 0;
    if (MultiplyOverflow(pg.inline_array_length, sizeof(uint32_t),
                         &total_bytes64) ||
        total_bytes64 > std::numeric_limits<size_t>::max()) {
        return false;
    }
    const size_t total_bytes = static_cast<size_t>(total_bytes64);
    size_t relative = 0;
    if (vertex > std::numeric_limits<size_t>::max() / vertex_size ||
        byte_offset > std::numeric_limits<size_t>::max() -
                          static_cast<size_t>(vertex) * vertex_size) {
        return false;
    }
    relative = byte_offset + static_cast<size_t>(vertex) * vertex_size;
    if (attribute.count > std::numeric_limits<size_t>::max() / scalar_size ||
        relative > total_bytes ||
        scalar_size * attribute.count > total_bytes - relative) {
        return false;
    }
    for (unsigned int component = 0; component < 4; ++component) {
        output[component] = component == 3 ? 1.0f : 0.0f;
    }
    if (attribute.format == NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP) {
        const uint32_t packed = ReadInlineWord(pg.inline_array, relative);
        int32_t x = static_cast<int32_t>(packed & 0x7ffu);
        int32_t y = static_cast<int32_t>((packed >> 11) & 0x7ffu);
        int32_t z = static_cast<int32_t>((packed >> 22) & 0x3ffu);
        if ((x & 0x400) != 0)
            x |= ~0x7ff;
        if ((y & 0x400) != 0)
            y |= ~0x7ff;
        if ((z & 0x200) != 0)
            z |= ~0x3ff;
        output[0] = (std::max)(-1.0f, static_cast<float>(x) / 1023.0f);
        output[1] = (std::max)(-1.0f, static_cast<float>(y) / 1023.0f);
        output[2] = (std::max)(-1.0f, static_cast<float>(z) / 511.0f);
        return true;
    }
    if (bgra) {
        output[0] =
            static_cast<float>(ReadInlineByte(pg.inline_array, relative + 2)) /
            255.0f;
        output[1] =
            static_cast<float>(ReadInlineByte(pg.inline_array, relative + 1)) /
            255.0f;
        output[2] =
            static_cast<float>(ReadInlineByte(pg.inline_array, relative)) /
            255.0f;
        output[3] =
            static_cast<float>(ReadInlineByte(pg.inline_array, relative + 3)) /
            255.0f;
        return true;
    }
    for (unsigned int component = 0; component < attribute.count; ++component) {
        switch (attribute.format) {
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
            output[component] = FloatFromLe32(
                ReadInlineWord(pg.inline_array, relative + component * 4));
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
            output[component] = static_cast<float>(ReadInlineByte(
                                    pg.inline_array, relative + component)) /
                                255.0f;
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1:
            output[component] =
                (std::max)(-1.0f, static_cast<float>(ReadLe16(
                                      reinterpret_cast<const uint8_t *>(
                                          pg.inline_array) +
                                      relative + component * 2)) /
                                      32767.0f);
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K:
            output[component] = static_cast<float>(
                ReadLe16(reinterpret_cast<const uint8_t *>(pg.inline_array) +
                         relative + component * 2));
            break;
        default:
            return false;
        }
    }
    return true;
}

bool BuildSourceIndices(const PGRAPHState &pg, SourceIndexList *source)
{
    if (source == nullptr) {
        return false;
    }
    source->values.clear();
    source->packed_inline_array = false;
    source->packed_vertex_count = 0;

    unsigned int modes =
        (pg.draw_arrays_length != 0) + (pg.inline_elements_length != 0) +
        (pg.inline_array_length != 0) + (pg.inline_buffer_length != 0);
    if (modes != 1 || pg.inline_elements_length > NV2A_MAX_BATCH_LENGTH ||
        pg.inline_array_length > NV2A_MAX_BATCH_LENGTH ||
        pg.inline_buffer_length > NV2A_MAX_BATCH_LENGTH ||
        pg.draw_arrays_length > kMaxDrawArrayRanges) {
        return false;
    }

    if (pg.draw_arrays_length != 0) {
        for (unsigned int range = 0; range < pg.draw_arrays_length; ++range) {
            const int32_t start = pg.draw_arrays_start[range];
            const int32_t count = pg.draw_arrays_count[range];
            if (start < 0 || count <= 0 || count % 3 != 0 ||
                static_cast<size_t>(count) >
                    kMaxPlanIndices - source->values.size()) {
                return false;
            }
            for (int32_t index = 0; index < count; ++index) {
                const uint64_t value = static_cast<uint64_t>(start) + index;
                if (value > std::numeric_limits<uint32_t>::max()) {
                    return false;
                }
                source->values.push_back(static_cast<uint32_t>(value));
            }
        }
    } else if (pg.inline_elements_length != 0) {
        source->values.reserve(pg.inline_elements_length);
        source->values.assign(pg.inline_elements,
                              pg.inline_elements + pg.inline_elements_length);
    } else if (pg.inline_buffer_length != 0) {
        source->values.resize(pg.inline_buffer_length);
        for (unsigned int i = 0; i < pg.inline_buffer_length; ++i) {
            source->values[i] = i;
        }
    } else if (pg.inline_array_length != 0) {
        source->packed_inline_array = true;
    } else {
        return false;
    }
    return true;
}

} // namespace

D3D11VertexAdapterStatus d3d11_build_vertex_plan(NV2AState *d,
                                                 const PGRAPHState *pg,
                                                 uint64_t vram_size,
                                                 D3D11VertexPlan *output)
{
    if (output == nullptr) {
        return D3D11VertexAdapterStatus::Invalid;
    }
    *output = {};
    if (d == nullptr || d->vram == nullptr || d->vram_ptr == nullptr ||
        d->ramin_ptr == nullptr || pg == nullptr || vram_size == 0 ||
        memory_region_size(d->vram) < vram_size) {
        return D3D11VertexAdapterStatus::Invalid;
    }
    if (pg->primitive_mode != PRIM_TYPE_TRIANGLES) {
        return D3D11VertexAdapterStatus::Unsupported;
    }

    for (unsigned int index = 0; index < NV2A_VERTEXSHADER_ATTRIBUTES;
         ++index) {
        if (!IsSupportedAttribute(pg->vertex_attributes[index],
                                  index == NV2A_VERTEX_ATTR_POSITION)) {
            return D3D11VertexAdapterStatus::Unsupported;
        }
    }

    SourceIndexList source;
    if (!BuildSourceIndices(*pg, &source)) {
        return D3D11VertexAdapterStatus::Invalid;
    }
    const bool inline_buffer_mode = pg->inline_buffer_length != 0;

    size_t inline_offsets[NV2A_VERTEXSHADER_ATTRIBUTES] = {};
    size_t inline_vertex_size = 0;
    if (source.packed_inline_array) {
        for (unsigned int index = 0; index < NV2A_VERTEXSHADER_ATTRIBUTES;
             ++index) {
            const VertexAttribute &attribute = pg->vertex_attributes[index];
            if (attribute.count == 0) {
                continue;
            }
            size_t scalar_size = 0;
            bool bgra = false;
            if (!AttributeLayout(attribute, index == NV2A_VERTEX_ATTR_POSITION,
                                 &scalar_size, &bgra) ||
                !RoundUp(inline_vertex_size, scalar_size,
                         &inline_offsets[index])) {
                return D3D11VertexAdapterStatus::Invalid;
            }
            const size_t bytes =
                attribute.format ==
                        NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP ?
                    sizeof(uint32_t) :
                    scalar_size * attribute.count;
            if (bytes >
                std::numeric_limits<size_t>::max() - inline_offsets[index]) {
                return D3D11VertexAdapterStatus::Invalid;
            }
            inline_vertex_size = inline_offsets[index] + bytes;
        }
        uint64_t inline_array_bytes = 0;
        if (MultiplyOverflow(pg->inline_array_length, sizeof(uint32_t),
                             &inline_array_bytes) ||
            inline_vertex_size == 0 ||
            inline_array_bytes % inline_vertex_size != 0 ||
            inline_array_bytes / inline_vertex_size >
                std::numeric_limits<unsigned int>::max()) {
            return D3D11VertexAdapterStatus::Invalid;
        }
        source.packed_vertex_count =
            static_cast<unsigned int>(inline_array_bytes / inline_vertex_size);
        if (source.packed_vertex_count == 0 ||
            source.packed_vertex_count > NV2A_MAX_BATCH_LENGTH) {
            return D3D11VertexAdapterStatus::Invalid;
        }
        source.values.resize(source.packed_vertex_count);
        for (unsigned int i = 0; i < source.packed_vertex_count; ++i) {
            source.values[i] = i;
        }
    }
    if (source.values.empty() || source.values.size() > kMaxPlanIndices ||
        source.values.size() % 3 != 0) {
        return D3D11VertexAdapterStatus::Invalid;
    }

    MappedAttribute mapped[NV2A_VERTEXSHADER_ATTRIBUTES] = {};
    if (!source.packed_inline_array && pg->inline_buffer_length == 0) {
        for (unsigned int index = 0; index < NV2A_VERTEXSHADER_ATTRIBUTES;
             ++index) {
            const VertexAttribute &attribute = pg->vertex_attributes[index];
            if (attribute.count != 0 &&
                !MapAttribute(d, *pg, attribute,
                              index == NV2A_VERTEX_ATTR_POSITION, vram_size,
                              &mapped[index])) {
                return D3D11VertexAdapterStatus::Invalid;
            }
        }
    }

    D3D11VertexPlan plan;
    std::unordered_map<uint32_t, uint32_t> remap;
    plan.indices.reserve(source.values.size());
    plan.vertices.reserve(source.values.size());
    plan.vsh_inputs.reserve(source.values.size());
    remap.reserve(source.values.size());
    for (uint32_t source_index : source.values) {
        std::unordered_map<uint32_t, uint32_t>::const_iterator found =
            remap.find(source_index);
        uint32_t dense_index = 0;
        if (found == remap.end()) {
            if (plan.vertices.size() >= kMaxPlanIndices ||
                plan.vertices.size() > std::numeric_limits<uint32_t>::max()) {
                return D3D11VertexAdapterStatus::Invalid;
            }
            D3D11CanonicalVertex vertex = {};
            D3D11CanonicalVshInputs vsh_inputs = {};
            for (unsigned int index = 0; index < NV2A_VERTEXSHADER_ATTRIBUTES;
                 ++index) {
                const VertexAttribute &attribute = pg->vertex_attributes[index];
                float *values = vsh_inputs.values[index];
                if (inline_buffer_mode && attribute.inline_buffer_populated) {
                    if (attribute.inline_buffer == nullptr ||
                        source_index >= pg->inline_buffer_length) {
                        return D3D11VertexAdapterStatus::Invalid;
                    }
                    std::memcpy(values,
                                attribute.inline_buffer + source_index * 4,
                                sizeof(vsh_inputs.values[index]));
                } else if (attribute.count == 0 || inline_buffer_mode) {
                    std::memcpy(values, attribute.inline_value,
                                sizeof(vsh_inputs.values[index]));
                } else if (source.packed_inline_array) {
                    if (!DecodeInlineArrayAttribute(
                            *pg, attribute, inline_offsets[index],
                            inline_vertex_size, source_index,
                            index == NV2A_VERTEX_ATTR_POSITION, values)) {
                        return D3D11VertexAdapterStatus::Invalid;
                    }
                } else if (!DecodeAttribute(mapped[index], source_index,
                                            vram_size, values)) {
                    return D3D11VertexAdapterStatus::Invalid;
                }
            }

            std::memcpy(vertex.position,
                        vsh_inputs.values[NV2A_VERTEX_ATTR_POSITION],
                        sizeof(vertex.position));
            std::memcpy(vertex.color,
                        vsh_inputs.values[NV2A_VERTEX_ATTR_DIFFUSE],
                        sizeof(vertex.color));

            dense_index = static_cast<uint32_t>(plan.vertices.size());
            plan.vertices.push_back(vertex);
            plan.vsh_inputs.push_back(vsh_inputs);
            remap.emplace(source_index, dense_index);
        } else {
            dense_index = found->second;
        }
        plan.indices.push_back(dense_index);
    }

    if (plan.indices.empty() || plan.indices.size() % 3 != 0) {
        return D3D11VertexAdapterStatus::Invalid;
    }
    *output = std::move(plan);
    return D3D11VertexAdapterStatus::Ready;
}

} // namespace xemu
