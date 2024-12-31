/*
 * Geforce NV2A PGRAPH OpenGL Renderer
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

#ifndef HW_XBOX_NV2A_PGRAPH_GL_CONSTANTS_H
#define HW_XBOX_NV2A_PGRAPH_GL_CONSTANTS_H

#include "qemu/osdep.h"
#include "hw/xbox/nv2a/nv2a_regs.h"
#include "gloffscreen.h"

static const GLenum pgraph_texture_min_filter_gl_map[] = {
    0,
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR,
    GL_LINEAR,
};

static const GLenum pgraph_texture_mag_filter_gl_map[] = {
    0,
    GL_NEAREST,
    GL_LINEAR,
    0,
    GL_LINEAR /* TODO: Convolution filter... */
};

static const GLenum pgraph_texture_addr_gl_map[] = {
    0,
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
    GL_CLAMP_TO_BORDER,
    GL_CLAMP_TO_EDGE, /* Approximate GL_CLAMP */
};

static const GLenum pgraph_blend_factor_gl_map[] = {
    GL_ZERO,
    GL_ONE,
    GL_SRC_COLOR,
    GL_ONE_MINUS_SRC_COLOR,
    GL_SRC_ALPHA,
    GL_ONE_MINUS_SRC_ALPHA,
    GL_DST_ALPHA,
    GL_ONE_MINUS_DST_ALPHA,
    GL_DST_COLOR,
    GL_ONE_MINUS_DST_COLOR,
    GL_SRC_ALPHA_SATURATE,
    0,
    GL_CONSTANT_COLOR,
    GL_ONE_MINUS_CONSTANT_COLOR,
    GL_CONSTANT_ALPHA,
    GL_ONE_MINUS_CONSTANT_ALPHA,
};

static const GLenum pgraph_blend_equation_gl_map[] = {
    GL_FUNC_SUBTRACT,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_ADD,
    GL_MIN,
    GL_MAX,
    GL_FUNC_REVERSE_SUBTRACT,
    GL_FUNC_ADD,
};

/* FIXME
static const GLenum pgraph_blend_logicop_map[] = {
    GL_CLEAR,
    GL_AND,
    GL_AND_REVERSE,
    GL_COPY,
    GL_AND_INVERTED,
    GL_NOOP,
    GL_XOR,
    GL_OR,
    GL_NOR,
    GL_EQUIV,
    GL_INVERT,
    GL_OR_REVERSE,
    GL_COPY_INVERTED,
    GL_OR_INVERTED,
    GL_NAND,
    GL_SET,
};
*/

static const GLenum pgraph_cull_face_gl_map[] = {
    0,
    GL_FRONT,
    GL_BACK,
    GL_FRONT_AND_BACK
};

