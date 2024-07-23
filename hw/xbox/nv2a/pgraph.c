/*
 * QEMU Geforce NV2A implementation
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2021 Matt Borgerson
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

#include "nv2a_int.h"

#include <math.h>

#include "nv2a_vsh_emulator.h"
#include "s3tc.h"
#include "ui/xemu-settings.h"
#include "qemu/fast-hash.h"

const float f16_max = 511.9375f;
const float f24_max = 1.0E30;

static NV2AState *g_nv2a;
GloContext *g_nv2a_context_render;
GloContext *g_nv2a_context_display;

NV2AStats g_nv2a_stats;

static void nv2a_profile_increment(void)
{
    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    const int64_t fps_update_interval = 250000;
    g_nv2a_stats.last_flip_time = now;

    static int64_t frame_count = 0;
    frame_count++;

    static int64_t ts = 0;
    int64_t delta = now - ts;
    if (delta >= fps_update_interval) {
        g_nv2a_stats.increment_fps = frame_count * 1000000 / delta;
        ts = now;
        frame_count = 0;
    }
}

static void nv2a_profile_flip_stall(void)
{
    glFinish();

    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    int64_t render_time = (now-g_nv2a_stats.last_flip_time)/1000;

    g_nv2a_stats.frame_working.mspf = render_time;
    g_nv2a_stats.frame_history[g_nv2a_stats.frame_ptr] =
        g_nv2a_stats.frame_working;
    g_nv2a_stats.frame_ptr =
        (g_nv2a_stats.frame_ptr + 1) % NV2A_PROF_NUM_FRAMES;
    g_nv2a_stats.frame_count++;
    memset(&g_nv2a_stats.frame_working, 0, sizeof(g_nv2a_stats.frame_working));
}

static void nv2a_profile_inc_counter(enum NV2A_PROF_COUNTERS_ENUM cnt)
{
    g_nv2a_stats.frame_working.counters[cnt] += 1;
}

const char *nv2a_profile_get_counter_name(unsigned int cnt)
{
    const char *default_names[NV2A_PROF__COUNT] = {
        #define _X(x) stringify(x),
        NV2A_PROF_COUNTERS_XMAC
        #undef _X
    };

    assert(cnt < NV2A_PROF__COUNT);
    return default_names[cnt] + 10; /* 'NV2A_PROF_' */
}

int nv2a_profile_get_counter_value(unsigned int cnt)
{
    assert(cnt < NV2A_PROF__COUNT);
    unsigned int idx = (g_nv2a_stats.frame_ptr + NV2A_PROF_NUM_FRAMES - 1) %
                       NV2A_PROF_NUM_FRAMES;
    return g_nv2a_stats.frame_history[idx].counters[cnt];
}

static const GLenum pgraph_texture_min_filter_map[] = {
    0,
    GL_NEAREST,
    GL_LINEAR,
    GL_NEAREST_MIPMAP_NEAREST,
    GL_LINEAR_MIPMAP_NEAREST,
    GL_NEAREST_MIPMAP_LINEAR,
    GL_LINEAR_MIPMAP_LINEAR,
    GL_LINEAR,
};

static const GLenum pgraph_texture_mag_filter_map[] = {
    0,
    GL_NEAREST,
    GL_LINEAR,
    0,
    GL_LINEAR /* TODO: Convolution filter... */
};

static const GLenum pgraph_texture_addr_map[] = {
    0,
    GL_REPEAT,
    GL_MIRRORED_REPEAT,
    GL_CLAMP_TO_EDGE,
    GL_CLAMP_TO_BORDER,
    GL_CLAMP_TO_EDGE, /* Approximate GL_CLAMP */
};

static const GLenum pgraph_blend_factor_map[] = {
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

static const GLenum pgraph_blend_equation_map[] = {
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

static const GLenum pgraph_cull_face_map[] = {
    0,
    GL_FRONT,
    GL_BACK,
    GL_FRONT_AND_BACK
};

static const GLenum pgraph_depth_func_map[] = {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const GLenum pgraph_stencil_func_map[] = {
    GL_NEVER,
    GL_LESS,
    GL_EQUAL,
    GL_LEQUAL,
    GL_GREATER,
    GL_NOTEQUAL,
    GL_GEQUAL,
    GL_ALWAYS,
};

static const GLenum pgraph_stencil_op_map[] = {
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

static const ColorFormatInfo kelvin_color_format_map[66] = {
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

static const SurfaceFormatInfo kelvin_surface_color_format_map[] = {
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

static const SurfaceFormatInfo kelvin_surface_zeta_float_format_map[] = {
    [NV097_SET_SURFACE_FORMAT_ZETA_Z16] =
        {2, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_HALF_FLOAT, GL_DEPTH_ATTACHMENT},
    [NV097_SET_SURFACE_FORMAT_ZETA_Z24S8] =
        /* FIXME: GL does not support packing floating-point Z24S8 OOTB, so for
         *        now just emulate this with fixed-point Z24S8. Possible compat
         *        improvement with custom conversion.
         */
        {4, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT},
};

static const SurfaceFormatInfo kelvin_surface_zeta_fixed_format_map[] = {
    [NV097_SET_SURFACE_FORMAT_ZETA_Z16] =
        {2, GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, GL_DEPTH_ATTACHMENT},
    [NV097_SET_SURFACE_FORMAT_ZETA_Z24S8] =
        {4, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, GL_DEPTH_STENCIL_ATTACHMENT},
};


// static void pgraph_set_context_user(NV2AState *d, uint32_t val);
static void pgraph_gl_fence(void);
static GLuint pgraph_compile_shader(const char *vs_src, const char *fs_src);
static void pgraph_init_render_to_texture(NV2AState *d);
static void pgraph_init_display_renderer(NV2AState *d);
static void pgraph_method_log(unsigned int subchannel, unsigned int graphics_class, unsigned int method, uint32_t parameter);
static void pgraph_allocate_inline_buffer_vertices(PGRAPHState *pg, unsigned int attr);
static void pgraph_finish_inline_buffer_vertex(PGRAPHState *pg);
static void pgraph_shader_update_constants(PGRAPHState *pg, ShaderBinding *binding, bool binding_changed, bool vertex_program, bool fixed_function);
static void pgraph_bind_shaders(PGRAPHState *pg);
static bool pgraph_framebuffer_dirty(PGRAPHState *pg);
static bool pgraph_color_write_enabled(PGRAPHState *pg);
static bool pgraph_zeta_write_enabled(PGRAPHState *pg);
static void pgraph_set_surface_dirty(PGRAPHState *pg, bool color, bool zeta);
static void pgraph_wait_for_surface_download(SurfaceBinding *e);
static void pgraph_surface_access_callback(void *opaque, MemoryRegion *mr, hwaddr addr, hwaddr len, bool write);
static SurfaceBinding *pgraph_surface_put(NV2AState *d, hwaddr addr, SurfaceBinding *e);
static SurfaceBinding *pgraph_surface_get(NV2AState *d, hwaddr addr);
static SurfaceBinding *pgraph_surface_get_within(NV2AState *d, hwaddr addr);
static void pgraph_unbind_surface(NV2AState *d, bool color);
static void pgraph_surface_invalidate(NV2AState *d, SurfaceBinding *e);
static void pgraph_surface_evict_old(NV2AState *d);
static void pgraph_download_surface_data_if_dirty(NV2AState *d, SurfaceBinding *surface);
static void pgraph_download_surface_data(NV2AState *d, SurfaceBinding *surface, bool force);
static void pgraph_download_surface_data_to_buffer(NV2AState *d,
                                                   SurfaceBinding *surface,
                                                   bool swizzle, bool flip,
                                                   bool downscale,
                                                   uint8_t *pixels);
static void pgraph_upload_surface_data(NV2AState *d, SurfaceBinding *surface, bool force);
static bool pgraph_check_surface_compatibility(SurfaceBinding *s1, SurfaceBinding *s2, bool strict);
static bool pgraph_check_surface_to_texture_compatibility(const SurfaceBinding *surface, const TextureShape *shape);
static void pgraph_render_surface_to_texture(NV2AState *d, SurfaceBinding *surface, TextureBinding *texture, TextureShape *texture_shape, int texture_unit);
static void pgraph_update_surface_part(NV2AState *d, bool upload, bool color);
static void pgraph_update_surface(NV2AState *d, bool upload, bool color_write, bool zeta_write);
static void pgraph_bind_textures(NV2AState *d);
static void pgraph_apply_anti_aliasing_factor(PGRAPHState *pg, unsigned int *width, unsigned int *height);
static void pgraph_apply_scaling_factor(PGRAPHState *pg, unsigned int *width, unsigned int *height);
static void pgraph_get_surface_dimensions(PGRAPHState *pg, unsigned int *width, unsigned int *height);
static void pgraph_update_memory_buffer(NV2AState *d, hwaddr addr, hwaddr size, bool quick);
static void pgraph_bind_vertex_attributes(NV2AState *d, unsigned int min_element, unsigned int max_element, bool inline_data, unsigned int inline_stride, unsigned int provoking_element);
static unsigned int pgraph_bind_inline_array(NV2AState *d);
static bool pgraph_is_texture_stage_active(PGRAPHState *pg, unsigned int stage);

static float convert_f16_to_float(uint16_t f16);
static float convert_f24_to_float(uint32_t f24);
static uint8_t cliptobyte(int x);
static void convert_yuy2_to_rgb(const uint8_t *line, unsigned int ix, uint8_t *r, uint8_t *g, uint8_t* b);
static void convert_uyvy_to_rgb(const uint8_t *line, unsigned int ix, uint8_t *r, uint8_t *g, uint8_t* b);
static uint8_t* convert_texture_data(const TextureShape s, const uint8_t *data, const uint8_t *palette_data, unsigned int width, unsigned int height, unsigned int depth, unsigned int row_pitch, unsigned int slice_pitch);
static void upload_gl_texture(GLenum gl_target, const TextureShape s, const uint8_t *texture_data, const uint8_t *palette_data);
static TextureBinding* generate_texture(const TextureShape s, const uint8_t *texture_data, const uint8_t *palette_data);
static void texture_binding_destroy(gpointer data);
static void texture_cache_entry_init(Lru *lru, LruNode *node, void *key);
static void texture_cache_entry_post_evict(Lru *lru, LruNode *node);
static bool texture_cache_entry_compare(Lru *lru, LruNode *node, void *key);

static void vertex_cache_entry_init(Lru *lru, LruNode *node, void *key)
{
    VertexLruNode *vnode = container_of(node, VertexLruNode, node);
    memcpy(&vnode->key, key, sizeof(struct VertexKey));
    vnode->initialized = false;
}

static bool vertex_cache_entry_compare(Lru *lru, LruNode *node, void *key)
{
    VertexLruNode *vnode = container_of(node, VertexLruNode, node);
    return memcmp(&vnode->key, key, sizeof(VertexKey));
}

static void pgraph_mark_textures_possibly_dirty(NV2AState *d, hwaddr addr, hwaddr size);
static bool pgraph_check_texture_dirty(NV2AState *d, hwaddr addr, hwaddr size);
static unsigned int kelvin_map_stencil_op(uint32_t parameter);
static unsigned int kelvin_map_polygon_mode(uint32_t parameter);
static unsigned int kelvin_map_texgen(uint32_t parameter, unsigned int channel);
static void pgraph_reload_surface_scale_factor(NV2AState *d);

static uint32_t pgraph_rdi_read(PGRAPHState *pg,
                                unsigned int select, unsigned int address)
{
    uint32_t r = 0;
    switch(select) {
    case RDI_INDEX_VTX_CONSTANTS0:
    case RDI_INDEX_VTX_CONSTANTS1:
        assert((address / 4) < NV2A_VERTEXSHADER_CONSTANTS);
        r = pg->vsh_constants[address / 4][3 - address % 4];
        break;
    default:
        fprintf(stderr, "nv2a: unknown rdi read select 0x%x address 0x%x\n",
                select, address);
        assert(false);
        break;
    }
    return r;
}

static void pgraph_rdi_write(PGRAPHState *pg,
                             unsigned int select, unsigned int address,
                             uint32_t val)
{
    switch(select) {
    case RDI_INDEX_VTX_CONSTANTS0:
    case RDI_INDEX_VTX_CONSTANTS1:
        assert(false); /* Untested */
        assert((address / 4) < NV2A_VERTEXSHADER_CONSTANTS);
        pg->vsh_constants_dirty[address / 4] |=
            (val != pg->vsh_constants[address / 4][3 - address % 4]);
        pg->vsh_constants[address / 4][3 - address % 4] = val;
        break;
    default:
        NV2A_DPRINTF("unknown rdi write select 0x%x, address 0x%x, val 0x%08x\n",
                     select, address, val);
        break;
    }
}

uint64_t pgraph_read(void *opaque, hwaddr addr, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;
    PGRAPHState *pg = &d->pgraph;

    qemu_mutex_lock(&pg->lock);

    uint64_t r = 0;
    switch (addr) {
    case NV_PGRAPH_INTR:
        r = pg->pending_interrupts;
        break;
    case NV_PGRAPH_INTR_EN:
        r = pg->enabled_interrupts;
        break;
    case NV_PGRAPH_RDI_DATA: {
        unsigned int select = GET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                                       NV_PGRAPH_RDI_INDEX_SELECT);
        unsigned int address = GET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                                        NV_PGRAPH_RDI_INDEX_ADDRESS);

        r = pgraph_rdi_read(pg, select, address);

        /* FIXME: Overflow into select? */
        assert(address < GET_MASK(NV_PGRAPH_RDI_INDEX_ADDRESS,
                                  NV_PGRAPH_RDI_INDEX_ADDRESS));
        SET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                 NV_PGRAPH_RDI_INDEX_ADDRESS, address + 1);
        break;
    }
    default:
        r = pg->regs[addr];
        break;
    }

    qemu_mutex_unlock(&pg->lock);

    nv2a_reg_log_read(NV_PGRAPH, addr, size, r);
    return r;
}

void pgraph_write(void *opaque, hwaddr addr, uint64_t val, unsigned int size)
{
    NV2AState *d = (NV2AState *)opaque;
    PGRAPHState *pg = &d->pgraph;

    nv2a_reg_log_write(NV_PGRAPH, addr, size, val);

    qemu_mutex_lock(&d->pfifo.lock); // FIXME: Factor out fifo lock here
    qemu_mutex_lock(&pg->lock);

    switch (addr) {
    case NV_PGRAPH_INTR:
        pg->pending_interrupts &= ~val;

        if (!(pg->pending_interrupts & NV_PGRAPH_INTR_ERROR)) {
            pg->waiting_for_nop = false;
        }
        if (!(pg->pending_interrupts & NV_PGRAPH_INTR_CONTEXT_SWITCH)) {
            pg->waiting_for_context_switch = false;
        }
        pfifo_kick(d);
        break;
    case NV_PGRAPH_INTR_EN:
        pg->enabled_interrupts = val;
        break;
    case NV_PGRAPH_INCREMENT:
        if (val & NV_PGRAPH_INCREMENT_READ_3D) {
            SET_MASK(pg->regs[NV_PGRAPH_SURFACE],
                     NV_PGRAPH_SURFACE_READ_3D,
                     (GET_MASK(pg->regs[NV_PGRAPH_SURFACE],
                              NV_PGRAPH_SURFACE_READ_3D)+1)
                        % GET_MASK(pg->regs[NV_PGRAPH_SURFACE],
                                   NV_PGRAPH_SURFACE_MODULO_3D) );
            nv2a_profile_increment();
            pfifo_kick(d);
        }
        break;
    case NV_PGRAPH_RDI_DATA: {
        unsigned int select = GET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                                       NV_PGRAPH_RDI_INDEX_SELECT);
        unsigned int address = GET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                                        NV_PGRAPH_RDI_INDEX_ADDRESS);

        pgraph_rdi_write(pg, select, address, val);

        /* FIXME: Overflow into select? */
        assert(address < GET_MASK(NV_PGRAPH_RDI_INDEX_ADDRESS,
                                  NV_PGRAPH_RDI_INDEX_ADDRESS));
        SET_MASK(pg->regs[NV_PGRAPH_RDI_INDEX],
                 NV_PGRAPH_RDI_INDEX_ADDRESS, address + 1);
        break;
    }
    case NV_PGRAPH_CHANNEL_CTX_TRIGGER: {
        hwaddr context_address =
            GET_MASK(pg->regs[NV_PGRAPH_CHANNEL_CTX_POINTER],
                     NV_PGRAPH_CHANNEL_CTX_POINTER_INST) << 4;

        if (val & NV_PGRAPH_CHANNEL_CTX_TRIGGER_READ_IN) {
#ifdef DEBUG_NV2A
            unsigned pgraph_channel_id =
                GET_MASK(pg->regs[NV_PGRAPH_CTX_USER], NV_PGRAPH_CTX_USER_CHID);
#endif
            NV2A_DPRINTF("PGRAPH: read channel %d context from %" HWADDR_PRIx "\n",
                         pgraph_channel_id, context_address);

            assert(context_address < memory_region_size(&d->ramin));

            uint8_t *context_ptr = d->ramin_ptr + context_address;
            uint32_t context_user = ldl_le_p((uint32_t*)context_ptr);

            NV2A_DPRINTF("    - CTX_USER = 0x%x\n", context_user);

            pg->regs[NV_PGRAPH_CTX_USER] = context_user;
            // pgraph_set_context_user(d, context_user);
        }
        if (val & NV_PGRAPH_CHANNEL_CTX_TRIGGER_WRITE_OUT) {
            /* do stuff ... */
        }

        break;
    }
    default:
        pg->regs[addr] = val;
        break;
    }

    // events
    switch (addr) {
    case NV_PGRAPH_FIFO:
        pfifo_kick(d);
        break;
    }

    qemu_mutex_unlock(&pg->lock);
    qemu_mutex_unlock(&d->pfifo.lock);
}

void pgraph_flush(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;

    bool update_surface = (pg->color_binding || pg->zeta_binding);

    /* Clear last surface shape to force recreation of buffers at next draw */
    pg->surface_color.draw_dirty = false;
    pg->surface_zeta.draw_dirty = false;
    memset(&pg->last_surface_shape, 0, sizeof(pg->last_surface_shape));
    pgraph_unbind_surface(d, true);
    pgraph_unbind_surface(d, false);

    SurfaceBinding *s, *next;
    QTAILQ_FOREACH_SAFE(s, &d->pgraph.surfaces, entry, next) {
        pgraph_surface_invalidate(d, s);
    }

    pgraph_mark_textures_possibly_dirty(d, 0, memory_region_size(d->vram));

    /* Sync all RAM */
    glBindBuffer(GL_ARRAY_BUFFER, d->pgraph.gl_memory_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, memory_region_size(d->vram), d->vram_ptr);

    /* FIXME: Flush more? */

    pgraph_reload_surface_scale_factor(d);

    if (update_surface) {
        pgraph_update_surface(d, true, true, true);
    }

    qatomic_set(&d->pgraph.flush_pending, false);
    qemu_event_set(&d->pgraph.flush_complete);
}

#define METHOD_ADDR(gclass, name) \
    gclass ## _ ## name
#define METHOD_ADDR_TO_INDEX(x) ((x)>>2)
#define METHOD_NAME_STR(gclass, name) \
    tostring(gclass ## _ ## name)
#define METHOD_FUNC_NAME(gclass, name) \
    pgraph_ ## gclass ## _ ## name ## _handler
#define METHOD_HANDLER_ARG_DECL \
    NV2AState *d, PGRAPHState *pg, \
    unsigned int subchannel, unsigned int method, \
    uint32_t parameter, uint32_t *parameters, \
    size_t num_words_available, size_t *num_words_consumed, bool inc
#define METHOD_HANDLER_ARGS \
    d, pg, subchannel, method, parameter, parameters, \
    num_words_available, num_words_consumed, inc
#define DEF_METHOD_PROTO(gclass, name) \
    static void METHOD_FUNC_NAME(gclass, name)(METHOD_HANDLER_ARG_DECL)

#define DEF_METHOD(gclass, name) \
    DEF_METHOD_PROTO(gclass, name);
#define DEF_METHOD_RANGE(gclass, name, range) \
    DEF_METHOD_PROTO(gclass, name);
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) /* Drop */
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    DEF_METHOD_PROTO(gclass, name);
#include "pgraph_methods.h"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4

typedef void (*MethodFunc)(METHOD_HANDLER_ARG_DECL);
static const struct {
    uint32_t base;
    const char *name;
    MethodFunc handler;
} pgraph_kelvin_methods[0x800] = {
#define DEF_METHOD(gclass, name)                        \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name))] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_RANGE(gclass, name, range) \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name)) \
     ... METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + 4*range - 1)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride * 2)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    }, \
    [METHOD_ADDR_TO_INDEX(METHOD_ADDR(gclass, name) + offset + stride * 3)] = \
    { \
        METHOD_ADDR(gclass, name), \
        METHOD_NAME_STR(gclass, name), \
        METHOD_FUNC_NAME(gclass, name), \
    },
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    DEF_METHOD_CASE_4_OFFSET(gclass, name, 0, stride)
#include "pgraph_methods.h"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4
};

#define METHOD_RANGE_END_NAME(gclass, name) \
    pgraph_ ## gclass ## _ ## name ## __END
#define DEF_METHOD(gclass, name) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4;
#define DEF_METHOD_RANGE(gclass, name, range) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4*range;
#define DEF_METHOD_CASE_4_OFFSET(gclass, name, offset, stride) /* drop */
#define DEF_METHOD_CASE_4(gclass, name, stride) \
    static const size_t METHOD_RANGE_END_NAME(gclass, name) = \
        METHOD_ADDR(gclass, name) + 4*stride;
#include "pgraph_methods.h"
#undef DEF_METHOD
#undef DEF_METHOD_RANGE
#undef DEF_METHOD_CASE_4_OFFSET
#undef DEF_METHOD_CASE_4

static void pgraph_method_inc(MethodFunc handler, uint32_t end,
                              METHOD_HANDLER_ARG_DECL)
{
    if (!inc) {
        handler(METHOD_HANDLER_ARGS);
        return;
    }
    size_t count = MIN(num_words_available, (end - method) / 4);
    for (size_t i = 0; i < count; i++) {
        parameter = ldl_le_p(parameters + i);
        if (i) {
            pgraph_method_log(subchannel, NV_KELVIN_PRIMITIVE, method,
                              parameter);
        }
        handler(METHOD_HANDLER_ARGS);
        method += 4;
    }
    *num_words_consumed = count;
}

static void pgraph_method_non_inc(MethodFunc handler, METHOD_HANDLER_ARG_DECL)
{
    if (inc) {
        handler(METHOD_HANDLER_ARGS);
        return;
    }

    for (size_t i = 0; i < num_words_available; i++) {
        parameter = ldl_le_p(parameters + i);
        if (i) {
            pgraph_method_log(subchannel, NV_KELVIN_PRIMITIVE, method,
                              parameter);
        }
        handler(METHOD_HANDLER_ARGS);
    }
    *num_words_consumed = num_words_available;
}

#define METHOD_FUNC_NAME_INT(gclass, name) METHOD_FUNC_NAME(gclass, name##_int)
#define DEF_METHOD_INT(gclass, name) DEF_METHOD(gclass, name##_int)
#define DEF_METHOD(gclass, name) DEF_METHOD_PROTO(gclass, name)

#define DEF_METHOD_INC(gclass, name)                           \
    DEF_METHOD_INT(gclass, name);                              \
    DEF_METHOD(gclass, name)                                   \
    {                                                          \
        pgraph_method_inc(METHOD_FUNC_NAME_INT(gclass, name),  \
                          METHOD_RANGE_END_NAME(gclass, name), \
                          METHOD_HANDLER_ARGS);                \
    }                                                          \
    DEF_METHOD_INT(gclass, name)

#define DEF_METHOD_NON_INC(gclass, name)                          \
    DEF_METHOD_INT(gclass, name);                                 \
    DEF_METHOD(gclass, name)                                      \
    {                                                             \
        pgraph_method_non_inc(METHOD_FUNC_NAME_INT(gclass, name), \
                              METHOD_HANDLER_ARGS);               \
    }                                                             \
    DEF_METHOD_INT(gclass, name)

// TODO: Optimize. Ideally this should all be done via OpenGL.
static void pgraph_image_blit(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    ContextSurfaces2DState *context_surfaces = &pg->context_surfaces_2d;
    ImageBlitState *image_blit = &pg->image_blit;
    BetaState *beta = &pg->beta;

    pgraph_update_surface(d, false, true, true);

    assert(context_surfaces->object_instance == image_blit->context_surfaces);

    unsigned int bytes_per_pixel;
    switch (context_surfaces->color_format) {
        case NV062_SET_COLOR_FORMAT_LE_Y8:
            bytes_per_pixel = 1;
            break;
        case NV062_SET_COLOR_FORMAT_LE_R5G6B5:
            bytes_per_pixel = 2;
            break;
        case NV062_SET_COLOR_FORMAT_LE_A8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8_Z8R8G8B8:
        case NV062_SET_COLOR_FORMAT_LE_Y32:
            bytes_per_pixel = 4;
            break;
        default:
            fprintf(stderr, "Unknown blit surface format: 0x%x\n",
                    context_surfaces->color_format);
            assert(false);
            break;
    }

    hwaddr source_dma_len, dest_dma_len;

    uint8_t *source = (uint8_t *)nv_dma_map(
        d, context_surfaces->dma_image_source, &source_dma_len);
    assert(context_surfaces->source_offset < source_dma_len);
    source += context_surfaces->source_offset;

    uint8_t *dest = (uint8_t *)nv_dma_map(d, context_surfaces->dma_image_dest,
                                          &dest_dma_len);
    assert(context_surfaces->dest_offset < dest_dma_len);
    dest += context_surfaces->dest_offset;

    hwaddr source_addr = source - d->vram_ptr;
    hwaddr dest_addr = dest - d->vram_ptr;

    SurfaceBinding *surf_src = pgraph_surface_get(d, source_addr);
    if (surf_src) {
        pgraph_download_surface_data_if_dirty(d, surf_src);
    }

    SurfaceBinding *surf_dest = pgraph_surface_get(d, dest_addr);
    if (surf_dest) {
        if (image_blit->height < surf_dest->height ||
            image_blit->width < surf_dest->width) {
            pgraph_download_surface_data_if_dirty(d, surf_dest);
        } else {
            // The blit will completely replace the surface so any pending
            // download should be discarded.
            surf_dest->download_pending = false;
            surf_dest->draw_dirty = false;
        }
        surf_dest->upload_pending = true;
        pg->draw_time++;
    }

    hwaddr source_offset = image_blit->in_y * context_surfaces->source_pitch +
                           image_blit->in_x * bytes_per_pixel;
    hwaddr dest_offset = image_blit->out_y * context_surfaces->dest_pitch +
                         image_blit->out_x * bytes_per_pixel;

    hwaddr source_size =
        (image_blit->height - 1) * context_surfaces->source_pitch +
        image_blit->width * bytes_per_pixel;
    hwaddr dest_size = (image_blit->height - 1) * context_surfaces->dest_pitch +
                       image_blit->width * bytes_per_pixel;

    /* FIXME: What does hardware do in this case? */
    assert(source_addr + source_offset + source_size <=
           memory_region_size(d->vram));
    assert(dest_addr + dest_offset + dest_size <= memory_region_size(d->vram));

    uint8_t *source_row = source + source_offset;
    uint8_t *dest_row = dest + dest_offset;

    if (image_blit->operation == NV09F_SET_OPERATION_SRCCOPY) {
        NV2A_GL_DPRINTF(false, "NV09F_SET_OPERATION_SRCCOPY");
        for (unsigned int y = 0; y < image_blit->height; y++) {
            memmove(dest_row, source_row, image_blit->width * bytes_per_pixel);
            source_row += context_surfaces->source_pitch;
            dest_row += context_surfaces->dest_pitch;
        }
    } else if (image_blit->operation == NV09F_SET_OPERATION_BLEND_AND) {
        NV2A_GL_DPRINTF(false, "NV09F_SET_OPERATION_BLEND_AND");
        uint32_t max_beta_mult = 0x7f80;
        uint32_t beta_mult = beta->beta >> 16;
        uint32_t inv_beta_mult = max_beta_mult - beta_mult;
        for (unsigned int y = 0; y < image_blit->height; y++) {
            for (unsigned int x = 0; x < image_blit->width; x++) {
                for (unsigned int ch = 0; ch < 3; ch++) {
                    uint32_t a = source_row[x * 4 + ch] * beta_mult;
                    uint32_t b = dest_row[x * 4 + ch] * inv_beta_mult;
                    dest_row[x * 4 + ch] = (a + b) / max_beta_mult;
                }
            }
            source_row += context_surfaces->source_pitch;
            dest_row += context_surfaces->dest_pitch;
        }
    } else {
        fprintf(stderr, "Unknown blit operation: 0x%x\n",
                image_blit->operation);
        assert(false && "Unknown blit operation");
    }

    NV2A_DPRINTF("  - 0x%tx -> 0x%tx\n", source_addr, dest_addr);

    bool needs_alpha_patching;
    uint8_t alpha_override;
    switch (context_surfaces->color_format) {
    case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8:
        needs_alpha_patching = true;
        alpha_override = 0xff;
        break;
    case NV062_SET_COLOR_FORMAT_LE_X8R8G8B8_Z8R8G8B8:
        needs_alpha_patching = true;
        alpha_override = 0;
        break;
    default:
        needs_alpha_patching = false;
        alpha_override = 0;
    }

    if (needs_alpha_patching) {
        dest_row = dest + dest_offset;
        for (unsigned int y = 0; y < image_blit->height; y++) {
            for (unsigned int x = 0; x < image_blit->width; x++) {
                dest_row[x * 4 + 3] = alpha_override;
            }
            dest_row += context_surfaces->dest_pitch;
        }
    }

    dest_addr += dest_offset;
    memory_region_set_client_dirty(d->vram, dest_addr, dest_size,
                                   DIRTY_MEMORY_VGA);
    memory_region_set_client_dirty(d->vram, dest_addr, dest_size,
                                   DIRTY_MEMORY_NV2A_TEX);
}

