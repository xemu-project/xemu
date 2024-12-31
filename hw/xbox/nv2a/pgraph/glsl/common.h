/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
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

#ifndef HW_NV2A_SHADERS_COMMON_H
#define HW_NV2A_SHADERS_COMMON_H

#include "qemu/mstring.h"
#include <stdbool.h>

#define GLSL_C(idx) "c[" stringify(idx) "]"
#define GLSL_LTCTXA(idx) "ltctxa[" stringify(idx) "]"

#define GLSL_C_MAT4(idx) \
    "mat4(" GLSL_C(idx) ", " GLSL_C(idx+1) ", " \
            GLSL_C(idx+2) ", " GLSL_C(idx+3) ")"

#define GLSL_DEFINE(a, b) "#define " stringify(a) " " b "\n"

MString *pgraph_get_glsl_vtx_header(MString *out, bool location, bool smooth, bool in, bool prefix, bool array);

#endif
