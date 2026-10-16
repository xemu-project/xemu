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

#include "hw/xbox/nv2a/pgraph/pgraph.h"
#include "ui/xemu-settings.h"
#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/pgraph/swizzle.h"
#include "debug.h"
#include "renderer.h"

static void surface_download(NV2AState *d, SurfaceBinding *surface, bool force);
static void surface_download_to_buffer(NV2AState *d, SurfaceBinding *surface,
                                       bool swizzle, bool flip, bool downscale,
                                       uint8_t *pixels);
static void surface_get_dimensions(PGRAPHState *pg, unsigned int *width, unsigned int *height);

void pgraph_gl_set_surface_scale_factor(NV2AState *d, unsigned int scale)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    g_config.display.quality.surface_scale = scale < 1 ? 1 : scale;

    qemu_mutex_lock(&d->pfifo.lock);
    qatomic_set(&d->pfifo.halt, true);
    qemu_mutex_unlock(&d->pfifo.lock);

    qemu_mutex_lock(&d->pgraph.lock);
    qemu_event_reset(&r->dirty_surfaces_download_complete);
    qatomic_set(&r->download_dirty_surfaces_pending, true);
    qemu_mutex_unlock(&d->pgraph.lock);
    qemu_mutex_lock(&d->pfifo.lock);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&r->dirty_surfaces_download_complete);

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
}

unsigned int pgraph_gl_get_surface_scale_factor(NV2AState *d)
{
    return d->pgraph.surface_scale_factor;
}

void pgraph_gl_reload_surface_scale_factor(PGRAPHState *pg)
{
    int factor = g_config.display.quality.surface_scale;
    pg->surface_scale_factor = factor < 1 ? 1 : factor;
}

// FIXME: Move to common
static bool framebuffer_dirty(PGRAPHState *pg)
{
    bool shape_changed = memcmp(&pg->surface_shape, &pg->last_surface_shape,
                                sizeof(SurfaceShape)) != 0;
    if (!shape_changed || (!pg->surface_shape.color_format
            && !pg->surface_shape.zeta_format)) {
        return false;
    }
    return true;
}

void pgraph_gl_set_surface_dirty(PGRAPHState *pg, bool color, bool zeta)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    NV2A_DPRINTF("pgraph_set_surface_dirty(%d, %d) -- %d %d\n",
                 color, zeta,
                 pgraph_color_write_enabled(pg), pgraph_zeta_write_enabled(pg));
    /* FIXME: Does this apply to CLEARs too? */
    color = color && pgraph_color_write_enabled(pg);
    zeta = zeta && pgraph_zeta_write_enabled(pg);
    pg->surface_color.draw_dirty |= color;
    pg->surface_zeta.draw_dirty |= zeta;

    if (r->color_binding) {
        r->color_binding->draw_dirty |= color;
        r->color_binding->frame_time = pg->frame_time;
        r->color_binding->cleared = false;

    }

    if (r->zeta_binding) {
        r->zeta_binding->draw_dirty |= zeta;
        r->zeta_binding->frame_time = pg->frame_time;
        r->zeta_binding->cleared = false;

    }
}

