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

typedef struct GenPshGlslOptions {
    bool vulkan;
    int ubo_binding;
    int tex_binding;
} GenPshGlslOptions;

MString *pgraph_gen_psh_glsl(const PshState state, GenPshGlslOptions opts);

#endif
