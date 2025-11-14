/*
 * Geforce NV2A PGRAPH GLSL Shader Generator
 *
 * Copyright (c) 2015 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2025 Matt Borgerson
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

#include "qemu/osdep.h"
#include "qemu/mstring.h"

typedef int ivec2[2];
typedef int ivec4[4];
typedef float mat2[2 * 2];
typedef unsigned int uint;
typedef float vec2[2];
typedef float vec3[3];
typedef float vec4[4];

#define UNIFORM_ELEMENT_TYPE_X(DECL) \
    DECL(float)                      \
    DECL(int)                        \
    DECL(ivec2)                      \
    DECL(ivec4)                      \
    DECL(mat2)                       \
    DECL(uint)                       \
    DECL(vec2)                       \
    DECL(vec3)                       \
    DECL(vec4)

enum UniformElementType {
#define DECL_UNIFORM_ELEMENT_TYPE(type) UniformElementType_##type,
    UNIFORM_ELEMENT_TYPE_X(DECL_UNIFORM_ELEMENT_TYPE)
};

extern const char *uniform_element_type_to_str[];

#define DECL_UNIFORM_ENUM_VALUE(s, name, type, count) s##_##name,
#define DECL_UNIFORM_ENUM_TYPE(name, decls)                 \
    enum name##Indices{                                     \
        decls(name, DECL_UNIFORM_ENUM_VALUE) name##__COUNT, \
    };

#define DECL_UNIFORM_LOC_STRUCT_TYPE(name, decls) \
    typedef int name##Locs[name##__COUNT];

#define DECL_UNIFORM_VAL_STRUCT_FIELD(s, name, type, count) type name[count];
#define DECL_UNIFORM_VAL_STRUCT_TYPE(name, decls)  \
    typedef struct name##Values {                  \
        decls(name, DECL_UNIFORM_VAL_STRUCT_FIELD) \
    } name##Values;

typedef struct UniformInfo {
    const char *name;
    enum UniformElementType type;
    size_t size;
    size_t count;
    size_t val_offs;
} UniformInfo;

#define DECL_UNIFORM_INFO_ITEM(s, name, type, count)         \
    { #name, UniformElementType_##type, sizeof(type), count, \
      offsetof(s##Values, name) },
#define DECL_UNIFORM_INFO_ARR(name, decls) \
    extern const UniformInfo name##Info[];
#define DEF_UNIFORM_INFO_ARR(name, decls) \
    const UniformInfo name##Info[] = { decls(name, DECL_UNIFORM_INFO_ITEM) };

#define DECL_UNIFORM_TYPES(name, decls)       \
    DECL_UNIFORM_ENUM_TYPE(name, decls)       \
    DECL_UNIFORM_LOC_STRUCT_TYPE(name, decls) \
    DECL_UNIFORM_VAL_STRUCT_TYPE(name, decls) \
    DECL_UNIFORM_INFO_ARR(name, decls)

#define GLSL_C(idx) "c[" stringify(idx) "]"
#define GLSL_LTCTXA(idx) "ltctxa[" stringify(idx) "]"

#define GLSL_C_MAT4(idx) \
    "mat4(" GLSL_C(idx) ", " GLSL_C(idx+1) ", " \
            GLSL_C(idx+2) ", " GLSL_C(idx+3) ")"

#define GLSL_DEFINE(a, b) "#define " stringify(a) " " b "\n"

MString *pgraph_glsl_get_vtx_header(MString *out, bool location, bool smooth,
                                    bool in, bool prefix, bool array);

typedef struct PGRAPHState PGRAPHState;

void pgraph_glsl_set_clip_range_uniform_value(PGRAPHState *pg,
                                              float clipRange[4]);

#endif