static void init_render_to_texture(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

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
        "    vec2 texCoord = gl_FragCoord.xy / textureSize(tex, 0).xy;\n"
        "    out_Color.rgba = texture(tex, texCoord);\n"
        "}\n";

    r->s2t_rndr.prog = pgraph_gl_compile_shader(vs, fs);
    r->s2t_rndr.tex_loc = glGetUniformLocation(r->s2t_rndr.prog, "tex");
    r->s2t_rndr.surface_size_loc = glGetUniformLocation(r->s2t_rndr.prog,
                                                    "surface_size");

    glGenVertexArrays(1, &r->s2t_rndr.vao);
    glBindVertexArray(r->s2t_rndr.vao);
    glGenBuffers(1, &r->s2t_rndr.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->s2t_rndr.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glGenFramebuffers(1, &r->s2t_rndr.fbo);
}

static void finalize_render_to_texture(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    glDeleteProgram(r->s2t_rndr.prog);
    r->s2t_rndr.prog = 0;

    glDeleteVertexArrays(1, &r->s2t_rndr.vao);
    r->s2t_rndr.vao = 0;

    glDeleteBuffers(1, &r->s2t_rndr.vbo);
    r->s2t_rndr.vbo = 0;

    glDeleteFramebuffers(1, &r->s2t_rndr.fbo);
    r->s2t_rndr.fbo = 0;
}

static bool surface_to_texture_can_fastpath(SurfaceBinding *surface,
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

static void render_surface_to(NV2AState *d, SurfaceBinding *surface,
                              int texture_unit, GLuint gl_target,
                              GLuint gl_texture, unsigned int width,
                              unsigned int height)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindFramebuffer(GL_FRAMEBUFFER, r->s2t_rndr.fbo);

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

    glBindVertexArray(r->s2t_rndr.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->s2t_rndr.vbo);
    glUseProgram(r->s2t_rndr.prog);
    glProgramUniform1i(r->s2t_rndr.prog, r->s2t_rndr.tex_loc,
                       texture_unit);
    glProgramUniform2f(r->s2t_rndr.prog,
                       r->s2t_rndr.surface_size_loc, width, height);

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
    glBindFramebuffer(GL_FRAMEBUFFER, r->gl_framebuffer);
    glBindVertexArray(r->gl_vertex_array);
    glBindTexture(gl_target, gl_texture);
    glUseProgram(
        r->shader_binding ? r->shader_binding->gl_program : 0);
}

static void render_surface_to_texture_slow(NV2AState *d,
                                           SurfaceBinding *surface,
                                           TextureBinding *texture,
                                           TextureShape *texture_shape,
                                           int texture_unit)
{
    PGRAPHState *pg = &d->pgraph;

    const ColorFormatInfo *f = &kelvin_color_format_gl_map[texture_shape->color_format];
    assert(texture_shape->color_format < ARRAY_SIZE(kelvin_color_format_gl_map));
    nv2a_profile_inc_counter(NV2A_PROF_SURF_TO_TEX_FALLBACK);

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(texture->gl_target, texture->gl_texture);

    unsigned int width = surface->width,
                 height = surface->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    size_t bufsize = width * height * surface->fmt.bytes_per_pixel;

    uint8_t *buf = g_malloc(bufsize);
    surface_download_to_buffer(d, surface, false, false, false, buf);

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
void pgraph_gl_render_surface_to_texture(NV2AState *d, SurfaceBinding *surface,
                                      TextureBinding *texture,
                                      TextureShape *texture_shape,
                                      int texture_unit)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    const ColorFormatInfo *f =
        &kelvin_color_format_gl_map[texture_shape->color_format];
    assert(texture_shape->color_format < ARRAY_SIZE(kelvin_color_format_gl_map));

    nv2a_profile_inc_counter(NV2A_PROF_SURF_TO_TEX);

    if (!surface_to_texture_can_fastpath(surface, texture_shape)) {
        render_surface_to_texture_slow(d, surface, texture,
                                              texture_shape, texture_unit);
        return;
    }

    unsigned int width = texture_shape->width, height = texture_shape->height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    glActiveTexture(GL_TEXTURE0 + texture_unit);
    glBindTexture(texture->gl_target, texture->gl_texture);
    glTexParameteri(texture->gl_target, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(texture->gl_target, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(texture->gl_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(texture->gl_target, 0, f->gl_internal_format, width, height, 0,
                 f->gl_format, f->gl_type, NULL);
    glBindTexture(texture->gl_target, 0);
    render_surface_to(d, surface, texture_unit, texture->gl_target,
                             texture->gl_texture, width, height);
    glBindTexture(texture->gl_target, texture->gl_texture);
    glUseProgram(
        r->shader_binding ? r->shader_binding->gl_program : 0);
}

bool pgraph_gl_check_surface_to_texture_compatibility(
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

static bool check_surface_overlaps_range(const SurfaceBinding *surface,
                                         hwaddr range_start, hwaddr range_len)
{
    hwaddr surface_end = surface->vram_addr + surface->size;
    hwaddr range_end = range_start + range_len;
    return !(surface->vram_addr >= range_end || range_start >= surface_end);
}

static void surface_access_callback(void *opaque, MemoryRegion *mr, hwaddr addr,
                                    hwaddr len, bool write)
{
    NV2AState *d = (NV2AState *)opaque;
    qemu_mutex_lock(&d->pgraph.lock);

    PGRAPHGLState *r = d->pgraph.gl_renderer_state;
    bool wait_for_downloads = false;

    SurfaceBinding *surface;
    QTAILQ_FOREACH(surface, &r->surfaces, entry) {
        if (!check_surface_overlaps_range(surface, addr, len)) {
            continue;
        }

        hwaddr offset = addr - surface->vram_addr;

        if (write) {
            trace_nv2a_pgraph_surface_cpu_write(surface->vram_addr, offset);
        } else {
            trace_nv2a_pgraph_surface_cpu_read(surface->vram_addr, offset);
        }

        if (surface->draw_dirty) {
            surface->download_pending = true;
            wait_for_downloads = true;
        }

        if (write) {
            surface->upload_pending = true;
        }
    }

    qemu_mutex_unlock(&d->pgraph.lock);

    if (wait_for_downloads) {
        qemu_mutex_lock(&d->pfifo.lock);
        qemu_event_reset(&r->downloads_complete);
        qatomic_set(&r->downloads_pending, true);
        pfifo_kick(d);
        qemu_mutex_unlock(&d->pfifo.lock);
        qemu_event_wait(&r->downloads_complete);
    }
}

static void register_cpu_access_callback(NV2AState *d, SurfaceBinding *surface)
{
    if (tcg_enabled()) {
        if (surface->width && surface->height) {
            surface->access_cb = mem_access_callback_insert(
                qemu_get_cpu(0), d->vram, surface->vram_addr, surface->size,
                &surface_access_callback, d);
        } else {
            surface->access_cb = NULL;
        }
    }
}

static void unregister_cpu_access_callback(NV2AState *d,
                                           SurfaceBinding const *surface)
{
    if (tcg_enabled()) {
        mem_access_callback_remove_by_ref(qemu_get_cpu(0), surface->access_cb);
    }
}

static bool check_surfaces_overlap(const SurfaceBinding *surface,
                                   const SurfaceBinding *other_surface)
{
    return check_surface_overlaps_range(surface, other_surface->vram_addr,
                                        other_surface->size);
}

static void invalidate_overlapping_surfaces(NV2AState *d, SurfaceBinding *surface)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding *other_surface, *next_surface;
    QTAILQ_FOREACH_SAFE(other_surface, &r->surfaces, entry, next_surface) {
        if (check_surfaces_overlap(surface, other_surface)) {
            trace_nv2a_pgraph_surface_evict_overlapping(
                other_surface->vram_addr, other_surface->width, other_surface->height,
                other_surface->pitch);
            pgraph_gl_surface_download_if_dirty(d, other_surface);
            pgraph_gl_surface_invalidate(d, other_surface);
        }
    }
}

static SurfaceBinding *surface_put(NV2AState *d, hwaddr addr,
                                   SurfaceBinding *surface_in)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    assert(pgraph_gl_surface_get(d, addr) == NULL);

    invalidate_overlapping_surfaces(d, surface_in);

    SurfaceBinding *surface_out = g_malloc(sizeof(SurfaceBinding));
    assert(surface_out != NULL);
    *surface_out = *surface_in;

    register_cpu_access_callback(d, surface_out);

    QTAILQ_INSERT_TAIL(&r->surfaces, surface_out, entry);

    return surface_out;
}

SurfaceBinding *pgraph_gl_surface_get(NV2AState *d, hwaddr addr)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding *surface;
    QTAILQ_FOREACH (surface, &r->surfaces, entry) {
        if (surface->vram_addr == addr) {
            return surface;
        }
    }

    return NULL;
}

SurfaceBinding *pgraph_gl_surface_get_within(NV2AState *d, hwaddr addr)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding *surface;
    QTAILQ_FOREACH (surface, &r->surfaces, entry) {
        if (addr >= surface->vram_addr &&
            addr < (surface->vram_addr + surface->size)) {
            return surface;
        }
    }

    return NULL;
}

void pgraph_gl_surface_invalidate(NV2AState *d, SurfaceBinding *surface)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    trace_nv2a_pgraph_surface_invalidated(surface->vram_addr);

    if (surface == r->color_binding) {
        assert(d->pgraph.surface_color.buffer_dirty);
        pgraph_gl_unbind_surface(d, true);
    }
    if (surface == r->zeta_binding) {
        assert(d->pgraph.surface_zeta.buffer_dirty);
        pgraph_gl_unbind_surface(d, false);
    }

    unregister_cpu_access_callback(d, surface);

    glDeleteTextures(1, &surface->gl_buffer);

    QTAILQ_REMOVE(&r->surfaces, surface, entry);
    g_free(surface);
}

