// xemu NV2A to D3D11 surface descriptor adapter

#include "surface-adapter.h"

#ifdef _WIN32

#include "../../nv2a_int.h"
#include "../../nv2a_regs.h"
#include "../clear.h"

namespace xemu {
namespace {

constexpr uint64_t kVramAddressMask = UINT64_C(0x07ffffff);

void SetStatus(D3D11SurfaceStatus *status, D3D11SurfaceStatus value)
{
    if (status != nullptr) {
        *status = value;
    }
}

bool RangeEnd(uint64_t offset, uint64_t size, uint64_t *end)
{
    if (end == nullptr || size == 0 || offset > UINT64_MAX - size) {
        return false;
    }
    *end = offset + size;
    return true;
}

} // namespace

bool d3d11_get_surface_dimensions(const PGRAPHState *pg, uint32_t *width,
                                  uint32_t *height)
{
    if (pg == nullptr || width == nullptr || height == nullptr) {
        return false;
    }
    /* Binding dimensions are already expanded by the surface binder. */
    if (pg->surface_binding_dim.width != 0 ||
        pg->surface_binding_dim.height != 0) {
        if (pg->surface_binding_dim.width <= 0 ||
            pg->surface_binding_dim.height <= 0) {
            return false;
        }
        *width = static_cast<uint32_t>(pg->surface_binding_dim.width);
        *height = static_cast<uint32_t>(pg->surface_binding_dim.height);
        return true;
    }
    const uint64_t expanded_width =
        static_cast<uint64_t>(pg->surface_shape.clip_x) +
        pg->surface_shape.clip_width;
    const uint64_t expanded_height =
        static_cast<uint64_t>(pg->surface_shape.clip_y) +
        pg->surface_shape.clip_height;
    if (expanded_width == 0 || expanded_height == 0 ||
        expanded_width > UINT32_MAX || expanded_height > UINT32_MAX) {
        return false;
    }
    *width = static_cast<uint32_t>(expanded_width);
    *height = static_cast<uint32_t>(expanded_height);
    return true;
}

bool d3d11_surface_descriptors_overlap(const D3D11SurfaceDescriptor &left,
                                       const D3D11SurfaceDescriptor &right)
{
    const uint64_t left_bytes = static_cast<uint64_t>(left.pitch) * left.height;
    const uint64_t right_bytes =
        static_cast<uint64_t>(right.pitch) * right.height;
    uint64_t left_end = 0;
    uint64_t right_end = 0;
    if (!RangeEnd(left.offset, left_bytes, &left_end) ||
        !RangeEnd(right.offset, right_bytes, &right_end)) {
        return false;
    }
    return left.offset < right_end && right.offset < left_end;
}

bool d3d11_build_surface_descriptor(NV2AState *state, bool color,
                                    uint32_t width, uint32_t height,
                                    D3D11SurfaceDescriptor *descriptor,
                                    D3D11SurfaceStatus *status)
{
    SetStatus(status, D3D11SurfaceStatus::InvalidDescriptor);
    if (descriptor == nullptr || status == nullptr || state == nullptr ||
        state->vram == nullptr || state->vram_ptr == nullptr ||
        state->ramin_ptr == nullptr || width == 0 || height == 0) {
        return false;
    }

    const PGRAPHState *pg = &state->pgraph;
    const Surface *surface = color ? &pg->surface_color : &pg->surface_zeta;
    D3D11SurfaceFormat format = D3D11SurfaceFormat::Bgr8A8;
    uint32_t bytes_per_pixel = 0;
    if (color) {
        if (pgraph_clear_classify_color_storage(
                pg->surface_shape.color_format) !=
            PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8) {
            SetStatus(status, D3D11SurfaceStatus::Unsupported);
            return false;
        }
        bytes_per_pixel = 4;
    } else if (pg->surface_shape.z_format != 0) {
        SetStatus(status, D3D11SurfaceStatus::Unsupported);
        return false;
    } else if (pg->surface_shape.zeta_format ==
               NV097_SET_SURFACE_FORMAT_ZETA_Z16) {
        format = D3D11SurfaceFormat::Z16;
        bytes_per_pixel = 2;
    } else if (pg->surface_shape.zeta_format ==
               NV097_SET_SURFACE_FORMAT_ZETA_Z24S8) {
        format = D3D11SurfaceFormat::Z24S8;
        bytes_per_pixel = 4;
    } else {
        SetStatus(status, D3D11SurfaceStatus::Unsupported);
        return false;
    }

    if (pg->surface_type != NV097_SET_SURFACE_FORMAT_TYPE_PITCH ||
        pg->surface_shape.anti_aliasing !=
            NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1 ||
        pg->surface_scale_factor != 1) {
        SetStatus(status, D3D11SurfaceStatus::Unsupported);
        return false;
    }
    const uint64_t minimum_pitch =
        static_cast<uint64_t>(width) * bytes_per_pixel;
    if (minimum_pitch > UINT32_MAX || surface->pitch < minimum_pitch ||
        surface->pitch % bytes_per_pixel != 0) {
        SetStatus(status, D3D11SurfaceStatus::InvalidDescriptor);
        return false;
    }
    const uint64_t surface_bytes =
        static_cast<uint64_t>(surface->pitch) * height;
    if (surface_bytes == 0 || surface_bytes > UINT64_MAX - surface->offset) {
        SetStatus(status, D3D11SurfaceStatus::InvalidDescriptor);
        return false;
    }

    const hwaddr dma_address = color ? pg->dma_color : pg->dma_zeta;
    const uint64_t ramin_size = memory_region_size(&state->ramin);
    if ((dma_address & (sizeof(uint32_t) - 1)) != 0 ||
        state->ramin_ptr == nullptr || ramin_size < sizeof(uint32_t) * 3 ||
        dma_address > ramin_size - sizeof(uint32_t) * 3) {
        SetStatus(status, D3D11SurfaceStatus::OutOfRange);
        return false;
    }
    const DMAObject dma = nv_dma_load(state, dma_address);
    if (dma.dma_class != NV_DMA_IN_MEMORY_CLASS ||
        dma.dma_target != NV_DMA_TARGET_NVM) {
        SetStatus(status, D3D11SurfaceStatus::Unsupported);
        return false;
    }
    if (dma.address > kVramAddressMask || surface->offset > dma.limit ||
        surface_bytes - 1 > dma.limit - surface->offset) {
        SetStatus(status, D3D11SurfaceStatus::OutOfRange);
        return false;
    }
    if (dma.address > UINT64_MAX - surface->offset) {
        SetStatus(status, D3D11SurfaceStatus::OutOfRange);
        return false;
    }
    const uint64_t absolute_offset = dma.address + surface->offset;
    const uint64_t vram_size = memory_region_size(state->vram);
    if (absolute_offset > vram_size ||
        surface_bytes > vram_size - absolute_offset) {
        SetStatus(status, D3D11SurfaceStatus::OutOfRange);
        return false;
    }

    *descriptor = { absolute_offset,
                    width,
                    height,
                    surface->pitch,
                    format,
                    D3D11SurfaceType::LinearPitch,
                    1,
                    D3D11SurfaceAntialias::Center1 };
    SetStatus(status, D3D11SurfaceStatus::Ok);
    return true;
}

} // namespace xemu

#endif // _WIN32