int pgraph_method(NV2AState *d, unsigned int subchannel,
                   unsigned int method, uint32_t parameter,
                   uint32_t *parameters, size_t num_words_available,
                   size_t max_lookahead_words, bool inc)
{
    int num_processed = 1;

    assert(glGetError() == GL_NO_ERROR);

    PGRAPHState *pg = &d->pgraph;

    bool channel_valid =
        d->pgraph.regs[NV_PGRAPH_CTX_CONTROL] & NV_PGRAPH_CTX_CONTROL_CHID;
    assert(channel_valid);

    ContextSurfaces2DState *context_surfaces_2d = &pg->context_surfaces_2d;
    ImageBlitState *image_blit = &pg->image_blit;
    BetaState *beta = &pg->beta;

    assert(subchannel < 8);

    if (method == NV_SET_OBJECT) {
        assert(parameter < memory_region_size(&d->ramin));
        uint8_t *obj_ptr = d->ramin_ptr + parameter;

        uint32_t ctx_1 = ldl_le_p((uint32_t*)obj_ptr);
        uint32_t ctx_2 = ldl_le_p((uint32_t*)(obj_ptr+4));
        uint32_t ctx_3 = ldl_le_p((uint32_t*)(obj_ptr+8));
        uint32_t ctx_4 = ldl_le_p((uint32_t*)(obj_ptr+12));
        uint32_t ctx_5 = parameter;

        pg->regs[NV_PGRAPH_CTX_CACHE1 + subchannel * 4] = ctx_1;
        pg->regs[NV_PGRAPH_CTX_CACHE2 + subchannel * 4] = ctx_2;
        pg->regs[NV_PGRAPH_CTX_CACHE3 + subchannel * 4] = ctx_3;
        pg->regs[NV_PGRAPH_CTX_CACHE4 + subchannel * 4] = ctx_4;
        pg->regs[NV_PGRAPH_CTX_CACHE5 + subchannel * 4] = ctx_5;
    }

    // is this right?
    pg->regs[NV_PGRAPH_CTX_SWITCH1] = pg->regs[NV_PGRAPH_CTX_CACHE1 + subchannel * 4];
    pg->regs[NV_PGRAPH_CTX_SWITCH2] = pg->regs[NV_PGRAPH_CTX_CACHE2 + subchannel * 4];
    pg->regs[NV_PGRAPH_CTX_SWITCH3] = pg->regs[NV_PGRAPH_CTX_CACHE3 + subchannel * 4];
    pg->regs[NV_PGRAPH_CTX_SWITCH4] = pg->regs[NV_PGRAPH_CTX_CACHE4 + subchannel * 4];
    pg->regs[NV_PGRAPH_CTX_SWITCH5] = pg->regs[NV_PGRAPH_CTX_CACHE5 + subchannel * 4];

    uint32_t graphics_class = GET_MASK(pg->regs[NV_PGRAPH_CTX_SWITCH1],
                                       NV_PGRAPH_CTX_SWITCH1_GRCLASS);

    pgraph_method_log(subchannel, graphics_class, method, parameter);

    if (subchannel != 0) {
        // catches context switching issues on xbox d3d
        assert(graphics_class != 0x97);
    }

    /* ugly switch for now */
    switch (graphics_class) {
    case NV_BETA: {
        switch (method) {
        case NV012_SET_OBJECT:
            beta->object_instance = parameter;
            break;
        case NV012_SET_BETA:
            if (parameter & 0x80000000) {
                beta->beta = 0;
            } else {
                // The parameter is a signed fixed-point number with a sign bit
                // and 31 fractional bits. Note that negative values are clamped
                // to 0, and only 8 fractional bits are actually implemented in
                // hardware.
                beta->beta = parameter & 0x7f800000;
            }
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_CONTEXT_PATTERN: {
        switch (method) {
        case NV044_SET_MONOCHROME_COLOR0:
            pg->regs[NV_PGRAPH_PATT_COLOR0] = parameter;
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_CONTEXT_SURFACES_2D: {
        switch (method) {
        case NV062_SET_OBJECT:
            context_surfaces_2d->object_instance = parameter;
            break;
        case NV062_SET_CONTEXT_DMA_IMAGE_SOURCE:
            context_surfaces_2d->dma_image_source = parameter;
            break;
        case NV062_SET_CONTEXT_DMA_IMAGE_DESTIN:
            context_surfaces_2d->dma_image_dest = parameter;
            break;
        case NV062_SET_COLOR_FORMAT:
            context_surfaces_2d->color_format = parameter;
            break;
        case NV062_SET_PITCH:
            context_surfaces_2d->source_pitch = parameter & 0xFFFF;
            context_surfaces_2d->dest_pitch = parameter >> 16;
            break;
        case NV062_SET_OFFSET_SOURCE:
            context_surfaces_2d->source_offset = parameter & 0x07FFFFFF;
            break;
        case NV062_SET_OFFSET_DESTIN:
            context_surfaces_2d->dest_offset = parameter & 0x07FFFFFF;
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_IMAGE_BLIT: {
        switch (method) {
        case NV09F_SET_OBJECT:
            image_blit->object_instance = parameter;
            break;
        case NV09F_SET_CONTEXT_SURFACES:
            image_blit->context_surfaces = parameter;
            break;
        case NV09F_SET_OPERATION:
            image_blit->operation = parameter;
            break;
        case NV09F_CONTROL_POINT_IN:
            image_blit->in_x = parameter & 0xFFFF;
            image_blit->in_y = parameter >> 16;
            break;
        case NV09F_CONTROL_POINT_OUT:
            image_blit->out_x = parameter & 0xFFFF;
            image_blit->out_y = parameter >> 16;
            break;
        case NV09F_SIZE:
            image_blit->width = parameter & 0xFFFF;
            image_blit->height = parameter >> 16;

            if (image_blit->width && image_blit->height) {
                pgraph_image_blit(d);
            }
            break;
        default:
            goto unhandled;
        }
        break;
    }
    case NV_KELVIN_PRIMITIVE: {
        MethodFunc handler =
            pgraph_kelvin_methods[METHOD_ADDR_TO_INDEX(method)].handler;
        if (handler == NULL) {
            goto unhandled;
        }
        size_t num_words_consumed = 1;
        handler(d, pg, subchannel, method, parameter, parameters,
                num_words_available, &num_words_consumed, inc);

        /* Squash repeated BEGIN,DRAW_ARRAYS,END */
        #define LAM(i, mthd) ((parameters[i*2+1] & 0x31fff) == (mthd))
        #define LAP(i, prm) (parameters[i*2+2] == (prm))
        #define LAMP(i, mthd, prm) (LAM(i, mthd) && LAP(i, prm))

        if (method == NV097_DRAW_ARRAYS && (max_lookahead_words >= 7) &&
            pg->inline_elements_length == 0 &&
            pg->draw_arrays_length <
                (ARRAY_SIZE(pg->gl_draw_arrays_start) - 1) &&
            LAMP(0, NV097_SET_BEGIN_END, NV097_SET_BEGIN_END_OP_END) &&
            LAMP(1, NV097_SET_BEGIN_END, pg->primitive_mode) &&
            LAM(2, NV097_DRAW_ARRAYS)) {
            num_words_consumed += 4;
            pg->draw_arrays_prevent_connect = true;
        }

        #undef LAM
        #undef LAP
        #undef LAMP

        num_processed = num_words_consumed;
        break;
    }
    default:
        goto unhandled;
    }

    return num_processed;

unhandled:
    trace_nv2a_pgraph_method_unhandled(subchannel, graphics_class,
                                           method, parameter);
    return num_processed;
}

DEF_METHOD(NV097, SET_OBJECT)
{
    pg->kelvin.object_instance = parameter;
}

DEF_METHOD(NV097, NO_OPERATION)
{
    /* The bios uses nop as a software method call -
     * it seems to expect a notify interrupt if the parameter isn't 0.
     * According to a nouveau guy it should still be a nop regardless
     * of the parameter. It's possible a debug register enables this,
     * but nothing obvious sticks out. Weird.
     */
    if (parameter == 0) {
        return;
    }

    unsigned channel_id =
        GET_MASK(pg->regs[NV_PGRAPH_CTX_USER], NV_PGRAPH_CTX_USER_CHID);

    assert(!(pg->pending_interrupts & NV_PGRAPH_INTR_ERROR));

    SET_MASK(pg->regs[NV_PGRAPH_TRAPPED_ADDR], NV_PGRAPH_TRAPPED_ADDR_CHID,
             channel_id);
    SET_MASK(pg->regs[NV_PGRAPH_TRAPPED_ADDR], NV_PGRAPH_TRAPPED_ADDR_SUBCH,
             subchannel);
    SET_MASK(pg->regs[NV_PGRAPH_TRAPPED_ADDR], NV_PGRAPH_TRAPPED_ADDR_MTHD,
             method);
    pg->regs[NV_PGRAPH_TRAPPED_DATA_LOW] = parameter;
    pg->regs[NV_PGRAPH_NSOURCE] =
        NV_PGRAPH_NSOURCE_NOTIFICATION; /* TODO: check this */
    pg->pending_interrupts |= NV_PGRAPH_INTR_ERROR;
    pg->waiting_for_nop = true;

    qemu_mutex_unlock(&pg->lock);
    qemu_mutex_lock_iothread();
    nv2a_update_irq(d);
    qemu_mutex_unlock_iothread();
    qemu_mutex_lock(&pg->lock);
}

DEF_METHOD(NV097, WAIT_FOR_IDLE)
{
    pgraph_update_surface(d, false, true, true);
}

DEF_METHOD(NV097, SET_FLIP_READ)
{
    SET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_READ_3D,
             parameter);
}

DEF_METHOD(NV097, SET_FLIP_WRITE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_WRITE_3D,
             parameter);
}

DEF_METHOD(NV097, SET_FLIP_MODULO)
{
    SET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_MODULO_3D,
             parameter);
}

DEF_METHOD(NV097, FLIP_INCREMENT_WRITE)
{
    uint32_t old =
        GET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_WRITE_3D);

    SET_MASK(pg->regs[NV_PGRAPH_SURFACE],
             NV_PGRAPH_SURFACE_WRITE_3D,
             (GET_MASK(pg->regs[NV_PGRAPH_SURFACE],
                      NV_PGRAPH_SURFACE_WRITE_3D)+1)
                % GET_MASK(pg->regs[NV_PGRAPH_SURFACE],
                           NV_PGRAPH_SURFACE_MODULO_3D) );

    uint32_t new =
        GET_MASK(pg->regs[NV_PGRAPH_SURFACE], NV_PGRAPH_SURFACE_WRITE_3D);

    trace_nv2a_pgraph_flip_increment_write(old, new);
    NV2A_GL_DFRAME_TERMINATOR();
    pg->frame_time++;
}

DEF_METHOD(NV097, FLIP_STALL)
{
    trace_nv2a_pgraph_flip_stall();
    pgraph_update_surface(d, false, true, true);
    nv2a_profile_flip_stall();
    pg->waiting_for_flip = true;
}

// TODO: these should be loading the dma objects from ramin here?

DEF_METHOD(NV097, SET_CONTEXT_DMA_NOTIFIES)
{
    pg->dma_notifies = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_A)
{
    pg->dma_a = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_B)
{
    pg->dma_b = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_STATE)
{
    pg->dma_state = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_COLOR)
{
    /* try to get any straggling draws in before the surface's changed :/ */
    pgraph_update_surface(d, false, true, true);

    pg->dma_color = parameter;
    pg->surface_color.buffer_dirty = true;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_ZETA)
{
    pg->dma_zeta = parameter;
    pg->surface_zeta.buffer_dirty = true;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_VERTEX_A)
{
    pg->dma_vertex_a = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_VERTEX_B)
{
    pg->dma_vertex_b = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_SEMAPHORE)
{
    pg->dma_semaphore = parameter;
}

DEF_METHOD(NV097, SET_CONTEXT_DMA_REPORT)
{
    pgraph_process_pending_reports(d);

    pg->dma_report = parameter;
}

DEF_METHOD(NV097, SET_SURFACE_CLIP_HORIZONTAL)
{
    pgraph_update_surface(d, false, true, true);

    pg->surface_shape.clip_x =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_HORIZONTAL_X);
    pg->surface_shape.clip_width =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_HORIZONTAL_WIDTH);
}

DEF_METHOD(NV097, SET_SURFACE_CLIP_VERTICAL)
{
    pgraph_update_surface(d, false, true, true);

    pg->surface_shape.clip_y =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_VERTICAL_Y);
    pg->surface_shape.clip_height =
        GET_MASK(parameter, NV097_SET_SURFACE_CLIP_VERTICAL_HEIGHT);
}

DEF_METHOD(NV097, SET_SURFACE_FORMAT)
{
    pgraph_update_surface(d, false, true, true);

    pg->surface_shape.color_format =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_COLOR);
    pg->surface_shape.zeta_format =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_ZETA);
    pg->surface_shape.anti_aliasing =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_ANTI_ALIASING);
    pg->surface_shape.log_width =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_WIDTH);
    pg->surface_shape.log_height =
        GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_HEIGHT);

    int surface_type = GET_MASK(parameter, NV097_SET_SURFACE_FORMAT_TYPE);
    if (surface_type != pg->surface_type) {
        pg->surface_type = surface_type;
        pg->surface_color.buffer_dirty = true;
        pg->surface_zeta.buffer_dirty = true;
    }
}

DEF_METHOD(NV097, SET_SURFACE_PITCH)
{
    pgraph_update_surface(d, false, true, true);
    unsigned int color_pitch = GET_MASK(parameter, NV097_SET_SURFACE_PITCH_COLOR);
    unsigned int zeta_pitch  = GET_MASK(parameter, NV097_SET_SURFACE_PITCH_ZETA);

    pg->surface_color.buffer_dirty |= (pg->surface_color.pitch != color_pitch);
    pg->surface_color.pitch = color_pitch;

    pg->surface_zeta.buffer_dirty |= (pg->surface_zeta.pitch != zeta_pitch);
    pg->surface_zeta.pitch = zeta_pitch;
}

DEF_METHOD(NV097, SET_SURFACE_COLOR_OFFSET)
{
    pgraph_update_surface(d, false, true, true);
    pg->surface_color.buffer_dirty |= (pg->surface_color.offset != parameter);
    pg->surface_color.offset = parameter;
}

DEF_METHOD(NV097, SET_SURFACE_ZETA_OFFSET)
{
    pgraph_update_surface(d, false, true, true);
    pg->surface_zeta.buffer_dirty |= (pg->surface_zeta.offset != parameter);
    pg->surface_zeta.offset = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_ALPHA_ICW)
{
    int slot = (method - NV097_SET_COMBINER_ALPHA_ICW) / 4;
    pg->regs[NV_PGRAPH_COMBINEALPHAI0 + slot*4] = parameter;
}

DEF_METHOD(NV097, SET_COMBINER_SPECULAR_FOG_CW0)
{
    pg->regs[NV_PGRAPH_COMBINESPECFOG0] = parameter;
}

DEF_METHOD(NV097, SET_COMBINER_SPECULAR_FOG_CW1)
{
    pg->regs[NV_PGRAPH_COMBINESPECFOG1] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_ADDRESS)
{
    int slot = (method - NV097_SET_TEXTURE_ADDRESS) / 64;
    pg->regs[NV_PGRAPH_TEXADDRESS0 + slot * 4] = parameter;
}

DEF_METHOD(NV097, SET_CONTROL0)
{
    pgraph_update_surface(d, false, true, true);

    bool stencil_write_enable =
        parameter & NV097_SET_CONTROL0_STENCIL_WRITE_ENABLE;
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE,
             stencil_write_enable);

    uint32_t z_format = GET_MASK(parameter, NV097_SET_CONTROL0_Z_FORMAT);
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_Z_FORMAT, z_format);

    bool z_perspective =
        parameter & NV097_SET_CONTROL0_Z_PERSPECTIVE_ENABLE;
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE,
             z_perspective);
}

DEF_METHOD(NV097, SET_COLOR_MATERIAL)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_EMISSION,
             (parameter >> 0) & 3);
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_AMBIENT,
             (parameter >> 2) & 3);
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_DIFFUSE,
             (parameter >> 4) & 3);
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_SPECULAR,
             (parameter >> 6) & 3);
}

DEF_METHOD(NV097, SET_FOG_MODE)
{
    /* FIXME: There is also NV_PGRAPH_CSV0_D_FOG_MODE */
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FOG_MODE_V_LINEAR:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_LINEAR; break;
    case NV097_SET_FOG_MODE_V_EXP:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP; break;
    case NV097_SET_FOG_MODE_V_EXP2:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP2; break;
    case NV097_SET_FOG_MODE_V_EXP_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP_ABS; break;
    case NV097_SET_FOG_MODE_V_EXP2_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_EXP2_ABS; break;
    case NV097_SET_FOG_MODE_V_LINEAR_ABS:
        mode = NV_PGRAPH_CONTROL_3_FOG_MODE_LINEAR_ABS; break;
    default:
        assert(false);
        break;
    }
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3], NV_PGRAPH_CONTROL_3_FOG_MODE,
             mode);
}

DEF_METHOD(NV097, SET_FOG_GEN_MODE)
{
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FOG_GEN_MODE_V_SPEC_ALPHA:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_SPEC_ALPHA; break;
    case NV097_SET_FOG_GEN_MODE_V_RADIAL:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_RADIAL; break;
    case NV097_SET_FOG_GEN_MODE_V_PLANAR:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_PLANAR; break;
    case NV097_SET_FOG_GEN_MODE_V_ABS_PLANAR:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_ABS_PLANAR; break;
    case NV097_SET_FOG_GEN_MODE_V_FOG_X:
        mode = NV_PGRAPH_CSV0_D_FOGGENMODE_FOG_X; break;
    default:
        assert(false);
        break;
    }
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_FOGGENMODE, mode);
}

DEF_METHOD(NV097, SET_FOG_ENABLE)
{
    /*
      FIXME: There is also:
        SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_FOGENABLE,
             parameter);
    */
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3], NV_PGRAPH_CONTROL_3_FOGENABLE,
         parameter);
}

DEF_METHOD(NV097, SET_FOG_COLOR)
{
    /* PGRAPH channels are ARGB, parameter channels are ABGR */
    uint8_t red = GET_MASK(parameter, NV097_SET_FOG_COLOR_RED);
    uint8_t green = GET_MASK(parameter, NV097_SET_FOG_COLOR_GREEN);
    uint8_t blue = GET_MASK(parameter, NV097_SET_FOG_COLOR_BLUE);
    uint8_t alpha = GET_MASK(parameter, NV097_SET_FOG_COLOR_ALPHA);
    SET_MASK(pg->regs[NV_PGRAPH_FOGCOLOR], NV_PGRAPH_FOGCOLOR_RED, red);
    SET_MASK(pg->regs[NV_PGRAPH_FOGCOLOR], NV_PGRAPH_FOGCOLOR_GREEN, green);
    SET_MASK(pg->regs[NV_PGRAPH_FOGCOLOR], NV_PGRAPH_FOGCOLOR_BLUE, blue);
    SET_MASK(pg->regs[NV_PGRAPH_FOGCOLOR], NV_PGRAPH_FOGCOLOR_ALPHA, alpha);
}

DEF_METHOD(NV097, SET_WINDOW_CLIP_TYPE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE, parameter);
}

DEF_METHOD_INC(NV097, SET_WINDOW_CLIP_HORIZONTAL)
{
    int slot = (method - NV097_SET_WINDOW_CLIP_HORIZONTAL) / 4;
    for (; slot < 8; ++slot) {
        pg->regs[NV_PGRAPH_WINDOWCLIPX0 + slot * 4] = parameter;
    }
}

DEF_METHOD_INC(NV097, SET_WINDOW_CLIP_VERTICAL)
{
    int slot = (method - NV097_SET_WINDOW_CLIP_VERTICAL) / 4;
    for (; slot < 8; ++slot) {
        pg->regs[NV_PGRAPH_WINDOWCLIPY0 + slot * 4] = parameter;
    }
}

DEF_METHOD(NV097, SET_ALPHA_TEST_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_ALPHATESTENABLE, parameter);
}

DEF_METHOD(NV097, SET_BLEND_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_EN, parameter);
}

DEF_METHOD(NV097, SET_CULL_FACE_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_CULLENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_DEPTH_TEST_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0], NV_PGRAPH_CONTROL_0_ZENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_DITHER_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_DITHERENABLE, parameter);
}

DEF_METHOD(NV097, SET_LIGHTING_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_LIGHTING,
             parameter);
}

DEF_METHOD(NV097, SET_POINT_PARAMS_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_POINTPARAMSENABLE,
             parameter);
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3],
             NV_PGRAPH_CONTROL_3_POINTPARAMSENABLE, parameter);
}

DEF_METHOD(NV097, SET_POINT_SMOOTH_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_POINTSMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_LINE_SMOOTH_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_LINESMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_SMOOTH_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE, parameter);
}

DEF_METHOD(NV097, SET_SKIN_MODE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_SKIN,
             parameter);
}

DEF_METHOD(NV097, SET_STENCIL_TEST_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
             NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_POINT_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_LINE_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE, parameter);
}

DEF_METHOD(NV097, SET_POLY_OFFSET_FILL_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE, parameter);
}

DEF_METHOD(NV097, SET_ALPHA_FUNC)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_ALPHAFUNC, parameter & 0xF);
}

DEF_METHOD(NV097, SET_ALPHA_REF)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_ALPHAREF, parameter);
}

DEF_METHOD(NV097, SET_BLEND_FUNC_SFACTOR)
{
    unsigned int factor;
    switch (parameter) {
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ZERO:
        factor = NV_PGRAPH_BLEND_SFACTOR_ZERO; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_DST_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_DST_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_SRC_ALPHA_SATURATE:
        factor = NV_PGRAPH_BLEND_SFACTOR_SRC_ALPHA_SATURATE; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_CONSTANT_ALPHA; break;
    case NV097_SET_BLEND_FUNC_SFACTOR_V_ONE_MINUS_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_SFACTOR_ONE_MINUS_CONSTANT_ALPHA; break;
    default:
        NV2A_DPRINTF("Unknown blend source factor: 0x%08x\n", parameter);
        return; /* discard */
    }
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_SFACTOR, factor);
}

DEF_METHOD(NV097, SET_BLEND_FUNC_DFACTOR)
{
    unsigned int factor;
    switch (parameter) {
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ZERO:
        factor = NV_PGRAPH_BLEND_DFACTOR_ZERO; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_SRC_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_SRC_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_SRC_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_DST_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_DST_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_DST_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_DST_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_DST_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_SRC_ALPHA_SATURATE:
        factor = NV_PGRAPH_BLEND_DFACTOR_SRC_ALPHA_SATURATE; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_CONSTANT_COLOR:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_CONSTANT_COLOR; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_CONSTANT_ALPHA; break;
    case NV097_SET_BLEND_FUNC_DFACTOR_V_ONE_MINUS_CONSTANT_ALPHA:
        factor = NV_PGRAPH_BLEND_DFACTOR_ONE_MINUS_CONSTANT_ALPHA; break;
    default:
        NV2A_DPRINTF("Unknown blend destination factor: 0x%08x\n", parameter);
        return; /* discard */
    }
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_DFACTOR, factor);
}

DEF_METHOD(NV097, SET_BLEND_COLOR)
{
    pg->regs[NV_PGRAPH_BLENDCOLOR] = parameter;
}

DEF_METHOD(NV097, SET_BLEND_EQUATION)
{
    unsigned int equation;
    switch (parameter) {
    case NV097_SET_BLEND_EQUATION_V_FUNC_SUBTRACT:
        equation = 0; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_REVERSE_SUBTRACT:
        equation = 1; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_ADD:
        equation = 2; break;
    case NV097_SET_BLEND_EQUATION_V_MIN:
        equation = 3; break;
    case NV097_SET_BLEND_EQUATION_V_MAX:
        equation = 4; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_REVERSE_SUBTRACT_SIGNED:
        equation = 5; break;
    case NV097_SET_BLEND_EQUATION_V_FUNC_ADD_SIGNED:
        equation = 6; break;
    default:
        NV2A_DPRINTF("Unknown blend equation: 0x%08x\n", parameter);
        return; /* discard */
    }
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_EQN, equation);
}

DEF_METHOD(NV097, SET_DEPTH_FUNC)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0], NV_PGRAPH_CONTROL_0_ZFUNC,
             parameter & 0xF);
}

DEF_METHOD(NV097, SET_COLOR_MASK)
{
    pg->surface_color.write_enabled_cache |= pgraph_color_write_enabled(pg);

    bool alpha = parameter & NV097_SET_COLOR_MASK_ALPHA_WRITE_ENABLE;
    bool red = parameter & NV097_SET_COLOR_MASK_RED_WRITE_ENABLE;
    bool green = parameter & NV097_SET_COLOR_MASK_GREEN_WRITE_ENABLE;
    bool blue = parameter & NV097_SET_COLOR_MASK_BLUE_WRITE_ENABLE;
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE, alpha);
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE, red);
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE, green);
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE, blue);
}

DEF_METHOD(NV097, SET_DEPTH_MASK)
{
    pg->surface_zeta.write_enabled_cache |= pgraph_zeta_write_enabled(pg);

    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
             NV_PGRAPH_CONTROL_0_ZWRITEENABLE, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_MASK)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
             NV_PGRAPH_CONTROL_1_STENCIL_MASK_WRITE, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
             NV_PGRAPH_CONTROL_1_STENCIL_FUNC, parameter & 0xF);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC_REF)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
             NV_PGRAPH_CONTROL_1_STENCIL_REF, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_FUNC_MASK)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
             NV_PGRAPH_CONTROL_1_STENCIL_MASK_READ, parameter);
}

DEF_METHOD(NV097, SET_STENCIL_OP_FAIL)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
             NV_PGRAPH_CONTROL_2_STENCIL_OP_FAIL,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_STENCIL_OP_ZFAIL)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
             NV_PGRAPH_CONTROL_2_STENCIL_OP_ZFAIL,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_STENCIL_OP_ZPASS)
{
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
             NV_PGRAPH_CONTROL_2_STENCIL_OP_ZPASS,
             kelvin_map_stencil_op(parameter));
}

DEF_METHOD(NV097, SET_SHADE_MODE)
{
    switch (parameter) {
    case NV097_SET_SHADE_MODE_V_FLAT:
        SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3], NV_PGRAPH_CONTROL_3_SHADEMODE,
                 NV_PGRAPH_CONTROL_3_SHADEMODE_FLAT);
        break;
    case NV097_SET_SHADE_MODE_V_SMOOTH:
        SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3], NV_PGRAPH_CONTROL_3_SHADEMODE,
                 NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH);
        break;
    default:
        /* Discard */
        break;
    }
}

DEF_METHOD(NV097, SET_POLYGON_OFFSET_SCALE_FACTOR)
{
    pg->regs[NV_PGRAPH_ZOFFSETFACTOR] = parameter;
}

DEF_METHOD(NV097, SET_POLYGON_OFFSET_BIAS)
{
    pg->regs[NV_PGRAPH_ZOFFSETBIAS] = parameter;
}

DEF_METHOD(NV097, SET_FRONT_POLYGON_MODE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_FRONTFACEMODE,
             kelvin_map_polygon_mode(parameter));
}

DEF_METHOD(NV097, SET_BACK_POLYGON_MODE)
{
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_BACKFACEMODE,
             kelvin_map_polygon_mode(parameter));
}

DEF_METHOD(NV097, SET_CLIP_MIN)
{
    pg->regs[NV_PGRAPH_ZCLIPMIN] = parameter;
}

DEF_METHOD(NV097, SET_CLIP_MAX)
{
    pg->regs[NV_PGRAPH_ZCLIPMAX] = parameter;
}

DEF_METHOD(NV097, SET_CULL_FACE)
{
    unsigned int face;
    switch (parameter) {
    case NV097_SET_CULL_FACE_V_FRONT:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT; break;
    case NV097_SET_CULL_FACE_V_BACK:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_BACK; break;
    case NV097_SET_CULL_FACE_V_FRONT_AND_BACK:
        face = NV_PGRAPH_SETUPRASTER_CULLCTRL_FRONT_AND_BACK; break;
    default:
        assert(false);
        break;
    }
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_CULLCTRL,
             face);
}

DEF_METHOD(NV097, SET_FRONT_FACE)
{
    bool ccw;
    switch (parameter) {
    case NV097_SET_FRONT_FACE_V_CW:
        ccw = false; break;
    case NV097_SET_FRONT_FACE_V_CCW:
        ccw = true; break;
    default:
        NV2A_DPRINTF("Unknown front face: 0x%08x\n", parameter);
        return; /* discard */
    }
    SET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
             NV_PGRAPH_SETUPRASTER_FRONTFACE,
             ccw ? 1 : 0);
}

DEF_METHOD(NV097, SET_NORMALIZATION_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C],
             NV_PGRAPH_CSV0_C_NORMALIZATION_ENABLE,
             parameter);
}

DEF_METHOD_INC(NV097, SET_MATERIAL_EMISSION)
{
    int slot = (method - NV097_SET_MATERIAL_EMISSION) / 4;
    // FIXME: Verify NV_IGRAPH_XF_LTCTXA_CM_COL is correct
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_CM_COL][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_CM_COL] = true;
}

DEF_METHOD(NV097, SET_MATERIAL_ALPHA)
{
    pg->material_alpha = *(float*)&parameter;
}

DEF_METHOD(NV097, SET_LIGHT_ENABLE_MASK)
{
    SET_MASK(d->pgraph.regs[NV_PGRAPH_CSV0_D],
             NV_PGRAPH_CSV0_D_LIGHTS,
             parameter);
}

DEF_METHOD(NV097, SET_TEXGEN_S)
{
    int slot = (method - NV097_SET_TEXGEN_S) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_S
                                   : NV_PGRAPH_CSV1_A_T0_S;
    SET_MASK(pg->regs[reg], mask, kelvin_map_texgen(parameter, 0));
}

DEF_METHOD(NV097, SET_TEXGEN_T)
{
    int slot = (method - NV097_SET_TEXGEN_T) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_T
                                   : NV_PGRAPH_CSV1_A_T0_T;
    SET_MASK(pg->regs[reg], mask, kelvin_map_texgen(parameter, 1));
}

DEF_METHOD(NV097, SET_TEXGEN_R)
{
    int slot = (method - NV097_SET_TEXGEN_R) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_R
                                   : NV_PGRAPH_CSV1_A_T0_R;
    SET_MASK(pg->regs[reg], mask, kelvin_map_texgen(parameter, 2));
}

DEF_METHOD(NV097, SET_TEXGEN_Q)
{
    int slot = (method - NV097_SET_TEXGEN_Q) / 16;
    unsigned int reg = (slot < 2) ? NV_PGRAPH_CSV1_A
                                  : NV_PGRAPH_CSV1_B;
    unsigned int mask = (slot % 2) ? NV_PGRAPH_CSV1_A_T1_Q
                                   : NV_PGRAPH_CSV1_A_T0_Q;
    SET_MASK(pg->regs[reg], mask, kelvin_map_texgen(parameter, 3));
}

DEF_METHOD_INC(NV097, SET_TEXTURE_MATRIX_ENABLE)
{
    int slot = (method - NV097_SET_TEXTURE_MATRIX_ENABLE) / 4;
    pg->texture_matrix_enable[slot] = parameter;
}

DEF_METHOD(NV097, SET_POINT_SIZE)
{
    SET_MASK(pg->regs[NV_PGRAPH_POINTSIZE], NV097_SET_POINT_SIZE_V, parameter);
}

DEF_METHOD_INC(NV097, SET_PROJECTION_MATRIX)
{
    int slot = (method - NV097_SET_PROJECTION_MATRIX) / 4;
    // pg->projection_matrix[slot] = *(float*)&parameter;
    unsigned int row = NV_IGRAPH_XF_XFCTX_PMAT0 + slot/4;
    pg->vsh_constants[row][slot%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_MODEL_VIEW_MATRIX)
{
    int slot = (method - NV097_SET_MODEL_VIEW_MATRIX) / 4;
    unsigned int matnum = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_MMAT0 + matnum*8 + entry/4;
    pg->vsh_constants[row][entry % 4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_INVERSE_MODEL_VIEW_MATRIX)
{
    int slot = (method - NV097_SET_INVERSE_MODEL_VIEW_MATRIX) / 4;
    unsigned int matnum = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_IMMAT0 + matnum*8 + entry/4;
    pg->vsh_constants[row][entry % 4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_COMPOSITE_MATRIX)
{
    int slot = (method - NV097_SET_COMPOSITE_MATRIX) / 4;
    unsigned int row = NV_IGRAPH_XF_XFCTX_CMAT0 + slot/4;
    pg->vsh_constants[row][slot%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_TEXTURE_MATRIX)
{
    int slot = (method - NV097_SET_TEXTURE_MATRIX) / 4;
    unsigned int tex = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_T0MAT + tex*8 + entry/4;
    pg->vsh_constants[row][entry%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD_INC(NV097, SET_FOG_PARAMS)
{
    int slot = (method - NV097_SET_FOG_PARAMS) / 4;
    if (slot < 2) {
        pg->regs[NV_PGRAPH_FOGPARAM0 + slot*4] = parameter;
    } else {
        /* FIXME: No idea where slot = 2 is */
    }

    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_FOG_K][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_FOG_K] = true;
}

/* Handles NV097_SET_TEXGEN_PLANE_S,T,R,Q */
DEF_METHOD_INC(NV097, SET_TEXGEN_PLANE_S)
{
    int slot = (method - NV097_SET_TEXGEN_PLANE_S) / 4;
    unsigned int tex = slot / 16;
    unsigned int entry = slot % 16;
    unsigned int row = NV_IGRAPH_XF_XFCTX_TG0MAT + tex*8 + entry/4;
    pg->vsh_constants[row][entry%4] = parameter;
    pg->vsh_constants_dirty[row] = true;
}

DEF_METHOD(NV097, SET_TEXGEN_VIEW_MODEL)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_TEXGEN_REF,
             parameter);
}

DEF_METHOD_INC(NV097, SET_FOG_PLANE)
{
    int slot = (method - NV097_SET_FOG_PLANE) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_FOG][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_FOG] = true;
}

DEF_METHOD_INC(NV097, SET_SCENE_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_SCENE_AMBIENT_COLOR) / 4;
    // ??
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_FR_AMB][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_FR_AMB] = true;
}

DEF_METHOD_INC(NV097, SET_VIEWPORT_OFFSET)
{
    int slot = (method - NV097_SET_VIEWPORT_OFFSET) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_VPOFF] = true;
}

DEF_METHOD_INC(NV097, SET_POINT_PARAMS)
{
    int slot = (method - NV097_SET_POINT_PARAMS) / 4;
    pg->point_params[slot] = *(float *)&parameter; /* FIXME: Where? */
}

DEF_METHOD_INC(NV097, SET_EYE_POSITION)
{
    int slot = (method - NV097_SET_EYE_POSITION) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_EYEP][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_EYEP] = true;
}

DEF_METHOD_INC(NV097, SET_COMBINER_FACTOR0)
{
    int slot = (method - NV097_SET_COMBINER_FACTOR0) / 4;
    pg->regs[NV_PGRAPH_COMBINEFACTOR0 + slot*4] = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_FACTOR1)
{
    int slot = (method - NV097_SET_COMBINER_FACTOR1) / 4;
    pg->regs[NV_PGRAPH_COMBINEFACTOR1 + slot*4] = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_ALPHA_OCW)
{
    int slot = (method - NV097_SET_COMBINER_ALPHA_OCW) / 4;
    pg->regs[NV_PGRAPH_COMBINEALPHAO0 + slot*4] = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_COLOR_ICW)
{
    int slot = (method - NV097_SET_COMBINER_COLOR_ICW) / 4;
    pg->regs[NV_PGRAPH_COMBINECOLORI0 + slot*4] = parameter;
}

DEF_METHOD_INC(NV097, SET_VIEWPORT_SCALE)
{
    int slot = (method - NV097_SET_VIEWPORT_SCALE) / 4;
    pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPSCL][slot] = parameter;
    pg->vsh_constants_dirty[NV_IGRAPH_XF_XFCTX_VPSCL] = true;
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_PROGRAM)
{
    int slot = (method - NV097_SET_TRANSFORM_PROGRAM) / 4;

    int program_load = GET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
                                NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR);

    assert(program_load < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    pg->program_data[program_load][slot%4] = parameter;
    pg->program_data_dirty = true;

    if (slot % 4 == 3) {
        SET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
                 NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR, program_load+1);
    }
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_CONSTANT)
{
    int slot = (method - NV097_SET_TRANSFORM_CONSTANT) / 4;
    int const_load = GET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
                              NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR);

    assert(const_load < NV2A_VERTEXSHADER_CONSTANTS);
    // VertexShaderConstant *constant = &pg->constants[const_load];
    pg->vsh_constants_dirty[const_load] |=
        (parameter != pg->vsh_constants[const_load][slot%4]);
    pg->vsh_constants[const_load][slot%4] = parameter;

    if (slot % 4 == 3) {
        SET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
                 NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR, const_load+1);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX3F)
{
    int slot = (method - NV097_SET_VERTEX3F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_POSITION);
    attribute->inline_value[slot] = *(float*)&parameter;
    attribute->inline_value[3] = 1.0f;
    if (slot == 2) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

/* Handles NV097_SET_BACK_LIGHT_* */
DEF_METHOD_INC(NV097, SET_BACK_LIGHT_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_BACK_LIGHT_AMBIENT_COLOR) / 4;
    unsigned int part = NV097_SET_BACK_LIGHT_AMBIENT_COLOR / 4 + slot % 16;
    slot /= 16; /* [Light index] */
    assert(slot < 8);
    switch(part * 4) {
    case NV097_SET_BACK_LIGHT_AMBIENT_COLOR ...
            NV097_SET_BACK_LIGHT_AMBIENT_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_AMBIENT_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BAMB + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BAMB + slot*6] = true;
        break;
    case NV097_SET_BACK_LIGHT_DIFFUSE_COLOR ...
            NV097_SET_BACK_LIGHT_DIFFUSE_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_DIFFUSE_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BDIF + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BDIF + slot*6] = true;
        break;
    case NV097_SET_BACK_LIGHT_SPECULAR_COLOR ...
            NV097_SET_BACK_LIGHT_SPECULAR_COLOR + 8:
        part -= NV097_SET_BACK_LIGHT_SPECULAR_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_BSPC + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_BSPC + slot*6] = true;
        break;
    default:
        assert(false);
        break;
    }
}

