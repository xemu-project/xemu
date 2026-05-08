/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2020-2025 Matt Borgerson
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

#ifndef HW_XBOX_NV2A_PGRAPH_GLSL_VSH_H
#define HW_XBOX_NV2A_PGRAPH_GLSL_VSH_H

#include "common.h"
#include "hw/xbox/nv2a/pgraph/vsh_regs.h"

typedef struct PGRAPHState PGRAPHState;

typedef struct FixedFunctionVshState {
    bool normalization;
    bool texture_matrix_enable[4];
    enum VshTexgen texgen[4][4];
    enum VshFoggen foggen;
    enum VshSkinning skinning;
    bool lighting;
    enum VshLight light[NV2A_MAX_LIGHTS];
    enum MaterialColorSource emission_src;
    enum MaterialColorSource ambient_src;
    enum MaterialColorSource diffuse_src;
    enum MaterialColorSource specular_src;
    bool local_eye;
} FixedFunctionVshState;

typedef struct ProgrammableVshState {
    uint32_t program_data[NV2A_MAX_TRANSFORM_PROGRAM_LENGTH][VSH_TOKEN_SIZE];
    int program_length;
} ProgrammableVshState;

typedef struct {
    unsigned int surface_scale_factor;  // FIXME: Remove

    uint16_t compressed_attrs;
    uint16_t uniform_attrs;
    uint16_t swizzle_attrs;

    bool fog_enable;
    enum VshFogMode fog_mode;

    bool specular_enable;
    bool separate_specular;
    bool ignore_specular_alpha;
    float specular_power;
    float specular_power_back;

    bool point_params_enable;
    float point_size;
    float point_params[8];

    bool smooth_shading;
    bool z_perspective;

    bool is_fixed_function;
    FixedFunctionVshState fixed_function;
    ProgrammableVshState programmable;
} VshState;

void pgraph_glsl_set_vsh_state(PGRAPHState *pg, VshState *state);

#define VSH_UNIFORM_DECL_X(S, DECL)                          \
    DECL(S, c, vec4, NV2A_VERTEXSHADER_CONSTANTS)            \
    DECL(S, clipRange, vec4, 1)                              \
    DECL(S, fogParam, vec2, 1)                               \
    DECL(S, inlineValue, vec4, NV2A_VERTEXSHADER_ATTRIBUTES) \
    DECL(S, lightInfiniteDirection, vec3, NV2A_MAX_LIGHTS)   \
    DECL(S, lightInfiniteHalfVector, vec3, NV2A_MAX_LIGHTS)  \
    DECL(S, lightLocalAttenuation, vec3, NV2A_MAX_LIGHTS)    \
    DECL(S, lightLocalPosition, vec3, NV2A_MAX_LIGHTS)       \
    DECL(S, ltc1, vec4, NV2A_LTC1_COUNT)                     \
    DECL(S, ltctxa, vec4, NV2A_LTCTXA_COUNT)                 \
    DECL(S, ltctxb, vec4, NV2A_LTCTXB_COUNT)                 \
    DECL(S, material_alpha, float, 1)                        \
    DECL(S, pointParams, float, 8)                           \
    DECL(S, specularPower, float, 1)                         \
    DECL(S, surfaceSize, vec2, 1)

DECL_UNIFORM_TYPES(VshUniform, VSH_UNIFORM_DECL_X)

typedef struct GenVshGlslOptions {
    bool vulkan;
    bool prefix_outputs;
    bool use_push_constants_for_uniform_attrs;
    int ubo_binding;
} GenVshGlslOptions;

MString *pgraph_glsl_gen_vsh(const VshState *state,
                             GenVshGlslOptions glsl_opts);

void pgraph_glsl_set_vsh_uniform_values(PGRAPHState *pg, const VshState *state,
                                        const VshUniformLocs locs,
                                        VshUniformValues *values);

#endif
