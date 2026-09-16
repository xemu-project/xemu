/*
 * NV2A framebuffer export contract
 *
 * Copyright (c) 2025 Matt Borgerson
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_XBOX_NV2A_FRAMEBUFFER_H
#define HW_XBOX_NV2A_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>

/*
 * A framebuffer export is a borrowed handle owned by the active renderer.
 * Keep this enum independent of the renderer's native headers so additional
 * export types (for example, a D3D11 texture) can be added without making
 * pgraph depend on those APIs.
 */
typedef enum NV2AFramebufferSurfaceType {
    NV2A_FRAMEBUFFER_SURFACE_NONE = 0,
    NV2A_FRAMEBUFFER_SURFACE_OPENGL_TEXTURE,
    NV2A_FRAMEBUFFER_SURFACE_D3D11_TEXTURE2D,
} NV2AFramebufferSurfaceType;

/*
 * A surface returned by nv2a_get_framebuffer_surface() is always borrowed.
 * The renderer owns the native object and the lease ends when
 * nv2a_release_framebuffer_surface() is called.  Keeping the ownership in
 * the ABI makes it explicit that a consumer must not Release/Delete the
 * object or retain it past the release call.
 */
typedef enum NV2AFramebufferSurfaceOwnership {
    NV2A_FRAMEBUFFER_SURFACE_BORROWED = 0,
} NV2AFramebufferSurfaceOwnership;

/* Renderer-neutral formats needed by the presentation handoff. */
typedef enum NV2AFramebufferSurfaceFormat {
    NV2A_FRAMEBUFFER_SURFACE_FORMAT_UNKNOWN = 0,
    NV2A_FRAMEBUFFER_SURFACE_FORMAT_RGBA8_UNORM,
    NV2A_FRAMEBUFFER_SURFACE_FORMAT_BGRA8_UNORM,
} NV2AFramebufferSurfaceFormat;

/* D3D11 bind flags expressed without including the Windows SDK in QEMU C. */
typedef enum NV2AFramebufferSurfaceBindFlags {
    NV2A_FRAMEBUFFER_SURFACE_BIND_NONE = 0,
    NV2A_FRAMEBUFFER_SURFACE_BIND_RENDER_TARGET = 1u << 0,
    NV2A_FRAMEBUFFER_SURFACE_BIND_SHADER_RESOURCE = 1u << 1,
} NV2AFramebufferSurfaceBindFlags;

typedef struct NV2AFramebufferSurface {
    NV2AFramebufferSurfaceType type;
    NV2AFramebufferSurfaceOwnership ownership;

    /* Existing OpenGL texture name.  Kept for source and ABI compatibility. */
    uint64_t handle;

    /*
     * Opaque native resource pointer, represented as a fixed-width integer
     * so this header remains valid in C, C++, and UWP builds.  For
     * NV2A_FRAMEBUFFER_SURFACE_D3D11_TEXTURE2D this is an
     * ID3D11Texture2D*.  The pointer is borrowed for the current lease.
     */
    uint64_t resource;

    /* Metadata describes the resource without exposing API-specific types. */
    uint32_t width;
    uint32_t height;
    NV2AFramebufferSurfaceFormat format;
    uint32_t sample_count;
    uint32_t mip_levels;
    uint32_t array_size;
    uint32_t bind_flags;
    uint32_t reserved;

    /* Assigned by the broker for each successful borrowed acquisition. */
    uint64_t lease_id;
} NV2AFramebufferSurface;

/*
 * Validate the renderer-neutral portion of an export before advertising it
 * to a consumer.  This intentionally cannot prove that an opaque pointer is
 * a live COM object; the producing renderer must supply the real resource,
 * while this check rejects incomplete/mismatched descriptors at the ABI
 * boundary.
 */
static inline bool
nv2a_framebuffer_surface_is_valid(const NV2AFramebufferSurface *surface)
{
    if (surface == NULL ||
        surface->ownership != NV2A_FRAMEBUFFER_SURFACE_BORROWED) {
        return false;
    }

    switch (surface->type) {
    case NV2A_FRAMEBUFFER_SURFACE_OPENGL_TEXTURE:
        return surface->handle != 0;

    case NV2A_FRAMEBUFFER_SURFACE_D3D11_TEXTURE2D:
        return surface->resource != 0 && surface->width != 0 &&
               surface->height != 0 &&
               (surface->format ==
                    NV2A_FRAMEBUFFER_SURFACE_FORMAT_RGBA8_UNORM ||
                surface->format ==
                    NV2A_FRAMEBUFFER_SURFACE_FORMAT_BGRA8_UNORM) &&
               surface->sample_count == 1 && surface->mip_levels == 1 &&
               surface->array_size == 1 &&
               (surface->bind_flags &
                NV2A_FRAMEBUFFER_SURFACE_BIND_RENDER_TARGET) != 0;

    case NV2A_FRAMEBUFFER_SURFACE_NONE:
    default:
        return false;
    }
}

/*
 * Acquires the current display framebuffer. On success, surface describes a
 * borrowed renderer-owned handle that remains valid until
 * nv2a_release_framebuffer_surface() is called. The caller must not destroy
 * or otherwise retain the handle after release. On failure, no handle is
 * borrowed and no release is required.
 */
bool nv2a_get_framebuffer_surface(NV2AFramebufferSurface *surface);
void nv2a_release_framebuffer_surface(void);

#endif