/* Handles all the light source props except for NV097_SET_BACK_LIGHT_* */
DEF_METHOD_INC(NV097, SET_LIGHT_AMBIENT_COLOR)
{
    int slot = (method - NV097_SET_LIGHT_AMBIENT_COLOR) / 4;
    unsigned int part = NV097_SET_LIGHT_AMBIENT_COLOR / 4 + slot % 32;
    slot /= 32; /* [Light index] */
    assert(slot < 8);
    switch(part * 4) {
    case NV097_SET_LIGHT_AMBIENT_COLOR ...
            NV097_SET_LIGHT_AMBIENT_COLOR + 8:
        part -= NV097_SET_LIGHT_AMBIENT_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_AMB + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_AMB + slot*6] = true;
        break;
    case NV097_SET_LIGHT_DIFFUSE_COLOR ...
           NV097_SET_LIGHT_DIFFUSE_COLOR + 8:
        part -= NV097_SET_LIGHT_DIFFUSE_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_DIF + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_DIF + slot*6] = true;
        break;
    case NV097_SET_LIGHT_SPECULAR_COLOR ...
            NV097_SET_LIGHT_SPECULAR_COLOR + 8:
        part -= NV097_SET_LIGHT_SPECULAR_COLOR / 4;
        pg->ltctxb[NV_IGRAPH_XF_LTCTXB_L0_SPC + slot*6][part] = parameter;
        pg->ltctxb_dirty[NV_IGRAPH_XF_LTCTXB_L0_SPC + slot*6] = true;
        break;
    case NV097_SET_LIGHT_LOCAL_RANGE:
        pg->ltc1[NV_IGRAPH_XF_LTC1_r0 + slot][0] = parameter;
        pg->ltc1_dirty[NV_IGRAPH_XF_LTC1_r0 + slot] = true;
        break;
    case NV097_SET_LIGHT_INFINITE_HALF_VECTOR ...
            NV097_SET_LIGHT_INFINITE_HALF_VECTOR + 8:
        part -= NV097_SET_LIGHT_INFINITE_HALF_VECTOR / 4;
        pg->light_infinite_half_vector[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_INFINITE_DIRECTION ...
            NV097_SET_LIGHT_INFINITE_DIRECTION + 8:
        part -= NV097_SET_LIGHT_INFINITE_DIRECTION / 4;
        pg->light_infinite_direction[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_SPOT_FALLOFF ...
            NV097_SET_LIGHT_SPOT_FALLOFF + 8:
        part -= NV097_SET_LIGHT_SPOT_FALLOFF / 4;
        pg->ltctxa[NV_IGRAPH_XF_LTCTXA_L0_K + slot*2][part] = parameter;
        pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_L0_K + slot*2] = true;
        break;
    case NV097_SET_LIGHT_SPOT_DIRECTION ...
            NV097_SET_LIGHT_SPOT_DIRECTION + 12:
        part -= NV097_SET_LIGHT_SPOT_DIRECTION / 4;
        pg->ltctxa[NV_IGRAPH_XF_LTCTXA_L0_SPT + slot*2][part] = parameter;
        pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_L0_SPT + slot*2] = true;
        break;
    case NV097_SET_LIGHT_LOCAL_POSITION ...
            NV097_SET_LIGHT_LOCAL_POSITION + 8:
        part -= NV097_SET_LIGHT_LOCAL_POSITION / 4;
        pg->light_local_position[slot][part] = *(float*)&parameter;
        break;
    case NV097_SET_LIGHT_LOCAL_ATTENUATION ...
            NV097_SET_LIGHT_LOCAL_ATTENUATION + 8:
        part -= NV097_SET_LIGHT_LOCAL_ATTENUATION / 4;
        pg->light_local_attenuation[slot][part] = *(float*)&parameter;
        break;
    default:
        assert(false);
        break;
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX4F)
{
    int slot = (method - NV097_SET_VERTEX4F) / 4;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_POSITION];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_POSITION);
    attribute->inline_value[slot] = *(float*)&parameter;
    if (slot == 3) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_NORMAL3S)
{
    int slot = (method - NV097_SET_NORMAL3S) / 4;
    unsigned int part = slot % 2;
    VertexAttribute *attribute =
        &pg->vertex_attributes[NV2A_VERTEX_ATTR_NORMAL];
    pgraph_allocate_inline_buffer_vertices(pg, NV2A_VERTEX_ATTR_NORMAL);
    int16_t val = parameter & 0xFFFF;
    attribute->inline_value[part * 2 + 0] = MAX(-1.0f, (float)val / 32767.0f);
    val = parameter >> 16;
    attribute->inline_value[part * 2 + 1] = MAX(-1.0f, (float)val / 32767.0f);
}

#define SET_VERTEX_ATTRIBUTE_4S(command, attr_index)                     \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        unsigned int part = slot % 2;                                      \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[part * 2 + 0] =                            \
            (float)(int16_t)(parameter & 0xFFFF);                          \
        attribute->inline_value[part * 2 + 1] =                            \
            (float)(int16_t)(parameter >> 16);                             \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD0_4S, NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD1_4S, NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD2_4S, NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_4S)
{
    SET_VERTEX_ATTRIBUTE_4S(NV097_SET_TEXCOORD3_4S, NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATTRIBUTE_4S

#define SET_VERTEX_ATRIBUTE_TEX_2S(attr_index)                             \
    do {                                                                   \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[0] = (float)(int16_t)(parameter & 0xFFFF); \
        attribute->inline_value[1] = (float)(int16_t)(parameter >> 16);    \
        attribute->inline_value[2] = 0.0f;                                 \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_2S)
{
    SET_VERTEX_ATRIBUTE_TEX_2S(NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATRIBUTE_TEX_2S

#define SET_VERTEX_COLOR_3F(command, attr_index)                           \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR3F)
{
    SET_VERTEX_COLOR_3F(NV097_SET_DIFFUSE_COLOR3F, NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR3F)
{
    SET_VERTEX_COLOR_3F(NV097_SET_SPECULAR_COLOR3F, NV2A_VERTEX_ATTR_SPECULAR);
}

#undef SET_VERTEX_COLOR_3F

#define SET_VERTEX_ATTRIBUTE_F(command, attr_index)                        \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
    } while (0)

DEF_METHOD_INC(NV097, SET_NORMAL3F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_NORMAL3F, NV2A_VERTEX_ATTR_NORMAL);
}

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_DIFFUSE_COLOR4F, NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_SPECULAR_COLOR4F,
                           NV2A_VERTEX_ATTR_SPECULAR);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD0_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD0_4F, NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD1_4F, NV2A_VERTEX_ATTR_TEXTURE1);
}


DEF_METHOD_INC(NV097, SET_TEXCOORD2_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD2_4F, NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_4F)
{
    SET_VERTEX_ATTRIBUTE_F(NV097_SET_TEXCOORD3_4F, NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATTRIBUTE_F

#define SET_VERTEX_ATRIBUTE_TEX_2F(command, attr_index)                    \
    do {                                                                   \
        int slot = (method - (command)) / 4;                               \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[slot] = *(float*)&parameter;               \
        attribute->inline_value[2] = 0.0f;                                 \
        attribute->inline_value[3] = 1.0f;                                 \
    } while (0)

DEF_METHOD_INC(NV097, SET_TEXCOORD0_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD0_2F,
                               NV2A_VERTEX_ATTR_TEXTURE0);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD1_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD1_2F,
                               NV2A_VERTEX_ATTR_TEXTURE1);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD2_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD2_2F,
                               NV2A_VERTEX_ATTR_TEXTURE2);
}

DEF_METHOD_INC(NV097, SET_TEXCOORD3_2F)
{
    SET_VERTEX_ATRIBUTE_TEX_2F(NV097_SET_TEXCOORD3_2F,
                               NV2A_VERTEX_ATTR_TEXTURE3);
}

#undef SET_VERTEX_ATRIBUTE_TEX_2F

#define SET_VERTEX_ATTRIBUTE_4UB(command, attr_index)                       \
    do {                                                                   \
        VertexAttribute *attribute = &pg->vertex_attributes[(attr_index)]; \
        pgraph_allocate_inline_buffer_vertices(pg, (attr_index));          \
        attribute->inline_value[0] = (parameter & 0xFF) / 255.0f;          \
        attribute->inline_value[1] = ((parameter >> 8) & 0xFF) / 255.0f;   \
        attribute->inline_value[2] = ((parameter >> 16) & 0xFF) / 255.0f;  \
        attribute->inline_value[3] = ((parameter >> 24) & 0xFF) / 255.0f;  \
    } while (0)

DEF_METHOD_INC(NV097, SET_DIFFUSE_COLOR4UB)
{
    SET_VERTEX_ATTRIBUTE_4UB(NV097_SET_DIFFUSE_COLOR4UB,
                             NV2A_VERTEX_ATTR_DIFFUSE);
}

DEF_METHOD_INC(NV097, SET_SPECULAR_COLOR4UB)
{
    SET_VERTEX_ATTRIBUTE_4UB(NV097_SET_SPECULAR_COLOR4UB,
                             NV2A_VERTEX_ATTR_SPECULAR);
}

#undef SET_VERTEX_ATTRIBUTE_4UB

DEF_METHOD_INC(NV097, SET_VERTEX_DATA_ARRAY_FORMAT)
{
    int slot = (method - NV097_SET_VERTEX_DATA_ARRAY_FORMAT) / 4;
    VertexAttribute *attr = &pg->vertex_attributes[slot];
    attr->format = GET_MASK(parameter, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE);
    attr->count = GET_MASK(parameter, NV097_SET_VERTEX_DATA_ARRAY_FORMAT_SIZE);
    attr->stride = GET_MASK(parameter,
                            NV097_SET_VERTEX_DATA_ARRAY_FORMAT_STRIDE);
    attr->gl_count = attr->count;

    NV2A_DPRINTF("vertex data array format=%d, count=%d, stride=%d\n",
                 attr->format, attr->count, attr->stride);

    switch (attr->format) {
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        attr->gl_type = GL_UNSIGNED_BYTE;
        attr->gl_normalize = GL_TRUE;
        attr->size = 1;
        assert(attr->count == 4);
        // http://www.opengl.org/registry/specs/ARB/vertex_array_bgra.txt
        attr->gl_count = GL_BGRA;
        attr->needs_conversion = false;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
        attr->gl_type = GL_UNSIGNED_BYTE;
        attr->gl_normalize = GL_TRUE;
        attr->size = 1;
        attr->needs_conversion = false;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1:
        attr->gl_type = GL_SHORT;
        attr->gl_normalize = GL_TRUE;
        attr->size = 2;
        attr->needs_conversion = false;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
        attr->gl_type = GL_FLOAT;
        attr->gl_normalize = GL_FALSE;
        attr->size = 4;
        attr->needs_conversion = false;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K:
        attr->gl_type = GL_SHORT;
        attr->gl_normalize = GL_FALSE;
        attr->size = 2;
        attr->needs_conversion = false;
        break;
    case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP:
        /* 3 signed, normalized components packed in 32-bits. (11,11,10) */
        attr->gl_type = GL_INT;
        attr->size = 4;
        assert(attr->count == 1);
        attr->needs_conversion = true;
        break;
    default:
        fprintf(stderr, "Unknown vertex type: 0x%x\n", attr->format);
        assert(false);
        break;
    }

    if (attr->needs_conversion) {
        pg->compressed_attrs |= (1 << slot);
    } else {
        pg->compressed_attrs &= ~(1 << slot);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA_ARRAY_OFFSET)
{
    int slot = (method - NV097_SET_VERTEX_DATA_ARRAY_OFFSET) / 4;

    pg->vertex_attributes[slot].dma_select = parameter & 0x80000000;
    pg->vertex_attributes[slot].offset = parameter & 0x7fffffff;
}

DEF_METHOD(NV097, SET_LOGIC_OP_ENABLE)
{
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_LOGICOP_ENABLE,
             parameter);
}

DEF_METHOD(NV097, SET_LOGIC_OP)
{
    SET_MASK(pg->regs[NV_PGRAPH_BLEND], NV_PGRAPH_BLEND_LOGICOP,
             parameter & 0xF);
}

static void pgraph_process_pending_report(NV2AState *d, QueryReport *r)
{
    PGRAPHState *pg = &d->pgraph;

    if (r->clear) {
        pg->zpass_pixel_count_result = 0;
        return;
    }

    uint8_t type = GET_MASK(r->parameter, NV097_GET_REPORT_TYPE);
    assert(type == NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT);

    /* FIXME: Multisampling affects this (both: OGL and Xbox GPU),
     *        not sure if CLEARs also count
     */
    /* FIXME: What about clipping regions etc? */
    for (int i = 0; i < r->query_count; i++) {
        GLuint gl_query_result = 0;
        glGetQueryObjectuiv(r->queries[i], GL_QUERY_RESULT, &gl_query_result);
        gl_query_result /= pg->surface_scale_factor * pg->surface_scale_factor;
        pg->zpass_pixel_count_result += gl_query_result;
    }

    if (r->query_count) {
        glDeleteQueries(r->query_count, r->queries);
        g_free(r->queries);
    }

    uint64_t timestamp = 0x0011223344556677; /* FIXME: Update timestamp?! */
    uint32_t done = 0;

    hwaddr report_dma_len;
    uint8_t *report_data =
        (uint8_t *)nv_dma_map(d, pg->dma_report, &report_dma_len);

    hwaddr offset = GET_MASK(r->parameter, NV097_GET_REPORT_OFFSET);
    assert(offset < report_dma_len);
    report_data += offset;

    stq_le_p((uint64_t *)&report_data[0], timestamp);
    stl_le_p((uint32_t *)&report_data[8], pg->zpass_pixel_count_result);
    stl_le_p((uint32_t *)&report_data[12], done);
}

void pgraph_process_pending_reports(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    QueryReport *r, *next;

    QSIMPLEQ_FOREACH_SAFE(r, &pg->report_queue, entry, next) {
        pgraph_process_pending_report(d, r);
        QSIMPLEQ_REMOVE_HEAD(&pg->report_queue, entry);
        g_free(r);
    }
}

DEF_METHOD(NV097, CLEAR_REPORT_VALUE)
{
    /* FIXME: Does this have a value in parameter? Also does this (also?) modify
     *        the report memory block?
     */
    if (pg->gl_zpass_pixel_count_query_count) {
        glDeleteQueries(pg->gl_zpass_pixel_count_query_count,
                        pg->gl_zpass_pixel_count_queries);
        pg->gl_zpass_pixel_count_query_count = 0;
    }

    QueryReport *r = g_malloc(sizeof(QueryReport));
    r->clear = true;
    QSIMPLEQ_INSERT_TAIL(&pg->report_queue, r, entry);
}

DEF_METHOD(NV097, SET_ZPASS_PIXEL_COUNT_ENABLE)
{
    pg->zpass_pixel_count_enable = parameter;
}

DEF_METHOD(NV097, GET_REPORT)
{
    uint8_t type = GET_MASK(parameter, NV097_GET_REPORT_TYPE);
    assert(type == NV097_GET_REPORT_TYPE_ZPASS_PIXEL_CNT);

    QueryReport *r = g_malloc(sizeof(QueryReport));
    r->clear = false;
    r->parameter = parameter;
    r->query_count = pg->gl_zpass_pixel_count_query_count;
    r->queries = pg->gl_zpass_pixel_count_queries;
    QSIMPLEQ_INSERT_TAIL(&pg->report_queue, r, entry);

    pg->gl_zpass_pixel_count_query_count = 0;
    pg->gl_zpass_pixel_count_queries = NULL;
}

DEF_METHOD_INC(NV097, SET_EYE_DIRECTION)
{
    int slot = (method - NV097_SET_EYE_DIRECTION) / 4;
    pg->ltctxa[NV_IGRAPH_XF_LTCTXA_EYED][slot] = parameter;
    pg->ltctxa_dirty[NV_IGRAPH_XF_LTCTXA_EYED] = true;
}

static void pgraph_reset_draw_arrays(PGRAPHState *pg)
{
    pg->draw_arrays_length = 0;
    pg->draw_arrays_min_start = -1;
    pg->draw_arrays_max_count = 0;
    pg->draw_arrays_prevent_connect = false;
}

static void pgraph_reset_inline_buffers(PGRAPHState *pg)
{
    pg->inline_elements_length = 0;
    pg->inline_array_length = 0;
    pg->inline_buffer_length = 0;
    pgraph_reset_draw_arrays(pg);
}

static void pgraph_flush_draw(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    if (!(pg->color_binding || pg->zeta_binding)) {
        pgraph_reset_inline_buffers(pg);
        return;
    }
    assert(pg->shader_binding);

    if (pg->draw_arrays_length) {
        NV2A_GL_DPRINTF(false, "Draw Arrays");
        nv2a_profile_inc_counter(NV2A_PROF_DRAW_ARRAYS);
        assert(pg->inline_elements_length == 0);
        assert(pg->inline_buffer_length == 0);
        assert(pg->inline_array_length == 0);

        pgraph_bind_vertex_attributes(d, pg->draw_arrays_min_start,
                                      pg->draw_arrays_max_count - 1,
                                      false, 0,
                                      pg->draw_arrays_max_count - 1);
        glMultiDrawArrays(pg->shader_binding->gl_primitive_mode,
                          pg->gl_draw_arrays_start,
                          pg->gl_draw_arrays_count,
                          pg->draw_arrays_length);
    } else if (pg->inline_elements_length) {
        NV2A_GL_DPRINTF(false, "Inline Elements");
        nv2a_profile_inc_counter(NV2A_PROF_INLINE_ELEMENTS);
        assert(pg->inline_buffer_length == 0);
        assert(pg->inline_array_length == 0);

        uint32_t min_element = (uint32_t)-1;
        uint32_t max_element = 0;
        for (int i=0; i < pg->inline_elements_length; i++) {
            max_element = MAX(pg->inline_elements[i], max_element);
            min_element = MIN(pg->inline_elements[i], min_element);
        }

        pgraph_bind_vertex_attributes(
                d, min_element, max_element, false, 0,
                pg->inline_elements[pg->inline_elements_length - 1]);

        VertexKey k;
        memset(&k, 0, sizeof(VertexKey));
        k.count = pg->inline_elements_length;
        k.gl_type = GL_UNSIGNED_INT;
        k.gl_normalize = GL_FALSE;
        k.stride = sizeof(uint32_t);
        uint64_t h = fast_hash((uint8_t*)pg->inline_elements,
                               pg->inline_elements_length * 4);

        LruNode *node = lru_lookup(&pg->element_cache, h, &k);
        VertexLruNode *found = container_of(node, VertexLruNode, node);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, found->gl_buffer);
        if (!found->initialized) {
            nv2a_profile_inc_counter(NV2A_PROF_GEOM_BUFFER_UPDATE_4);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         pg->inline_elements_length * 4,
                         pg->inline_elements, GL_STATIC_DRAW);
            found->initialized = true;
        } else {
            nv2a_profile_inc_counter(NV2A_PROF_GEOM_BUFFER_UPDATE_4_NOTDIRTY);
        }
        glDrawElements(pg->shader_binding->gl_primitive_mode,
                       pg->inline_elements_length, GL_UNSIGNED_INT,
                       (void *)0);
    } else if (pg->inline_buffer_length) {
        NV2A_GL_DPRINTF(false, "Inline Buffer");
        nv2a_profile_inc_counter(NV2A_PROF_INLINE_BUFFERS);
        assert(pg->inline_array_length == 0);

        if (pg->compressed_attrs) {
            pg->compressed_attrs = 0;
            pgraph_bind_shaders(pg);
        }

        for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
            VertexAttribute *attr = &pg->vertex_attributes[i];
            if (attr->inline_buffer_populated) {
                nv2a_profile_inc_counter(NV2A_PROF_GEOM_BUFFER_UPDATE_3);
                glBindBuffer(GL_ARRAY_BUFFER, attr->gl_inline_buffer);
                glBufferData(GL_ARRAY_BUFFER,
                             pg->inline_buffer_length * sizeof(float) * 4,
                             attr->inline_buffer, GL_STREAM_DRAW);
                glVertexAttribPointer(i, 4, GL_FLOAT, GL_FALSE, 0, 0);
                glEnableVertexAttribArray(i);
                attr->inline_buffer_populated = false;
                memcpy(attr->inline_value,
                       attr->inline_buffer + (pg->inline_buffer_length - 1) * 4,
                       sizeof(attr->inline_value));
            } else {
                glDisableVertexAttribArray(i);
                glVertexAttrib4fv(i, attr->inline_value);
            }
        }

        glDrawArrays(pg->shader_binding->gl_primitive_mode,
                     0, pg->inline_buffer_length);
    } else if (pg->inline_array_length) {
        NV2A_GL_DPRINTF(false, "Inline Array");
        nv2a_profile_inc_counter(NV2A_PROF_INLINE_ARRAYS);

        unsigned int index_count = pgraph_bind_inline_array(d);
        glDrawArrays(pg->shader_binding->gl_primitive_mode,
                     0, index_count);
    } else {
        NV2A_GL_DPRINTF(true, "EMPTY NV097_SET_BEGIN_END");
        NV2A_UNCONFIRMED("EMPTY NV097_SET_BEGIN_END");
    }

    pgraph_reset_inline_buffers(pg);
}

DEF_METHOD(NV097, SET_BEGIN_END)
{
    uint32_t control_0 = pg->regs[NV_PGRAPH_CONTROL_0];
    bool mask_alpha = control_0 & NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE;
    bool mask_red = control_0 & NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE;
    bool mask_green = control_0 & NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE;
    bool mask_blue = control_0 & NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE;
    bool color_write = mask_alpha || mask_red || mask_green || mask_blue;
    bool depth_test = control_0 & NV_PGRAPH_CONTROL_0_ZENABLE;
    bool stencil_test =
        pg->regs[NV_PGRAPH_CONTROL_1] & NV_PGRAPH_CONTROL_1_STENCIL_TEST_ENABLE;
    bool is_nop_draw = !(color_write || depth_test || stencil_test);

    if (parameter == NV097_SET_BEGIN_END_OP_END) {
        if (pg->primitive_mode == PRIM_TYPE_INVALID) {
            NV2A_DPRINTF("End without Begin!\n");
        }
        nv2a_profile_inc_counter(NV2A_PROF_BEGIN_ENDS);

        if (is_nop_draw) {
            // FIXME: Check PGRAPH register 0x880.
            // HW uses bit 11 in 0x880 to enable or disable a color/zeta limit
            // check that will raise an exception in the case that a draw should
            // modify the color and/or zeta buffer but the target(s) are masked
            // off. This check only seems to trigger during the fragment
            // processing, it is legal to attempt a draw that is entirely
            // clipped regardless of 0x880. See xemu#635 for context.
            return;
        }

        pgraph_flush_draw(d);

        /* End of visibility testing */
        if (pg->zpass_pixel_count_enable) {
            nv2a_profile_inc_counter(NV2A_PROF_QUERY);
            glEndQuery(GL_SAMPLES_PASSED);
        }

        pg->draw_time++;
        if (pg->color_binding && pgraph_color_write_enabled(pg)) {
            pg->color_binding->draw_time = pg->draw_time;
        }
        if (pg->zeta_binding && pgraph_zeta_write_enabled(pg)) {
            pg->zeta_binding->draw_time = pg->draw_time;
        }

        pgraph_set_surface_dirty(pg, color_write, depth_test || stencil_test);

        NV2A_GL_DGROUP_END();
        pg->primitive_mode = PRIM_TYPE_INVALID;
    } else {
        NV2A_GL_DGROUP_BEGIN("NV097_SET_BEGIN_END: 0x%x", parameter);
        if (pg->primitive_mode != PRIM_TYPE_INVALID) {
            NV2A_DPRINTF("Begin without End!\n");
        }
        assert(parameter <= NV097_SET_BEGIN_END_OP_POLYGON);
        pg->primitive_mode = parameter;

        pgraph_update_surface(d, true, true, depth_test || stencil_test);
        pgraph_reset_inline_buffers(pg);

        if (is_nop_draw) {
            return;
        }

        assert(pg->color_binding || pg->zeta_binding);

        pgraph_bind_textures(d);
        pgraph_bind_shaders(pg);

        glColorMask(mask_red, mask_green, mask_blue, mask_alpha);
        glDepthMask(!!(control_0 & NV_PGRAPH_CONTROL_0_ZWRITEENABLE));
        glStencilMask(GET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
                               NV_PGRAPH_CONTROL_1_STENCIL_MASK_WRITE));

        if (pg->regs[NV_PGRAPH_BLEND] & NV_PGRAPH_BLEND_EN) {
            glEnable(GL_BLEND);
            uint32_t sfactor = GET_MASK(pg->regs[NV_PGRAPH_BLEND],
                                        NV_PGRAPH_BLEND_SFACTOR);
            uint32_t dfactor = GET_MASK(pg->regs[NV_PGRAPH_BLEND],
                                        NV_PGRAPH_BLEND_DFACTOR);
            assert(sfactor < ARRAY_SIZE(pgraph_blend_factor_map));
            assert(dfactor < ARRAY_SIZE(pgraph_blend_factor_map));
            glBlendFunc(pgraph_blend_factor_map[sfactor],
                        pgraph_blend_factor_map[dfactor]);

            uint32_t equation = GET_MASK(pg->regs[NV_PGRAPH_BLEND],
                                         NV_PGRAPH_BLEND_EQN);
            assert(equation < ARRAY_SIZE(pgraph_blend_equation_map));
            glBlendEquation(pgraph_blend_equation_map[equation]);

            uint32_t blend_color = pg->regs[NV_PGRAPH_BLENDCOLOR];
            glBlendColor( ((blend_color >> 16) & 0xFF) / 255.0f, /* red */
                          ((blend_color >> 8) & 0xFF) / 255.0f,  /* green */
                          (blend_color & 0xFF) / 255.0f,         /* blue */
                          ((blend_color >> 24) & 0xFF) / 255.0f);/* alpha */
        } else {
            glDisable(GL_BLEND);
        }

        /* Face culling */
        if (pg->regs[NV_PGRAPH_SETUPRASTER]
                & NV_PGRAPH_SETUPRASTER_CULLENABLE) {
            uint32_t cull_face = GET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
                                          NV_PGRAPH_SETUPRASTER_CULLCTRL);
            assert(cull_face < ARRAY_SIZE(pgraph_cull_face_map));
            glCullFace(pgraph_cull_face_map[cull_face]);
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }

        /* Clipping */
        glEnable(GL_CLIP_DISTANCE0);
        glEnable(GL_CLIP_DISTANCE1);

        /* Front-face select */
        glFrontFace(pg->regs[NV_PGRAPH_SETUPRASTER]
                        & NV_PGRAPH_SETUPRASTER_FRONTFACE
                            ? GL_CCW : GL_CW);

        /* Polygon offset */
        /* FIXME: GL implementation-specific, maybe do this in VS? */
        if (pg->regs[NV_PGRAPH_SETUPRASTER] &
                NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE) {
            glEnable(GL_POLYGON_OFFSET_FILL);
        } else {
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
        if (pg->regs[NV_PGRAPH_SETUPRASTER] &
                NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE) {
            glEnable(GL_POLYGON_OFFSET_LINE);
        } else {
            glDisable(GL_POLYGON_OFFSET_LINE);
        }
        if (pg->regs[NV_PGRAPH_SETUPRASTER] &
                NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE) {
            glEnable(GL_POLYGON_OFFSET_POINT);
        } else {
            glDisable(GL_POLYGON_OFFSET_POINT);
        }
        if (pg->regs[NV_PGRAPH_SETUPRASTER] &
                (NV_PGRAPH_SETUPRASTER_POFFSETFILLENABLE |
                 NV_PGRAPH_SETUPRASTER_POFFSETLINEENABLE |
                 NV_PGRAPH_SETUPRASTER_POFFSETPOINTENABLE)) {
            GLfloat zfactor = *(float*)&pg->regs[NV_PGRAPH_ZOFFSETFACTOR];
            GLfloat zbias = *(float*)&pg->regs[NV_PGRAPH_ZOFFSETBIAS];
            glPolygonOffset(zfactor, zbias);
        }

        /* Depth testing */
        if (depth_test) {
            glEnable(GL_DEPTH_TEST);

            uint32_t depth_func = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
                                           NV_PGRAPH_CONTROL_0_ZFUNC);
            assert(depth_func < ARRAY_SIZE(pgraph_depth_func_map));
            glDepthFunc(pgraph_depth_func_map[depth_func]);
        } else {
            glDisable(GL_DEPTH_TEST);
        }

        if (GET_MASK(pg->regs[NV_PGRAPH_ZCOMPRESSOCCLUDE],
                     NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN) ==
            NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CLAMP) {
            glEnable(GL_DEPTH_CLAMP);
        } else {
            glDisable(GL_DEPTH_CLAMP);
        }

        if (GET_MASK(pg->regs[NV_PGRAPH_CONTROL_3],
                     NV_PGRAPH_CONTROL_3_SHADEMODE) ==
            NV_PGRAPH_CONTROL_3_SHADEMODE_FLAT) {
            glProvokingVertex(GL_FIRST_VERTEX_CONVENTION);
        }

        if (stencil_test) {
            glEnable(GL_STENCIL_TEST);

            uint32_t stencil_func = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
                                        NV_PGRAPH_CONTROL_1_STENCIL_FUNC);
            uint32_t stencil_ref = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
                                        NV_PGRAPH_CONTROL_1_STENCIL_REF);
            uint32_t func_mask = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_1],
                                    NV_PGRAPH_CONTROL_1_STENCIL_MASK_READ);
            uint32_t op_fail = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
                                    NV_PGRAPH_CONTROL_2_STENCIL_OP_FAIL);
            uint32_t op_zfail = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
                                    NV_PGRAPH_CONTROL_2_STENCIL_OP_ZFAIL);
            uint32_t op_zpass = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_2],
                                    NV_PGRAPH_CONTROL_2_STENCIL_OP_ZPASS);

            assert(stencil_func < ARRAY_SIZE(pgraph_stencil_func_map));
            assert(op_fail < ARRAY_SIZE(pgraph_stencil_op_map));
            assert(op_zfail < ARRAY_SIZE(pgraph_stencil_op_map));
            assert(op_zpass < ARRAY_SIZE(pgraph_stencil_op_map));

            glStencilFunc(
                pgraph_stencil_func_map[stencil_func],
                stencil_ref,
                func_mask);

            glStencilOp(
                pgraph_stencil_op_map[op_fail],
                pgraph_stencil_op_map[op_zfail],
                pgraph_stencil_op_map[op_zpass]);

        } else {
            glDisable(GL_STENCIL_TEST);
        }

        /* Dither */
        /* FIXME: GL implementation dependent */
        if (pg->regs[NV_PGRAPH_CONTROL_0] &
                NV_PGRAPH_CONTROL_0_DITHERENABLE) {
            glEnable(GL_DITHER);
        } else {
            glDisable(GL_DITHER);
        }

        glEnable(GL_PROGRAM_POINT_SIZE);

        bool anti_aliasing = GET_MASK(pg->regs[NV_PGRAPH_ANTIALIASING], NV_PGRAPH_ANTIALIASING_ENABLE);

        /* Edge Antialiasing */
        if (!anti_aliasing && pg->regs[NV_PGRAPH_SETUPRASTER] &
                                  NV_PGRAPH_SETUPRASTER_LINESMOOTHENABLE) {
            glEnable(GL_LINE_SMOOTH);
        } else {
            glDisable(GL_LINE_SMOOTH);
        }
        if (!anti_aliasing && pg->regs[NV_PGRAPH_SETUPRASTER] &
                                  NV_PGRAPH_SETUPRASTER_POLYSMOOTHENABLE) {
            glEnable(GL_POLYGON_SMOOTH);
        } else {
            glDisable(GL_POLYGON_SMOOTH);
        }

        unsigned int vp_width = pg->surface_binding_dim.width,
                     vp_height = pg->surface_binding_dim.height;
        pgraph_apply_scaling_factor(pg, &vp_width, &vp_height);
        glViewport(0, 0, vp_width, vp_height);

        /* Surface clip */
        /* FIXME: Consider moving to PSH w/ window clip */
        unsigned int xmin = pg->surface_shape.clip_x - pg->surface_binding_dim.clip_x,
                     ymin = pg->surface_shape.clip_y - pg->surface_binding_dim.clip_y;
        unsigned int xmax = xmin + pg->surface_shape.clip_width - 1,
                     ymax = ymin + pg->surface_shape.clip_height - 1;

        unsigned int scissor_width = xmax - xmin + 1,
                     scissor_height = ymax - ymin + 1;
        pgraph_apply_anti_aliasing_factor(pg, &xmin, &ymin);
        pgraph_apply_anti_aliasing_factor(pg, &scissor_width, &scissor_height);
        ymin = pg->surface_binding_dim.height - (ymin + scissor_height);
        pgraph_apply_scaling_factor(pg, &xmin, &ymin);
        pgraph_apply_scaling_factor(pg, &scissor_width, &scissor_height);

        glEnable(GL_SCISSOR_TEST);
        glScissor(xmin, ymin, scissor_width, scissor_height);

        /* Visibility testing */
        if (pg->zpass_pixel_count_enable) {
            pg->gl_zpass_pixel_count_query_count++;
            pg->gl_zpass_pixel_count_queries = (GLuint*)g_realloc(
                pg->gl_zpass_pixel_count_queries,
                sizeof(GLuint) * pg->gl_zpass_pixel_count_query_count);

            GLuint gl_query;
            glGenQueries(1, &gl_query);
            pg->gl_zpass_pixel_count_queries[
                pg->gl_zpass_pixel_count_query_count - 1] = gl_query;
            glBeginQuery(GL_SAMPLES_PASSED, gl_query);
        }
    }
}

