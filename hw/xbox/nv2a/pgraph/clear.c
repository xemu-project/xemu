/*
 * Renderer-independent helpers for NV2A CLEAR_SURFACE.
 *
 * Copyright (C) 2026 xemu Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "clear.h"
#include "../nv2a_regs.h"

bool pgraph_clear_rect_clamp(const PGRAPHClearRect *input, uint32_t width,
                             uint32_t height, PGRAPHClearRect *output)
{
    if (!input || !output || !width || !height || input->left > input->right ||
        input->top > input->bottom || input->left >= width ||
        input->top >= height) {
        return false;
    }

    *output = *input;
    if (output->right >= width) {
        output->right = width - 1;
    }
    if (output->bottom >= height) {
        output->bottom = height - 1;
    }
    return output->left <= output->right && output->top <= output->bottom;
}

bool pgraph_clear_rect_is_full(const PGRAPHClearRect *rect, uint32_t width,
                               uint32_t height)
{
    return rect && width && height && rect->left == 0 && rect->top == 0 &&
           rect->right == width - 1 && rect->bottom == height - 1;
}

PGRAPHClearColorStorage
pgraph_clear_classify_color_storage(unsigned int color_format)
{
    switch (color_format) {
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8:
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8:
        return PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8;
    default:
        return PGRAPH_CLEAR_COLOR_STORAGE_UNSUPPORTED;
    }
}

bool pgraph_clear_extract_fixed_depth_stencil(unsigned int zeta_format,
                                              unsigned int z_format,
                                              uint32_t clear_value,
                                              uint32_t *depth, uint8_t *stencil)
{
    if (!depth || !stencil || z_format) {
        return false;
    }

    switch (zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        *depth = clear_value & 0xffffu;
        *stencil = 0;
        return true;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        *depth = (clear_value >> 8) & 0xffffffu;
        *stencil = (uint8_t)(clear_value & 0xffu);
        return true;
    default:
        return false;
    }
}
