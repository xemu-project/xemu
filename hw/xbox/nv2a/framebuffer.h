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
} NV2AFramebufferSurfaceType;

typedef struct NV2AFramebufferSurface {
    NV2AFramebufferSurfaceType type;
    uint64_t handle;
} NV2AFramebufferSurface;

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