DEF_METHOD(NV097, SET_TEXTURE_OFFSET)
{
    int slot = (method - NV097_SET_TEXTURE_OFFSET) / 64;
    pg->regs[NV_PGRAPH_TEXOFFSET0 + slot * 4] = parameter;
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_FORMAT)
{
    int slot = (method - NV097_SET_TEXTURE_FORMAT) / 64;

    bool dma_select =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_CONTEXT_DMA) == 2;
    bool cubemap =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_CUBEMAP_ENABLE);
    unsigned int border_source =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BORDER_SOURCE);
    unsigned int dimensionality =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_DIMENSIONALITY);
    unsigned int color_format =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_COLOR);
    unsigned int levels =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_MIPMAP_LEVELS);
    unsigned int log_width =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_U);
    unsigned int log_height =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_V);
    unsigned int log_depth =
        GET_MASK(parameter, NV097_SET_TEXTURE_FORMAT_BASE_SIZE_P);

    uint32_t *reg = &pg->regs[NV_PGRAPH_TEXFMT0 + slot * 4];
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_CONTEXT_DMA, dma_select);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE, cubemap);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_BORDER_SOURCE, border_source);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_DIMENSIONALITY, dimensionality);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_COLOR, color_format);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_MIPMAP_LEVELS, levels);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_U, log_width);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_V, log_height);
    SET_MASK(*reg, NV_PGRAPH_TEXFMT0_BASE_SIZE_P, log_depth);

    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_CONTROL0)
{
    int slot = (method - NV097_SET_TEXTURE_CONTROL0) / 64;
    pg->regs[NV_PGRAPH_TEXCTL0_0 + slot*4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_CONTROL1)
{
    int slot = (method - NV097_SET_TEXTURE_CONTROL1) / 64;
    pg->regs[NV_PGRAPH_TEXCTL1_0 + slot*4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_FILTER)
{
    int slot = (method - NV097_SET_TEXTURE_FILTER) / 64;
    pg->regs[NV_PGRAPH_TEXFILTER0 + slot * 4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_IMAGE_RECT)
{
    int slot = (method - NV097_SET_TEXTURE_IMAGE_RECT) / 64;
    pg->regs[NV_PGRAPH_TEXIMAGERECT0 + slot * 4] = parameter;
    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_PALETTE)
{
    int slot = (method - NV097_SET_TEXTURE_PALETTE) / 64;

    bool dma_select =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_CONTEXT_DMA) == 1;
    unsigned int length =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_LENGTH);
    unsigned int offset =
        GET_MASK(parameter, NV097_SET_TEXTURE_PALETTE_OFFSET);

    uint32_t *reg = &pg->regs[NV_PGRAPH_TEXPALETTE0 + slot * 4];
    SET_MASK(*reg, NV_PGRAPH_TEXPALETTE0_CONTEXT_DMA, dma_select);
    SET_MASK(*reg, NV_PGRAPH_TEXPALETTE0_LENGTH, length);
    SET_MASK(*reg, NV_PGRAPH_TEXPALETTE0_OFFSET, offset);

    pg->texture_dirty[slot] = true;
}

DEF_METHOD(NV097, SET_TEXTURE_BORDER_COLOR)
{
    int slot = (method - NV097_SET_TEXTURE_BORDER_COLOR) / 64;
    pg->regs[NV_PGRAPH_BORDERCOLOR0 + slot * 4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_MAT)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_MAT) / 4;
    if (slot < 16) {
        /* discard */
        return;
    }

    slot -= 16;
    const int swizzle[4] = { NV_PGRAPH_BUMPMAT00, NV_PGRAPH_BUMPMAT01,
                             NV_PGRAPH_BUMPMAT11, NV_PGRAPH_BUMPMAT10 };
    pg->regs[swizzle[slot % 4] + slot / 4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_SCALE)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_SCALE) / 64;
    if (slot == 0) {
        /* discard */
        return;
    }

    slot--;
    pg->regs[NV_PGRAPH_BUMPSCALE1 + slot * 4] = parameter;
}

DEF_METHOD(NV097, SET_TEXTURE_SET_BUMP_ENV_OFFSET)
{
    int slot = (method - NV097_SET_TEXTURE_SET_BUMP_ENV_OFFSET) / 64;
    if (slot == 0) {
        /* discard */
        return;
    }

    slot--;
    pg->regs[NV_PGRAPH_BUMPOFFSET1 + slot * 4] = parameter;
}

static void pgraph_expand_draw_arrays(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    GLint start = pg->gl_draw_arrays_start[pg->draw_arrays_length - 1];
    GLsizei count = pg->gl_draw_arrays_count[pg->draw_arrays_length - 1];

    /* Render any previously squashed DRAW_ARRAYS calls. This case would be
     * triggered if a set of BEGIN+DA+END triplets is followed by the
     * BEGIN+DA+ARRAY_ELEMENT+... chain that caused this expansion. */
    if (pg->draw_arrays_length > 1) {
        pgraph_flush_draw(d);
    }
    assert((pg->inline_elements_length + count) < NV2A_MAX_BATCH_LENGTH);
    for (unsigned int i = 0; i < count; i++) {
        pg->inline_elements[pg->inline_elements_length++] = start + i;
    }

    pgraph_reset_draw_arrays(pg);
}

static void pgraph_check_within_begin_end_block(PGRAPHState *pg)
{
    if (pg->primitive_mode == PRIM_TYPE_INVALID) {
        NV2A_DPRINTF("Vertex data being sent outside of begin/end block!\n");
    }
}

DEF_METHOD_NON_INC(NV097, ARRAY_ELEMENT16)
{
    pgraph_check_within_begin_end_block(pg);

    if (pg->draw_arrays_length) {
        pgraph_expand_draw_arrays(d);
    }

    assert(pg->inline_elements_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_elements[pg->inline_elements_length++] = parameter & 0xFFFF;
    pg->inline_elements[pg->inline_elements_length++] = parameter >> 16;
}

DEF_METHOD_NON_INC(NV097, ARRAY_ELEMENT32)
{
    pgraph_check_within_begin_end_block(pg);

    if (pg->draw_arrays_length) {
        pgraph_expand_draw_arrays(d);
    }

    assert(pg->inline_elements_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_elements[pg->inline_elements_length++] = parameter;
}

DEF_METHOD(NV097, DRAW_ARRAYS)
{
    pgraph_check_within_begin_end_block(pg);

    unsigned int start = GET_MASK(parameter, NV097_DRAW_ARRAYS_START_INDEX);
    unsigned int count = GET_MASK(parameter, NV097_DRAW_ARRAYS_COUNT) + 1;

    if (pg->inline_elements_length) {
        /* FIXME: Determine HW behavior for overflow case. */
        assert((pg->inline_elements_length + count) < NV2A_MAX_BATCH_LENGTH);
        assert(!pg->draw_arrays_prevent_connect);

        for (unsigned int i = 0; i < count; i++) {
            pg->inline_elements[pg->inline_elements_length++] = start + i;
        }
        return;
    }

    pg->draw_arrays_min_start = MIN(pg->draw_arrays_min_start, start);
    pg->draw_arrays_max_count = MAX(pg->draw_arrays_max_count, start + count);

    assert(pg->draw_arrays_length < ARRAY_SIZE(pg->gl_draw_arrays_start));

    /* Attempt to connect contiguous primitives */
    if (!pg->draw_arrays_prevent_connect && pg->draw_arrays_length > 0) {
        unsigned int last_start =
            pg->gl_draw_arrays_start[pg->draw_arrays_length - 1];
        GLsizei* last_count =
            &pg->gl_draw_arrays_count[pg->draw_arrays_length - 1];
        if (start == (last_start + *last_count)) {
            *last_count += count;
            return;
        }
    }

    pg->gl_draw_arrays_start[pg->draw_arrays_length] = start;
    pg->gl_draw_arrays_count[pg->draw_arrays_length] = count;
    pg->draw_arrays_length++;
    pg->draw_arrays_prevent_connect = false;
}

DEF_METHOD_NON_INC(NV097, INLINE_ARRAY)
{
    pgraph_check_within_begin_end_block(pg);
    assert(pg->inline_array_length < NV2A_MAX_BATCH_LENGTH);
    pg->inline_array[pg->inline_array_length++] = parameter;
}

DEF_METHOD_INC(NV097, SET_EYE_VECTOR)
{
    int slot = (method - NV097_SET_EYE_VECTOR) / 4;
    pg->regs[NV_PGRAPH_EYEVEC0 + slot * 4] = parameter;
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA2F_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA2F_M) / 4;
    unsigned int part = slot % 2;
    slot /= 2;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[part] = *(float*)&parameter;
    /* FIXME: Should these really be set to 0.0 and 1.0 ? Conditions? */
    attribute->inline_value[2] = 0.0;
    attribute->inline_value[3] = 1.0;
    if ((slot == 0) && (part == 1)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4F_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA4F_M) / 4;
    unsigned int part = slot % 4;
    slot /= 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[part] = *(float*)&parameter;
    if ((slot == 0) && (part == 3)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA2S)
{
    int slot = (method - NV097_SET_VERTEX_DATA2S) / 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[0] = (float)(int16_t)(parameter & 0xFFFF);
    attribute->inline_value[1] = (float)(int16_t)(parameter >> 16);
    attribute->inline_value[2] = 0.0;
    attribute->inline_value[3] = 1.0;
    if (slot == 0) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4UB)
{
    int slot = (method - NV097_SET_VERTEX_DATA4UB) / 4;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);
    attribute->inline_value[0] = (parameter & 0xFF) / 255.0;
    attribute->inline_value[1] = ((parameter >> 8) & 0xFF) / 255.0;
    attribute->inline_value[2] = ((parameter >> 16) & 0xFF) / 255.0;
    attribute->inline_value[3] = ((parameter >> 24) & 0xFF) / 255.0;
    if (slot == 0) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD_INC(NV097, SET_VERTEX_DATA4S_M)
{
    int slot = (method - NV097_SET_VERTEX_DATA4S_M) / 4;
    unsigned int part = slot % 2;
    slot /= 2;
    VertexAttribute *attribute = &pg->vertex_attributes[slot];
    pgraph_allocate_inline_buffer_vertices(pg, slot);

    attribute->inline_value[part * 2 + 0] = (float)(int16_t)(parameter & 0xFFFF);
    attribute->inline_value[part * 2 + 1] = (float)(int16_t)(parameter >> 16);
    if ((slot == 0) && (part == 1)) {
        pgraph_finish_inline_buffer_vertex(pg);
    }
}

DEF_METHOD(NV097, SET_SEMAPHORE_OFFSET)
{
    pg->regs[NV_PGRAPH_SEMAPHOREOFFSET] = parameter;
}

DEF_METHOD(NV097, BACK_END_WRITE_SEMAPHORE_RELEASE)
{
    pgraph_update_surface(d, false, true, true);

    //qemu_mutex_unlock(&d->pgraph.lock);
    //qemu_mutex_lock_iothread();

    uint32_t semaphore_offset = pg->regs[NV_PGRAPH_SEMAPHOREOFFSET];

    hwaddr semaphore_dma_len;
    uint8_t *semaphore_data = (uint8_t*)nv_dma_map(d, pg->dma_semaphore,
                                                   &semaphore_dma_len);
    assert(semaphore_offset < semaphore_dma_len);
    semaphore_data += semaphore_offset;

    stl_le_p((uint32_t*)semaphore_data, parameter);

    //qemu_mutex_lock(&d->pgraph.lock);
    //qemu_mutex_unlock_iothread();
}

DEF_METHOD(NV097, SET_ZMIN_MAX_CONTROL)
{
    switch (GET_MASK(parameter, NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN)) {
    case NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN_CULL:
        SET_MASK(pg->regs[NV_PGRAPH_ZCOMPRESSOCCLUDE],
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CULL);
        break;
    case NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN_CLAMP:
        SET_MASK(pg->regs[NV_PGRAPH_ZCOMPRESSOCCLUDE],
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN,
                 NV_PGRAPH_ZCOMPRESSOCCLUDE_ZCLAMP_EN_CLAMP);
        break;
    default:
        /* FIXME: Should raise NV_PGRAPH_NSOURCE_DATA_ERROR_PENDING */
        assert(!"Invalid zclamp value");
        break;
    }
}

DEF_METHOD(NV097, SET_ANTI_ALIASING_CONTROL)
{
    SET_MASK(pg->regs[NV_PGRAPH_ANTIALIASING], NV_PGRAPH_ANTIALIASING_ENABLE,
             GET_MASK(parameter, NV097_SET_ANTI_ALIASING_CONTROL_ENABLE));
    // FIXME: Handle the remaining bits (observed values 0xFFFF0000, 0xFFFF0001)
}

DEF_METHOD(NV097, SET_ZSTENCIL_CLEAR_VALUE)
{
    pg->regs[NV_PGRAPH_ZSTENCILCLEARVALUE] = parameter;
}

DEF_METHOD(NV097, SET_COLOR_CLEAR_VALUE)
{
    pg->regs[NV_PGRAPH_COLORCLEARVALUE] = parameter;
}

DEF_METHOD(NV097, CLEAR_SURFACE)
{
    pg->clearing = true;

    NV2A_DPRINTF("---------PRE CLEAR ------\n");
    GLbitfield gl_mask = 0;

    bool write_color = (parameter & NV097_CLEAR_SURFACE_COLOR);
    bool write_zeta =
        (parameter & (NV097_CLEAR_SURFACE_Z | NV097_CLEAR_SURFACE_STENCIL));

    if (write_zeta) {
        uint32_t clear_zstencil =
            d->pgraph.regs[NV_PGRAPH_ZSTENCILCLEARVALUE];
        GLint gl_clear_stencil;
        GLfloat gl_clear_depth;

        switch(pg->surface_shape.zeta_format) {
        case NV097_SET_SURFACE_FORMAT_ZETA_Z16: {
            uint16_t z = clear_zstencil & 0xFFFF;
            /* FIXME: Remove bit for stencil clear? */
            if (pg->surface_shape.z_format) {
                gl_clear_depth = convert_f16_to_float(z) / f16_max;
            } else {
                gl_clear_depth = z / (float)0xFFFF;
            }
            break;
        }
        case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8: {
            gl_clear_stencil = clear_zstencil & 0xFF;
            uint32_t z = clear_zstencil >> 8;
            if (pg->surface_shape.z_format) {
                gl_clear_depth = convert_f24_to_float(z) / f24_max;
            } else {
                gl_clear_depth = z / (float)0xFFFFFF;
            }
            break;
        }
        default:
            fprintf(stderr, "Unknown zeta surface format: 0x%x\n", pg->surface_shape.zeta_format);
            assert(false);
            break;
        }
        if (parameter & NV097_CLEAR_SURFACE_Z) {
            gl_mask |= GL_DEPTH_BUFFER_BIT;
            glDepthMask(GL_TRUE);
            glClearDepth(gl_clear_depth);
        }
        if (parameter & NV097_CLEAR_SURFACE_STENCIL) {
            gl_mask |= GL_STENCIL_BUFFER_BIT;
            glStencilMask(0xff);
            glClearStencil(gl_clear_stencil);
        }
    }
    if (write_color) {
        gl_mask |= GL_COLOR_BUFFER_BIT;
        glColorMask((parameter & NV097_CLEAR_SURFACE_R)
                         ? GL_TRUE : GL_FALSE,
                    (parameter & NV097_CLEAR_SURFACE_G)
                         ? GL_TRUE : GL_FALSE,
                    (parameter & NV097_CLEAR_SURFACE_B)
                         ? GL_TRUE : GL_FALSE,
                    (parameter & NV097_CLEAR_SURFACE_A)
                         ? GL_TRUE : GL_FALSE);
        uint32_t clear_color = d->pgraph.regs[NV_PGRAPH_COLORCLEARVALUE];

        /* Handle RGB */
        GLfloat red, green, blue;
        switch(pg->surface_shape.color_format) {
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_O1R5G5B5:
            red = ((clear_color >> 10) & 0x1F) / 31.0f;
            green = ((clear_color >> 5) & 0x1F) / 31.0f;
            blue = (clear_color & 0x1F) / 31.0f;
            break;
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5:
            red = ((clear_color >> 11) & 0x1F) / 31.0f;
            green = ((clear_color >> 5) & 0x3F) / 63.0f;
            blue = (clear_color & 0x1F) / 31.0f;
            break;
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_O8R8G8B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8:
            red = ((clear_color >> 16) & 0xFF) / 255.0f;
            green = ((clear_color >> 8) & 0xFF) / 255.0f;
            blue = (clear_color & 0xFF) / 255.0f;
            break;
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_G8B8:
            /* Xbox D3D doesn't support clearing those */
        default:
            red = 1.0f;
            green = 0.0f;
            blue = 1.0f;
            fprintf(stderr, "CLEAR_SURFACE for color_format 0x%x unsupported",
                    pg->surface_shape.color_format);
            assert(false);
            break;
        }

        /* Handle alpha */
        GLfloat alpha;
        switch(pg->surface_shape.color_format) {
        /* FIXME: CLEAR_SURFACE seems to work like memset, so maybe we
         *        also have to clear non-alpha bits with alpha value?
         *        As GL doesn't own those pixels we'd have to do this on
         *        our own in xbox memory.
         */
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_Z1A7R8G8B8:
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1A7R8G8B8_O1A7R8G8B8:
            alpha = ((clear_color >> 24) & 0x7F) / 127.0f;
            assert(false); /* Untested */
            break;
        case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8:
            alpha = ((clear_color >> 24) & 0xFF) / 255.0f;
            break;
        default:
            alpha = 1.0f;
            break;
        }

        glClearColor(red, green, blue, alpha);
    }

    pgraph_update_surface(d, true, write_color, write_zeta);

    /* FIXME: Needs confirmation */
    unsigned int xmin =
        GET_MASK(pg->regs[NV_PGRAPH_CLEARRECTX], NV_PGRAPH_CLEARRECTX_XMIN);
    unsigned int xmax =
        GET_MASK(pg->regs[NV_PGRAPH_CLEARRECTX], NV_PGRAPH_CLEARRECTX_XMAX);
    unsigned int ymin =
        GET_MASK(pg->regs[NV_PGRAPH_CLEARRECTY], NV_PGRAPH_CLEARRECTY_YMIN);
    unsigned int ymax =
        GET_MASK(pg->regs[NV_PGRAPH_CLEARRECTY], NV_PGRAPH_CLEARRECTY_YMAX);

    NV2A_DPRINTF(
        "------------------CLEAR 0x%x %d,%d - %d,%d  %x---------------\n",
        parameter, xmin, ymin, xmax, ymax,
        d->pgraph.regs[NV_PGRAPH_COLORCLEARVALUE]);

    unsigned int scissor_width = xmax - xmin + 1,
                 scissor_height = ymax - ymin + 1;
    pgraph_apply_anti_aliasing_factor(pg, &xmin, &ymin);
    pgraph_apply_anti_aliasing_factor(pg, &scissor_width, &scissor_height);
    ymin = pg->surface_binding_dim.height - (ymin + scissor_height);

    NV2A_DPRINTF("Translated clear rect to %d,%d - %d,%d\n", xmin, ymin,
                 xmin + scissor_width - 1, ymin + scissor_height - 1);

    bool full_clear = !xmin && !ymin &&
                      scissor_width >= pg->surface_binding_dim.width &&
                      scissor_height >= pg->surface_binding_dim.height;

    pgraph_apply_scaling_factor(pg, &xmin, &ymin);
    pgraph_apply_scaling_factor(pg, &scissor_width, &scissor_height);

    /* FIXME: Respect window clip?!?! */
    glEnable(GL_SCISSOR_TEST);
    glScissor(xmin, ymin, scissor_width, scissor_height);

    /* Dither */
    /* FIXME: Maybe also disable it here? + GL implementation dependent */
    if (pg->regs[NV_PGRAPH_CONTROL_0] & NV_PGRAPH_CONTROL_0_DITHERENABLE) {
        glEnable(GL_DITHER);
    } else {
        glDisable(GL_DITHER);
    }

    glClear(gl_mask);

    glDisable(GL_SCISSOR_TEST);

    pgraph_set_surface_dirty(pg, write_color, write_zeta);

    if (pg->color_binding) {
        pg->color_binding->cleared = full_clear && write_color;
    }
    if (pg->zeta_binding) {
        pg->zeta_binding->cleared = full_clear && write_zeta;
    }

    pg->clearing = false;
}

DEF_METHOD(NV097, SET_CLEAR_RECT_HORIZONTAL)
{
    pg->regs[NV_PGRAPH_CLEARRECTX] = parameter;
}

DEF_METHOD(NV097, SET_CLEAR_RECT_VERTICAL)
{
    pg->regs[NV_PGRAPH_CLEARRECTY] = parameter;
}

DEF_METHOD_INC(NV097, SET_SPECULAR_FOG_FACTOR)
{
    int slot = (method - NV097_SET_SPECULAR_FOG_FACTOR) / 4;
    pg->regs[NV_PGRAPH_SPECFOGFACTOR0 + slot*4] = parameter;
}

DEF_METHOD(NV097, SET_SHADER_CLIP_PLANE_MODE)
{
    pg->regs[NV_PGRAPH_SHADERCLIPMODE] = parameter;
}

DEF_METHOD_INC(NV097, SET_COMBINER_COLOR_OCW)
{
    int slot = (method - NV097_SET_COMBINER_COLOR_OCW) / 4;
    pg->regs[NV_PGRAPH_COMBINECOLORO0 + slot*4] = parameter;
}

DEF_METHOD(NV097, SET_COMBINER_CONTROL)
{
    pg->regs[NV_PGRAPH_COMBINECTL] = parameter;
}

DEF_METHOD(NV097, SET_SHADOW_ZSLOPE_THRESHOLD)
{
    pg->regs[NV_PGRAPH_SHADOWZSLOPETHRESHOLD] = parameter;
    assert(parameter == 0x7F800000); /* FIXME: Unimplemented */
}

DEF_METHOD(NV097, SET_SHADOW_DEPTH_FUNC)
{
    SET_MASK(pg->regs[NV_PGRAPH_SHADOWCTL], NV_PGRAPH_SHADOWCTL_SHADOW_ZFUNC,
             parameter);
}

DEF_METHOD(NV097, SET_SHADER_STAGE_PROGRAM)
{
    pg->regs[NV_PGRAPH_SHADERPROG] = parameter;
}

DEF_METHOD(NV097, SET_DOT_RGBMAPPING)
{
    SET_MASK(pg->regs[NV_PGRAPH_SHADERCTL], 0xFFF,
             GET_MASK(parameter, 0xFFF));
}

DEF_METHOD(NV097, SET_SHADER_OTHER_STAGE_INPUT)
{
    SET_MASK(pg->regs[NV_PGRAPH_SHADERCTL], 0xFFFF000,
             GET_MASK(parameter, 0xFFFF000));
}

DEF_METHOD_INC(NV097, SET_TRANSFORM_DATA)
{
    int slot = (method - NV097_SET_TRANSFORM_DATA) / 4;
    pg->vertex_state_shader_v0[slot] = parameter;
}

DEF_METHOD(NV097, LAUNCH_TRANSFORM_PROGRAM)
{
    unsigned int program_start = parameter;
    assert(program_start < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    Nv2aVshProgram program;
    Nv2aVshParseResult result = nv2a_vsh_parse_program(
            &program,
            pg->program_data[program_start],
            NV2A_MAX_TRANSFORM_PROGRAM_LENGTH - program_start);
    assert(result == NV2AVPR_SUCCESS);

    Nv2aVshCPUXVSSExecutionState state_linkage;
    Nv2aVshExecutionState state = nv2a_vsh_emu_initialize_xss_execution_state(
            &state_linkage, (float*)pg->vsh_constants);
    memcpy(state_linkage.input_regs, pg->vertex_state_shader_v0, sizeof(pg->vertex_state_shader_v0));

    nv2a_vsh_emu_execute_track_context_writes(&state, &program, pg->vsh_constants_dirty);

    nv2a_vsh_program_destroy(&program);
}

DEF_METHOD(NV097, SET_TRANSFORM_EXECUTION_MODE)
{
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_MODE,
             GET_MASK(parameter,
                      NV097_SET_TRANSFORM_EXECUTION_MODE_MODE));
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_D], NV_PGRAPH_CSV0_D_RANGE_MODE,
             GET_MASK(parameter,
                      NV097_SET_TRANSFORM_EXECUTION_MODE_RANGE_MODE));
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_CXT_WRITE_EN)
{
    pg->enable_vertex_program_write = parameter;
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_LOAD)
{
    assert(parameter < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    SET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
             NV_PGRAPH_CHEOPS_OFFSET_PROG_LD_PTR, parameter);
}

DEF_METHOD(NV097, SET_TRANSFORM_PROGRAM_START)
{
    assert(parameter < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH);
    SET_MASK(pg->regs[NV_PGRAPH_CSV0_C],
             NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START, parameter);
}

DEF_METHOD(NV097, SET_TRANSFORM_CONSTANT_LOAD)
{
    assert(parameter < NV2A_VERTEXSHADER_CONSTANTS);
    SET_MASK(pg->regs[NV_PGRAPH_CHEOPS_OFFSET],
             NV_PGRAPH_CHEOPS_OFFSET_CONST_LD_PTR, parameter);
}


void pgraph_context_switch(NV2AState *d, unsigned int channel_id)
{
    bool channel_valid =
        d->pgraph.regs[NV_PGRAPH_CTX_CONTROL] & NV_PGRAPH_CTX_CONTROL_CHID;
    unsigned pgraph_channel_id = GET_MASK(d->pgraph.regs[NV_PGRAPH_CTX_USER], NV_PGRAPH_CTX_USER_CHID);

    bool valid = channel_valid && pgraph_channel_id == channel_id;
    if (!valid) {
        SET_MASK(d->pgraph.regs[NV_PGRAPH_TRAPPED_ADDR],
                 NV_PGRAPH_TRAPPED_ADDR_CHID, channel_id);

        NV2A_DPRINTF("pgraph switching to ch %d\n", channel_id);

        /* TODO: hardware context switching */
        assert(!(d->pgraph.regs[NV_PGRAPH_DEBUG_3]
                & NV_PGRAPH_DEBUG_3_HW_CONTEXT_SWITCH));

        d->pgraph.waiting_for_context_switch = true;
        qemu_mutex_unlock(&d->pgraph.lock);
        qemu_mutex_lock_iothread();
        d->pgraph.pending_interrupts |= NV_PGRAPH_INTR_CONTEXT_SWITCH;
        nv2a_update_irq(d);
        qemu_mutex_unlock_iothread();
        qemu_mutex_lock(&d->pgraph.lock);
    }
}

static void pgraph_method_log(unsigned int subchannel,
                              unsigned int graphics_class,
                              unsigned int method, uint32_t parameter)
{
    const char *method_name = "?";
    static unsigned int last = 0;
    static unsigned int count = 0;

    if (last == NV097_ARRAY_ELEMENT16 && method != last) {
        method_name = "NV097_ARRAY_ELEMENT16";
        trace_nv2a_pgraph_method_abbrev(subchannel, graphics_class, last,
                                        method_name, count);
        NV2A_GL_DPRINTF(false, "pgraph method (%d) 0x%x %s * %d", subchannel,
                        last, method_name, count);
    }

    if (method != NV097_ARRAY_ELEMENT16) {
        uint32_t base = method;
        switch (graphics_class) {
        case NV_KELVIN_PRIMITIVE: {
            int idx = METHOD_ADDR_TO_INDEX(method);
            if (idx < ARRAY_SIZE(pgraph_kelvin_methods) &&
                pgraph_kelvin_methods[idx].handler) {
                method_name = pgraph_kelvin_methods[idx].name;
                base = pgraph_kelvin_methods[idx].base;
            }
            break;
        }
        default:
            break;
        }

        uint32_t offset = method - base;
        trace_nv2a_pgraph_method(subchannel, graphics_class, method,
                                 method_name, offset, parameter);
        NV2A_GL_DPRINTF(false,
                        "pgraph method (%d): 0x%" PRIx32 " -> 0x%04" PRIx32
                        " %s[%" PRId32 "] 0x%" PRIx32,
                        subchannel, graphics_class, method, method_name, offset,
                        parameter);
    }

    if (method == last) {
        count++;
    } else {
        count = 0;
    }
    last = method;
}

static void pgraph_allocate_inline_buffer_vertices(PGRAPHState *pg,
                                                   unsigned int attr)
{
    VertexAttribute *attribute = &pg->vertex_attributes[attr];

    if (attribute->inline_buffer_populated || pg->inline_buffer_length == 0) {
        return;
    }

    /* Now upload the previous attribute value */
    attribute->inline_buffer_populated = true;
    for (int i = 0; i < pg->inline_buffer_length; i++) {
        memcpy(&attribute->inline_buffer[i * 4], attribute->inline_value,
               sizeof(float) * 4);
    }
}

static void pgraph_finish_inline_buffer_vertex(PGRAPHState *pg)
{
    pgraph_check_within_begin_end_block(pg);
    assert(pg->inline_buffer_length < NV2A_MAX_BATCH_LENGTH);

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attribute = &pg->vertex_attributes[i];
        if (attribute->inline_buffer_populated) {
            memcpy(&attribute->inline_buffer[pg->inline_buffer_length * 4],
                   attribute->inline_value, sizeof(float) * 4);
        }
    }

    pg->inline_buffer_length++;
}

void nv2a_gl_context_init(void)
{
    g_nv2a_context_render = glo_context_create();
    g_nv2a_context_display = glo_context_create();
}

void nv2a_set_surface_scale_factor(unsigned int scale)
{
    NV2AState *d = g_nv2a;

    g_config.display.quality.surface_scale = scale < 1 ? 1 : scale;

    qemu_mutex_unlock_iothread();

    qemu_mutex_lock(&d->pfifo.lock);
    qatomic_set(&d->pfifo.halt, true);
    qemu_mutex_unlock(&d->pfifo.lock);

    qemu_mutex_lock(&d->pgraph.lock);
    qemu_event_reset(&d->pgraph.dirty_surfaces_download_complete);
    qatomic_set(&d->pgraph.download_dirty_surfaces_pending, true);
    qemu_mutex_unlock(&d->pgraph.lock);
    qemu_mutex_lock(&d->pfifo.lock);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&d->pgraph.dirty_surfaces_download_complete);

    qemu_mutex_lock(&d->pgraph.lock);
    qemu_event_reset(&d->pgraph.flush_complete);
    qatomic_set(&d->pgraph.flush_pending, true);
    qemu_mutex_unlock(&d->pgraph.lock);
    qemu_mutex_lock(&d->pfifo.lock);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&d->pgraph.flush_complete);

    qemu_mutex_lock(&d->pfifo.lock);
    qatomic_set(&d->pfifo.halt, false);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);

    qemu_mutex_lock_iothread();
}

unsigned int nv2a_get_surface_scale_factor(void)
{
    return g_nv2a->pgraph.surface_scale_factor;
}

static void pgraph_reload_surface_scale_factor(NV2AState *d)
{
    int factor = g_config.display.quality.surface_scale;
    d->pgraph.surface_scale_factor = factor < 1 ? 1 : factor;
}

void pgraph_init(NV2AState *d)
{
    int i;

    g_nv2a = d;
    PGRAPHState *pg = &d->pgraph;

    pgraph_reload_surface_scale_factor(d);

    pg->frame_time = 0;
    pg->draw_time = 0;
    pg->downloads_pending = false;

    qemu_mutex_init(&pg->lock);
    qemu_mutex_init(&pg->shader_cache_lock);
    qemu_event_init(&pg->gl_sync_complete, false);
    qemu_event_init(&pg->downloads_complete, false);
    qemu_event_init(&pg->dirty_surfaces_download_complete, false);
    qemu_event_init(&pg->flush_complete, false);
    qemu_event_init(&pg->shader_cache_writeback_complete, false);

    /* fire up opengl */
    glo_set_current(g_nv2a_context_render);

#ifdef DEBUG_NV2A_GL
    gl_debug_initialize();
#endif

    /* DXT textures */
    assert(glo_check_extension("GL_EXT_texture_compression_s3tc"));
    /*  Internal RGB565 texture format */
    assert(glo_check_extension("GL_ARB_ES2_compatibility"));

    GLint max_vertex_attributes;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_vertex_attributes);
    assert(max_vertex_attributes >= NV2A_VERTEXSHADER_ATTRIBUTES);


    glGenFramebuffers(1, &pg->gl_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, pg->gl_framebuffer);

    pgraph_init_render_to_texture(d);
    QTAILQ_INIT(&pg->surfaces);

    QSIMPLEQ_INIT(&pg->report_queue);

    //glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

    // Initialize texture cache
    const size_t texture_cache_size = 512;
    lru_init(&pg->texture_cache);
    pg->texture_cache_entries = malloc(texture_cache_size * sizeof(TextureLruNode));
    assert(pg->texture_cache_entries != NULL);
    for (i = 0; i < texture_cache_size; i++) {
        lru_add_free(&pg->texture_cache, &pg->texture_cache_entries[i].node);
    }

    pg->texture_cache.init_node = texture_cache_entry_init;
    pg->texture_cache.compare_nodes = texture_cache_entry_compare;
    pg->texture_cache.post_node_evict = texture_cache_entry_post_evict;

    // Initialize element cache
    const size_t element_cache_size = 50*1024;
    lru_init(&pg->element_cache);
    pg->element_cache_entries = malloc(element_cache_size * sizeof(VertexLruNode));
    assert(pg->element_cache_entries != NULL);
    GLuint element_cache_buffers[element_cache_size];
    glGenBuffers(element_cache_size, element_cache_buffers);
    for (i = 0; i < element_cache_size; i++) {
        pg->element_cache_entries[i].gl_buffer = element_cache_buffers[i];
        lru_add_free(&pg->element_cache, &pg->element_cache_entries[i].node);
    }

    pg->element_cache.init_node = vertex_cache_entry_init;
    pg->element_cache.compare_nodes = vertex_cache_entry_compare;

    shader_cache_init(pg);

    pg->material_alpha = 0.0f;
    SET_MASK(pg->regs[NV_PGRAPH_CONTROL_3], NV_PGRAPH_CONTROL_3_SHADEMODE,
         NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH);
    pg->primitive_mode = PRIM_TYPE_INVALID;

    for (i=0; i<NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attribute = &pg->vertex_attributes[i];
        glGenBuffers(1, &attribute->gl_inline_buffer);
        attribute->inline_buffer = (float*)g_malloc(NV2A_MAX_BATCH_LENGTH
                                              * sizeof(float) * 4);
        attribute->inline_buffer_populated = false;
    }
    glGenBuffers(1, &pg->gl_inline_array_buffer);

    glGenBuffers(1, &pg->gl_memory_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, pg->gl_memory_buffer);
    glBufferData(GL_ARRAY_BUFFER, memory_region_size(d->vram),
                 NULL, GL_DYNAMIC_DRAW);

    glGenVertexArrays(1, &pg->gl_vertex_array);
    glBindVertexArray(pg->gl_vertex_array);

    assert(glGetError() == GL_NO_ERROR);

    glo_set_current(g_nv2a_context_display);
    pgraph_init_display_renderer(d);

    glo_set_current(NULL);
}

