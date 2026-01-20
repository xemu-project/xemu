/*
 * Geforce NV2A PGRAPH OpenGL Renderer
 *
 * Copyright (c) 2012 espes
 * Copyright (c) 2015 Jannik Vogel
 * Copyright (c) 2018-2025 Matt Borgerson
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

#ifndef HW_XBOX_NV2A_PGRAPH_GL_RENDERER_H
#define HW_XBOX_NV2A_PGRAPH_GL_RENDERER_H

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "qemu/queue.h"
#include "qemu/lru.h"

#include "hw/hw.h"

#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/nv2a_regs.h"
#include "hw/xbox/nv2a/pgraph/surface.h"
#include "hw/xbox/nv2a/pgraph/texture.h"
#include "hw/xbox/nv2a/pgraph/glsl/shaders.h"

#include "gloffscreen.h"
#include "constants.h"

typedef struct SurfaceBinding {
    QTAILQ_ENTRY(SurfaceBinding) entry;
    MemAccessCallback *access_cb;

    hwaddr vram_addr;

    SurfaceShape shape;
    uintptr_t dma_addr;
    uintptr_t dma_len;
    bool color;
    bool swizzle;

    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    size_t size;

    bool cleared;
    int frame_time;
    int draw_time;
    bool draw_dirty;
    bool download_pending;
    bool upload_pending;

    GLuint gl_buffer;
    SurfaceFormatInfo fmt;
} SurfaceBinding;

typedef struct TextureBinding {
    unsigned int refcnt;
    int draw_time;
    uint64_t data_hash;
    unsigned int scale;
    unsigned int min_filter;
    unsigned int mag_filter;
    uint32_t lod_bias;
    unsigned int addru;
    unsigned int addrv;
    unsigned int addrp;
    uint32_t border_color;
    bool border_color_set;
    GLenum gl_target;
    GLuint gl_texture;
} TextureBinding;

typedef struct ShaderModuleCacheKey {
    GLenum kind;
    union {
        struct {
            VshState state;
            GenVshGlslOptions glsl_opts;
        } vsh;
        struct {
            GeomState state;
            GenGeomGlslOptions glsl_opts;
        } geom;
        struct {
            PshState state;
            GenPshGlslOptions glsl_opts;
        } psh;
    };
} ShaderModuleCacheKey;

typedef struct ShaderModuleCacheEntry {
    LruNode node;
    ShaderModuleCacheKey key;
    GLuint gl_shader;
} ShaderModuleCacheEntry;

typedef struct ShaderBinding {
    LruNode node;
    bool initialized;

    bool cached;
    void *program;
    size_t program_size;
    GLenum program_format;
    ShaderState state;
    QemuThread *save_thread;

    GLuint gl_program;
    GLenum gl_primitive_mode;

    struct {
        PshUniformLocs psh;
        VshUniformLocs vsh;
    } uniform_locs;
} ShaderBinding;

typedef struct VertexKey {
    size_t count;
    size_t stride;
    hwaddr addr;

    GLboolean gl_normalize;
    GLuint gl_type;
} VertexKey;

typedef struct VertexLruNode {
    LruNode node;
    VertexKey key;
    bool initialized;

    GLuint gl_buffer;
} VertexLruNode;

typedef struct TextureKey {
    TextureShape state;
    hwaddr texture_vram_offset;
    hwaddr texture_length;
    hwaddr palette_vram_offset;
    hwaddr palette_length;
} TextureKey;

typedef struct TextureLruNode {
    LruNode node;
    TextureKey key;
    TextureBinding *binding;
    bool possibly_dirty;
} TextureLruNode;

typedef struct QueryReport {
    QSIMPLEQ_ENTRY(QueryReport) entry;
    bool clear;
    uint32_t parameter;
    unsigned int query_count;
    GLuint *queries;
} QueryReport;

typedef struct PGRAPHGLState {
    GLuint gl_framebuffer;
    GLuint gl_display_buffer;
    GLint gl_display_buffer_internal_format;
    GLsizei gl_display_buffer_width;
    GLsizei gl_display_buffer_height;
    GLenum gl_display_buffer_format;
    GLenum gl_display_buffer_type;

    Lru element_cache;
    VertexLruNode *element_cache_entries;
    GLuint gl_inline_array_buffer;
    GLuint gl_memory_buffer;
    GLuint gl_vertex_array;
    GLuint gl_inline_buffer[NV2A_VERTEXSHADER_ATTRIBUTES];

    QTAILQ_HEAD(, SurfaceBinding) surfaces;
    SurfaceBinding *color_binding, *zeta_binding;
    bool downloads_pending;
    QemuEvent downloads_complete;
    bool download_dirty_surfaces_pending;
    QemuEvent dirty_surfaces_download_complete; // common

    TextureBinding *texture_binding[NV2A_MAX_TEXTURES];
    Lru texture_cache;
    TextureLruNode *texture_cache_entries;

    Lru shader_cache;
    ShaderBinding *shader_cache_entries;
    ShaderBinding *shader_binding;
    QemuMutex shader_cache_lock;
    QemuThread shader_disk_thread;

    Lru shader_module_cache;
    ShaderModuleCacheEntry *shader_module_cache_entries;

    unsigned int zpass_pixel_count_result;
    unsigned int gl_zpass_pixel_count_query_count;
    GLuint *gl_zpass_pixel_count_queries;
    QSIMPLEQ_HEAD(, QueryReport) report_queue;

    bool shader_cache_writeback_pending;
    QemuEvent shader_cache_writeback_complete;

    struct s2t_rndr {
        GLuint fbo, vao, vbo, prog;
        GLuint tex_loc, surface_size_loc;
    } s2t_rndr;

    struct disp_rndr {
        GLuint fbo, vao, vbo, prog;
        GLuint display_size_loc;
        GLuint line_offset_loc;
        GLuint tex_loc;
        GLuint pvideo_tex;
        GLint pvideo_enable_loc;
        GLint pvideo_tex_loc;
        GLint pvideo_in_pos_loc;
        GLint pvideo_pos_loc;
        GLint pvideo_scale_loc;
        GLint pvideo_color_key_enable_loc;
        GLint pvideo_color_key_loc;
        GLint palette_loc[256];
    } disp_rndr;

    GLfloat supported_aliased_line_width_range[2];
    GLfloat supported_smooth_line_width_range[2];

    struct supported_extensions {
        GLboolean texture_filter_anisotropic;
    } supported_extensions;
} PGRAPHGLState;

extern GloContext *g_nv2a_context_render;
extern GloContext *g_nv2a_context_display;

unsigned int pgraph_gl_bind_inline_array(NV2AState *d);
void pgraph_gl_bind_shaders(PGRAPHState *pg);
void pgraph_gl_bind_textures(NV2AState *d);
void pgraph_gl_bind_vertex_attributes(NV2AState *d, unsigned int min_element, unsigned int max_element, bool inline_data, unsigned int inline_stride, unsigned int provoking_element);
bool pgraph_gl_check_surface_to_texture_compatibility(const SurfaceBinding *surface, const TextureShape *shape);
GLuint pgraph_gl_compile_shader(const char *vs_src, const char *fs_src);
void pgraph_gl_download_dirty_surfaces(NV2AState *d);
void pgraph_gl_clear_report_value(NV2AState *d);
void pgraph_gl_clear_surface(NV2AState *d, uint32_t parameter);
void pgraph_gl_draw_begin(NV2AState *d);
void pgraph_gl_draw_end(NV2AState *d);
void pgraph_gl_flush_draw(NV2AState *d);
void pgraph_gl_get_report(NV2AState *d, uint32_t parameter);
void pgraph_gl_image_blit(NV2AState *d);
void pgraph_gl_mark_textures_possibly_dirty(NV2AState *d, hwaddr addr, hwaddr size);
void pgraph_gl_process_pending_reports(NV2AState *d);
void pgraph_gl_surface_flush(NV2AState *d);
void pgraph_gl_surface_update(NV2AState *d, bool upload, bool color_write, bool zeta_write);
void pgraph_gl_sync(NV2AState *d);
void pgraph_gl_update_entire_memory_buffer(NV2AState *d);
void pgraph_gl_init_display(NV2AState *d);
void pgraph_gl_finalize_display(PGRAPHState *pg);
void pgraph_gl_init_reports(NV2AState *d);
void pgraph_gl_finalize_reports(PGRAPHState *pg);
void pgraph_gl_init_shaders(PGRAPHState *pg);
void pgraph_gl_finalize_shaders(PGRAPHState *pg);
void pgraph_gl_init_surfaces(PGRAPHState *pg);
void pgraph_gl_finalize_surfaces(PGRAPHState *pg);
void pgraph_gl_init_textures(NV2AState *d);
void pgraph_gl_finalize_textures(PGRAPHState *pg);
void pgraph_gl_init_buffers(NV2AState *d);
void pgraph_gl_finalize_buffers(PGRAPHState *pg);
void pgraph_gl_process_pending_downloads(NV2AState *d);
void pgraph_gl_reload_surface_scale_factor(PGRAPHState *pg);
void pgraph_gl_render_surface_to_texture(NV2AState *d, SurfaceBinding *surface, TextureBinding *texture, TextureShape *texture_shape, int texture_unit);
void pgraph_gl_set_surface_dirty(PGRAPHState *pg, bool color, bool zeta);
void pgraph_gl_surface_download_if_dirty(NV2AState *d, SurfaceBinding *surface);
SurfaceBinding *pgraph_gl_surface_get(NV2AState *d, hwaddr addr);
SurfaceBinding *pgraph_gl_surface_get_within(NV2AState *d, hwaddr addr);
void pgraph_gl_surface_invalidate(NV2AState *d, SurfaceBinding *e);
void pgraph_gl_unbind_surface(NV2AState *d, bool color);
void pgraph_gl_upload_surface_data(NV2AState *d, SurfaceBinding *surface, bool force);
void pgraph_gl_shader_cache_to_disk(ShaderBinding *snode);
bool pgraph_gl_shader_load_from_memory(ShaderBinding *snode);
void pgraph_gl_shader_write_cache_reload_list(PGRAPHState *pg);
void pgraph_gl_set_surface_scale_factor(NV2AState *d, unsigned int scale);
unsigned int pgraph_gl_get_surface_scale_factor(NV2AState *d);
int pgraph_gl_get_framebuffer_surface(NV2AState *d);
/**  Note: The caller must set up a clean GL context before invoking. */
void pgraph_gl_determine_gpu_properties(void);
GPUProperties *pgraph_gl_get_gpu_properties(void);

#endif
