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

#ifndef HW_XBOX_NV2A_PGRAPH_CLEAR_H
#define HW_XBOX_NV2A_PGRAPH_CLEAR_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CLEAR_SURFACE and the backend APIs both use inclusive coordinates at this
 * boundary.  Backends convert the normalized rectangle to their own extent
 * convention only after this helper has validated it. */
typedef struct PGRAPHClearRect {
    uint32_t left;
    uint32_t top;
    uint32_t right;
    uint32_t bottom;
} PGRAPHClearRect;

/* Clip only the far edges.  A rectangle whose near edge is outside the
 * target, or whose coordinates are inverted, is invalid. */
bool pgraph_clear_rect_clamp(const PGRAPHClearRect *input, uint32_t width,
                             uint32_t height, PGRAPHClearRect *output);
bool pgraph_clear_rect_is_full(const PGRAPHClearRect *rect, uint32_t width,
                               uint32_t height);

typedef enum PGRAPHClearColorStorage {
    PGRAPH_CLEAR_COLOR_STORAGE_UNSUPPORTED = 0,
    /* Native little-endian X8/A8R8G8B8 storage, represented by a host
     * B8G8R8A8-compatible render target. */
    PGRAPH_CLEAR_COLOR_STORAGE_BGR8A8,
} PGRAPHClearColorStorage;

PGRAPHClearColorStorage
pgraph_clear_classify_color_storage(unsigned int color_format);

/* Extract the exact integer values consumed by NV097_CLEAR_SURFACE.  The
 * fixed path is deliberately separate from pgraph_get_clear_depth_stencil
 * (which converts both fixed and floating encodings to normalized floats).
 * Returns false for missing/unknown formats or for the floating z_format. */
bool pgraph_clear_extract_fixed_depth_stencil(unsigned int zeta_format,
                                              unsigned int z_format,
                                              uint32_t clear_value,
                                              uint32_t *depth,
                                              uint8_t *stencil);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* HW_XBOX_NV2A_PGRAPH_CLEAR_H */