void pgraph_destroy(PGRAPHState *pg)
{
    qemu_mutex_destroy(&pg->lock);
    qemu_mutex_destroy(&pg->shader_cache_lock);

    glo_set_current(g_nv2a_context_render);

    // TODO: clear out surfaces

    glDeleteFramebuffers(1, &pg->gl_framebuffer);

    // Clear out shader cache
    shader_write_cache_reload_list(pg);
    free(pg->shader_cache_entries);

    // Clear out texture cache
    lru_flush(&pg->texture_cache);
    free(pg->texture_cache_entries);

    glo_set_current(NULL);
    glo_context_destroy(g_nv2a_context_render);
    glo_context_destroy(g_nv2a_context_display);
}

static void pgraph_shader_update_constants(PGRAPHState *pg,
                                           ShaderBinding *binding,
                                           bool binding_changed,
                                           bool vertex_program,
                                           bool fixed_function)
{
    int i, j;

    /* update combiner constants */
    for (i = 0; i < 9; i++) {
        uint32_t constant[2];
        if (i == 8) {
            /* final combiner */
            constant[0] = pg->regs[NV_PGRAPH_SPECFOGFACTOR0];
            constant[1] = pg->regs[NV_PGRAPH_SPECFOGFACTOR1];
        } else {
            constant[0] = pg->regs[NV_PGRAPH_COMBINEFACTOR0 + i * 4];
            constant[1] = pg->regs[NV_PGRAPH_COMBINEFACTOR1 + i * 4];
        }

        for (j = 0; j < 2; j++) {
            GLint loc = binding->psh_constant_loc[i][j];
            if (loc != -1) {
                float value[4];
                value[0] = (float) ((constant[j] >> 16) & 0xFF) / 255.0f;
                value[1] = (float) ((constant[j] >> 8) & 0xFF) / 255.0f;
                value[2] = (float) (constant[j] & 0xFF) / 255.0f;
                value[3] = (float) ((constant[j] >> 24) & 0xFF) / 255.0f;

                glUniform4fv(loc, 1, value);
            }
        }
    }
    if (binding->alpha_ref_loc != -1) {
        float alpha_ref = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
                                   NV_PGRAPH_CONTROL_0_ALPHAREF) / 255.0;
        glUniform1f(binding->alpha_ref_loc, alpha_ref);
    }


    /* For each texture stage */
    for (i = 0; i < NV2A_MAX_TEXTURES; i++) {
        GLint loc;

        /* Bump luminance only during stages 1 - 3 */
        if (i > 0) {
            loc = binding->bump_mat_loc[i];
            if (loc != -1) {
                float m[4];
                m[0] = *(float*)&pg->regs[NV_PGRAPH_BUMPMAT00 + 4 * (i - 1)];
                m[1] = *(float*)&pg->regs[NV_PGRAPH_BUMPMAT01 + 4 * (i - 1)];
                m[2] = *(float*)&pg->regs[NV_PGRAPH_BUMPMAT10 + 4 * (i - 1)];
                m[3] = *(float*)&pg->regs[NV_PGRAPH_BUMPMAT11 + 4 * (i - 1)];
                glUniformMatrix2fv(loc, 1, GL_FALSE, m);
            }
            loc = binding->bump_scale_loc[i];
            if (loc != -1) {
                glUniform1f(loc, *(float*)&pg->regs[
                                NV_PGRAPH_BUMPSCALE1 + (i - 1) * 4]);
            }
            loc = binding->bump_offset_loc[i];
            if (loc != -1) {
                glUniform1f(loc, *(float*)&pg->regs[
                            NV_PGRAPH_BUMPOFFSET1 + (i - 1) * 4]);
            }
        }

        loc = pg->shader_binding->tex_scale_loc[i];
        if (loc != -1) {
            assert(pg->texture_binding[i] != NULL);
            glUniform1f(loc, (float)pg->texture_binding[i]->scale);
        }
    }

    if (binding->fog_color_loc != -1) {
        uint32_t fog_color = pg->regs[NV_PGRAPH_FOGCOLOR];
        glUniform4f(binding->fog_color_loc,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_RED) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_GREEN) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_BLUE) / 255.0,
                    GET_MASK(fog_color, NV_PGRAPH_FOGCOLOR_ALPHA) / 255.0);
    }
    if (binding->fog_param_loc[0] != -1) {
        glUniform1f(binding->fog_param_loc[0],
                    *(float*)&pg->regs[NV_PGRAPH_FOGPARAM0]);
    }
    if (binding->fog_param_loc[1] != -1) {
        glUniform1f(binding->fog_param_loc[1],
                    *(float*)&pg->regs[NV_PGRAPH_FOGPARAM1]);
    }

    float zmax;
    switch (pg->surface_shape.zeta_format) {
    case NV097_SET_SURFACE_FORMAT_ZETA_Z16:
        zmax = pg->surface_shape.z_format ? f16_max : (float)0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT_ZETA_Z24S8:
        zmax = pg->surface_shape.z_format ? f24_max : (float)0xFFFFFF;
        break;
    default:
        assert(0);
    }

    if (fixed_function) {
        /* update lighting constants */
        struct {
            uint32_t* v;
            bool* dirty;
            GLint* locs;
            size_t len;
        } lighting_arrays[] = {
            {&pg->ltctxa[0][0], &pg->ltctxa_dirty[0], binding->ltctxa_loc, NV2A_LTCTXA_COUNT},
            {&pg->ltctxb[0][0], &pg->ltctxb_dirty[0], binding->ltctxb_loc, NV2A_LTCTXB_COUNT},
            {&pg->ltc1[0][0], &pg->ltc1_dirty[0], binding->ltc1_loc, NV2A_LTC1_COUNT},
        };

        for (i=0; i<ARRAY_SIZE(lighting_arrays); i++) {
            uint32_t *lighting_v = lighting_arrays[i].v;
            bool *lighting_dirty = lighting_arrays[i].dirty;
            GLint *lighting_locs = lighting_arrays[i].locs;
            size_t lighting_len = lighting_arrays[i].len;
            for (j=0; j<lighting_len; j++) {
                if (!lighting_dirty[j] && !binding_changed) continue;
                GLint loc = lighting_locs[j];
                if (loc != -1) {
                    glUniform4fv(loc, 1, (const GLfloat*)&lighting_v[j*4]);
                }
                lighting_dirty[j] = false;
            }
        }

        for (i = 0; i < NV2A_MAX_LIGHTS; i++) {
            GLint loc;
            loc = binding->light_infinite_half_vector_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_infinite_half_vector[i]);
            }
            loc = binding->light_infinite_direction_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_infinite_direction[i]);
            }

            loc = binding->light_local_position_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_local_position[i]);
            }
            loc = binding->light_local_attenuation_loc[i];
            if (loc != -1) {
                glUniform3fv(loc, 1, pg->light_local_attenuation[i]);
            }
        }

        /* estimate the viewport by assuming it matches the surface ... */
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);

        float m11 = 0.5 * (pg->surface_binding_dim.width/aa_width);
        float m22 = -0.5 * (pg->surface_binding_dim.height/aa_height);
        float m33 = zmax;
        float m41 = *(float*)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][0];
        float m42 = *(float*)&pg->vsh_constants[NV_IGRAPH_XF_XFCTX_VPOFF][1];

        float invViewport[16] = {
            1.0/m11, 0, 0, 0,
            0, 1.0/m22, 0, 0,
            0, 0, 1.0/m33, 0,
            -1.0+m41/m11, 1.0+m42/m22, 0, 1.0
        };

        if (binding->inv_viewport_loc != -1) {
            glUniformMatrix4fv(binding->inv_viewport_loc,
                               1, GL_FALSE, &invViewport[0]);
        }
    }

    /* update vertex program constants */
    for (i=0; i<NV2A_VERTEXSHADER_CONSTANTS; i++) {
        if (!pg->vsh_constants_dirty[i] && !binding_changed) continue;

        GLint loc = binding->vsh_constant_loc[i];
        if ((loc != -1) &&
            memcmp(binding->vsh_constants[i], pg->vsh_constants[i],
                   sizeof(pg->vsh_constants[1]))) {
            glUniform4fv(loc, 1, (const GLfloat *)pg->vsh_constants[i]);
            memcpy(binding->vsh_constants[i], pg->vsh_constants[i],
                   sizeof(pg->vsh_constants[i]));
        }

        pg->vsh_constants_dirty[i] = false;
    }

    if (binding->surface_size_loc != -1) {
        unsigned int aa_width = 1, aa_height = 1;
        pgraph_apply_anti_aliasing_factor(pg, &aa_width, &aa_height);
        glUniform2f(binding->surface_size_loc,
                    pg->surface_binding_dim.width / aa_width,
                    pg->surface_binding_dim.height / aa_height);
    }

    if (binding->clip_range_loc != -1) {
        float zclip_min = *(float*)&pg->regs[NV_PGRAPH_ZCLIPMIN] / zmax * 2.0 - 1.0;
        float zclip_max = *(float*)&pg->regs[NV_PGRAPH_ZCLIPMAX] / zmax * 2.0 - 1.0;
        glUniform4f(binding->clip_range_loc, 0, zmax, zclip_min, zclip_max);
    }

    /* Clipping regions */
    unsigned int max_gl_width = pg->surface_binding_dim.width;
    unsigned int max_gl_height = pg->surface_binding_dim.height;
    pgraph_apply_scaling_factor(pg, &max_gl_width, &max_gl_height);

    for (i = 0; i < 8; i++) {
        uint32_t x = pg->regs[NV_PGRAPH_WINDOWCLIPX0 + i * 4];
        unsigned int x_min = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMIN);
        unsigned int x_max = GET_MASK(x, NV_PGRAPH_WINDOWCLIPX0_XMAX) + 1;
        uint32_t y = pg->regs[NV_PGRAPH_WINDOWCLIPY0 + i * 4];
        unsigned int y_min = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMIN);
        unsigned int y_max = GET_MASK(y, NV_PGRAPH_WINDOWCLIPY0_YMAX) + 1;
        pgraph_apply_anti_aliasing_factor(pg, &x_min, &y_min);
        pgraph_apply_anti_aliasing_factor(pg, &x_max, &y_max);

        pgraph_apply_scaling_factor(pg, &x_min, &y_min);
        pgraph_apply_scaling_factor(pg, &x_max, &y_max);

        /* Translate for the GL viewport origin */
        int y_min_xlat = MAX((int)max_gl_height - (int)y_max, 0);
        int y_max_xlat = MIN((int)max_gl_height - (int)y_min, max_gl_height);

        glUniform4i(pg->shader_binding->clip_region_loc[i],
                    x_min, y_min_xlat, x_max, y_max_xlat);
    }

    if (binding->material_alpha_loc != -1) {
        glUniform1f(binding->material_alpha_loc, pg->material_alpha);
    }
}

static bool pgraph_bind_shaders_test_dirty(PGRAPHState *pg)
{
    #define CR_1(reg) CR_x(reg, 1)
    #define CR_4(reg) CR_x(reg, 4)
    #define CR_8(reg) CR_x(reg, 8)
    #define CF(src, name)  CF_x(typeof(src), (&src), name, 1)
    #define CFA(src, name) CF_x(typeof(src[0]), src, name, ARRAY_SIZE(src))
    #define CNAME(name) reg_check__ ## name
    #define CX_x__define(type, name, x) static type CNAME(name)[x];
    #define CR_x__define(reg, x) CX_x__define(uint32_t, reg, x)
    #define CF_x__define(type, src, name, x) CX_x__define(type, name, x)
    #define CR_x__check(reg, x) \
        for (int i = 0; i < x; i++) { if (pg->regs[reg+i*4] != CNAME(reg)[i]) goto dirty; }
    #define CF_x__check(type, src, name, x) \
        for (int i = 0; i < x; i++) { if (src[i] != CNAME(name)[i]) goto dirty; }
    #define CR_x__update(reg, x) \
        for (int i = 0; i < x; i++) { CNAME(reg)[i] = pg->regs[reg+i*4]; }
    #define CF_x__update(type, src, name, x) \
        for (int i = 0; i < x; i++) { CNAME(name)[i] = src[i]; }

    #define DIRTY_REGS \
        CR_1(NV_PGRAPH_COMBINECTL) \
        CR_1(NV_PGRAPH_SHADERCTL) \
        CR_1(NV_PGRAPH_SHADOWCTL) \
        CR_1(NV_PGRAPH_COMBINESPECFOG0) \
        CR_1(NV_PGRAPH_COMBINESPECFOG1) \
        CR_1(NV_PGRAPH_CONTROL_0) \
        CR_1(NV_PGRAPH_CONTROL_3) \
        CR_1(NV_PGRAPH_CSV0_C) \
        CR_1(NV_PGRAPH_CSV0_D) \
        CR_1(NV_PGRAPH_CSV1_A) \
        CR_1(NV_PGRAPH_CSV1_B) \
        CR_1(NV_PGRAPH_SETUPRASTER) \
        CR_1(NV_PGRAPH_SHADERPROG) \
        CR_8(NV_PGRAPH_COMBINECOLORI0) \
        CR_8(NV_PGRAPH_COMBINECOLORO0) \
        CR_8(NV_PGRAPH_COMBINEALPHAI0) \
        CR_8(NV_PGRAPH_COMBINEALPHAO0) \
        CR_8(NV_PGRAPH_COMBINEFACTOR0) \
        CR_8(NV_PGRAPH_COMBINEFACTOR1) \
        CR_1(NV_PGRAPH_SHADERCLIPMODE) \
        CR_4(NV_PGRAPH_TEXCTL0_0) \
        CR_4(NV_PGRAPH_TEXFMT0) \
        CR_4(NV_PGRAPH_TEXFILTER0) \
        CR_8(NV_PGRAPH_WINDOWCLIPX0) \
        CR_8(NV_PGRAPH_WINDOWCLIPY0) \
        CF(pg->primitive_mode, primitive_mode) \
        CF(pg->surface_scale_factor, surface_scale_factor) \
        CF(pg->compressed_attrs, compressed_attrs) \
        CFA(pg->texture_matrix_enable, texture_matrix_enable)

    #define CR_x(reg, x) CR_x__define(reg, x)
    #define CF_x(type, src, name, x) CF_x__define(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x

    #define CR_x(reg, x) CR_x__check(reg, x)
    #define CF_x(type, src, name, x) CF_x__check(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x
    return false;

dirty:
    #define CR_x(reg, x) CR_x__update(reg, x)
    #define CF_x(type, src, name, x) CF_x__update(type, src, name, x)
    DIRTY_REGS
    #undef CR_x
    #undef CF_x
    return true;
}

static void pgraph_bind_shaders(PGRAPHState *pg)
{
    int i, j;

    bool vertex_program = GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                   NV_PGRAPH_CSV0_D_MODE) == 2;

    bool fixed_function = GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                   NV_PGRAPH_CSV0_D_MODE) == 0;

    int program_start = GET_MASK(pg->regs[NV_PGRAPH_CSV0_C],
                                 NV_PGRAPH_CSV0_C_CHEOPS_PROGRAM_START);

    NV2A_GL_DGROUP_BEGIN("%s (VP: %s FFP: %s)", __func__,
                         vertex_program ? "yes" : "no",
                         fixed_function ? "yes" : "no");

    bool binding_changed = false;
    if (!pgraph_bind_shaders_test_dirty(pg) && !pg->program_data_dirty) {
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND_NOTDIRTY);
        goto update_constants;
    }

    pg->program_data_dirty = false;

    ShaderBinding* old_binding = pg->shader_binding;

    ShaderState state;
    memset(&state, 0, sizeof(ShaderState));

    state.surface_scale_factor = pg->surface_scale_factor;

    state.compressed_attrs = pg->compressed_attrs;

    /* register combiner stuff */
    state.psh.window_clip_exclusive = pg->regs[NV_PGRAPH_SETUPRASTER]
                                       & NV_PGRAPH_SETUPRASTER_WINDOWCLIPTYPE;
    state.psh.combiner_control = pg->regs[NV_PGRAPH_COMBINECTL];
    state.psh.shader_stage_program = pg->regs[NV_PGRAPH_SHADERPROG];
    state.psh.other_stage_input = pg->regs[NV_PGRAPH_SHADERCTL];
    state.psh.final_inputs_0 = pg->regs[NV_PGRAPH_COMBINESPECFOG0];
    state.psh.final_inputs_1 = pg->regs[NV_PGRAPH_COMBINESPECFOG1];

    state.psh.alpha_test = pg->regs[NV_PGRAPH_CONTROL_0]
                            & NV_PGRAPH_CONTROL_0_ALPHATESTENABLE;
    state.psh.alpha_func = (enum PshAlphaFunc)GET_MASK(pg->regs[NV_PGRAPH_CONTROL_0],
                                   NV_PGRAPH_CONTROL_0_ALPHAFUNC);

    state.psh.point_sprite = pg->regs[NV_PGRAPH_SETUPRASTER] &
                                 NV_PGRAPH_SETUPRASTER_POINTSMOOTHENABLE;

    state.psh.shadow_depth_func = (enum PshShadowDepthFunc)GET_MASK(
        pg->regs[NV_PGRAPH_SHADOWCTL], NV_PGRAPH_SHADOWCTL_SHADOW_ZFUNC);

    state.fixed_function = fixed_function;

    /* fixed function stuff */
    if (fixed_function) {
        state.skinning = (enum VshSkinning)GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                               NV_PGRAPH_CSV0_D_SKIN);
        state.lighting = GET_MASK(pg->regs[NV_PGRAPH_CSV0_C],
                             NV_PGRAPH_CSV0_C_LIGHTING);
        state.normalization = pg->regs[NV_PGRAPH_CSV0_C]
                           & NV_PGRAPH_CSV0_C_NORMALIZATION_ENABLE;

        /* color material */
        state.emission_src = (enum MaterialColorSource)GET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_EMISSION);
        state.ambient_src = (enum MaterialColorSource)GET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_AMBIENT);
        state.diffuse_src = (enum MaterialColorSource)GET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_DIFFUSE);
        state.specular_src = (enum MaterialColorSource)GET_MASK(pg->regs[NV_PGRAPH_CSV0_C], NV_PGRAPH_CSV0_C_SPECULAR);
    }

    /* vertex program stuff */
    state.vertex_program = vertex_program,
    state.z_perspective = pg->regs[NV_PGRAPH_CONTROL_0]
                        & NV_PGRAPH_CONTROL_0_Z_PERSPECTIVE_ENABLE;

    state.point_params_enable = GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                         NV_PGRAPH_CSV0_D_POINTPARAMSENABLE);
    state.point_size =
        GET_MASK(pg->regs[NV_PGRAPH_POINTSIZE], NV097_SET_POINT_SIZE_V) / 8.0f;
    if (state.point_params_enable) {
        for (int i = 0; i < 8; i++) {
            state.point_params[i] = pg->point_params[i];
        }
    }

    /* geometry shader stuff */
    state.primitive_mode = (enum ShaderPrimitiveMode)pg->primitive_mode;
    state.polygon_front_mode = (enum ShaderPolygonMode)GET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
                                                           NV_PGRAPH_SETUPRASTER_FRONTFACEMODE);
    state.polygon_back_mode = (enum ShaderPolygonMode)GET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
                                                          NV_PGRAPH_SETUPRASTER_BACKFACEMODE);

    state.smooth_shading = GET_MASK(pg->regs[NV_PGRAPH_CONTROL_3],
                                      NV_PGRAPH_CONTROL_3_SHADEMODE) ==
                             NV_PGRAPH_CONTROL_3_SHADEMODE_SMOOTH;
    state.psh.smooth_shading = state.smooth_shading;

    state.program_length = 0;

    if (vertex_program) {
        // copy in vertex program tokens
        for (i = program_start; i < NV2A_MAX_TRANSFORM_PROGRAM_LENGTH; i++) {
            uint32_t *cur_token = (uint32_t*)&pg->program_data[i];
            memcpy(&state.program_data[state.program_length],
                   cur_token,
                   VSH_TOKEN_SIZE * sizeof(uint32_t));
            state.program_length++;

            if (vsh_get_field(cur_token, FLD_FINAL)) {
                break;
            }
        }
    }

    /* Texgen */
    for (i = 0; i < 4; i++) {
        unsigned int reg = (i < 2) ? NV_PGRAPH_CSV1_A : NV_PGRAPH_CSV1_B;
        for (j = 0; j < 4; j++) {
            unsigned int masks[] = {
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_S : NV_PGRAPH_CSV1_A_T0_S,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_T : NV_PGRAPH_CSV1_A_T0_T,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_R : NV_PGRAPH_CSV1_A_T0_R,
                (i % 2) ? NV_PGRAPH_CSV1_A_T1_Q : NV_PGRAPH_CSV1_A_T0_Q
            };
            state.texgen[i][j] = (enum VshTexgen)GET_MASK(pg->regs[reg], masks[j]);
        }
    }

    /* Fog */
    state.fog_enable = pg->regs[NV_PGRAPH_CONTROL_3]
                           & NV_PGRAPH_CONTROL_3_FOGENABLE;
    if (state.fog_enable) {
        /*FIXME: Use CSV0_D? */
        state.fog_mode = (enum VshFogMode)GET_MASK(pg->regs[NV_PGRAPH_CONTROL_3],
                                  NV_PGRAPH_CONTROL_3_FOG_MODE);
        state.foggen = (enum VshFoggen)GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                NV_PGRAPH_CSV0_D_FOGGENMODE);
    } else {
        /* FIXME: Do we still pass the fogmode? */
        state.fog_mode = (enum VshFogMode)0;
        state.foggen = (enum VshFoggen)0;
    }

    /* Texture matrices */
    for (i = 0; i < 4; i++) {
        state.texture_matrix_enable[i] = pg->texture_matrix_enable[i];
    }

    /* Lighting */
    if (state.lighting) {
        for (i = 0; i < NV2A_MAX_LIGHTS; i++) {
            state.light[i] = (enum VshLight)GET_MASK(pg->regs[NV_PGRAPH_CSV0_D],
                                      NV_PGRAPH_CSV0_D_LIGHT0 << (i * 2));
        }
    }

    /* Copy content of enabled combiner stages */
    int num_stages = pg->regs[NV_PGRAPH_COMBINECTL] & 0xFF;
    for (i = 0; i < num_stages; i++) {
        state.psh.rgb_inputs[i] = pg->regs[NV_PGRAPH_COMBINECOLORI0 + i * 4];
        state.psh.rgb_outputs[i] = pg->regs[NV_PGRAPH_COMBINECOLORO0 + i * 4];
        state.psh.alpha_inputs[i] = pg->regs[NV_PGRAPH_COMBINEALPHAI0 + i * 4];
        state.psh.alpha_outputs[i] = pg->regs[NV_PGRAPH_COMBINEALPHAO0 + i * 4];
        //constant_0[i] = pg->regs[NV_PGRAPH_COMBINEFACTOR0 + i * 4];
        //constant_1[i] = pg->regs[NV_PGRAPH_COMBINEFACTOR1 + i * 4];
    }

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            state.psh.compare_mode[i][j] =
                (pg->regs[NV_PGRAPH_SHADERCLIPMODE] >> (4 * i + j)) & 1;
        }

        uint32_t ctl_0 = pg->regs[NV_PGRAPH_TEXCTL0_0 + i*4];
        bool enabled = pgraph_is_texture_stage_active(pg, i) &&
                       (ctl_0 & NV_PGRAPH_TEXCTL0_0_ENABLE);
        if (!enabled) {
            continue;
        }

        state.psh.alphakill[i] = ctl_0 & NV_PGRAPH_TEXCTL0_0_ALPHAKILLEN;

        uint32_t tex_fmt = pg->regs[NV_PGRAPH_TEXFMT0 + i*4];
        unsigned int color_format = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_COLOR);
        ColorFormatInfo f = kelvin_color_format_map[color_format];
        state.psh.rect_tex[i] = f.linear;

        uint32_t border_source = GET_MASK(tex_fmt,
                                          NV_PGRAPH_TEXFMT0_BORDER_SOURCE);
        bool cubemap = GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE);
        state.psh.border_logical_size[i][0] = 0.0f;
        state.psh.border_logical_size[i][1] = 0.0f;
        state.psh.border_logical_size[i][2] = 0.0f;
        if (border_source != NV_PGRAPH_TEXFMT0_BORDER_SOURCE_COLOR) {
            if (!f.linear && !cubemap) {
                // The actual texture will be (at least) double the reported
                // size and shifted by a 4 texel border but texture coordinates
                // will still be relative to the reported size.
                unsigned int reported_width =
                        1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_U);
                unsigned int reported_height =
                        1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_V);
                unsigned int reported_depth =
                    1 << GET_MASK(tex_fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_P);

                state.psh.border_logical_size[i][0] = reported_width;
                state.psh.border_logical_size[i][1] = reported_height;
                state.psh.border_logical_size[i][2] = reported_depth;

                if (reported_width < 8) {
                    state.psh.border_inv_real_size[i][0] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][0] =
                            1.0f / (reported_width * 2.0f);
                }
                if (reported_height < 8) {
                    state.psh.border_inv_real_size[i][1] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][1] =
                            1.0f / (reported_height * 2.0f);
                }
                if (reported_depth < 8) {
                    state.psh.border_inv_real_size[i][2] = 0.0625f;
                } else {
                    state.psh.border_inv_real_size[i][2] =
                            1.0f / (reported_depth * 2.0f);
                }
            } else {
                NV2A_UNIMPLEMENTED("Border source texture with linear %d cubemap %d",
                                   f.linear, cubemap);
            }
        }

        /* Keep track of whether texture data has been loaded as signed
         * normalized integers or not. This dictates whether or not we will need
         * to re-map in fragment shader for certain texture modes (e.g.
         * bumpenvmap).
         *
         * FIXME: When signed texture data is loaded as unsigned and remapped in
         * fragment shader, there may be interpolation artifacts. Fix this to
         * support signed textures more appropriately.
         */
        state.psh.snorm_tex[i] = (f.gl_internal_format == GL_RGB8_SNORM)
                                 || (f.gl_internal_format == GL_RG8_SNORM);

        state.psh.shadow_map[i] = f.depth;

        uint32_t filter = pg->regs[NV_PGRAPH_TEXFILTER0 + i*4];
        unsigned int min_filter = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MIN);
        enum ConvolutionFilter kernel = CONVOLUTION_FILTER_DISABLED;
        /* FIXME: We do not distinguish between min and mag when
         * performing convolution. Just use it if specified for min (common AA
         * case).
         */
        if (min_filter == NV_PGRAPH_TEXFILTER0_MIN_CONVOLUTION_2D_LOD0) {
            int k = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL);
            assert(k == NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL_QUINCUNX ||
                   k == NV_PGRAPH_TEXFILTER0_CONVOLUTION_KERNEL_GAUSSIAN_3);
            kernel = (enum ConvolutionFilter)k;
        }

        state.psh.conv_tex[i] = kernel;
    }

    uint64_t shader_state_hash = fast_hash((uint8_t*) &state, sizeof(ShaderState));
    qemu_mutex_lock(&pg->shader_cache_lock);
    LruNode *node = lru_lookup(&pg->shader_cache, shader_state_hash, &state);
    ShaderLruNode *snode = container_of(node, ShaderLruNode, node);
    if (snode->binding || shader_load_from_memory(snode)) {
        pg->shader_binding = snode->binding;
    } else {
        pg->shader_binding = generate_shaders(&state);
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_GEN);

        /* cache it */
        snode->binding = pg->shader_binding;
        if (g_config.perf.cache_shaders) {
            shader_cache_to_disk(snode);
        }
    }

    qemu_mutex_unlock(&pg->shader_cache_lock);

    binding_changed = (pg->shader_binding != old_binding);
    if (binding_changed) {
        nv2a_profile_inc_counter(NV2A_PROF_SHADER_BIND);
        glUseProgram(pg->shader_binding->gl_program);
    }

update_constants:
    pgraph_shader_update_constants(pg, pg->shader_binding, binding_changed,
                                   vertex_program, fixed_function);

    NV2A_GL_DGROUP_END();
}

static bool pgraph_framebuffer_dirty(PGRAPHState *pg)
{
    bool shape_changed = memcmp(&pg->surface_shape, &pg->last_surface_shape,
                                sizeof(SurfaceShape)) != 0;
    if (!shape_changed || (!pg->surface_shape.color_format
            && !pg->surface_shape.zeta_format)) {
        return false;
    }
    return true;
}

static bool pgraph_color_write_enabled(PGRAPHState *pg)
{
    return pg->regs[NV_PGRAPH_CONTROL_0] & (
        NV_PGRAPH_CONTROL_0_ALPHA_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_RED_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_GREEN_WRITE_ENABLE
        | NV_PGRAPH_CONTROL_0_BLUE_WRITE_ENABLE);
}

static bool pgraph_zeta_write_enabled(PGRAPHState *pg)
{
    return pg->regs[NV_PGRAPH_CONTROL_0] & (
        NV_PGRAPH_CONTROL_0_ZWRITEENABLE
        | NV_PGRAPH_CONTROL_0_STENCIL_WRITE_ENABLE);
}

static void pgraph_set_surface_dirty(PGRAPHState *pg, bool color, bool zeta)
{
    NV2A_DPRINTF("pgraph_set_surface_dirty(%d, %d) -- %d %d\n",
                 color, zeta,
                 pgraph_color_write_enabled(pg), pgraph_zeta_write_enabled(pg));
    /* FIXME: Does this apply to CLEARs too? */
    color = color && pgraph_color_write_enabled(pg);
    zeta = zeta && pgraph_zeta_write_enabled(pg);
    pg->surface_color.draw_dirty |= color;
    pg->surface_zeta.draw_dirty |= zeta;

    if (pg->color_binding) {
        pg->color_binding->draw_dirty |= color;
        pg->color_binding->frame_time = pg->frame_time;
        pg->color_binding->cleared = false;

    }

    if (pg->zeta_binding) {
        pg->zeta_binding->draw_dirty |= zeta;
        pg->zeta_binding->frame_time = pg->frame_time;
        pg->zeta_binding->cleared = false;

    }
}

static GLuint pgraph_compile_shader(const char *vs_src, const char *fs_src)
{
    GLint status;
    char err_buf[512];

    // Compile vertex shader
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(vs, sizeof(err_buf), NULL, err_buf);
        err_buf[sizeof(err_buf)-1] = '\0';
        fprintf(stderr, "Vertex shader compilation failed: %s\n", err_buf);
        exit(1);
    }

    // Compile fragment shader
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        glGetShaderInfoLog(fs, sizeof(err_buf), NULL, err_buf);
        err_buf[sizeof(err_buf)-1] = '\0';
        fprintf(stderr, "Fragment shader compilation failed: %s\n", err_buf);
        exit(1);
    }

    // Link vertex and fragment shaders
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    // Flag shaders for deletion (will still be retained for lifetime of prog)
    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

static void pgraph_init_render_to_texture(NV2AState *d)
{
    struct PGRAPHState *pg = &d->pgraph;
    const char *vs =
        "#version 330\n"
        "void main()\n"
        "{\n"
        "    float x = -1.0 + float((gl_VertexID & 1) << 2);\n"
        "    float y = -1.0 + float((gl_VertexID & 2) << 1);\n"
        "    gl_Position = vec4(x, y, 0, 1);\n"
        "}\n";
    const char *fs =
        "#version 330\n"
        "uniform sampler2D tex;\n"
        "uniform vec2 surface_size;\n"
        "layout(location = 0) out vec4 out_Color;\n"
        "void main()\n"
        "{\n"
        "    vec2 texCoord;\n"
        "    texCoord.x = gl_FragCoord.x;\n"
        "    texCoord.y = (surface_size.y - gl_FragCoord.y)\n"
        "                 + (textureSize(tex,0).y - surface_size.y);\n"
        "    texCoord /= textureSize(tex,0).xy;\n"
        "    out_Color.rgba = texture(tex, texCoord);\n"
        "}\n";

    pg->s2t_rndr.prog = pgraph_compile_shader(vs, fs);
    pg->s2t_rndr.tex_loc = glGetUniformLocation(pg->s2t_rndr.prog, "tex");
    pg->s2t_rndr.surface_size_loc = glGetUniformLocation(pg->s2t_rndr.prog,
                                                    "surface_size");

    glGenVertexArrays(1, &pg->s2t_rndr.vao);
    glBindVertexArray(pg->s2t_rndr.vao);
    glGenBuffers(1, &pg->s2t_rndr.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pg->s2t_rndr.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glGenFramebuffers(1, &pg->s2t_rndr.fbo);
}

static bool pgraph_surface_to_texture_can_fastpath(SurfaceBinding *surface,
                                                   TextureShape *shape)
{
    // FIXME: Better checks/handling on formats and surface-texture compat

    int surface_fmt = surface->shape.color_format;
    int texture_fmt = shape->color_format;

    if (!surface->color) {
        // FIXME: Support zeta to color
        return false;
    }

    switch (surface_fmt) {
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8: switch(texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8: return true;
        default: break;
        }
        break;
    default: break;
    }

    trace_nv2a_pgraph_surface_texture_compat_failed(
        surface_fmt, texture_fmt);
    return false;
}


static void pgraph_render_surface_to(NV2AState *d, SurfaceBinding *surface,
                                     int texture_unit, GLuint gl_target,
                                     GLuint gl_texture, unsigned int width,
                                     unsigned int height)
{
    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindFramebuffer(GL_FRAMEBUFFER, d->pgraph.s2t_rndr.fbo);

    GLenum draw_buffers[1] = { GL_COLOR_ATTACHMENT0 };
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_target,
                           gl_texture, 0);
    glDrawBuffers(1, draw_buffers);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    assert(glGetError() == GL_NO_ERROR);

    float color[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glBindTexture(GL_TEXTURE_2D, surface->gl_buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);

    glBindVertexArray(d->pgraph.s2t_rndr.vao);
    glBindBuffer(GL_ARRAY_BUFFER, d->pgraph.s2t_rndr.vbo);
    glUseProgram(d->pgraph.s2t_rndr.prog);
    glProgramUniform1i(d->pgraph.s2t_rndr.prog, d->pgraph.s2t_rndr.tex_loc,
                       texture_unit);
    glProgramUniform2f(d->pgraph.s2t_rndr.prog,
                       d->pgraph.s2t_rndr.surface_size_loc, width, height);

    glViewport(0, 0, width, height);
    glColorMask(true, true, true, true);
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_target, 0,
                           0);
    glBindFramebuffer(GL_FRAMEBUFFER, d->pgraph.gl_framebuffer);
    glBindVertexArray(d->pgraph.gl_vertex_array);
    glBindTexture(gl_target, gl_texture);
    glUseProgram(
        d->pgraph.shader_binding ? d->pgraph.shader_binding->gl_program : 0);
}

