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

#include "qemu/mstring.h"
#include "hw/xbox/nv2a/pgraph/vsh.h"
#include "common.h"

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

typedef struct PGRAPHState PGRAPHState;

MString *pgraph_gen_vsh_glsl(const VshState *state,
                             GenVshGlslOptions glsl_opts);

void pgraph_set_vsh_uniform_values(PGRAPHState *pg, const VshState *state,
                                   const VshUniformLocs locs,
                                   VshUniformValues *values);

#endif
