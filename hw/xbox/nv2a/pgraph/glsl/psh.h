/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2013 espes
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

#ifndef HW_XBOX_NV2A_PGRAPH_GLSL_PSH_H
#define HW_XBOX_NV2A_PGRAPH_GLSL_PSH_H

#include "qemu/mstring.h"
#include "hw/xbox/nv2a/pgraph/psh.h"
#include "common.h"

#define PSH_UNIFORM_DECL_X(S, DECL) \
    DECL(S, alphaRef, int, 1)       \
    DECL(S, bumpMat, mat2, 4)       \
    DECL(S, bumpOffset, float, 4)   \
    DECL(S, bumpScale, float, 4)    \
    DECL(S, clipRange, vec4, 1)     \
    DECL(S, clipRegion, ivec4, 8)   \
    DECL(S, colorKey, uint, 4)      \
    DECL(S, colorKeyMask, uint, 4)  \
    DECL(S, consts, vec4, 18)       \
    DECL(S, depthOffset, float, 1)  \
    DECL(S, fogColor, vec4, 1)      \
    DECL(S, texScale, float, 4)

DECL_UNIFORM_TYPES(PshUniform, PSH_UNIFORM_DECL_X)

typedef struct GenPshGlslOptions {
    bool vulkan;
    int ubo_binding;
    int tex_binding;
} GenPshGlslOptions;

typedef struct PGRAPHState PGRAPHState;

MString *pgraph_gen_psh_glsl(const PshState state, GenPshGlslOptions opts);

void pgraph_set_psh_uniform_values(PGRAPHState *pg,
                                   const PshUniformLocs locs,
                                   PshUniformValues *values);

#endif