static void pgraph_render_surface_to_texture_slow(
    NV2AState *d, SurfaceBinding *surface, TextureBinding *texture,
    TextureShape *texture_shape, int texture_unit)
{
    PGRAPHState *pg = &d->pgraph;

    const ColorFormatInfo *f = &kelvin_color_format_map[texture_shape->color_format];
    assert(texture_shape->color_format < ARRAY_SIZE(kelvin_color_format_map));
    nv2a_profile_inc_counter(NV2A_PROF_SURF_TO_TEX_FALLBACK);

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(texture->gl_target, texture->gl_texture);

    unsigned int width = surface->width,
                 height = surface->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    size_t bufsize = width * height * surface->fmt.bytes_per_pixel;

    uint8_t *buf = g_malloc(bufsize);
    pgraph_download_surface_data_to_buffer(d, surface, false, true, false, buf);

    width = texture_shape->width;
    height = texture_shape->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    glTexImage2D(texture->gl_target, 0, f->gl_internal_format, width, height, 0,
                 f->gl_format, f->gl_type, buf);
    g_free(buf);
    glBindTexture(texture->gl_target, texture->gl_texture);
}

/* Note: This function is intended to be called before PGRAPH configures GL
 * state for rendering; it will configure GL state here but only restore a
 * couple of items.
 */
static void pgraph_render_surface_to_texture(NV2AState *d,
                                             SurfaceBinding *surface,
                                             TextureBinding *texture,
                                             TextureShape *texture_shape,
                                             int texture_unit)
{
    PGRAPHState *pg = &d->pgraph;

    const ColorFormatInfo *f =
        &kelvin_color_format_map[texture_shape->color_format];
    assert(texture_shape->color_format < ARRAY_SIZE(kelvin_color_format_map));

    nv2a_profile_inc_counter(NV2A_PROF_SURF_TO_TEX);

    if (!pgraph_surface_to_texture_can_fastpath(surface, texture_shape)) {
        pgraph_render_surface_to_texture_slow(d, surface, texture,
                                              texture_shape, texture_unit);
        return;
    }


    unsigned int width = texture_shape->width,
                 height = texture_shape->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(texture->gl_target, texture->gl_texture);
    glTexParameteri(texture->gl_target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(texture->gl_target, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(texture->gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(texture->gl_target, 0, f->gl_internal_format, width, height, 0,
                 f->gl_format, f->gl_type, NULL);
    glBindTexture(texture->gl_target, 0);
    pgraph_render_surface_to(d, surface, texture_unit, texture->gl_target,
                             texture->gl_texture, width, height);
    glBindTexture(texture->gl_target, texture->gl_texture);
    glUseProgram(
        d->pgraph.shader_binding ? d->pgraph.shader_binding->gl_program : 0);
}

static void pgraph_gl_fence(void)
{
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    int result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT,
                                         (GLuint64)(5000000000));
    assert(result == GL_CONDITION_SATISFIED || result == GL_ALREADY_SIGNALED);
    glDeleteSync(fence);
}

static void pgraph_init_display_renderer(NV2AState *d)
{
    struct PGRAPHState *pg = &d->pgraph;

    glGenTextures(1, &pg->gl_display_buffer);
    pg->gl_display_buffer_internal_format = 0;
    pg->gl_display_buffer_width = 0;
    pg->gl_display_buffer_height = 0;
    pg->gl_display_buffer_format = 0;
    pg->gl_display_buffer_type = 0;

    const char *vs =
        "#version 330\n"
        "void main()\n"
        "{\n"
        "    float x = -1.0 + float((gl_VertexID & 1) << 2);\n"
        "    float y = -1.0 + float((gl_VertexID & 2) << 1);\n"
        "    gl_Position = vec4(x, y, 0, 1);\n"
        "}\n";
    /* FIXME: improve interlace handling, pvideo */

    const char *fs =
        "#version 330\n"
        "uniform sampler2D tex;\n"
        "uniform bool pvideo_enable;\n"
        "uniform sampler2D pvideo_tex;\n"
        "uniform vec2 pvideo_in_pos;\n"
        "uniform vec4 pvideo_pos;\n"
        "uniform vec3 pvideo_scale;\n"
        "uniform bool pvideo_color_key_enable;\n"
        "uniform vec4 pvideo_color_key;\n"
        "uniform vec2 display_size;\n"
        "uniform float line_offset;\n"
        "layout(location = 0) out vec4 out_Color;\n"
        "void main()\n"
        "{\n"
        "    vec2 texCoord = gl_FragCoord.xy/display_size;\n"
        "    float rel = display_size.y/textureSize(tex, 0).y/line_offset;\n"
        "    texCoord.y = 1 + rel*(texCoord.y - 1);"
        "    out_Color.rgba = texture(tex, texCoord);\n"
        "    if (pvideo_enable) {\n"
        "        vec2 screenCoord = gl_FragCoord.xy - 0.5;\n"
        "        vec4 output_region = vec4(pvideo_pos.xy, pvideo_pos.xy + pvideo_pos.zw);\n"
        "        bvec4 clip = bvec4(lessThan(screenCoord, output_region.xy),\n"
        "                           greaterThan(screenCoord, output_region.zw));\n"
        "        if (!any(clip) && (!pvideo_color_key_enable || out_Color.rgba == pvideo_color_key)) {\n"
        "            vec2 out_xy = (screenCoord - pvideo_pos.xy) * pvideo_scale.z;\n"
        "            vec2 in_st = (pvideo_in_pos + out_xy * pvideo_scale.xy) / textureSize(pvideo_tex, 0);\n"
        "            in_st.y *= -1.0;\n"
        "            out_Color.rgba = texture(pvideo_tex, in_st);\n"
        "        }\n"
        "    }\n"
        "}\n";

    pg->disp_rndr.prog = pgraph_compile_shader(vs, fs);
    pg->disp_rndr.tex_loc = glGetUniformLocation(pg->disp_rndr.prog, "tex");
    pg->disp_rndr.pvideo_enable_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_enable");
    pg->disp_rndr.pvideo_tex_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_tex");
    pg->disp_rndr.pvideo_in_pos_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_in_pos");
    pg->disp_rndr.pvideo_pos_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_pos");
    pg->disp_rndr.pvideo_scale_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_scale");
    pg->disp_rndr.pvideo_color_key_enable_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_color_key_enable");
    pg->disp_rndr.pvideo_color_key_loc = glGetUniformLocation(pg->disp_rndr.prog, "pvideo_color_key");
    pg->disp_rndr.display_size_loc = glGetUniformLocation(pg->disp_rndr.prog, "display_size");
    pg->disp_rndr.line_offset_loc = glGetUniformLocation(pg->disp_rndr.prog, "line_offset");

    glGenVertexArrays(1, &pg->disp_rndr.vao);
    glBindVertexArray(pg->disp_rndr.vao);
    glGenBuffers(1, &pg->disp_rndr.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, pg->disp_rndr.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glGenFramebuffers(1, &pg->disp_rndr.fbo);
    glGenTextures(1, &pg->disp_rndr.pvideo_tex);
    assert(glGetError() == GL_NO_ERROR);
}

static uint8_t *convert_texture_data__CR8YB8CB8YA8(const uint8_t *data,
                                                   unsigned int width,
                                                   unsigned int height,
                                                   unsigned int pitch)
{
    uint8_t *converted_data = (uint8_t *)g_malloc(width * height * 4);
    int x, y;
    for (y = 0; y < height; y++) {
        const uint8_t *line = &data[y * pitch];
        const uint32_t row_offset = y * width;
        for (x = 0; x < width; x++) {
            uint8_t *pixel = &converted_data[(row_offset + x) * 4];
            convert_yuy2_to_rgb(line, x, &pixel[0], &pixel[1], &pixel[2]);
            pixel[3] = 255;
        }
    }
    return converted_data;
}

static inline float pvideo_calculate_scale(unsigned int din_dout,
                                           unsigned int output_size)
{
    float calculated_in = din_dout * (output_size - 1);
    calculated_in = floorf(calculated_in / (1 << 20) + 0.5f);
    return (calculated_in + 1.0f) / output_size;
}

static void pgraph_render_display_pvideo_overlay(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;

    // FIXME: This check against PVIDEO_SIZE_IN does not match HW behavior.
    // Many games seem to pass this value when initializing or tearing down
    // PVIDEO. On its own, this generally does not result in the overlay being
    // hidden, however there are certain games (e.g., Ultimate Beach Soccer)
    // that use an unknown mechanism to hide the overlay without explicitly
    // stopping it.
    // Since the value seems to be set to 0xFFFFFFFF only in cases where the
    // content is not valid, it is probably good enough to treat it as an
    // implicit stop.
    bool enabled = (d->pvideo.regs[NV_PVIDEO_BUFFER] & NV_PVIDEO_BUFFER_0_USE)
        && d->pvideo.regs[NV_PVIDEO_SIZE_IN] != 0xFFFFFFFF;
    glUniform1ui(d->pgraph.disp_rndr.pvideo_enable_loc, enabled);
    if (!enabled) {
        return;
    }

    hwaddr base = d->pvideo.regs[NV_PVIDEO_BASE];
    hwaddr limit = d->pvideo.regs[NV_PVIDEO_LIMIT];
    hwaddr offset = d->pvideo.regs[NV_PVIDEO_OFFSET];

    int in_width =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN], NV_PVIDEO_SIZE_IN_WIDTH);
    int in_height =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_IN], NV_PVIDEO_SIZE_IN_HEIGHT);

    int in_s = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_S);
    int in_t = GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_IN],
                        NV_PVIDEO_POINT_IN_T);

    int in_pitch =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_PITCH);
    int in_color =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_COLOR);

    unsigned int out_width =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT], NV_PVIDEO_SIZE_OUT_WIDTH);
    unsigned int out_height =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_SIZE_OUT], NV_PVIDEO_SIZE_OUT_HEIGHT);

    float scale_x = 1.0f;
    float scale_y = 1.0f;
    unsigned int ds_dx = d->pvideo.regs[NV_PVIDEO_DS_DX];
    unsigned int dt_dy = d->pvideo.regs[NV_PVIDEO_DT_DY];
    if (ds_dx != NV_PVIDEO_DIN_DOUT_UNITY) {
        scale_x = pvideo_calculate_scale(ds_dx, out_width);
    }
    if (dt_dy != NV_PVIDEO_DIN_DOUT_UNITY) {
        scale_y = pvideo_calculate_scale(dt_dy, out_height);
    }

    // On HW, setting NV_PVIDEO_SIZE_IN larger than NV_PVIDEO_SIZE_OUT results
    // in them being capped to the output size, content is not scaled. This is
    // particularly important as NV_PVIDEO_SIZE_IN may be set to 0xFFFFFFFF
    // during initialization or teardown.
    if (in_width > out_width) {
        in_width = floorf((float)out_width * scale_x + 0.5f);
    }
    if (in_height > out_height) {
        in_height = floorf((float)out_height * scale_y + 0.5f);
    }

    /* TODO: support other color formats */
    assert(in_color == NV_PVIDEO_FORMAT_COLOR_LE_CR8YB8CB8YA8);

    unsigned int out_x =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT], NV_PVIDEO_POINT_OUT_X);
    unsigned int out_y =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_POINT_OUT], NV_PVIDEO_POINT_OUT_Y);

    unsigned int color_key_enabled =
        GET_MASK(d->pvideo.regs[NV_PVIDEO_FORMAT], NV_PVIDEO_FORMAT_DISPLAY);
    glUniform1ui(d->pgraph.disp_rndr.pvideo_color_key_enable_loc,
                 color_key_enabled);

    // TODO: Verify that masking off the top byte is correct.
    // SeaBlade sets a color key of 0x80000000 but the texture passed into the
    // shader is cleared to 0 alpha.
    unsigned int color_key = d->pvideo.regs[NV_PVIDEO_COLOR_KEY] & 0xFFFFFF;
    glUniform4f(d->pgraph.disp_rndr.pvideo_color_key_loc,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_RED) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_GREEN) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_BLUE) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_ALPHA) / 255.0);

    assert(offset + in_pitch * in_height <= limit);
    hwaddr end = base + offset + in_pitch * in_height;
    assert(end <= memory_region_size(d->vram));

    pgraph_apply_scaling_factor(pg, &out_x, &out_y);
    pgraph_apply_scaling_factor(pg, &out_width, &out_height);

    // Translate for the GL viewport origin.
    out_y = MAX(pg->gl_display_buffer_height - 1 - (int)(out_y + out_height), 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, g_nv2a->pgraph.disp_rndr.pvideo_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    uint8_t *tex_rgba = convert_texture_data__CR8YB8CB8YA8(
        d->vram_ptr + base + offset, in_width, in_height, in_pitch);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, in_width, in_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex_rgba);
    g_free(tex_rgba);
    glUniform1i(d->pgraph.disp_rndr.pvideo_tex_loc, 1);
    glUniform2f(d->pgraph.disp_rndr.pvideo_in_pos_loc, in_s, in_t);
    glUniform4f(d->pgraph.disp_rndr.pvideo_pos_loc,
                out_x, out_y, out_width, out_height);
    glUniform3f(d->pgraph.disp_rndr.pvideo_scale_loc,
                scale_x, scale_y, 1.0f / pg->surface_scale_factor);
}

static void pgraph_render_display(NV2AState *d, SurfaceBinding *surface)
{
    struct PGRAPHState *pg = &d->pgraph;

    unsigned int width, height;
    uint32_t pline_offset, pstart_addr, pline_compare;
    d->vga.get_resolution(&d->vga, (int*)&width, (int*)&height);
    d->vga.get_offsets(&d->vga, &pline_offset, &pstart_addr, &pline_compare);
    int line_offset = surface->pitch / pline_offset;

    /* Adjust viewport height for interlaced mode, used only in 1080i */
    if (d->vga.cr[NV_PRMCIO_INTERLACE_MODE] != NV_PRMCIO_INTERLACE_MODE_DISABLED) {
        height *= 2;
    }

    pgraph_apply_scaling_factor(pg, &width, &height);

    glBindFramebuffer(GL_FRAMEBUFFER, d->pgraph.disp_rndr.fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, pg->gl_display_buffer);
    bool recreate = (
        surface->fmt.gl_internal_format != pg->gl_display_buffer_internal_format
        || width != pg->gl_display_buffer_width
        || height != pg->gl_display_buffer_height
        || surface->fmt.gl_format != pg->gl_display_buffer_format
        || surface->fmt.gl_type != pg->gl_display_buffer_type
        );

    if (recreate) {
        /* XXX: There's apparently a bug in some Intel OpenGL drivers for
         * Windows that will leak this texture when its orphaned after use in
         * another context, apparently regardless of which thread it's created
         * or released on.
         *
         * Driver: 27.20.100.8729 9/11/2020 W10 x64
         * Track: https://community.intel.com/t5/Graphics/OpenGL-Windows-drivers-for-Intel-HD-630-leaking-GPU-memory-when/td-p/1274423
         */
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        pg->gl_display_buffer_internal_format = surface->fmt.gl_internal_format;
        pg->gl_display_buffer_width = width;
        pg->gl_display_buffer_height = height;
        pg->gl_display_buffer_format = surface->fmt.gl_format;
        pg->gl_display_buffer_type = surface->fmt.gl_type;
        glTexImage2D(GL_TEXTURE_2D, 0,
            pg->gl_display_buffer_internal_format,
            pg->gl_display_buffer_width,
            pg->gl_display_buffer_height,
            0,
            pg->gl_display_buffer_format,
            pg->gl_display_buffer_type,
            NULL);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, pg->gl_display_buffer, 0);
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindTexture(GL_TEXTURE_2D, surface->gl_buffer);
    glBindVertexArray(pg->disp_rndr.vao);
    glBindBuffer(GL_ARRAY_BUFFER, pg->disp_rndr.vbo);
    glUseProgram(pg->disp_rndr.prog);
    glProgramUniform1i(pg->disp_rndr.prog, pg->disp_rndr.tex_loc, 0);
    glUniform2f(d->pgraph.disp_rndr.display_size_loc, width, height);
    glUniform1f(d->pgraph.disp_rndr.line_offset_loc, line_offset);
    pgraph_render_display_pvideo_overlay(d);

    glViewport(0, 0, width, height);
    glColorMask(true, true, true, true);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, 0, 0);
}

void pgraph_gl_sync(NV2AState *d)
{
    uint32_t pline_offset, pstart_addr, pline_compare;
    d->vga.get_offsets(&d->vga, &pline_offset, &pstart_addr, &pline_compare);
    SurfaceBinding *surface = pgraph_surface_get_within(d, d->pcrtc.start + pline_offset);
    if (surface == NULL) {
        qemu_event_set(&d->pgraph.gl_sync_complete);
        return;
    }

    /* FIXME: Sanity check surface dimensions */

    /* Wait for queued commands to complete */
    pgraph_upload_surface_data(d, surface, !tcg_enabled());
    pgraph_gl_fence();
    assert(glGetError() == GL_NO_ERROR);

    /* Render framebuffer in display context */
    glo_set_current(g_nv2a_context_display);
    pgraph_render_display(d, surface);
    pgraph_gl_fence();
    assert(glGetError() == GL_NO_ERROR);

    /* Switch back to original context */
    glo_set_current(g_nv2a_context_render);

    qatomic_set(&d->pgraph.gl_sync_pending, false);
    qemu_event_set(&d->pgraph.gl_sync_complete);
}

const uint8_t *nv2a_get_dac_palette(void)
{
    return g_nv2a->puserdac.palette;
}

int nv2a_get_screen_off(void)
{
    return g_nv2a->vga.sr[VGA_SEQ_CLOCK_MODE] & VGA_SR01_SCREEN_OFF;
}

int nv2a_get_framebuffer_surface(void)
{
    NV2AState *d = g_nv2a;
    PGRAPHState *pg = &d->pgraph;

    qemu_mutex_lock(&d->pfifo.lock);
    // FIXME: Possible race condition with pgraph, consider lock
    uint32_t pline_offset, pstart_addr, pline_compare;
    d->vga.get_offsets(&d->vga, &pline_offset, &pstart_addr, &pline_compare);
    SurfaceBinding *surface = pgraph_surface_get_within(d, d->pcrtc.start + pline_offset);
    if (surface == NULL || !surface->color) {
        qemu_mutex_unlock(&d->pfifo.lock);
        return 0;
    }

    assert(surface->color);
    assert(surface->fmt.gl_attachment == GL_COLOR_ATTACHMENT0);
    assert(surface->fmt.gl_format == GL_RGBA
        || surface->fmt.gl_format == GL_RGB
        || surface->fmt.gl_format == GL_BGR
        || surface->fmt.gl_format == GL_BGRA
        );

    surface->frame_time = pg->frame_time;
    qemu_event_reset(&d->pgraph.gl_sync_complete);
    qatomic_set(&pg->gl_sync_pending, true);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&d->pgraph.gl_sync_complete);

    return pg->gl_display_buffer;
}

static bool pgraph_check_surface_to_texture_compatibility(
    const SurfaceBinding *surface,
    const TextureShape *shape)
{
    // FIXME: Better checks/handling on formats and surface-texture compat

    if ((!surface->swizzle && surface->pitch != shape->pitch) ||
        surface->width != shape->width ||
        surface->height != shape->height) {
        return false;
    }

    int surface_fmt = surface->shape.color_format;
    int texture_fmt = shape->color_format;

    if (!surface->color) {
        // FIXME: Support zeta to color
        return false;
    }

    if (shape->cubemap) {
        // FIXME: Support rendering surface to cubemap face
        return false;
    }

    if (shape->levels > 1) {
        // FIXME: Support rendering surface to mip levels
        return false;
    }

    switch (surface_fmt) {
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X1R5G5B5: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R5G6B5: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R5G6B5: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8: switch(texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_X8R8G8B8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8: return true;
        default: break;
        }
        break;
    case NV097_SET_SURFACE_FORMAT_COLOR_LE_A8R8G8B8: switch (texture_fmt) {
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8B8G8R8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_R8G8B8A8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_LU_IMAGE_A8R8G8B8: return true;
        case NV097_SET_TEXTURE_FORMAT_COLOR_SZ_A8R8G8B8: return true;
        default: break;
        }
        break;
    default:
        break;
    }

    trace_nv2a_pgraph_surface_texture_compat_failed(
        surface_fmt, texture_fmt);
    return false;
}

static void pgraph_wait_for_surface_download(SurfaceBinding *e)
{
    NV2AState *d = g_nv2a;

    if (qatomic_read(&e->draw_dirty)) {
        qemu_mutex_lock(&d->pfifo.lock);
        qemu_event_reset(&d->pgraph.downloads_complete);
        qatomic_set(&e->download_pending, true);
        qatomic_set(&d->pgraph.downloads_pending, true);
        pfifo_kick(d);
        qemu_mutex_unlock(&d->pfifo.lock);
        qemu_event_wait(&d->pgraph.downloads_complete);
    }
}

static void pgraph_surface_access_callback(
    void *opaque,
    MemoryRegion *mr,
    hwaddr addr,
    hwaddr len,
    bool write)
{
    SurfaceBinding *e = opaque;
    assert(addr >= e->vram_addr);
    hwaddr offset = addr - e->vram_addr;
    assert(offset < e->size);

    if (qatomic_read(&e->draw_dirty)) {
        trace_nv2a_pgraph_surface_cpu_access(e->vram_addr, offset);
        pgraph_wait_for_surface_download(e);
    }

    if (write && !qatomic_read(&e->upload_pending)) {
        trace_nv2a_pgraph_surface_cpu_access(e->vram_addr, offset);
        qatomic_set(&e->upload_pending, true);
    }
}

static SurfaceBinding *pgraph_surface_put(NV2AState *d,
    hwaddr addr,
    SurfaceBinding *surface_in)
{
    assert(pgraph_surface_get(d, addr) == NULL);

    SurfaceBinding *surface, *next;
    uintptr_t e_end = surface_in->vram_addr + surface_in->size - 1;
    QTAILQ_FOREACH_SAFE(surface, &d->pgraph.surfaces, entry, next) {
        uintptr_t s_end = surface->vram_addr + surface->size - 1;
        bool overlapping = !(surface->vram_addr > e_end
                             || surface_in->vram_addr > s_end);
        if (overlapping) {
            trace_nv2a_pgraph_surface_evict_overlapping(
                surface->vram_addr, surface->width, surface->height,
                surface->pitch);
            pgraph_download_surface_data_if_dirty(d, surface);
            pgraph_surface_invalidate(d, surface);
        }
    }

    SurfaceBinding *surface_out = g_malloc(sizeof(SurfaceBinding));
    assert(surface_out != NULL);
    *surface_out = *surface_in;

    if (tcg_enabled()) {
        qemu_mutex_unlock(&d->pgraph.lock);
        qemu_mutex_lock_iothread();
        mem_access_callback_insert(qemu_get_cpu(0),
            d->vram, surface_out->vram_addr, surface_out->size,
            &surface_out->access_cb, &pgraph_surface_access_callback,
            surface_out);
        qemu_mutex_unlock_iothread();
        qemu_mutex_lock(&d->pgraph.lock);
    }

    QTAILQ_INSERT_TAIL(&d->pgraph.surfaces, surface_out, entry);

    return surface_out;
}

static SurfaceBinding *pgraph_surface_get(NV2AState *d, hwaddr addr)
{
    SurfaceBinding *surface;
    QTAILQ_FOREACH (surface, &d->pgraph.surfaces, entry) {
        if (surface->vram_addr == addr) {
            return surface;
        }
    }

    return NULL;
}

static SurfaceBinding *pgraph_surface_get_within(NV2AState *d, hwaddr addr)
{
    SurfaceBinding *surface;
    QTAILQ_FOREACH (surface, &d->pgraph.surfaces, entry) {
        if (addr >= surface->vram_addr &&
            addr < (surface->vram_addr + surface->size)) {
            return surface;
        }
    }

    return NULL;
}

static void pgraph_surface_invalidate(NV2AState *d, SurfaceBinding *surface)
{
    trace_nv2a_pgraph_surface_invalidated(surface->vram_addr);

    if (surface == d->pgraph.color_binding) {
        assert(d->pgraph.surface_color.buffer_dirty);
        pgraph_unbind_surface(d, true);
    }
    if (surface == d->pgraph.zeta_binding) {
        assert(d->pgraph.surface_zeta.buffer_dirty);
        pgraph_unbind_surface(d, false);
    }

    if (tcg_enabled()) {
        qemu_mutex_unlock(&d->pgraph.lock);
        qemu_mutex_lock_iothread();
        mem_access_callback_remove_by_ref(qemu_get_cpu(0), surface->access_cb);
        qemu_mutex_unlock_iothread();
        qemu_mutex_lock(&d->pgraph.lock);
    }

    glDeleteTextures(1, &surface->gl_buffer);

    QTAILQ_REMOVE(&d->pgraph.surfaces, surface, entry);
    g_free(surface);
}

static void pgraph_surface_evict_old(NV2AState *d)
{
    const int surface_age_limit = 5;

    SurfaceBinding *s, *next;
    QTAILQ_FOREACH_SAFE(s, &d->pgraph.surfaces, entry, next) {
        int last_used = d->pgraph.frame_time - s->frame_time;
        if (last_used >= surface_age_limit) {
            trace_nv2a_pgraph_surface_evict_reason("old", s->vram_addr);
            pgraph_download_surface_data_if_dirty(d, s);
            pgraph_surface_invalidate(d, s);
        }
    }
}

static bool pgraph_check_surface_compatibility(SurfaceBinding *s1,
                                               SurfaceBinding *s2, bool strict)
{
    bool format_compatible =
        (s1->color == s2->color) &&
        (s1->fmt.gl_attachment == s2->fmt.gl_attachment) &&
        (s1->fmt.gl_internal_format == s2->fmt.gl_internal_format) &&
        (s1->pitch == s2->pitch) &&
        (s1->shape.clip_x <= s2->shape.clip_x) &&
        (s1->shape.clip_y <= s2->shape.clip_y);
    if (!format_compatible) {
        return false;
    }

    if (!strict) {
        return (s1->width >= s2->width) && (s1->height >= s2->height);
    } else {
        return (s1->width == s2->width) && (s1->height == s2->height);
    }
}

static void pgraph_download_surface_data_if_dirty(NV2AState *d,
    SurfaceBinding *surface)
{
    if (surface->draw_dirty) {
        pgraph_download_surface_data(d, surface, true);
    }
}

static void pgraph_bind_current_surface(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;

    if (pg->color_binding) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, pg->color_binding->fmt.gl_attachment,
                               GL_TEXTURE_2D, pg->color_binding->gl_buffer, 0);
    }

    if (pg->zeta_binding) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, pg->zeta_binding->fmt.gl_attachment,
                               GL_TEXTURE_2D, pg->zeta_binding->gl_buffer, 0);
    }

    if (pg->color_binding || pg->zeta_binding) {
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
               GL_FRAMEBUFFER_COMPLETE);
    }
}

static void surface_copy_shrink_row(uint8_t *out, uint8_t *in,
                                    unsigned int width,
                                    unsigned int bytes_per_pixel,
                                    unsigned int factor)
{
    if (bytes_per_pixel == 4) {
        for (unsigned int x = 0; x < width; x++) {
            *(uint32_t *)out = *(uint32_t *)in;
            out += 4;
            in += 4 * factor;
        }
    } else if (bytes_per_pixel == 2) {
        for (unsigned int x = 0; x < width; x++) {
            *(uint16_t *)out = *(uint16_t *)in;
            out += 2;
            in += 2 * factor;
        }
    } else {
        for (unsigned int x = 0; x < width; x++) {
            memcpy(out, in, bytes_per_pixel);
            out += bytes_per_pixel;
            in += bytes_per_pixel * factor;
        }
    }
}


static void pgraph_download_surface_data_to_buffer(NV2AState *d,
                                                   SurfaceBinding *surface,
                                                   bool swizzle, bool flip,
                                                   bool downscale,
                                                   uint8_t *pixels)
{
    PGRAPHState *pg = &d->pgraph;
    swizzle &= surface->swizzle;
    downscale &= (pg->surface_scale_factor != 1);

    trace_nv2a_pgraph_surface_download(
        surface->color ? "COLOR" : "ZETA",
        surface->swizzle ? "sz" : "lin", surface->vram_addr,
        surface->width, surface->height, surface->pitch,
        surface->fmt.bytes_per_pixel);

    /*  Bind destination surface to framebuffer */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, surface->fmt.gl_attachment,
                           GL_TEXTURE_2D, surface->gl_buffer, 0);

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    /* Read surface into memory */
    uint8_t *gl_read_buf = pixels;

    uint8_t *swizzle_buf = pixels;
    if (swizzle) {
        /* FIXME: Allocate big buffer up front and re-alloc if necessary.
         * FIXME: Consider swizzle in shader
         */
        assert(pg->surface_scale_factor == 1 || downscale);
        swizzle_buf = (uint8_t *)g_malloc(surface->size);
        gl_read_buf = swizzle_buf;
    }

    if (downscale) {
        pg->scale_buf = (uint8_t *)g_realloc(
            pg->scale_buf, pg->surface_scale_factor * pg->surface_scale_factor *
                               surface->size);
        gl_read_buf = pg->scale_buf;
    }

    glo_readpixels(
        surface->fmt.gl_format, surface->fmt.gl_type, surface->fmt.bytes_per_pixel,
        pg->surface_scale_factor * surface->pitch,
        pg->surface_scale_factor * surface->width,
        pg->surface_scale_factor * surface->height, flip, gl_read_buf);

    /* FIXME: Replace this with a hw accelerated version */
    if (downscale) {
        assert(surface->pitch >= (surface->width * surface->fmt.bytes_per_pixel));
        uint8_t *out = swizzle_buf, *in = pg->scale_buf;
        for (unsigned int y = 0; y < surface->height; y++) {
            surface_copy_shrink_row(out, in, surface->width,
                                    surface->fmt.bytes_per_pixel,
                                    pg->surface_scale_factor);
            in += surface->pitch * pg->surface_scale_factor *
                  pg->surface_scale_factor;
            out += surface->pitch;
        }
    }

    if (swizzle) {
        swizzle_rect(swizzle_buf, surface->width, surface->height, pixels,
                     surface->pitch, surface->fmt.bytes_per_pixel);
        g_free(swizzle_buf);
    }

    /* Re-bind original framebuffer target */
    glFramebufferTexture2D(GL_FRAMEBUFFER, surface->fmt.gl_attachment,
                           GL_TEXTURE_2D, 0, 0);
    pgraph_bind_current_surface(d);
}

static void pgraph_download_surface_data(NV2AState *d, SurfaceBinding *surface,
    bool force)
{
    if (!(surface->download_pending || force)) {
        return;
    }

    /* FIXME: Respect write enable at last TOU? */

    nv2a_profile_inc_counter(NV2A_PROF_SURF_DOWNLOAD);

    pgraph_download_surface_data_to_buffer(
        d, surface, true, true, true, d->vram_ptr + surface->vram_addr);

    memory_region_set_client_dirty(d->vram, surface->vram_addr,
                                   surface->pitch * surface->height,
                                   DIRTY_MEMORY_VGA);
    memory_region_set_client_dirty(d->vram, surface->vram_addr,
                                   surface->pitch * surface->height,
                                   DIRTY_MEMORY_NV2A_TEX);

    surface->download_pending = false;
    surface->draw_dirty = false;
}

void pgraph_process_pending_downloads(NV2AState *d)
{
    SurfaceBinding *surface;
    QTAILQ_FOREACH(surface, &d->pgraph.surfaces, entry) {
        pgraph_download_surface_data(d, surface, false);
    }

    qatomic_set(&d->pgraph.downloads_pending, false);
    qemu_event_set(&d->pgraph.downloads_complete);
}

void pgraph_download_dirty_surfaces(NV2AState *d)
{
    SurfaceBinding *surface;
    QTAILQ_FOREACH(surface, &d->pgraph.surfaces, entry) {
        pgraph_download_surface_data_if_dirty(d, surface);
    }

    qatomic_set(&d->pgraph.download_dirty_surfaces_pending, false);
    qemu_event_set(&d->pgraph.dirty_surfaces_download_complete);
}


static void surface_copy_expand_row(uint8_t *out, uint8_t *in,
                                    unsigned int width,
                                    unsigned int bytes_per_pixel,
                                    unsigned int factor)
{
    if (bytes_per_pixel == 4) {
        for (unsigned int x = 0; x < width; x++) {
            for (unsigned int i = 0; i < factor; i++) {
                *(uint32_t *)out = *(uint32_t *)in;
                out += bytes_per_pixel;
            }
            in += bytes_per_pixel;
        }
    } else if (bytes_per_pixel == 2) {
        for (unsigned int x = 0; x < width; x++) {
            for (unsigned int i = 0; i < factor; i++) {
                *(uint16_t *)out = *(uint16_t *)in;
                out += bytes_per_pixel;
            }
            in += bytes_per_pixel;
        }
    } else {
        for (unsigned int x = 0; x < width; x++) {
            for (unsigned int i = 0; i < factor; i++) {
                memcpy(out, in, bytes_per_pixel);
                out += bytes_per_pixel;
            }
            in += bytes_per_pixel;
        }
    }
}

static void surface_copy_expand(uint8_t *out, uint8_t *in, unsigned int width,
                                unsigned int height,
                                unsigned int bytes_per_pixel,
                                unsigned int factor)
{
    size_t out_pitch = width * bytes_per_pixel * factor;

    for (unsigned int y = 0; y < height; y++) {
        surface_copy_expand_row(out, in, width, bytes_per_pixel, factor);
        uint8_t *row_in = out;
        for (unsigned int i = 1; i < factor; i++) {
            out += out_pitch;
            memcpy(out, row_in, out_pitch);
        }
        in += width * bytes_per_pixel;
        out += out_pitch;
    }
}