static const GLenum pgraph_depth_func_gl_map[] = {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const GLenum pgraph_stencil_func_gl_map[] = {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const GLenum pgraph_stencil_op_gl_map[] = {
    0,
    GL_KEEP,
    GL_ZERO,
    GL_REPLACE,
    GL_INCR,
    GL_DECR,
    GL_INVERT,
    GL_INCR_WRAP,
    GL_DECR_WRAP,
};

typedef struct ColorFormatInfo {
    unsigned int bytes_per_pixel;
    bool linear;
    GLint gl_internal_format;
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_swizzle_mask[4];
    bool depth;
} ColorFormatInfo;

static const ColorFormatInfo kelvin_color_format_gl_map[66] = {
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_Y8] =
        {1, false, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_ONE}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_AY8] =
        {1, false, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_RED}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A1R5G5B5] =
        {2, false, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X1R5G5B5] =
        {2, false, GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A4R4G4B4] =
        {2, false, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5] =
        {2, false, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8] =
        {4, false, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8] =
        {4, false, GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},

    /* paletted texture */
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8] =
        {1, false, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},

    [NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT1_A1R5G5B5] =
        {4, false, GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, GL_RGBA},
    [NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT23_A8R8G8B8] =
        {4, false, GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, GL_RGBA},
    [NV097_SET_TEXTURE_FORMAT_COLOR_L_DXT45_A8R8G8B8] =
        {4, false, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, GL_RGBA},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A1R5G5B5] =
        {2, true, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5] =
        {2, true, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8] =
        {4, true, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y8] =
        {1, true, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_ONE}},

    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_G8B8] =
        {2, true, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
         {GL_RED, GL_GREEN, GL_RED, GL_GREEN}},

    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8] =
        {1, false, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_ONE, GL_ONE, GL_ONE, GL_RED}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8Y8] =
        {2, false, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_GREEN}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_AY8] =
        {1, true, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_RED}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5] =
        {2, true, GL_RGB5, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A4R4G4B4] =
        {2, true, GL_RGBA4, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8] =
        {4, true, GL_RGB8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8] =
        {1, true, GL_R8, GL_RED, GL_UNSIGNED_BYTE,
         {GL_ONE, GL_ONE, GL_ONE, GL_RED}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8Y8] =
        {2, true, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
         {GL_RED, GL_RED, GL_RED, GL_GREEN}},

    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5] =
        {2, false, GL_RGB8_SNORM, GL_RGB, GL_BYTE}, /* FIXME: This might be signed */
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_G8B8] =
        {2, false, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
         {GL_RED, GL_GREEN, GL_RED, GL_GREEN}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8B8] =
        {2, false, GL_RG8, GL_RG, GL_UNSIGNED_BYTE,
         {GL_GREEN, GL_RED, GL_RED, GL_GREEN}},

    [NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8] =
        {2, true, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8] =
        {2, true, GL_RGBA8,  GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},

    /* Additional information is passed to the pixel shader via the swizzle:
     * RED: The depth value.
     * GREEN: 0 for 16-bit, 1 for 24 bit
     * BLUE: 0 for fixed, 1 for float
     */
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_DEPTH_Y16_FIXED] =
        {2, false, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
         {GL_RED, GL_ZERO, GL_ZERO, GL_ZERO}, true},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FIXED] =
        {4, true, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
         {GL_RED, GL_ONE, GL_ZERO, GL_ZERO}, true},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_X8_Y24_FLOAT] =
        /* FIXME: Uses fixed-point format to match surface format hack below. */
        {4, true, GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
         {GL_RED, GL_ONE, GL_ZERO, GL_ZERO}, true},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FIXED] =
        {2, true, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT,
         {GL_RED, GL_ZERO, GL_ZERO, GL_ZERO}, true},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_DEPTH_Y16_FLOAT] =
        {2, true, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_HALF_FLOAT,
          {GL_RED, GL_ZERO, GL_ONE, GL_ZERO}, true},

    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_Y16] =
        {2, true, GL_R16, GL_RED, GL_UNSIGNED_SHORT,
         {GL_RED, GL_RED, GL_RED, GL_ONE}},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8B8G8R8] =
        {4, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_B8G8R8A8] =
        {4, false, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8},

    [NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R8G8B8A8] =
        {4, false, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},

    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8] =
        {4, true, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_B8G8R8A8] =
        {4, true, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8},
    [NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8] =
        {4, true, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8}
};

typedef struct SurfaceFormatInfo {
    unsigned int bytes_per_pixel;
    GLint gl_internal_format;
    GLenum gl_format;
    GLenum gl_type;
    GLenum gl_attachment;
} SurfaceFormatInfo;

static const SurfaceFormatInfo kelvin_surface_color_format_gl_map[] = {
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5] =
        {2, GL_RGB5_A1, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, GL_COLOR_ATTACHMENT0},
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5] =
        {2, GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, GL_COLOR_ATTACHMENT0},
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8] =
        {4, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_COLOR_ATTACHMENT0},
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8] =
        {4, GL_RGBA8, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, GL_COLOR_ATTACHMENT0},

    // FIXME: Map channel color
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_B8] =
        {1, GL_R8, GL_RED, GL_UNSIGNED_BYTE, GL_COLOR_ATTACHMENT0},
    [NV097_SET_SURFACE_FORMAT_COLOR_LE_G8B8] =
        {2, GL_RG8, GL_RG, GL_UNSIGNED_SHORT, GL_COLOR_ATTACHMENT0},
};

static const SurfaceFormatInfo kelvin_surface_zeta_float_format_gl_map[] = {
    [NV097_SET_SURFACE_FORMAT_ZETA_Z16] =
        {2, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, GL_DEPTH_ATTACHMENT},
    [NV097_SET_SURFACE_FORMAT_ZETA_Z24S8] =
        /* FIXME: GL does not support packing floating-point Z24S8 OOTB, so for
         *        now just emulate this with fixed-point Z24S8. Possible compat
         *        improvement with custom conversion.
         */
        {4, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT},
};

static const SurfaceFormatInfo kelvin_surface_zeta_fixed_format_gl_map[] = {
    [NV097_SET_SURFACE_FORMAT_ZETA_Z16] =
        {2, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_ATTACHMENT},
    [NV097_SET_SURFACE_FORMAT_ZETA_Z24S8] =
        {4, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT},
};

#endif
