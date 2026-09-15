/* C++ side of the fixed-width D3D11 bridge ABI contract. */

#include "hw/xbox/nv2a/pgraph/d3d11/d3d11_bridge.h"

#include <cstddef>

static_assert(alignof(D3D11BridgeSurface) == 8);
static_assert(alignof(D3D11BridgePgraphSnapshot) == 8);
static_assert(alignof(D3D11BridgeVertexAttribute) == 8);
static_assert(sizeof(D3D11BridgeSurface) == 16);
static_assert(sizeof(D3D11BridgePgraphSnapshot) == 416);
static_assert(sizeof(D3D11BridgeVertexAttribute) == 56);
static_assert(sizeof(D3D11BridgeDrawArraysRange) == 8);
static_assert(sizeof(D3D11BridgeSurfaceDescriptor) == 24);
static_assert(offsetof(D3D11BridgePgraphSnapshot, color_surface) == 72);
static_assert(offsetof(D3D11BridgePgraphSnapshot, window_clip_x_min) == 248);
static_assert(offsetof(D3D11BridgePgraphSnapshot, clear_color) == 400);
static_assert(offsetof(D3D11BridgeVertexAttribute, inline_value) == 20);
static_assert(offsetof(D3D11BridgeSurfaceDescriptor, format) == 20);

static_assert(D3D11_BRIDGE_VERTEX_ATTRIBUTES == 16);
static_assert(D3D11_BRIDGE_MAX_DRAW_ARRAY_RANGES == 1250);
static_assert(D3D11_BRIDGE_MAX_BATCH_WORDS == 0x07ffff);
static_assert(D3D11_BRIDGE_VSH_TOKEN_WORDS == 4);