static void pgraph_upload_surface_data(NV2AState *d, SurfaceBinding *surface,
                                       bool force)
{
    if (!(surface->upload_pending || force)) {
        return;
    }

    nv2a_profile_inc_counter(NV2A_PROF_SURF_UPLOAD);

    trace_nv2a_pgraph_surface_upload(
                 surface->color ? "COLOR" : "ZETA",
                 surface->swizzle ? "sz" : "lin", surface->vram_addr,
                 surface->width, surface->height, surface->pitch,
                 surface->fmt.bytes_per_pixel);

    PGRAPHState *pg = &d->pgraph;

    surface->upload_pending = false;
    surface->draw_time = pg->draw_time;

    // FIXME: Don't query GL for texture binding
    GLint last_texture_binding;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture_binding);

    // FIXME: Replace with FBO to not disturb current state
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                           0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                           GL_TEXTURE_2D, 0, 0);

    uint8_t *data = d->vram_ptr;
    uint8_t *buf = data + surface->vram_addr;

    if (surface->swizzle) {
        buf = (uint8_t*)g_malloc(surface->size);
        unswizzle_rect(data + surface->vram_addr,
                       surface->width, surface->height,
                       buf,
                       surface->pitch,
                       surface->fmt.bytes_per_pixel);
    }

    /* FIXME: Replace this flip/scaling */

    // This is VRAM so we can't do this inplace!
    uint8_t *flipped_buf = (uint8_t *)g_malloc(
        surface->height * surface->width * surface->fmt.bytes_per_pixel);
    unsigned int irow;
    for (irow = 0; irow < surface->height; irow++) {
        memcpy(&flipped_buf[surface->width * (surface->height - irow - 1)
                                 * surface->fmt.bytes_per_pixel],
               &buf[surface->pitch * irow],
               surface->width * surface->fmt.bytes_per_pixel);
    }

    uint8_t *gl_read_buf = flipped_buf;
    unsigned int width = surface->width, height = surface->height;

    if (pg->surface_scale_factor > 1) {
        pgraph_apply_scaling_factor(pg, &width, &height);
        pg->scale_buf = (uint8_t *)g_realloc(
            pg->scale_buf, width * height * surface->fmt.bytes_per_pixel);
        gl_read_buf = pg->scale_buf;
        uint8_t *out = gl_read_buf, *in = flipped_buf;
        surface_copy_expand(out, in, surface->width, surface->height,
                            surface->fmt.bytes_per_pixel,
                            d->pgraph.surface_scale_factor);
    }

    int prev_unpack_alignment;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack_alignment);
    if (unlikely((width * surface->fmt.bytes_per_pixel) % 4 != 0)) {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    } else {
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    glBindTexture(GL_TEXTURE_2D, surface->gl_buffer);
    glTexImage2D(GL_TEXTURE_2D, 0, surface->fmt.gl_internal_format, width,
                 height, 0, surface->fmt.gl_format, surface->fmt.gl_type,
                 gl_read_buf);
    glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack_alignment);
    g_free(flipped_buf);
    if (surface->swizzle) {
        g_free(buf);
    }

    // Rebind previous framebuffer binding
    glBindTexture(GL_TEXTURE_2D, last_texture_binding);

    pgraph_bind_current_surface(d);
}

static void pgraph_compare_surfaces(SurfaceBinding *s1, SurfaceBinding *s2)
{
    #define DO_CMP(fld) \
        if (s1->fld != s2->fld) \
            trace_nv2a_pgraph_surface_compare_mismatch( \
                #fld, (long int)s1->fld, (long int)s2->fld);
    DO_CMP(shape.clip_x)
    DO_CMP(shape.clip_width)
    DO_CMP(shape.clip_y)
    DO_CMP(shape.clip_height)
    DO_CMP(gl_buffer)
    DO_CMP(fmt.bytes_per_pixel)
    DO_CMP(fmt.gl_attachment)
    DO_CMP(fmt.gl_internal_format)
    DO_CMP(fmt.gl_format)
    DO_CMP(fmt.gl_type)
    DO_CMP(color)
    DO_CMP(swizzle)
    DO_CMP(vram_addr)
    DO_CMP(width)
    DO_CMP(height)
    DO_CMP(pitch)
    DO_CMP(size)
    DO_CMP(dma_addr)
    DO_CMP(dma_len)
    DO_CMP(frame_time)
    DO_CMP(draw_time)
    #undef DO_CMP
}

static void pgraph_populate_surface_binding_entry_sized(NV2AState *d,
                                                        bool color,
                                                        unsigned int width,
                                                        unsigned int height,
                                                        SurfaceBinding *entry)
{
    PGRAPHState *pg = &d->pgraph;
    Surface *surface;
    hwaddr dma_address;
    SurfaceFormatInfo fmt;

    if (color) {
        surface = &pg->surface_color;
        dma_address = pg->dma_color;
        assert(pg->surface_shape.color_format != 0);
        assert(pg->surface_shape.color_format <
               ARRAY_SIZE(kelvin_surface_color_format_map));
        fmt = kelvin_surface_color_format_map[pg->surface_shape.color_format];
        if (fmt.bytes_per_pixel == 0) {
            fprintf(stderr, "nv2a: unimplemented color surface format 0x%x\n",
                    pg->surface_shape.color_format);
            abort();
        }
    } else {
        surface = &pg->surface_zeta;
        dma_address = pg->dma_zeta;
        assert(pg->surface_shape.zeta_format != 0);
        assert(pg->surface_shape.zeta_format <
               ARRAY_SIZE(kelvin_surface_zeta_float_format_map));
        const SurfaceFormatInfo *map =
            pg->surface_shape.z_format ? kelvin_surface_zeta_float_format_map :
                                         kelvin_surface_zeta_fixed_format_map;
        fmt = map[pg->surface_shape.zeta_format];
    }

    DMAObject dma = nv_dma_load(d, dma_address);
    /* There's a bunch of bugs that could cause us to hit this function
     * at the wrong time and get a invalid dma object.
     * Check that it's sane. */
    assert(dma.dma_class == NV_DMA_IN_MEMORY_CLASS);
    // assert(dma.address + surface->offset != 0);
    assert(surface->offset <= dma.limit);
    assert(surface->offset + surface->pitch * height <= dma.limit + 1);
    assert(surface->pitch % fmt.bytes_per_pixel == 0);
    assert((dma.address & ~0x07FFFFFF) == 0);

    entry->shape = (color || !pg->color_binding) ? pg->surface_shape :
                                                   pg->color_binding->shape;
    entry->gl_buffer = 0;
    entry->fmt = fmt;
    entry->color = color;
    entry->swizzle =
        (pg->surface_type == NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE);
    entry->vram_addr = dma.address + surface->offset;
    entry->width = width;
    entry->height = height;
    entry->pitch = surface->pitch;
    entry->size = height * MAX(surface->pitch, width * fmt.bytes_per_pixel);
    entry->upload_pending = true;
    entry->download_pending = false;
    entry->draw_dirty = false;
    entry->dma_addr = dma.address;
    entry->dma_len = dma.limit;
    entry->frame_time = pg->frame_time;
    entry->draw_time = pg->draw_time;
    entry->cleared = false;
}

static void pgraph_populate_surface_binding_entry(NV2AState *d, bool color,
                                                  SurfaceBinding *entry)
{
    PGRAPHState *pg = &d->pgraph;
    unsigned int width, height;

    if (color || !pg->color_binding) {
        pgraph_get_surface_dimensions(pg, &width, &height);
        pgraph_apply_anti_aliasing_factor(pg, &width, &height);

        /* Since we determine surface dimensions based on the clipping
         * rectangle, make sure to include the surface offset as well.
         */
        if (pg->surface_type != NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE) {
            width += pg->surface_shape.clip_x;
            height += pg->surface_shape.clip_y;
        }
    } else {
        width = pg->color_binding->width;
        height = pg->color_binding->height;
    }

    pgraph_populate_surface_binding_entry_sized(d, color, width, height, entry);
}

static void pgraph_update_surface_part(NV2AState *d, bool upload, bool color)
{
    PGRAPHState *pg = &d->pgraph;

    SurfaceBinding entry;
    pgraph_populate_surface_binding_entry(d, color, &entry);

    Surface *surface = color ? &pg->surface_color : &pg->surface_zeta;

    bool mem_dirty = !tcg_enabled() && memory_region_test_and_clear_dirty(
                                           d->vram, entry.vram_addr, entry.size,
                                           DIRTY_MEMORY_NV2A);

    if (upload && (surface->buffer_dirty || mem_dirty)) {
        pgraph_unbind_surface(d, color);

        SurfaceBinding *found = pgraph_surface_get(d, entry.vram_addr);
        if (found != NULL) {
            /* FIXME: Support same color/zeta surface target? In the mean time,
             * if the surface we just found is currently bound, just unbind it.
             */
            SurfaceBinding *other = (color ? pg->zeta_binding
                                           : pg->color_binding);
            if (found == other) {
                NV2A_UNIMPLEMENTED("Same color & zeta surface offset");
                pgraph_unbind_surface(d, !color);
            }
        }

        trace_nv2a_pgraph_surface_target(
            color ? "COLOR" : "ZETA", entry.vram_addr,
            entry.swizzle ? "sz" : "ln",
            pg->surface_shape.anti_aliasing,
            pg->surface_shape.clip_x,
            pg->surface_shape.clip_width, pg->surface_shape.clip_y,
            pg->surface_shape.clip_height);

        bool should_create = true;

        if (found != NULL) {
            bool is_compatible =
                pgraph_check_surface_compatibility(found, &entry, false);

#define TRACE_ARGS found->vram_addr, found->width, found->height, \
            found->swizzle ? "sz" : "ln", \
            found->shape.anti_aliasing, found->shape.clip_x, \
            found->shape.clip_width, found->shape.clip_y, \
            found->shape.clip_height, found->pitch
            if (found->color) {
                trace_nv2a_pgraph_surface_match_color(TRACE_ARGS);
            } else {
                trace_nv2a_pgraph_surface_match_zeta(TRACE_ARGS);
            }
#undef TRACE_ARGS

            assert(!(entry.swizzle && pg->clearing));

            if (found->swizzle != entry.swizzle) {
                /* Clears should only be done on linear surfaces. Avoid
                 * synchronization by allowing (1) a surface marked swizzled to
                 * be cleared under the assumption the entire surface is
                 * destined to be cleared and (2) a fully cleared linear surface
                 * to be marked swizzled. Strictly match size to avoid
                 * pathological cases.
                 */
                is_compatible &= (pg->clearing || found->cleared) &&
                    pgraph_check_surface_compatibility(found, &entry, true);
                if (is_compatible) {
                    trace_nv2a_pgraph_surface_migrate_type(
                        entry.swizzle ? "swizzled" : "linear");
                }
            }

            if (is_compatible && color &&
                !pgraph_check_surface_compatibility(found, &entry, true)) {
                SurfaceBinding zeta_entry;
                pgraph_populate_surface_binding_entry_sized(
                    d, !color, found->width, found->height, &zeta_entry);
                hwaddr color_end = found->vram_addr + found->size;
                hwaddr zeta_end = zeta_entry.vram_addr + zeta_entry.size;
                is_compatible &= found->vram_addr >= zeta_end ||
                                 zeta_entry.vram_addr >= color_end;
            }

            if (is_compatible && !color && pg->color_binding) {
                is_compatible &= (found->width == pg->color_binding->width) &&
                                 (found->height == pg->color_binding->height);
            }

            if (is_compatible) {
                /* FIXME: Refactor */
                pg->surface_binding_dim.width = found->width;
                pg->surface_binding_dim.clip_x = found->shape.clip_x;
                pg->surface_binding_dim.clip_width = found->shape.clip_width;
                pg->surface_binding_dim.height = found->height;
                pg->surface_binding_dim.clip_y = found->shape.clip_y;
                pg->surface_binding_dim.clip_height = found->shape.clip_height;
                found->upload_pending |= mem_dirty;
                pg->surface_zeta.buffer_dirty |= color;
                should_create = false;
            } else {
                trace_nv2a_pgraph_surface_evict_reason(
                    "incompatible", found->vram_addr);
                pgraph_compare_surfaces(found, &entry);
                pgraph_download_surface_data_if_dirty(d, found);
                pgraph_surface_invalidate(d, found);
            }
        }

        if (should_create) {
            glGenTextures(1, &entry.gl_buffer);
            glBindTexture(GL_TEXTURE_2D, entry.gl_buffer);
            NV2A_GL_DLABEL(GL_TEXTURE, entry.gl_buffer,
                           "%s format: %0X, width: %d, height: %d "
                           "(addr %" HWADDR_PRIx ")",
                           color ? "color" : "zeta",
                           color ? pg->surface_shape.color_format
                                 : pg->surface_shape.zeta_format,
                           entry.width, entry.height, surface->offset);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            unsigned int width = entry.width, height = entry.height;
            pgraph_apply_scaling_factor(pg, &width, &height);
            glTexImage2D(GL_TEXTURE_2D, 0, entry.fmt.gl_internal_format, width,
                         height, 0, entry.fmt.gl_format, entry.fmt.gl_type,
                         NULL);
            found = pgraph_surface_put(d, entry.vram_addr, &entry);

            /* FIXME: Refactor */
            pg->surface_binding_dim.width = entry.width;
            pg->surface_binding_dim.clip_x = entry.shape.clip_x;
            pg->surface_binding_dim.clip_width = entry.shape.clip_width;
            pg->surface_binding_dim.height = entry.height;
            pg->surface_binding_dim.clip_y = entry.shape.clip_y;
            pg->surface_binding_dim.clip_height = entry.shape.clip_height;

            if (color && pg->zeta_binding && (pg->zeta_binding->width != entry.width || pg->zeta_binding->height != entry.height)) {
                pg->surface_zeta.buffer_dirty = true;
            }
        }

#define TRACE_ARGS found->vram_addr, found->width, found->height, \
                   found->swizzle ? "sz" : "ln", found->shape.anti_aliasing, \
                   found->shape.clip_x, found->shape.clip_width, \
                   found->shape.clip_y, found->shape.clip_height, found->pitch

        if (color) {
            if (should_create) {
                trace_nv2a_pgraph_surface_create_color(TRACE_ARGS);
            } else {
                trace_nv2a_pgraph_surface_hit_color(TRACE_ARGS);
            }

            pg->color_binding = found;
        } else {
            if (should_create) {
                trace_nv2a_pgraph_surface_create_zeta(TRACE_ARGS);
            } else {
                trace_nv2a_pgraph_surface_hit_zeta(TRACE_ARGS);
            }
            pg->zeta_binding = found;
        }
#undef TRACE_ARGS

        glFramebufferTexture2D(GL_FRAMEBUFFER, entry.fmt.gl_attachment,
                               GL_TEXTURE_2D, found->gl_buffer, 0);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
               GL_FRAMEBUFFER_COMPLETE);

        surface->buffer_dirty = false;
    }

    if (!upload && surface->draw_dirty) {
        if (!tcg_enabled()) {
            /* FIXME: Cannot monitor for reads/writes; flush now */
            pgraph_download_surface_data(d,
                color ? pg->color_binding : pg->zeta_binding, true);
        }

        surface->write_enabled_cache = false;
        surface->draw_dirty = false;
    }
}

static void pgraph_unbind_surface(NV2AState *d, bool color)
{
    PGRAPHState *pg = &d->pgraph;

    if (color) {
        if (pg->color_binding) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, 0, 0);
            pg->color_binding = NULL;
        }
    } else {
        if (pg->zeta_binding) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, 0, 0);
            pg->zeta_binding = NULL;
        }
    }
}

static void pgraph_update_surface(NV2AState *d, bool upload,
                                  bool color_write, bool zeta_write)
{
    PGRAPHState *pg = &d->pgraph;

    pg->surface_shape.z_format = GET_MASK(pg->regs[NV_PGRAPH_SETUPRASTER],
                                          NV_PGRAPH_SETUPRASTER_Z_FORMAT);

    color_write = color_write &&
            (pg->clearing || pgraph_color_write_enabled(pg));
    zeta_write = zeta_write && (pg->clearing || pgraph_zeta_write_enabled(pg));

    if (upload) {
        bool fb_dirty = pgraph_framebuffer_dirty(pg);
        if (fb_dirty) {
            memcpy(&pg->last_surface_shape, &pg->surface_shape,
                   sizeof(SurfaceShape));
            pg->surface_color.buffer_dirty = true;
            pg->surface_zeta.buffer_dirty = true;
        }

        if (pg->surface_color.buffer_dirty) {
            pgraph_unbind_surface(d, true);
        }

        if (color_write) {
            pgraph_update_surface_part(d, true, true);
        }

        if (pg->surface_zeta.buffer_dirty) {
            pgraph_unbind_surface(d, false);
        }

        if (zeta_write) {
            pgraph_update_surface_part(d, true, false);
        }
    } else {
        if ((color_write || pg->surface_color.write_enabled_cache)
            && pg->surface_color.draw_dirty) {
            pgraph_update_surface_part(d, false, true);
        }
        if ((zeta_write || pg->surface_zeta.write_enabled_cache)
            && pg->surface_zeta.draw_dirty) {
            pgraph_update_surface_part(d, false, false);
        }
    }

    if (upload) {
        pg->draw_time++;
    }

    bool swizzle = (pg->surface_type == NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE);

    if (pg->color_binding) {
        pg->color_binding->frame_time = pg->frame_time;
        if (upload) {
            pgraph_upload_surface_data(d, pg->color_binding, false);
            pg->color_binding->draw_time = pg->draw_time;
            pg->color_binding->swizzle = swizzle;
        }
    }

    if (pg->zeta_binding) {
        pg->zeta_binding->frame_time = pg->frame_time;
        if (upload) {
            pgraph_upload_surface_data(d, pg->zeta_binding, false);
            pg->zeta_binding->draw_time = pg->draw_time;
            pg->zeta_binding->swizzle = swizzle;
        }
    }

    // Sanity check color and zeta dimensions match
    if (pg->color_binding && pg->zeta_binding) {
        assert((pg->color_binding->width == pg->zeta_binding->width)
               && (pg->color_binding->height == pg->zeta_binding->height));
    }

    pgraph_surface_evict_old(d);
}

struct pgraph_texture_possibly_dirty_struct {
    hwaddr addr, end;
};

static void pgraph_mark_textures_possibly_dirty_visitor(Lru *lru, LruNode *node, void *opaque)
{
    struct pgraph_texture_possibly_dirty_struct *test =
        (struct pgraph_texture_possibly_dirty_struct *)opaque;

    struct TextureLruNode *tnode = container_of(node, TextureLruNode, node);
    if (tnode->binding == NULL || tnode->possibly_dirty) {
        return;
    }

    uintptr_t k_tex_addr = tnode->key.texture_vram_offset;
    uintptr_t k_tex_end = k_tex_addr + tnode->key.texture_length - 1;
    bool overlapping = !(test->addr > k_tex_end || k_tex_addr > test->end);

    if (tnode->key.palette_length > 0) {
        uintptr_t k_pal_addr = tnode->key.palette_vram_offset;
        uintptr_t k_pal_end = k_pal_addr + tnode->key.palette_length - 1;
        overlapping |= !(test->addr > k_pal_end || k_pal_addr > test->end);
    }

    tnode->possibly_dirty |= overlapping;
}


static void pgraph_mark_textures_possibly_dirty(NV2AState *d,
    hwaddr addr, hwaddr size)
{
    hwaddr end = TARGET_PAGE_ALIGN(addr + size) - 1;
    addr &= TARGET_PAGE_MASK;
    assert(end <= memory_region_size(d->vram));

    struct pgraph_texture_possibly_dirty_struct test = {
        .addr = addr,
        .end = end,
    };

    lru_visit_active(&d->pgraph.texture_cache,
                     pgraph_mark_textures_possibly_dirty_visitor,
                     &test);
}

static bool pgraph_check_texture_dirty(NV2AState *d, hwaddr addr, hwaddr size)
{
    hwaddr end = TARGET_PAGE_ALIGN(addr + size);
    addr &= TARGET_PAGE_MASK;
    assert(end < memory_region_size(d->vram));
    return memory_region_test_and_clear_dirty(d->vram, addr, end - addr,
                                              DIRTY_MEMORY_NV2A_TEX);
}

static bool pgraph_is_texture_stage_active(PGRAPHState *pg, unsigned int stage)
{
    assert(stage < NV2A_MAX_TEXTURES);
    uint32_t mode = (pg->regs[NV_PGRAPH_SHADERPROG] >> (stage * 5)) & 0x1F;
    return !!mode;
}

// Check if any of the pages spanned by the a texture are dirty.
static bool pgraph_check_texture_possibly_dirty(NV2AState *d, hwaddr texture_vram_offset, unsigned int length, hwaddr palette_vram_offset, unsigned int palette_length)
{
    bool possibly_dirty = false;
    if (pgraph_check_texture_dirty(d, texture_vram_offset, length)) {
        possibly_dirty = true;
        pgraph_mark_textures_possibly_dirty(d, texture_vram_offset, length);
    }
    if (palette_length && pgraph_check_texture_dirty(d, palette_vram_offset,
                                                     palette_length)) {
        possibly_dirty = true;
        pgraph_mark_textures_possibly_dirty(d, palette_vram_offset,
                                            palette_length);
    }
    return possibly_dirty;
}

static void apply_texture_parameters(TextureBinding *binding,
                                     const ColorFormatInfo *f,
                                     unsigned int dimensionality,
                                     unsigned int filter,
                                     unsigned int address,
                                     bool is_bordered,
                                     uint32_t border_color)
{
    unsigned int min_filter = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MIN);
    unsigned int mag_filter = GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MAG);
    unsigned int addru = GET_MASK(address, NV_PGRAPH_TEXADDRESS0_ADDRU);
    unsigned int addrv = GET_MASK(address, NV_PGRAPH_TEXADDRESS0_ADDRV);
    unsigned int addrp = GET_MASK(address, NV_PGRAPH_TEXADDRESS0_ADDRP);

    if (f->linear) {
        /* somtimes games try to set mipmap min filters on linear textures.
             * this could indicate a bug... */
        switch (min_filter) {
        case NV_PGRAPH_TEXFILTER0_MIN_BOX_NEARESTLOD:
        case NV_PGRAPH_TEXFILTER0_MIN_BOX_TENT_LOD:
            min_filter = NV_PGRAPH_TEXFILTER0_MIN_BOX_LOD0;
            break;
        case NV_PGRAPH_TEXFILTER0_MIN_TENT_NEARESTLOD:
        case NV_PGRAPH_TEXFILTER0_MIN_TENT_TENT_LOD:
            min_filter = NV_PGRAPH_TEXFILTER0_MIN_TENT_LOD0;
            break;
        }
    }

    if (min_filter != binding->min_filter) {
        glTexParameteri(binding->gl_target, GL_TEXTURE_MIN_FILTER,
                        pgraph_texture_min_filter_map[min_filter]);
        binding->min_filter = min_filter;
    }
    if (mag_filter != binding->mag_filter) {
        glTexParameteri(binding->gl_target, GL_TEXTURE_MAG_FILTER,
                        pgraph_texture_mag_filter_map[mag_filter]);
        binding->mag_filter = mag_filter;
    }

    /* Texture wrapping */
    assert(addru < ARRAY_SIZE(pgraph_texture_addr_map));
    if (addru != binding->addru) {
        glTexParameteri(binding->gl_target, GL_TEXTURE_WRAP_S,
                        pgraph_texture_addr_map[addru]);
        binding->addru = addru;
    }
    bool needs_border_color = binding->addru == NV_PGRAPH_TEXADDRESS0_ADDRU_BORDER;
    if (dimensionality > 1) {
        if (addrv != binding->addrv) {
            assert(addrv < ARRAY_SIZE(pgraph_texture_addr_map));
            glTexParameteri(binding->gl_target, GL_TEXTURE_WRAP_T,
                            pgraph_texture_addr_map[addrv]);
            binding->addrv = addrv;
        }
        needs_border_color = needs_border_color || binding->addrv == NV_PGRAPH_TEXADDRESS0_ADDRU_BORDER;
    }
    if (dimensionality > 2) {
        if (addrp != binding->addrp) {
            assert(addrp < ARRAY_SIZE(pgraph_texture_addr_map));
            glTexParameteri(binding->gl_target, GL_TEXTURE_WRAP_R,
                            pgraph_texture_addr_map[addrp]);
            binding->addrp = addrp;
        }
        needs_border_color = needs_border_color || binding->addrp == NV_PGRAPH_TEXADDRESS0_ADDRU_BORDER;
    }

    if (!is_bordered && needs_border_color) {
        if (!binding->border_color_set || binding->border_color != border_color) {
            GLfloat gl_border_color[] = {
                /* FIXME: Color channels might be wrong order */
                ((border_color >> 16) & 0xFF) / 255.0f, /* red */
                ((border_color >> 8) & 0xFF) / 255.0f, /* green */
                (border_color & 0xFF) / 255.0f, /* blue */
                ((border_color >> 24) & 0xFF) / 255.0f /* alpha */
            };
            glTexParameterfv(binding->gl_target, GL_TEXTURE_BORDER_COLOR,
                             gl_border_color);

            binding->border_color_set = true;
            binding->border_color = border_color;
        }
    }
}

static void pgraph_bind_textures(NV2AState *d)
{
    int i;
    PGRAPHState *pg = &d->pgraph;

    NV2A_GL_DGROUP_BEGIN("%s", __func__);

    for (i=0; i<NV2A_MAX_TEXTURES; i++) {
        uint32_t ctl_0 = pg->regs[NV_PGRAPH_TEXCTL0_0 + i*4];
        bool enabled = pgraph_is_texture_stage_active(pg, i) &&
                       GET_MASK(ctl_0, NV_PGRAPH_TEXCTL0_0_ENABLE);
        /* FIXME: What happens if texture is disabled but stage is active? */

        glActiveTexture(GL_TEXTURE0 + i);
        if (!enabled) {
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glBindTexture(GL_TEXTURE_RECTANGLE, 0);
            glBindTexture(GL_TEXTURE_1D, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_3D, 0);
            continue;
        }

        uint32_t ctl_1 = pg->regs[NV_PGRAPH_TEXCTL1_0 + i*4];
        uint32_t fmt = pg->regs[NV_PGRAPH_TEXFMT0 + i*4];
        uint32_t filter = pg->regs[NV_PGRAPH_TEXFILTER0 + i*4];
        uint32_t address = pg->regs[NV_PGRAPH_TEXADDRESS0 + i*4];
        uint32_t palette = pg->regs[NV_PGRAPH_TEXPALETTE0 + i*4];

        unsigned int min_mipmap_level =
            GET_MASK(ctl_0, NV_PGRAPH_TEXCTL0_0_MIN_LOD_CLAMP);
        unsigned int max_mipmap_level =
            GET_MASK(ctl_0, NV_PGRAPH_TEXCTL0_0_MAX_LOD_CLAMP);

        unsigned int pitch =
            GET_MASK(ctl_1, NV_PGRAPH_TEXCTL1_0_IMAGE_PITCH);

        unsigned int dma_select =
            GET_MASK(fmt, NV_PGRAPH_TEXFMT0_CONTEXT_DMA);
        bool cubemap =
            GET_MASK(fmt, NV_PGRAPH_TEXFMT0_CUBEMAPENABLE);
        unsigned int dimensionality =
            GET_MASK(fmt, NV_PGRAPH_TEXFMT0_DIMENSIONALITY);
        unsigned int color_format = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_COLOR);
        unsigned int levels = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_MIPMAP_LEVELS);
        unsigned int log_width = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_U);
        unsigned int log_height = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_V);
        unsigned int log_depth = GET_MASK(fmt, NV_PGRAPH_TEXFMT0_BASE_SIZE_P);

        unsigned int rect_width =
            GET_MASK(pg->regs[NV_PGRAPH_TEXIMAGERECT0 + i*4],
                     NV_PGRAPH_TEXIMAGERECT0_WIDTH);
        unsigned int rect_height =
            GET_MASK(pg->regs[NV_PGRAPH_TEXIMAGERECT0 + i*4],
                     NV_PGRAPH_TEXIMAGERECT0_HEIGHT);
#ifdef DEBUG_NV2A
        unsigned int lod_bias =
            GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MIPMAP_LOD_BIAS);
#endif
        unsigned int border_source = GET_MASK(fmt,
                                              NV_PGRAPH_TEXFMT0_BORDER_SOURCE);
        uint32_t border_color = pg->regs[NV_PGRAPH_BORDERCOLOR0 + i*4];

        hwaddr offset = pg->regs[NV_PGRAPH_TEXOFFSET0 + i*4];

        bool palette_dma_select =
            GET_MASK(palette, NV_PGRAPH_TEXPALETTE0_CONTEXT_DMA);
        unsigned int palette_length_index =
            GET_MASK(palette, NV_PGRAPH_TEXPALETTE0_LENGTH);
        unsigned int palette_offset =
            palette & NV_PGRAPH_TEXPALETTE0_OFFSET;

        unsigned int palette_length = 0;
        switch (palette_length_index) {
        case NV_PGRAPH_TEXPALETTE0_LENGTH_256: palette_length = 256; break;
        case NV_PGRAPH_TEXPALETTE0_LENGTH_128: palette_length = 128; break;
        case NV_PGRAPH_TEXPALETTE0_LENGTH_64: palette_length = 64; break;
        case NV_PGRAPH_TEXPALETTE0_LENGTH_32: palette_length = 32; break;
        default: assert(false); break;
        }

        /* Check for unsupported features */
        if (filter & NV_PGRAPH_TEXFILTER0_ASIGNED) NV2A_UNIMPLEMENTED("NV_PGRAPH_TEXFILTER0_ASIGNED");
        if (filter & NV_PGRAPH_TEXFILTER0_RSIGNED) NV2A_UNIMPLEMENTED("NV_PGRAPH_TEXFILTER0_RSIGNED");
        if (filter & NV_PGRAPH_TEXFILTER0_GSIGNED) NV2A_UNIMPLEMENTED("NV_PGRAPH_TEXFILTER0_GSIGNED");
        if (filter & NV_PGRAPH_TEXFILTER0_BSIGNED) NV2A_UNIMPLEMENTED("NV_PGRAPH_TEXFILTER0_BSIGNED");

        nv2a_profile_inc_counter(NV2A_PROF_TEX_BIND);

        hwaddr dma_len;
        uint8_t *texture_data;
        if (dma_select) {
            texture_data = (uint8_t*)nv_dma_map(d, pg->dma_b, &dma_len);
        } else {
            texture_data = (uint8_t*)nv_dma_map(d, pg->dma_a, &dma_len);
        }
        assert(offset < dma_len);
        texture_data += offset;
        hwaddr texture_vram_offset = texture_data - d->vram_ptr;

        hwaddr palette_dma_len;
        uint8_t *palette_data;
        if (palette_dma_select) {
            palette_data = (uint8_t*)nv_dma_map(d, pg->dma_b, &palette_dma_len);
        } else {
            palette_data = (uint8_t*)nv_dma_map(d, pg->dma_a, &palette_dma_len);
        }
        assert(palette_offset < palette_dma_len);
        palette_data += palette_offset;
        hwaddr palette_vram_offset = palette_data - d->vram_ptr;

        NV2A_DPRINTF(" texture %d is format 0x%x, "
                        "off 0x%" HWADDR_PRIx " (r %d, %d or %d, %d, %d; %d%s),"
                        " filter %x %x, levels %d-%d %d bias %d\n",
                     i, color_format, offset,
                     rect_width, rect_height,
                     1 << log_width, 1 << log_height, 1 << log_depth,
                     pitch,
                     cubemap ? "; cubemap" : "",
                     GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MIN),
                     GET_MASK(filter, NV_PGRAPH_TEXFILTER0_MAG),
                     min_mipmap_level, max_mipmap_level, levels,
                     lod_bias);

        assert(color_format < ARRAY_SIZE(kelvin_color_format_map));
        ColorFormatInfo f = kelvin_color_format_map[color_format];
        if (f.bytes_per_pixel == 0) {
            fprintf(stderr, "nv2a: unimplemented texture color format 0x%x\n",
                    color_format);
            abort();
        }

        unsigned int width, height, depth;
        if (f.linear) {
            assert(dimensionality == 2);
            width = rect_width;
            height = rect_height;
            depth = 1;
        } else {
            width = 1 << log_width;
            height = 1 << log_height;
            depth = 1 << log_depth;
            pitch = 0;

            levels = MIN(levels, max_mipmap_level + 1);

            /* Discard mipmap levels that would be smaller than 1x1.
             * FIXME: Is this actually needed?
             *
             * >> Level 0: 32 x 4
             *    Level 1: 16 x 2
             *    Level 2: 8 x 1
             *    Level 3: 4 x 1
             *    Level 4: 2 x 1
             *    Level 5: 1 x 1
             */
            levels = MIN(levels, MAX(log_width, log_height) + 1);
            assert(levels > 0);

            if (dimensionality == 3) {
                /* FIXME: What about 3D mipmaps? */
                if (log_width < 2 || log_height < 2) {
                    /* Base level is smaller than 4x4... */
                    levels = 1;
                } else {
                    levels = MIN(levels, MIN(log_width, log_height) - 1);
                }
            }
            min_mipmap_level = MIN(levels-1, min_mipmap_level);
            max_mipmap_level = MIN(levels-1, max_mipmap_level);
        }

        size_t length = 0;
        if (f.linear) {
            assert(cubemap == false);
            assert(dimensionality == 2);
            length = height * pitch;
        } else {
            if (dimensionality >= 2) {
                unsigned int w = width, h = height;
                int level;
                if (f.gl_format != 0) {
                    for (level = 0; level < levels; level++) {
                        w = MAX(w, 1);
                        h = MAX(h, 1);
                        length += w * h * f.bytes_per_pixel;
                        w /= 2;
                        h /= 2;
                    }
                } else {
                    /* Compressed textures are a bit different */
                    unsigned int block_size =
                        f.gl_internal_format ==
                                GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ?
                            8 : 16;
                    for (level = 0; level < levels; level++) {
                        w = MAX(w, 1);
                        h = MAX(h, 1);
                        unsigned int phys_w = (w + 3) & ~3,
                                     phys_h = (h + 3) & ~3;
                        length += phys_w/4 * phys_h/4 * block_size;
                        w /= 2;
                        h /= 2;
                    }
                }
                if (cubemap) {
                    assert(dimensionality == 2);
                    length = (length + NV2A_CUBEMAP_FACE_ALIGNMENT - 1) & ~(NV2A_CUBEMAP_FACE_ALIGNMENT - 1);
                    length *= 6;
                }
                if (dimensionality >= 3) {
                    length *= depth;
                }
            }
        }

        bool is_bordered = border_source != NV_PGRAPH_TEXFMT0_BORDER_SOURCE_COLOR;

        assert((texture_vram_offset + length) < memory_region_size(d->vram));
        assert((palette_vram_offset + palette_length)
               < memory_region_size(d->vram));
        bool is_indexed = (color_format ==
                NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8);
        bool possibly_dirty = false;
        bool possibly_dirty_checked = false;

        SurfaceBinding *surface = pgraph_surface_get(d, texture_vram_offset);
        TextureBinding *tbind = pg->texture_binding[i];
        if (!pg->texture_dirty[i] && tbind) {
            bool reusable = false;
            if (surface && tbind->draw_time == surface->draw_time) {
                reusable = true;
            } else if (!surface) {
                possibly_dirty = pgraph_check_texture_possibly_dirty(
                        d,
                        texture_vram_offset,
                        length,
                        palette_vram_offset,
                        is_indexed ? palette_length : 0);
                possibly_dirty_checked = true;
                reusable = !possibly_dirty;
            }

            if (reusable) {
                glBindTexture(pg->texture_binding[i]->gl_target,
                              pg->texture_binding[i]->gl_texture);
                apply_texture_parameters(pg->texture_binding[i],
                                         &f,
                                         dimensionality,
                                         filter,
                                         address,
                                         is_bordered,
                                         border_color);
                continue;
            }
        }

        TextureShape state;
        memset(&state, 0, sizeof(TextureShape));
        state.cubemap = cubemap;
        state.dimensionality = dimensionality;
        state.color_format = color_format;
        state.levels = levels;
        state.width = width;
        state.height = height;
        state.depth = depth;
        state.min_mipmap_level = min_mipmap_level;
        state.max_mipmap_level = max_mipmap_level;
        state.pitch = pitch;
        state.border = is_bordered;

        /*
         * Check active surfaces to see if this texture was a render target
         */
        bool surf_to_tex = false;
        if (surface != NULL) {
            surf_to_tex = pgraph_check_surface_to_texture_compatibility(
                    surface, &state);

            if (surf_to_tex && surface->upload_pending) {
                pgraph_upload_surface_data(d, surface, false);
            }
        }

        if (!surf_to_tex) {
            // FIXME: Restructure to support rendering surfaces to cubemap faces

            // Writeback any surfaces which this texture may index
            hwaddr tex_vram_end = texture_vram_offset + length - 1;
            QTAILQ_FOREACH(surface, &d->pgraph.surfaces, entry) {
                hwaddr surf_vram_end = surface->vram_addr + surface->size - 1;
                bool overlapping = !(surface->vram_addr >= tex_vram_end
                                     || texture_vram_offset >= surf_vram_end);
                if (overlapping) {
                    pgraph_download_surface_data_if_dirty(d, surface);
                }
            }
        }

        TextureKey key;
        memset(&key, 0, sizeof(TextureKey));
        key.state = state;
        key.texture_vram_offset = texture_vram_offset;
        key.texture_length = length;
        if (is_indexed) {
            key.palette_vram_offset = palette_vram_offset;
            key.palette_length = palette_length;
        }

        // Search for existing texture binding in cache
        uint64_t tex_binding_hash = fast_hash((uint8_t*)&key, sizeof(key));
        LruNode *found = lru_lookup(&pg->texture_cache,
                                     tex_binding_hash, &key);
        TextureLruNode *key_out = container_of(found, TextureLruNode, node);
        possibly_dirty |= (key_out->binding == NULL) || key_out->possibly_dirty;

        if (!surf_to_tex && !possibly_dirty_checked) {
            possibly_dirty |= pgraph_check_texture_possibly_dirty(
                    d,
                    texture_vram_offset,
                    length,
                    palette_vram_offset,
                    is_indexed ? palette_length : 0);
        }

        // Calculate hash of texture data, if necessary
        uint64_t tex_data_hash = 0;
        if (!surf_to_tex && possibly_dirty) {
            tex_data_hash = fast_hash(texture_data, length);
            if (is_indexed) {
                tex_data_hash ^= fast_hash(palette_data, palette_length);
            }
        }

        // Free existing binding, if texture data has changed
        bool must_destroy = (key_out->binding != NULL)
                            && possibly_dirty
                            && (key_out->binding->data_hash != tex_data_hash);
        if (must_destroy) {
            texture_binding_destroy(key_out->binding);
            key_out->binding = NULL;
        }

        if (key_out->binding == NULL) {
            // Must create the texture
            key_out->binding = generate_texture(state, texture_data, palette_data);
            key_out->binding->data_hash = tex_data_hash;
            key_out->binding->scale = 1;
        } else {
            // Saved an upload! Reuse existing texture in graphics memory.
            glBindTexture(key_out->binding->gl_target,
                          key_out->binding->gl_texture);
        }

        key_out->possibly_dirty = false;
        TextureBinding *binding = key_out->binding;
        binding->refcnt++;

        if (surf_to_tex && binding->draw_time < surface->draw_time) {

            trace_nv2a_pgraph_surface_render_to_texture(
                surface->vram_addr, surface->width, surface->height);
            pgraph_render_surface_to_texture(d, surface, binding, &state, i);
            binding->draw_time = surface->draw_time;
            if (binding->gl_target == GL_TEXTURE_RECTANGLE) {
                binding->scale = pg->surface_scale_factor;
            } else {
                binding->scale = 1;
            }
        }

        apply_texture_parameters(binding,
                                 &f,
                                 dimensionality,
                                 filter,
                                 address,
                                 is_bordered,
                                 border_color);

        if (pg->texture_binding[i]) {
            if (pg->texture_binding[i]->gl_target != binding->gl_target) {
                glBindTexture(pg->texture_binding[i]->gl_target, 0);
            }
            texture_binding_destroy(pg->texture_binding[i]);
        }
        pg->texture_binding[i] = binding;
        pg->texture_dirty[i] = false;
    }
    NV2A_GL_DGROUP_END();
}