static void surface_evict_old(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    const int surface_age_limit = 5;

    SurfaceBinding *s, *next;
    QTAILQ_FOREACH_SAFE(s, &r->surfaces, entry, next) {
        int last_used = d->pgraph.frame_time - s->frame_time;
        if (last_used >= surface_age_limit) {
            trace_nv2a_pgraph_surface_evict_reason("old", s->vram_addr);
            pgraph_gl_surface_download_if_dirty(d, s);
            pgraph_gl_surface_invalidate(d, s);
        }
    }
}

static bool check_surface_compatibility(SurfaceBinding *s1, SurfaceBinding *s2,
                                        bool strict)
{
    bool format_compatible =
        (s1->color == s2->color) &&
        (s1->fmt.gl_attachment == s2->fmt.gl_attachment) &&
        (s1->fmt.gl_internal_format == s2->fmt.gl_internal_format) &&
        (s1->pitch == s2->pitch);
    if (!format_compatible) {
        return false;
    }

    if (!strict) {
        return (s1->width >= s2->width) && (s1->height >= s2->height);
    } else {
        return (s1->width == s2->width) && (s1->height == s2->height);
    }
}

void pgraph_gl_surface_download_if_dirty(NV2AState *d,
                                           SurfaceBinding *surface)
{
    if (surface->draw_dirty) {
        surface_download(d, surface, true);
    }
}

