/*
 * QEMU Geforce NV2A shader generator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
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

#ifndef HW_XBOX_NV2A_PGRAPH_SHADERS_H
#define HW_XBOX_NV2A_PGRAPH_SHADERS_H

#include <stdint.h>
#include "hw/xbox/nv2a/nv2a_regs.h"

#include "vsh.h"
#include "psh.h"

enum ShaderPrimitiveMode {
    PRIM_TYPE_INVALID,
    PRIM_TYPE_POINTS,
    PRIM_TYPE_LINES,
    PRIM_TYPE_LINE_LOOP,
    PRIM_TYPE_LINE_STRIP,
    PRIM_TYPE_TRIANGLES,
    PRIM_TYPE_TRIANGLE_STRIP,
    PRIM_TYPE_TRIANGLE_FAN,
    PRIM_TYPE_QUADS,
    PRIM_TYPE_QUAD_STRIP,
    PRIM_TYPE_POLYGON,
};

enum ShaderPolygonMode {
    POLY_MODE_FILL,
    POLY_MODE_POINT,
    POLY_MODE_LINE,
};

enum MaterialColorSource {
    MATERIAL_COLOR_SRC_MATERIAL,
    MATERIAL_COLOR_SRC_DIFFUSE,
    MATERIAL_COLOR_SRC_SPECULAR,
};

typedef struct ShaderState {
    bool vulkan;
    bool use_push_constants_for_uniform_attrs;
    unsigned int surface_scale_factor;

    PshState psh;
    uint16_t compressed_attrs;
    uint16_t uniform_attrs;
    uint16_t swizzle_attrs;

    bool texture_matrix_enable[4];
    enum VshTexgen texgen[4][4];

    bool fog_enable;
    enum VshFoggen foggen;
    enum VshFogMode fog_mode;

    enum VshSkinning skinning;

    bool normalization;

    enum MaterialColorSource emission_src;
    enum MaterialColorSource ambient_src;
    enum MaterialColorSource diffuse_src;
    enum MaterialColorSource specular_src;

    bool lighting;
    enum VshLight light[NV2A_MAX_LIGHTS];

    bool fixed_function;

    /* vertex program */
    bool vertex_program;
    uint32_t program_data[NV2A_MAX_TRANSFORM_PROGRAM_LENGTH][VSH_TOKEN_SIZE];
    int program_length;
    bool z_perspective;

    /* primitive format for geometry shader */
    enum ShaderPolygonMode polygon_front_mode;
    enum ShaderPolygonMode polygon_back_mode;
    enum ShaderPrimitiveMode primitive_mode;

    bool point_params_enable;
    float point_size;
    float point_params[8];

    bool smooth_shading;
} ShaderState;

typedef struct PGRAPHState PGRAPHState;

ShaderState pgraph_get_shader_state(PGRAPHState *pg);

#endif
