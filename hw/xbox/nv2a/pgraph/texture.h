/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2024 Matt Borgerson
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

#ifndef HW_XBOX_NV2A_PGRAPH_TEXTURE_H
#define HW_XBOX_NV2A_PGRAPH_TEXTURE_H

#include "qemu/osdep.h"
#include "cpu.h"

#include <stdbool.h>
#include <stdint.h>

#include "hw/xbox/nv2a/nv2a_regs.h"

typedef struct PGRAPHState PGRAPHState;

typedef struct TextureShape {
    bool cubemap;
    unsigned int dimensionality;
    unsigned int color_format;
    unsigned int levels;
    unsigned int width, height, depth;
    bool border;

    unsigned int min_mipmap_level, max_mipmap_level;
    unsigned int pitch;
} TextureShape;

typedef struct BasicColorFormatInfo {
    unsigned int bytes_per_pixel;
    bool linear;
    bool depth;
} BasicColorFormatInfo;

extern const BasicColorFormatInfo kelvin_color_format_info_map[66];

uint8_t *pgraph_convert_texture_data(const TextureShape s, const uint8_t *data,
                                     const uint8_t *palette_data,
                                     unsigned int width, unsigned int height,
                                     unsigned int depth, unsigned int row_pitch,
                                     unsigned int slice_pitch,
                                     size_t *converted_size);

hwaddr pgraph_get_texture_phys_addr(PGRAPHState *pg, int texture_idx);
hwaddr pgraph_get_texture_palette_phys_addr_length(PGRAPHState *pg, int texture_idx, size_t *length);
TextureShape pgraph_get_texture_shape(PGRAPHState *pg, int texture_idx);
size_t pgraph_get_texture_length(PGRAPHState *pg, TextureShape *shape);

static inline float pgraph_convert_lod_bias_to_float(uint32_t lod_bias)
{
    int sign_extended_bias = lod_bias;
    if (lod_bias & (1 << 12)) {
        sign_extended_bias |= ~NV_PGRAPH_TEXFILTER0_MIPMAP_LOD_BIAS;
    }
    return (float)sign_extended_bias / 256.f;
}

#endif