static void bind_current_surface(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    if (r->color_binding) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, r->color_binding->fmt.gl_attachment,
                               GL_TEXTURE_2D, r->color_binding->gl_buffer, 0);
    }

    if (r->zeta_binding) {
        glFramebufferTexture2D(GL_FRAMEBUFFER, r->zeta_binding->fmt.gl_attachment,
                               GL_TEXTURE_2D, r->zeta_binding->gl_buffer, 0);
    }

    if (r->color_binding || r->zeta_binding) {
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

static void surface_download_to_buffer(NV2AState *d, SurfaceBinding *surface,
                                       bool swizzle, bool flip, bool downscale,
                                       uint8_t *pixels)
{
    PGRAPHState *pg = &d->pgraph;

    swizzle &= surface->swizzle;
    downscale &= (pg->surface_scale_factor != 1);

    if (!surface->width || !surface->height) {
        return;
    }

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
    bind_current_surface(d);
}

static void surface_download(NV2AState *d, SurfaceBinding *surface, bool force)
{
    if (!(surface->download_pending || force) || !surface->width ||
        !surface->height) {
        return;
    }

    /* FIXME: Respect write enable at last TOU? */

    nv2a_profile_inc_counter(NV2A_PROF_SURF_DOWNLOAD);

    surface_download_to_buffer(d, surface, true, false, true,
                               d->vram_ptr + surface->vram_addr);

    memory_region_set_client_dirty(d->vram, surface->vram_addr,
                                   surface->pitch * surface->height,
                                   DIRTY_MEMORY_VGA);
    memory_region_set_client_dirty(d->vram, surface->vram_addr,
                                   surface->pitch * surface->height,
                                   DIRTY_MEMORY_NV2A_TEX);

    surface->download_pending = false;
    surface->draw_dirty = false;
}

void pgraph_gl_process_pending_downloads(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding *surface;
    QTAILQ_FOREACH(surface, &r->surfaces, entry) {
        surface_download(d, surface, false);
    }

    qatomic_set(&r->downloads_pending, false);
    qemu_event_set(&r->downloads_complete);
}

void pgraph_gl_download_dirty_surfaces(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding *surface;
    QTAILQ_FOREACH(surface, &r->surfaces, entry) {
        pgraph_gl_surface_download_if_dirty(d, surface);
    }

    qatomic_set(&r->download_dirty_surfaces_pending, false);
    qemu_event_set(&r->dirty_surfaces_download_complete);
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

void pgraph_gl_upload_surface_data(NV2AState *d, SurfaceBinding *surface,
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

    if (!surface->width || !surface->height) {
        return;
    }

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

    /* FIXME: Replace this scaling */

    // This is VRAM so we can't do this inplace!
    uint8_t *optimal_buf = buf;
    unsigned int optimal_pitch = surface->width * surface->fmt.bytes_per_pixel;

    if (surface->pitch != optimal_pitch) {
        optimal_buf = (uint8_t *)g_malloc(surface->height * optimal_pitch);

        uint8_t *src = buf;
        uint8_t *dst = optimal_buf;
        unsigned int irow;
        for (irow = 0; irow < surface->height; irow++) {
            memcpy(dst, src, optimal_pitch);
            src += surface->pitch;
            dst += optimal_pitch;
        }
    }

    uint8_t *gl_read_buf = optimal_buf;
    unsigned int width = surface->width, height = surface->height;

    if (pg->surface_scale_factor > 1) {
        pgraph_apply_scaling_factor(pg, &width, &height);
        pg->scale_buf = (uint8_t *)g_realloc(
            pg->scale_buf, width * height * surface->fmt.bytes_per_pixel);
        gl_read_buf = pg->scale_buf;
        uint8_t *out = gl_read_buf, *in = optimal_buf;
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
    if (optimal_buf != buf) {
        g_free(optimal_buf);
    }
    if (surface->swizzle) {
        g_free(buf);
    }

    // Rebind previous framebuffer binding
    glBindTexture(GL_TEXTURE_2D, last_texture_binding);

    bind_current_surface(d);
}

static void compare_surfaces(SurfaceBinding *s1, SurfaceBinding *s2)
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

static void populate_surface_binding_entry_sized(NV2AState *d, bool color,
                                                 unsigned int width,
                                                 unsigned int height,
                                                 SurfaceBinding *entry)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    Surface *surface;
    hwaddr dma_address;
    SurfaceFormatInfo fmt;

    if (color) {
        surface = &pg->surface_color;
        dma_address = pg->dma_color;
        assert(pg->surface_shape.color_format != 0);
        assert(pg->surface_shape.color_format <
               ARRAY_SIZE(kelvin_surface_color_format_gl_map));
        fmt = kelvin_surface_color_format_gl_map[pg->surface_shape.color_format];
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
               ARRAY_SIZE(kelvin_surface_zeta_float_format_gl_map));
        const SurfaceFormatInfo *map =
            pg->surface_shape.z_format ? kelvin_surface_zeta_float_format_gl_map :
                                         kelvin_surface_zeta_fixed_format_gl_map;
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

    entry->shape = (color || !r->color_binding) ? pg->surface_shape :
                                                   r->color_binding->shape;
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

static void populate_surface_binding_entry(NV2AState *d, bool color,
                                                  SurfaceBinding *entry)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    unsigned int width, height;

    if (color || !r->color_binding) {
        surface_get_dimensions(pg, &width, &height);
        pgraph_apply_anti_aliasing_factor(pg, &width, &height);

        /* Since we determine surface dimensions based on the clipping
         * rectangle, make sure to include the surface offset as well.
         */
        if (pg->surface_type != NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE) {
            width += pg->surface_shape.clip_x;
            height += pg->surface_shape.clip_y;
        }
    } else {
        width = r->color_binding->width;
        height = r->color_binding->height;
    }

    populate_surface_binding_entry_sized(d, color, width, height, entry);
}

static void update_surface_part(NV2AState *d, bool upload, bool color)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    SurfaceBinding entry;
    populate_surface_binding_entry(d, color, &entry);

    Surface *surface = color ? &pg->surface_color : &pg->surface_zeta;

    bool mem_dirty = !tcg_enabled() && memory_region_test_and_clear_dirty(
                                           d->vram, entry.vram_addr, entry.size,
                                           DIRTY_MEMORY_NV2A);

    if (upload && (surface->buffer_dirty || mem_dirty)) {
        pgraph_gl_unbind_surface(d, color);

        SurfaceBinding *found = pgraph_gl_surface_get(d, entry.vram_addr);
        if (found != NULL) {
            /* FIXME: Support same color/zeta surface target? In the mean time,
             * if the surface we just found is currently bound, just unbind it.
             */
            SurfaceBinding *other = (color ? r->zeta_binding
                                           : r->color_binding);
            if (found == other) {
                NV2A_UNIMPLEMENTED("Same color & zeta surface offset");
                pgraph_gl_unbind_surface(d, !color);
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
                check_surface_compatibility(found, &entry, false);

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
                    check_surface_compatibility(found, &entry, true);
                if (is_compatible) {
                    trace_nv2a_pgraph_surface_migrate_type(
                        entry.swizzle ? "swizzled" : "linear");
                }
            }

            if (is_compatible && color &&
                !check_surface_compatibility(found, &entry, true)) {
                SurfaceBinding zeta_entry;
                populate_surface_binding_entry_sized(
                    d, !color, found->width, found->height, &zeta_entry);
                hwaddr color_end = found->vram_addr + found->size;
                hwaddr zeta_end = zeta_entry.vram_addr + zeta_entry.size;
                is_compatible &= found->vram_addr >= zeta_end ||
                                 zeta_entry.vram_addr >= color_end;
            }

            if (is_compatible && !color && r->color_binding) {
                is_compatible &= (found->width == r->color_binding->width) &&
                                 (found->height == r->color_binding->height);
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
                compare_surfaces(found, &entry);
                pgraph_gl_surface_download_if_dirty(d, found);
                pgraph_gl_surface_invalidate(d, found);
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
            unsigned int width = entry.width ? entry.width : 1;
            unsigned int height = entry.height ? entry.height : 1;
            pgraph_apply_scaling_factor(pg, &width, &height);
            glTexImage2D(GL_TEXTURE_2D, 0, entry.fmt.gl_internal_format, width,
                         height, 0, entry.fmt.gl_format, entry.fmt.gl_type,
                         NULL);
            found = surface_put(d, entry.vram_addr, &entry);

            /* FIXME: Refactor */
            pg->surface_binding_dim.width = entry.width;
            pg->surface_binding_dim.clip_x = entry.shape.clip_x;
            pg->surface_binding_dim.clip_width = entry.shape.clip_width;
            pg->surface_binding_dim.height = entry.height;
            pg->surface_binding_dim.clip_y = entry.shape.clip_y;
            pg->surface_binding_dim.clip_height = entry.shape.clip_height;

            if (color && r->zeta_binding && (r->zeta_binding->width != entry.width || r->zeta_binding->height != entry.height)) {
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

            r->color_binding = found;
        } else {
            if (should_create) {
                trace_nv2a_pgraph_surface_create_zeta(TRACE_ARGS);
            } else {
                trace_nv2a_pgraph_surface_hit_zeta(TRACE_ARGS);
            }
            r->zeta_binding = found;
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
            surface_download(d,
                             color ? r->color_binding :
                                     r->zeta_binding,
                             true);
        }

        surface->write_enabled_cache = false;
        surface->draw_dirty = false;
    }
}

void pgraph_gl_unbind_surface(NV2AState *d, bool color)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    if (color) {
        if (r->color_binding) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, 0, 0);
            r->color_binding = NULL;
        }
    } else {
        if (r->zeta_binding) {
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_DEPTH_ATTACHMENT,
                                   GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_DEPTH_STENCIL_ATTACHMENT,
                                   GL_TEXTURE_2D, 0, 0);
            r->zeta_binding = NULL;
        }
    }
}

