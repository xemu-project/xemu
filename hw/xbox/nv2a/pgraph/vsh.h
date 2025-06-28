/*
 * QEMU Geforce NV2A vertex shader translation
 *
 * Copyright (c) 2012 espes
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HW_NV2A_VSH_H
#define HW_NV2A_VSH_H

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/nv2a_regs.h"

enum VshLight {
    LIGHT_OFF,
    LIGHT_INFINITE,
    LIGHT_LOCAL,
    LIGHT_SPOT
};

enum VshTexgen {
    TEXGEN_DISABLE,
    TEXGEN_EYE_LINEAR,
    TEXGEN_OBJECT_LINEAR,
    TEXGEN_SPHERE_MAP,
    TEXGEN_NORMAL_MAP,
    TEXGEN_REFLECTION_MAP,
};

enum VshFogMode {
    FOG_MODE_LINEAR,
    FOG_MODE_EXP,
    FOG_MODE_ERROR2, /* Doesn't exist */
    FOG_MODE_EXP2,
    FOG_MODE_LINEAR_ABS,
    FOG_MODE_EXP_ABS,
    FOG_MODE_ERROR6, /* Doesn't exist */
    FOG_MODE_EXP2_ABS
};

enum VshFoggen {
    FOGGEN_SPEC_ALPHA,
    FOGGEN_RADIAL,
    FOGGEN_PLANAR,
    FOGGEN_ABS_PLANAR,
    FOGGEN_FOG_X
};

enum VshSkinning {
    SKINNING_OFF,
    SKINNING_1WEIGHTS,
    SKINNING_2WEIGHTS2MATRICES,
    SKINNING_2WEIGHTS,
    SKINNING_3WEIGHTS3MATRICES,
    SKINNING_3WEIGHTS,
    SKINNING_4WEIGHTS4MATRICES,
};

// vs.1.1, not an official value
#define VSH_VERSION_VS                     0xF078

// Xbox vertex shader
#define VSH_VERSION_XVS                    0x2078

// Xbox vertex state shader
#define VSH_VERSION_XVSS                   0x7378

// Xbox vertex read/write shader
#define VSH_VERSION_XVSW                   0x7778

#define VSH_TOKEN_SIZE 4

typedef enum {
    FLD_ILU = 0,
    FLD_MAC,
    FLD_CONST,
    FLD_V,
    // Input A
    FLD_A_NEG,
    FLD_A_SWZ_X,
    FLD_A_SWZ_Y,
    FLD_A_SWZ_Z,
    FLD_A_SWZ_W,
    FLD_A_R,
    FLD_A_MUX,
    // Input B
    FLD_B_NEG,
    FLD_B_SWZ_X,
    FLD_B_SWZ_Y,
    FLD_B_SWZ_Z,
    FLD_B_SWZ_W,
    FLD_B_R,
    FLD_B_MUX,
    // Input C
    FLD_C_NEG,
    FLD_C_SWZ_X,
    FLD_C_SWZ_Y,
    FLD_C_SWZ_Z,
    FLD_C_SWZ_W,
    FLD_C_R_HIGH,
    FLD_C_R_LOW,
    FLD_C_MUX,
    // Output
    FLD_OUT_MAC_MASK,
    FLD_OUT_R,
    FLD_OUT_ILU_MASK,
    FLD_OUT_O_MASK,
    FLD_OUT_ORB,
    FLD_OUT_ADDRESS,
    FLD_OUT_MUX,
    // Relative addressing
    FLD_A0X,
    // Final instruction
    FLD_FINAL
} VshFieldName;

uint8_t vsh_get_field(const uint32_t *shader_token, VshFieldName field_name);

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

    /* primitive format for geometry shader */
    enum ShaderPolygonMode polygon_front_mode;
    enum ShaderPolygonMode polygon_back_mode;
    enum ShaderPrimitiveMode primitive_mode;

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

#endif
