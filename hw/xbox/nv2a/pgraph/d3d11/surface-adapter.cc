// xemu NV2A to D3D11 surface descriptor adapter

#include "surface-adapter.h"

#ifdef _WIN32

#include "d3d11_bridge.h"

#include <limits>

namespace xemu {
namespace {

bool RangeEnd(uint64_t offset, uint64_t size, uint64_t *end)
{
    if (end == nullptr || size == 0 ||
        offset > std::numeric_limits<uint64_t>::max() - size) {
        return false;
    }
    *end = offset + size;
    return true;
}

void SetStatus(D3D11SurfaceStatus *status, D3D11SurfaceStatus value)
{
    if (status != nullptr) {
        *status = value;
    }
}

D3D11SurfaceStatus ConvertStatus(uint32_t status)
{
    switch (status) {
    case D3D11_BRIDGE_SURFACE_OK:
        return D3D11SurfaceStatus::Ok;
    case D3D11_BRIDGE_SURFACE_UNSUPPORTED:
        return D3D11SurfaceStatus::Unsupported;
    case D3D11_BRIDGE_SURFACE_OUT_OF_RANGE:
        return D3D11SurfaceStatus::OutOfRange;
    default:
        return D3D11SurfaceStatus::InvalidDescriptor;
    }
}

} // namespace

bool d3d11_get_surface_dimensions(const PGRAPHState *pg, uint32_t *width,
                                  uint32_t *height)
{
    D3D11BridgePgraphSnapshot snapshot = {};
    if (width == nullptr || height == nullptr ||
        !d3d11_bridge_snapshot_pgraph(pg, &snapshot)) {
        return false;
    }
    if (snapshot.binding_width != 0 || snapshot.binding_height != 0) {
        if (snapshot.binding_width <= 0 || snapshot.binding_height <= 0) {
            return false;
        }
        *width = static_cast<uint32_t>(snapshot.binding_width);
        *height = static_cast<uint32_t>(snapshot.binding_height);
        return true;
    }
    const uint64_t expanded_width =
        static_cast<uint64_t>(snapshot.clip_x) + snapshot.clip_width;
    const uint64_t expanded_height =
        static_cast<uint64_t>(snapshot.clip_y) + snapshot.clip_height;
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
    if (descriptor == nullptr || status == nullptr || state == nullptr) {
        return false;
    }
    D3D11BridgeSurfaceDescriptor bridge = {};
    uint32_t bridge_status = D3D11_BRIDGE_SURFACE_INVALID;
    if (!d3d11_bridge_describe_surface(state, color, width, height, &bridge,
                                       &bridge_status)) {
        *status = ConvertStatus(bridge_status);
        return false;
    }
    D3D11SurfaceFormat format;
    switch (bridge.format) {
    case 1:
        format = D3D11SurfaceFormat::Bgr8A8;
        break;
    case 2:
        format = D3D11SurfaceFormat::Z16;
        break;
    case 3:
        format = D3D11SurfaceFormat::Z24S8;
        break;
    default:
        *status = D3D11SurfaceStatus::Unsupported;
        return false;
    }
    *descriptor = { bridge.offset,
                    bridge.width,
                    bridge.height,
                    bridge.pitch,
                    format,
                    D3D11SurfaceType::LinearPitch,
                    1,
                    D3D11SurfaceAntialias::Center1 };
    *status = D3D11SurfaceStatus::Ok;
    return true;
}

} // namespace xemu

#endif // _WIN32