void pgraph_gl_surface_update(NV2AState *d, bool upload, bool color_write,
                           bool zeta_write)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    pg->surface_shape.z_format =
        GET_MASK(pgraph_reg_r(pg, NV_PGRAPH_SETUPRASTER),
                 NV_PGRAPH_SETUPRASTER_Z_FORMAT);

    color_write = color_write &&
            (pg->clearing || pgraph_color_write_enabled(pg));
    zeta_write = zeta_write && (pg->clearing || pgraph_zeta_write_enabled(pg));

    if (upload) {
        bool fb_dirty = framebuffer_dirty(pg);
        if (fb_dirty) {
            memcpy(&pg->last_surface_shape, &pg->surface_shape,
                   sizeof(SurfaceShape));
            pg->surface_color.buffer_dirty = true;
            pg->surface_zeta.buffer_dirty = true;
        }

        if (pg->surface_color.buffer_dirty) {
            pgraph_gl_unbind_surface(d, true);
        }

        if (color_write) {
            update_surface_part(d, true, true);
        }

        if (pg->surface_zeta.buffer_dirty) {
            pgraph_gl_unbind_surface(d, false);
        }

        if (zeta_write) {
            update_surface_part(d, true, false);
        }
    } else {
        if ((color_write || pg->surface_color.write_enabled_cache)
            && pg->surface_color.draw_dirty) {
            update_surface_part(d, false, true);
        }
        if ((zeta_write || pg->surface_zeta.write_enabled_cache)
            && pg->surface_zeta.draw_dirty) {
            update_surface_part(d, false, false);
        }
    }

    if (upload) {
        pg->draw_time++;
    }

    bool swizzle = (pg->surface_type == NV097_SET_SURFACE_FORMAT_TYPE_SWIZZLE);

    if (r->color_binding) {
        r->color_binding->frame_time = pg->frame_time;
        if (upload) {
            pgraph_gl_upload_surface_data(d, r->color_binding, false);
            r->color_binding->draw_time = pg->draw_time;
            r->color_binding->swizzle = swizzle;
        }
    }

    if (r->zeta_binding) {
        r->zeta_binding->frame_time = pg->frame_time;
        if (upload) {
            pgraph_gl_upload_surface_data(d, r->zeta_binding, false);
            r->zeta_binding->draw_time = pg->draw_time;
            r->zeta_binding->swizzle = swizzle;
        }
    }

    // Sanity check color and zeta dimensions match
    if (r->color_binding && r->zeta_binding) {
        assert((r->color_binding->width == r->zeta_binding->width)
               && (r->color_binding->height == r->zeta_binding->height));
    }

    surface_evict_old(d);
}