static void pgraph_apply_anti_aliasing_factor(PGRAPHState *pg,
                                              unsigned int *width,
                                              unsigned int *height)
{
    switch (pg->surface_shape.anti_aliasing) {
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_1:
        break;
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_CENTER_CORNER_2:
        if (width) { *width *= 2; }
        break;
    case NV097_SET_SURFACE_FORMAT_ANTI_ALIASING_SQUARE_OFFSET_4:
        if (width) { *width *= 2; }
        if (height) { *height *= 2; }
        break;
    default:
        assert(false);
        break;
    }
}

static void pgraph_apply_scaling_factor(PGRAPHState *pg,
                                        unsigned int *width,
                                        unsigned int *height)
{
    *width *= pg->surface_scale_factor;
    *height *= pg->surface_scale_factor;
}

static void pgraph_get_surface_dimensions(PGRAPHState *pg,
                                          unsigned int *width,
                                          unsigned int *height)
{
    bool swizzle = (pg->surface_type == NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE);
    if (swizzle) {
        *width = 1 << pg->surface_shape.log_width;
        *height = 1 << pg->surface_shape.log_height;
    } else {
        *width = pg->surface_shape.clip_width;
        *height = pg->surface_shape.clip_height;
    }
}

static void pgraph_update_memory_buffer(NV2AState *d, hwaddr addr, hwaddr size,
                                        bool quick)
{
    glBindBuffer(GL_ARRAY_BUFFER, d->pgraph.gl_memory_buffer);

    hwaddr end = TARGET_PAGE_ALIGN(addr + size);
    addr &= TARGET_PAGE_MASK;
    assert(end < memory_region_size(d->vram));

    static hwaddr last_addr, last_end;
    if (quick && (addr >= last_addr) && (end <= last_end)) {
        return;
    }
    last_addr = addr;
    last_end = end;

    size = end - addr;
    if (memory_region_test_and_clear_dirty(d->vram, addr, size,
                                           DIRTY_MEMORY_NV2A)) {
        glBufferSubData(GL_ARRAY_BUFFER, addr, size,
                        d->vram_ptr + addr);
        nv2a_profile_inc_counter(NV2A_PROF_GEOM_BUFFER_UPDATE_1);
    }
}

static void pgraph_update_inline_value(VertexAttribute *attr,
                                       const uint8_t *data)
{
    assert(attr->count <= 4);
    attr->inline_value[0] = 0.0f;
    attr->inline_value[1] = 0.0f;
    attr->inline_value[2] = 0.0f;
    attr->inline_value[3] = 1.0f;

    switch (attr->format) {
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_D3D:
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_UB_OGL:
            for (uint32_t i = 0; i < attr->count; ++i) {
                attr->inline_value[i] = (float)data[i] / 255.0f;
            }
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S1: {
            const int16_t *val = (const int16_t *) data;
            for (uint32_t i = 0; i < attr->count; ++i, ++val) {
                attr->inline_value[i] = MAX(-1.0f, (float) *val / 32767.0f);
            }
            break;
        }
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F:
            memcpy(attr->inline_value, data, attr->size * attr->count);
            break;
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_S32K: {
            const int16_t *val = (const int16_t *) data;
            for (uint32_t i = 0; i < attr->count; ++i, ++val) {
                attr->inline_value[i] = (float)*val;
            }
            break;
        }
        case NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_CMP: {
            /* 3 signed, normalized components packed in 32-bits. (11,11,10) */
            const int32_t val = *(const int32_t *)data;
            int32_t x = val & 0x7FF;
            if (x & 0x400) {
                x |= 0xFFFFF800;
            }
            int32_t y = (val >> 11) & 0x7FF;
            if (y & 0x400) {
                y |= 0xFFFFF800;
            }
            int32_t z = (val >> 22) & 0x7FF;
            if (z & 0x200) {
                z |= 0xFFFFFC00;
            }

            attr->inline_value[0] = MAX(-1.0f, (float)x / 1023.0f);
            attr->inline_value[1] = MAX(-1.0f, (float)y / 1023.0f);
            attr->inline_value[2] = MAX(-1.0f, (float)z / 511.0f);
            break;
        }
    default:
        fprintf(stderr, "Unknown vertex attribute type: 0x%x for format 0x%x\n",
                attr->gl_type, attr->format);
        assert(!"Unsupported attribute type");
        break;
    }
}

static void pgraph_bind_vertex_attributes(NV2AState *d,
                                          unsigned int min_element,
                                          unsigned int max_element,
                                          bool inline_data,
                                          unsigned int inline_stride,
                                          unsigned int provoking_element)
{
    PGRAPHState *pg = &d->pgraph;
    bool updated_memory_buffer = false;
    unsigned int num_elements = max_element - min_element + 1;

    if (inline_data) {
        NV2A_GL_DGROUP_BEGIN("%s (num_elements: %d inline stride: %d)",
                             __func__, num_elements, inline_stride);
    } else {
        NV2A_GL_DGROUP_BEGIN("%s (num_elements: %d)", __func__, num_elements);
    }

    pg->compressed_attrs = 0;

    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attr = &pg->vertex_attributes[i];

        if (!attr->count) {
            glDisableVertexAttribArray(i);
            glVertexAttrib4fv(i, attr->inline_value);
            continue;
        }

        nv2a_profile_inc_counter(NV2A_PROF_ATTR_BIND);
        hwaddr attrib_data_addr;
        size_t stride;

        if (attr->needs_conversion) {
            pg->compressed_attrs |= (1 << i);
        }

        hwaddr start = 0;
        if (inline_data) {
            glBindBuffer(GL_ARRAY_BUFFER, pg->gl_inline_array_buffer);
            attrib_data_addr = attr->inline_array_offset;
            stride = inline_stride;
        } else {
            hwaddr dma_len;
            uint8_t *attr_data = (uint8_t *)nv_dma_map(
                d, attr->dma_select ? pg->dma_vertex_b : pg->dma_vertex_a,
                &dma_len);
            assert(attr->offset < dma_len);
            attrib_data_addr = attr_data + attr->offset - d->vram_ptr;
            stride = attr->stride;
            start = attrib_data_addr + min_element * stride;
            pgraph_update_memory_buffer(d, start, num_elements * stride,
                                        updated_memory_buffer);
            updated_memory_buffer = true;
        }

        uint32_t provoking_element_index = provoking_element - min_element;
        size_t element_size = attr->size * attr->count;
        assert(element_size <= sizeof(attr->inline_value));
        const uint8_t *last_entry;

        if (inline_data) {
            last_entry = (uint8_t*)pg->inline_array + attr->inline_array_offset;
        } else {
            last_entry = d->vram_ptr + start;
        }
        if (!stride) {
            // Stride of 0 indicates that only the first element should be
            // used.
            pgraph_update_inline_value(attr, last_entry);
            glDisableVertexAttribArray(i);
            glVertexAttrib4fv(i, attr->inline_value);
            continue;
        }

        if (attr->needs_conversion) {
            glVertexAttribIPointer(i, attr->gl_count, attr->gl_type, stride,
                                   (void *)attrib_data_addr);
        } else {
            glVertexAttribPointer(i, attr->gl_count, attr->gl_type,
                                  attr->gl_normalize, stride,
                                  (void *)attrib_data_addr);
        }

        glEnableVertexAttribArray(i);
        last_entry += stride * provoking_element_index;
        pgraph_update_inline_value(attr, last_entry);
    }

    NV2A_GL_DGROUP_END();
}

static unsigned int pgraph_bind_inline_array(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;

    unsigned int offset = 0;
    for (int i = 0; i < NV2A_VERTEXSHADER_ATTRIBUTES; i++) {
        VertexAttribute *attr = &pg->vertex_attributes[i];
        if (attr->count == 0) {
            continue;
        }

        /* FIXME: Double check */
        offset = ROUND_UP(offset, attr->size);
        attr->inline_array_offset = offset;
        NV2A_DPRINTF("bind inline attribute %d size=%d, count=%d\n",
            i, attr->size, attr->count);
        offset += attr->size * attr->count;
        offset = ROUND_UP(offset, attr->size);
    }

    unsigned int vertex_size = offset;
    unsigned int index_count = pg->inline_array_length*4 / vertex_size;

    NV2A_DPRINTF("draw inline array %d, %d\n", vertex_size, index_count);

    nv2a_profile_inc_counter(NV2A_PROF_GEOM_BUFFER_UPDATE_2);
    glBindBuffer(GL_ARRAY_BUFFER, pg->gl_inline_array_buffer);
    glBufferData(GL_ARRAY_BUFFER, NV2A_MAX_BATCH_LENGTH * sizeof(uint32_t),
                 NULL, GL_STREAM_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, index_count * vertex_size, pg->inline_array);
    pgraph_bind_vertex_attributes(d, 0, index_count-1, true, vertex_size,
                                  index_count-1);

    return index_count;
}

/* 16 bit to [0.0, F16_MAX = 511.9375] */
static float convert_f16_to_float(uint16_t f16) {
    if (f16 == 0x0000) { return 0.0; }
    uint32_t i = (f16 << 11) + 0x3C000000;
    return *(float*)&i;
}

/* 24 bit to [0.0, F24_MAX] */
static float convert_f24_to_float(uint32_t f24) {
    assert(!(f24 >> 24));
    f24 &= 0xFFFFFF;
    if (f24 == 0x000000) { return 0.0; }
    uint32_t i = f24 << 7;
    return *(float*)&i;
}

static uint8_t cliptobyte(int x)
{
    return (uint8_t)((x < 0) ? 0 : ((x > 255) ? 255 : x));
}

static void convert_yuy2_to_rgb(const uint8_t *line, unsigned int ix,
                                uint8_t *r, uint8_t *g, uint8_t* b) {
    int c, d, e;
    c = (int)line[ix * 2] - 16;
    if (ix % 2) {
        d = (int)line[ix * 2 - 1] - 128;
        e = (int)line[ix * 2 + 1] - 128;
    } else {
        d = (int)line[ix * 2 + 1] - 128;
        e = (int)line[ix * 2 + 3] - 128;
    }
    *r = cliptobyte((298 * c + 409 * e + 128) >> 8);
    *g = cliptobyte((298 * c - 100 * d - 208 * e + 128) >> 8);
    *b = cliptobyte((298 * c + 516 * d + 128) >> 8);
}

static void convert_uyvy_to_rgb(const uint8_t *line, unsigned int ix,
                                uint8_t *r, uint8_t *g, uint8_t* b) {
    int c, d, e;
    c = (int)line[ix * 2 + 1] - 16;
    if (ix % 2) {
        d = (int)line[ix * 2 - 2] - 128;
        e = (int)line[ix * 2 + 0] - 128;
    } else {
        d = (int)line[ix * 2 + 0] - 128;
        e = (int)line[ix * 2 + 2] - 128;
    }
    *r = cliptobyte((298 * c + 409 * e + 128) >> 8);
    *g = cliptobyte((298 * c - 100 * d - 208 * e + 128) >> 8);
    *b = cliptobyte((298 * c + 516 * d + 128) >> 8);
}

static uint8_t* convert_texture_data(const TextureShape s,
                                     const uint8_t *data,
                                     const uint8_t *palette_data,
                                     unsigned int width,
                                     unsigned int height,
                                     unsigned int depth,
                                     unsigned int row_pitch,
                                     unsigned int slice_pitch)
{
    if (s.color_format == NV097_SET_TEXTURE_FORMAT_COLOR_SZ_I8_A8R8G8B8) {
        uint8_t* converted_data = (uint8_t*)g_malloc(width * height * depth * 4);
        int x, y, z;
        const uint8_t* src = data;
        uint32_t* dst = (uint32_t*)converted_data;
        for (z = 0; z < depth; z++) {
            for (y = 0; y < height; y++) {
                for (x = 0; x < width; x++) {
                    uint8_t index = src[y * row_pitch + x];
                    uint32_t color = *(uint32_t * )(palette_data + index * 4);
                    *dst++ = color;
                }
            }
            src += slice_pitch;
        }
        return converted_data;
    } else if (s.color_format
                   == NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8 ||
                   s.color_format
                   == NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_YB8CR8YA8CB8) {
        // TODO: Investigate whether a non-1 depth is possible.
        // Generally the hardware asserts when attempting to use volumetric
        // textures in linear formats.
        assert(depth == 1); /* FIXME */
        // FIXME: only valid if control0 register allows for colorspace conversion
        uint8_t* converted_data = (uint8_t*)g_malloc(width * height * 4);
        int x, y;
        uint8_t* pixel = converted_data;
        for (y = 0; y < height; y++) {
            const uint8_t* line = &data[y * row_pitch * depth];
            for (x = 0; x < width; x++, pixel += 4) {
                if (s.color_format
                    == NV097_SET_TEXTURE_FORMAT_COLOR_LC_IMAGE_CR8YB8CB8YA8) {
                    convert_yuy2_to_rgb(line, x, &pixel[0], &pixel[1], &pixel[2]);
                } else {
                    convert_uyvy_to_rgb(line, x, &pixel[0], &pixel[1], &pixel[2]);
                }
                pixel[3] = 255;
          }
        }
        return converted_data;
    } else if (s.color_format
                   == NV097_SET_TEXTURE_FORMAT_COLOR_SZ_R6G5B5) {
        assert(depth == 1); /* FIXME */
        uint8_t *converted_data = (uint8_t*)g_malloc(width * height * 3);
        int x, y;
        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                uint16_t rgb655 = *(uint16_t*)(data + y * row_pitch + x * 2);
                int8_t *pixel = (int8_t*)&converted_data[(y * width + x) * 3];
                /* Maps 5 bit G and B signed value range to 8 bit
                 * signed values. R is probably unsigned.
                 */
                rgb655 ^= (1 << 9) | (1 << 4);
                pixel[0] = ((rgb655 & 0xFC00) >> 10) * 0x7F / 0x3F;
                pixel[1] = ((rgb655 & 0x03E0) >> 5) * 0xFF / 0x1F - 0x80;
                pixel[2] = (rgb655 & 0x001F) * 0xFF / 0x1F - 0x80;
            }
        }
        return converted_data;
    } else {
        return NULL;
    }
}

static void upload_gl_texture(GLenum gl_target,
                              const TextureShape s,
                              const uint8_t *texture_data,
                              const uint8_t *palette_data)
{
    ColorFormatInfo f = kelvin_color_format_map[s.color_format];
    nv2a_profile_inc_counter(NV2A_PROF_TEX_UPLOAD);

    unsigned int adjusted_width = s.width;
    unsigned int adjusted_height = s.height;
    unsigned int adjusted_pitch = s.pitch;
    unsigned int adjusted_depth = s.depth;
    if (!f.linear && s.border) {
        adjusted_width = MAX(16, adjusted_width * 2);
        adjusted_height = MAX(16, adjusted_height * 2);
        adjusted_pitch = adjusted_width * (s.pitch / s.width);
        adjusted_depth = MAX(16, s.depth * 2);
    }

    switch(gl_target) {
    case GL_TEXTURE_1D:
        assert(false);
        break;
    case GL_TEXTURE_RECTANGLE: {
        /* Can't handle strides unaligned to pixels */
        assert(s.pitch % f.bytes_per_pixel == 0);

        uint8_t *converted = convert_texture_data(s, texture_data,
                                                  palette_data,
                                                  adjusted_width,
                                                  adjusted_height, 1,
                                                  adjusted_pitch, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,
                      converted ? 0 : adjusted_pitch / f.bytes_per_pixel);
        glTexImage2D(gl_target, 0, f.gl_internal_format,
                     adjusted_width, adjusted_height, 0,
                     f.gl_format, f.gl_type,
                     converted ? converted : texture_data);

        if (converted) {
          g_free(converted);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        break;
    }
    case GL_TEXTURE_2D:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: {

        unsigned int width = adjusted_width, height = adjusted_height;

        int level;
        for (level = 0; level < s.levels; level++) {
            width = MAX(width, 1);
            height = MAX(height, 1);

            if (f.gl_format == 0) { /* compressed */
                 // https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-block-compression#virtual-size-versus-physical-size
                unsigned int block_size =
                    f.gl_internal_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT ?
                        8 : 16;
                unsigned int physical_width = (width + 3) & ~3,
                             physical_height = (height + 3) & ~3;
                if (physical_width != width) {
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, physical_width);
                }
                uint8_t *converted = decompress_2d_texture_data(
                    f.gl_internal_format, texture_data, physical_width,
                    physical_height);
                unsigned int tex_width = width;
                unsigned int tex_height = height;

                if (s.cubemap && adjusted_width != s.width) {
                    // FIXME: Consider preserving the border.
                    // There does not seem to be a way to reference the border
                    // texels in a cubemap, so they are discarded.
                    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 4);
                    glPixelStorei(GL_UNPACK_SKIP_ROWS, 4);
                    tex_width = s.width;
                    tex_height = s.height;
                    if (physical_width == width) {
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, adjusted_width);
                    }
                }

                glTexImage2D(gl_target, level, GL_RGBA, tex_width, tex_height, 0,
                             GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, converted);
                g_free(converted);
                if (physical_width != width) {
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                }
                if (s.cubemap && adjusted_width != s.width) {
                    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
                    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
                    if (physical_width == width) {
                        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                    }
                }
                texture_data +=
                    physical_width / 4 * physical_height / 4 * block_size;
            } else {
                unsigned int pitch = width * f.bytes_per_pixel;
                uint8_t *unswizzled = (uint8_t*)g_malloc(height * pitch);
                unswizzle_rect(texture_data, width, height,
                               unswizzled, pitch, f.bytes_per_pixel);
                uint8_t *converted = convert_texture_data(s, unswizzled,
                                                          palette_data,
                                                          width, height, 1,
                                                          pitch, 0);
                uint8_t *pixel_data = converted ? converted : unswizzled;
                unsigned int tex_width = width;
                unsigned int tex_height = height;

                if (s.cubemap && adjusted_width != s.width) {
                    // FIXME: Consider preserving the border.
                    // There does not seem to be a way to reference the border
                    // texels in a cubemap, so they are discarded.
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, adjusted_width);
                    tex_width = s.width;
                    tex_height = s.height;
                    pixel_data += 4 * f.bytes_per_pixel + 4 * pitch;
                }

                glTexImage2D(gl_target, level, f.gl_internal_format, tex_width,
                             tex_height, 0, f.gl_format, f.gl_type,
                             pixel_data);
                if (s.cubemap && s.border) {
                    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                }
                if (converted) {
                    g_free(converted);
                }
                g_free(unswizzled);

                texture_data += width * height * f.bytes_per_pixel;
            }

            width /= 2;
            height /= 2;
        }

        break;
    }
    case GL_TEXTURE_3D: {

        unsigned int width = adjusted_width;
        unsigned int height = adjusted_height;
        unsigned int depth = adjusted_depth;

        assert(f.linear == false);

        int level;
        for (level = 0; level < s.levels; level++) {
            if (f.gl_format == 0) { /* compressed */
                assert(width % 4 == 0 && height % 4 == 0 &&
                       "Compressed 3D texture virtual size");
                width = MAX(width, 4);
                height = MAX(height, 4);
                depth = MAX(depth, 1);

                unsigned int block_size;
                if (f.gl_internal_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
                    block_size = 8;
                } else {
                    block_size = 16;
                }

                size_t texture_size = width/4 * height/4 * depth * block_size;

                uint8_t *converted = decompress_3d_texture_data(f.gl_internal_format, texture_data, width, height, depth);

                glTexImage3D(gl_target, level,  GL_RGBA8,
                             width, height, depth, 0,
                             GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,
                             converted);

                g_free(converted);

                texture_data += texture_size;
            } else {
                width = MAX(width, 1);
                height = MAX(height, 1);
                depth = MAX(depth, 1);

                unsigned int row_pitch = width * f.bytes_per_pixel;
                unsigned int slice_pitch = row_pitch * height;
                uint8_t *unswizzled = (uint8_t*)g_malloc(slice_pitch * depth);
                unswizzle_box(texture_data, width, height, depth, unswizzled,
                               row_pitch, slice_pitch, f.bytes_per_pixel);

                uint8_t *converted = convert_texture_data(s, unswizzled,
                                                          palette_data,
                                                          width, height, depth,
                                                          row_pitch, slice_pitch);

                glTexImage3D(gl_target, level, f.gl_internal_format,
                             width, height, depth, 0,
                             f.gl_format, f.gl_type,
                             converted ? converted : unswizzled);

                if (converted) {
                    g_free(converted);
                }
                g_free(unswizzled);

                texture_data += width * height * depth * f.bytes_per_pixel;
            }

            width /= 2;
            height /= 2;
            depth /= 2;
        }
        break;
    }
    default:
        assert(false);
        break;
    }
}

static TextureBinding* generate_texture(const TextureShape s,
                                        const uint8_t *texture_data,
                                        const uint8_t *palette_data)
{
    ColorFormatInfo f = kelvin_color_format_map[s.color_format];

    /* Create a new opengl texture */
    GLuint gl_texture;
    glGenTextures(1, &gl_texture);

    GLenum gl_target;
    if (s.cubemap) {
        assert(f.linear == false);
        assert(s.dimensionality == 2);
        gl_target = GL_TEXTURE_CUBE_MAP;
    } else {
        if (f.linear) {
            /* linear textures use unnormalised texcoords.
             * GL_TEXTURE_RECTANGLE_ARB conveniently also does, but
             * does not allow repeat and mirror wrap modes.
             *  (or mipmapping, but xbox d3d says 'Non swizzled and non
             *   compressed textures cannot be mip mapped.')
             * Not sure if that'll be an issue. */

            /* FIXME: GLSL 330 provides us with textureSize()! Use that? */
            gl_target = GL_TEXTURE_RECTANGLE;
            assert(s.dimensionality == 2);
        } else {
            switch(s.dimensionality) {
            case 1: gl_target = GL_TEXTURE_1D; break;
            case 2: gl_target = GL_TEXTURE_2D; break;
            case 3: gl_target = GL_TEXTURE_3D; break;
            default:
                assert(false);
                break;
            }
        }
    }

    glBindTexture(gl_target, gl_texture);

    NV2A_GL_DLABEL(GL_TEXTURE, gl_texture,
                   "offset: 0x%08lx, format: 0x%02X%s, %d dimensions%s, "
                   "width: %d, height: %d, depth: %d",
                   texture_data - g_nv2a->vram_ptr,
                   s.color_format, f.linear ? "" : " (SZ)",
                   s.dimensionality, s.cubemap ? " (Cubemap)" : "",
                   s.width, s.height, s.depth);

    if (gl_target == GL_TEXTURE_CUBE_MAP) {

        ColorFormatInfo f = kelvin_color_format_map[s.color_format];
        unsigned int block_size;
        if (f.gl_internal_format == GL_COMPRESSED_RGBA_S3TC_DXT1_EXT) {
            block_size = 8;
        } else {
            block_size = 16;
        }

        size_t length = 0;
        unsigned int w = s.width;
        unsigned int h = s.height;
        if (!f.linear && s.border) {
            w = MAX(16, w * 2);
            h = MAX(16, h * 2);
        }

        int level;
        for (level = 0; level < s.levels; level++) {
            if (f.gl_format == 0) {
                length += w/4 * h/4 * block_size;
            } else {
                length += w * h * f.bytes_per_pixel;
            }

            w /= 2;
            h /= 2;
        }

        length = (length + NV2A_CUBEMAP_FACE_ALIGNMENT - 1) & ~(NV2A_CUBEMAP_FACE_ALIGNMENT - 1);

        upload_gl_texture(GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                          s, texture_data + 0 * length, palette_data);
        upload_gl_texture(GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                          s, texture_data + 1 * length, palette_data);
        upload_gl_texture(GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                          s, texture_data + 2 * length, palette_data);
        upload_gl_texture(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                          s, texture_data + 3 * length, palette_data);
        upload_gl_texture(GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                          s, texture_data + 4 * length, palette_data);
        upload_gl_texture(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
                          s, texture_data + 5 * length, palette_data);
    } else {
        upload_gl_texture(gl_target, s, texture_data, palette_data);
    }

    /* Linear textures don't support mipmapping */
    if (!f.linear) {
        glTexParameteri(gl_target, GL_TEXTURE_BASE_LEVEL,
            s.min_mipmap_level);
        glTexParameteri(gl_target, GL_TEXTURE_MAX_LEVEL,
            s.levels - 1);
    }

    if (f.gl_swizzle_mask[0] != 0 || f.gl_swizzle_mask[1] != 0
        || f.gl_swizzle_mask[2] != 0 || f.gl_swizzle_mask[3] != 0) {
        glTexParameteriv(gl_target, GL_TEXTURE_SWIZZLE_RGBA,
                         (const GLint *)f.gl_swizzle_mask);
    }

    TextureBinding* ret = (TextureBinding *)g_malloc(sizeof(TextureBinding));
    ret->gl_target = gl_target;
    ret->gl_texture = gl_texture;
    ret->refcnt = 1;
    ret->draw_time = 0;
    ret->data_hash = 0;
    ret->min_filter = 0xFFFFFFFF;
    ret->mag_filter = 0xFFFFFFFF;
    ret->addru = 0xFFFFFFFF;
    ret->addrv = 0xFFFFFFFF;
    ret->addrp = 0xFFFFFFFF;
    ret->border_color_set = false;
    return ret;
}

static void texture_binding_destroy(gpointer data)
{
    TextureBinding *binding = (TextureBinding *)data;
    assert(binding->refcnt > 0);
    binding->refcnt--;
    if (binding->refcnt == 0) {
        glDeleteTextures(1, &binding->gl_texture);
        g_free(binding);
    }
}

/* functions for texture LRU cache */
static void texture_cache_entry_init(Lru *lru, LruNode *node, void *key)
{
    TextureLruNode *tnode = container_of(node, TextureLruNode, node);
    memcpy(&tnode->key, key, sizeof(TextureKey));

    tnode->binding = NULL;
    tnode->possibly_dirty = false;
}

static void texture_cache_entry_post_evict(Lru *lru, LruNode *node)
{
    TextureLruNode *tnode = container_of(node, TextureLruNode, node);
    if (tnode->binding) {
        texture_binding_destroy(tnode->binding);
        tnode->binding = NULL;
        tnode->possibly_dirty = false;
    }
}

static bool texture_cache_entry_compare(Lru *lru, LruNode *node, void *key)
{
    TextureLruNode *tnode = container_of(node, TextureLruNode, node);
    return memcmp(&tnode->key, key, sizeof(TextureKey));
}

static unsigned int kelvin_map_stencil_op(uint32_t parameter)
{
    unsigned int op;
    switch (parameter) {
    case NV097_SET_STENCIL_OP_V_KEEP:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_KEEP; break;
    case NV097_SET_STENCIL_OP_V_ZERO:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_ZERO; break;
    case NV097_SET_STENCIL_OP_V_REPLACE:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_REPLACE; break;
    case NV097_SET_STENCIL_OP_V_INCRSAT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCRSAT; break;
    case NV097_SET_STENCIL_OP_V_DECRSAT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECRSAT; break;
    case NV097_SET_STENCIL_OP_V_INVERT:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INVERT; break;
    case NV097_SET_STENCIL_OP_V_INCR:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_INCR; break;
    case NV097_SET_STENCIL_OP_V_DECR:
        op = NV_PGRAPH_CONTROL_2_STENCIL_OP_V_DECR; break;
    default:
        assert(false);
        break;
    }
    return op;
}

static unsigned int kelvin_map_polygon_mode(uint32_t parameter)
{
    unsigned int mode;
    switch (parameter) {
    case NV097_SET_FRONT_POLYGON_MODE_V_POINT:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_POINT; break;
    case NV097_SET_FRONT_POLYGON_MODE_V_LINE:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_LINE; break;
    case NV097_SET_FRONT_POLYGON_MODE_V_FILL:
        mode = NV_PGRAPH_SETUPRASTER_FRONTFACEMODE_FILL; break;
    default:
        assert(false);
        break;
    }
    return mode;
}

static unsigned int kelvin_map_texgen(uint32_t parameter, unsigned int channel)
{
    assert(channel < 4);
    unsigned int texgen;
    switch (parameter) {
    case NV097_SET_TEXGEN_S_DISABLE:
        texgen = NV_PGRAPH_CSV1_A_T0_S_DISABLE; break;
    case NV097_SET_TEXGEN_S_EYE_LINEAR:
        texgen = NV_PGRAPH_CSV1_A_T0_S_EYE_LINEAR; break;
    case NV097_SET_TEXGEN_S_OBJECT_LINEAR:
        texgen = NV_PGRAPH_CSV1_A_T0_S_OBJECT_LINEAR; break;
    case NV097_SET_TEXGEN_S_SPHERE_MAP:
        assert(channel < 2);
        texgen = NV_PGRAPH_CSV1_A_T0_S_SPHERE_MAP; break;
    case NV097_SET_TEXGEN_S_REFLECTION_MAP:
        assert(channel < 3);
        texgen = NV_PGRAPH_CSV1_A_T0_S_REFLECTION_MAP; break;
    case NV097_SET_TEXGEN_S_NORMAL_MAP:
        assert(channel < 3);
        texgen = NV_PGRAPH_CSV1_A_T0_S_NORMAL_MAP; break;
    default:
        assert(false);
        break;
    }
    return texgen;
}
