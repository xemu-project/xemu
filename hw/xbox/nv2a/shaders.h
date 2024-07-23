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

#ifndef HW_NV2A_SHADERS_H
#define HW_NV2A_SHADERS_H

#include "qemu/thread.h"
#include "qapi/qmp/qstring.h"
#include "gl/gloffscreen.h"

#include "nv2a_regs.h"
#include "vsh.h"
#include "psh.h"
#include "lru.h"

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
    unsigned int surface_scale_factor;

    PshState psh;
    uint16_t compressed_attrs;

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

typedef struct ShaderBinding {
    GLuint gl_program;
    GLenum gl_primitive_mode;

    GLint psh_constant_loc[9][2];
    GLint alpha_ref_loc;

    GLint bump_mat_loc[NV2A_MAX_TEXTURES];
    GLint bump_scale_loc[NV2A_MAX_TEXTURES];
    GLint bump_offset_loc[NV2A_MAX_TEXTURES];
    GLint tex_scale_loc[NV2A_MAX_TEXTURES];

    GLint surface_size_loc;
    GLint clip_range_loc;

    GLint vsh_constant_loc[NV2A_VERTEXSHADER_CONSTANTS];
    uint32_t vsh_constants[NV2A_VERTEXSHADER_CONSTANTS][4];

    GLint inv_viewport_loc;
    GLint ltctxa_loc[NV2A_LTCTXA_COUNT];
    GLint ltctxb_loc[NV2A_LTCTXB_COUNT];
    GLint ltc1_loc[NV2A_LTC1_COUNT];

    GLint fog_color_loc;
    GLint fog_param_loc[2];
    GLint light_infinite_half_vector_loc[NV2A_MAX_LIGHTS];
    GLint light_infinite_direction_loc[NV2A_MAX_LIGHTS];
    GLint light_local_position_loc[NV2A_MAX_LIGHTS];
    GLint light_local_attenuation_loc[NV2A_MAX_LIGHTS];

    GLint clip_region_loc[8];

    GLint material_alpha_loc;
} ShaderBinding;

typedef struct ShaderLruNode {
    LruNode node;
    bool cached;
    void *program;
    size_t program_size;
    GLenum program_format;
    ShaderState state;
    ShaderBinding *binding;
    QemuThread *save_thread;
} ShaderLruNode;

typedef struct PGRAPHState PGRAPHState;

GLenum get_gl_primitive_mode(enum ShaderPolygonMode polygon_mode, enum ShaderPrimitiveMode primitive_mode);
void update_shader_constant_locations(ShaderBinding *binding, const ShaderState *state);
ShaderBinding *generate_shaders(const ShaderState *state);

void shader_cache_init(PGRAPHState *pg);
void shader_write_cache_reload_list(PGRAPHState *pg);
bool shader_load_from_memory(ShaderLruNode *snode);
void shader_cache_to_disk(ShaderLruNode *snode);

#endif
