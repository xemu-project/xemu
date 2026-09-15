/* Compile-time ABI contract for the QEMU/D3D11 bridge. */

#include "hw/xbox/nv2a/pgraph/d3d11/d3d11_bridge.h"

#include <stddef.h>

/* Keep this probe valid for MSVC's C mode as well as C11 compilers. */
#define D3D11_BRIDGE_STATIC_ASSERT(condition, name) \
    typedef char name[(condition) ? 1 : -1]

D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgeSurface, offset) == 0,
                           d3d11_bridge_surface_offset_must_be_zero);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgePgraphSnapshot, color_surface) >
                               offsetof(D3D11BridgePgraphSnapshot, clip_height),
                           d3d11_bridge_surface_follows_geometry);
D3D11_BRIDGE_STATIC_ASSERT(
    sizeof(((D3D11BridgePgraphSnapshot *)0)->window_clip_x_min) ==
        8 * sizeof(uint32_t),
    d3d11_bridge_window_clip_array_is_bounded);
D3D11_BRIDGE_STATIC_ASSERT(
    sizeof(((D3D11BridgeVertexAttribute *)0)->inline_value) ==
        4 * sizeof(float),
    d3d11_bridge_inline_values_are_float4);
D3D11_BRIDGE_STATIC_ASSERT(sizeof(D3D11BridgeSurface) == 16,
                           d3d11_bridge_surface_size);
D3D11_BRIDGE_STATIC_ASSERT(sizeof(D3D11BridgePgraphSnapshot) == 416,
                           d3d11_bridge_pgraph_snapshot_size);
D3D11_BRIDGE_STATIC_ASSERT(sizeof(D3D11BridgeVertexAttribute) == 56,
                           d3d11_bridge_vertex_attribute_size);
D3D11_BRIDGE_STATIC_ASSERT(sizeof(D3D11BridgeDrawArraysRange) == 8,
                           d3d11_bridge_draw_arrays_range_size);
D3D11_BRIDGE_STATIC_ASSERT(sizeof(D3D11BridgeSurfaceDescriptor) == 24,
                           d3d11_bridge_surface_descriptor_size);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgePgraphSnapshot, color_surface) ==
                               72,
                           d3d11_bridge_color_surface_offset);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgePgraphSnapshot,
                                    window_clip_x_min) == 248,
                           d3d11_bridge_window_clip_offset);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgePgraphSnapshot, clear_color) ==
                               400,
                           d3d11_bridge_clear_color_offset);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgeVertexAttribute, inline_value) ==
                               20,
                           d3d11_bridge_inline_value_offset);
D3D11_BRIDGE_STATIC_ASSERT(offsetof(D3D11BridgeSurfaceDescriptor, format) == 20,
                           d3d11_bridge_surface_format_offset);

int main(void)
{
    return D3D11_BRIDGE_VERTEX_ATTRIBUTES == 16 &&
                   D3D11_BRIDGE_MAX_DRAW_ARRAY_RANGES == 1250 &&
                   D3D11_BRIDGE_MAX_BATCH_WORDS == 0x07ffff &&
                   D3D11_BRIDGE_VSH_TOKEN_WORDS == 4 ?
               0 :
               1;
}
