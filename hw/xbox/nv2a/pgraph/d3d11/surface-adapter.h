// xemu NV2A to D3D11 surface descriptor adapter
//
// Copyright (C) 2026 xemu Project
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 or (at your option) any
// later version.

#ifndef XEMU_NV2A_PGRAPH_D3D11_SURFACE_ADAPTER_H
#define XEMU_NV2A_PGRAPH_D3D11_SURFACE_ADAPTER_H

#ifdef _WIN32

#include "surface.h"

#include <cstdint>

struct NV2AState;
struct PGRAPHState;

namespace xemu {

bool d3d11_get_surface_dimensions(const PGRAPHState *pg, uint32_t *width,
                                  uint32_t *height);

bool d3d11_surface_descriptors_overlap(const D3D11SurfaceDescriptor &left,
                                       const D3D11SurfaceDescriptor &right);

/* Build the linear-pitch descriptor used by both the clear and draw
 * executors.  The function does not alter PGRAPH, VRAM, or any cache state.
 * `color` selects surface_color/dma_color; false selects surface_zeta/dma_zeta.
 * Width and height are the complete bound target dimensions. */
bool d3d11_build_surface_descriptor(NV2AState *state, bool color,
                                    uint32_t width, uint32_t height,
                                    D3D11SurfaceDescriptor *descriptor,
                                    D3D11SurfaceStatus *status);

} // namespace xemu

#endif // _WIN32

#endif // XEMU_NV2A_PGRAPH_D3D11_SURFACE_ADAPTER_H