// FIXME: Move to common
static void surface_get_dimensions(PGRAPHState *pg, unsigned int *width,
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

void pgraph_gl_init_surfaces(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    pgraph_gl_reload_surface_scale_factor(pg);
    glGenFramebuffers(1, &r->gl_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, r->gl_framebuffer);
    QTAILQ_INIT(&r->surfaces);
    r->downloads_pending = false;
    qemu_event_init(&r->downloads_complete, false);
    qemu_event_init(&r->dirty_surfaces_download_complete, false);

    init_render_to_texture(pg);
}

static void flush_surfaces(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    /* Clear last surface shape to force recreation of buffers at next draw */
    pg->surface_color.draw_dirty = false;
    pg->surface_zeta.draw_dirty = false;
    memset(&pg->last_surface_shape, 0, sizeof(pg->last_surface_shape));
    pgraph_gl_unbind_surface(d, true);
    pgraph_gl_unbind_surface(d, false);

    SurfaceBinding *s, *next;
    QTAILQ_FOREACH_SAFE(s, &r->surfaces, entry, next) {
        // FIXME: We should download all surfaces to ram, but need to
        //        investigate corruption issue
        // pgraph_gl_surface_download_if_dirty(d, s);
        pgraph_gl_surface_invalidate(d, s);
    }
}

void pgraph_gl_finalize_surfaces(PGRAPHState *pg)
{
    NV2AState *d = container_of(pg, NV2AState, pgraph);
    PGRAPHGLState *r = pg->gl_renderer_state;

    flush_surfaces(d);
    glDeleteFramebuffers(1, &r->gl_framebuffer);
    r->gl_framebuffer = 0;

    finalize_render_to_texture(pg);
}

void pgraph_gl_surface_flush(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    bool update_surface = (r->color_binding || r->zeta_binding);

    flush_surfaces(d);

    pgraph_gl_reload_surface_scale_factor(pg);

    if (update_surface) {
        pgraph_gl_surface_update(d, true, true, true);
    }
}
