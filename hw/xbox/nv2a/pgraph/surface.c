/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
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

#include "surface.h"

#include "hw/xbox/nv2a/nv2a_regs.h"

#if (NV_PGRAPH_BLEND_SFACTOR_DST_ALPHA !=                           \
     NV_PGRAPH_BLEND_DFACTOR_DST_ALPHA) ||                          \
    (NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_ALPHA !=                 \
     NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_DST_ALPHA) ||                \
    (NV_PGRAPH_BLEND_SFACTOR_ONE != NV_PGRAPH_BLEND_DFACTOR_ONE) || \
    (NV_PGRAPH_BLEND_SFACTOR_ZERO != NV_PGRAPH_BLEND_DFACTOR_ZERO)
#error NV_PGRAPH_BLEND_SFACTOR defines expected to equal DFACTOR ones
#endif

uint32_t fixup_blend_factor_for_surface(uint32_t blend_factor,
                                        const struct SurfaceShape *surface)
{
    switch (surface->color_format) {
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_O1R5G5B5:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8:
        switch (blend_factor) {
        case NV_PGRAPH_BLEND_SFACTOR_DST_ALPHA:
            return NV_PGRAPH_BLEND_SFACTOR_ONE;
        case NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_ALPHA:
            return NV_PGRAPH_BLEND_SFACTOR_ZERO;
        default:
            break;
        }
        break;

    default:
        break;
    }

    return blend_factor;
}
