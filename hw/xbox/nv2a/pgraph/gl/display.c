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

#include "qemu/osdep.h"
#include "hw/display/vga_int.h"
#include "hw/xbox/nv2a/nv2a_int.h"
#include "hw/xbox/nv2a/pgraph/util.h"
#include "renderer.h"

#include <math.h>

void pgraph_gl_init_display(NV2AState *d)
{
    struct PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    glo_set_current(g_nv2a_context_display);

    glGenTextures(1, &r->gl_display_buffer);
    r->gl_display_buffer_internal_format = 0;
    r->gl_display_buffer_width = 0;
    r->gl_display_buffer_height = 0;
    r->gl_display_buffer_format = 0;
    r->gl_display_buffer_type = 0;

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
        "uniform vec3 pvideo_color_key;\n"
        "uniform vec2 display_size;\n"
        "uniform float line_offset;\n"
        "layout(location = 0) out vec4 out_Color;\n"
        "void main()\n"
        "{\n"
        "    vec2 texCoord = gl_FragCoord.xy/display_size;\n"
        "    float rel = display_size.y/textureSize(tex, 0).y/line_offset;\n"
        "    texCoord.y = rel*(1.0f - texCoord.y);\n"
        "    out_Color.rgba = texture(tex, texCoord);\n"
        "    if (pvideo_enable) {\n"
        "        vec2 screenCoord = gl_FragCoord.xy - 0.5;\n"
        "        vec4 output_region = vec4(pvideo_pos.xy, pvideo_pos.xy + "
        "pvideo_pos.zw);\n"
        "        bvec4 clip = bvec4(lessThan(screenCoord, output_region.xy),\n"
        "                           greaterThan(screenCoord, output_region.zw));\n"
        "        if (!any(clip) && (!pvideo_color_key_enable || out_Color.rgb == pvideo_color_key)) {\n"
        "            vec2 out_xy = (screenCoord - pvideo_pos.xy) * pvideo_scale.z;\n"
        "            vec2 in_st = (pvideo_in_pos + out_xy * pvideo_scale.xy) / textureSize(pvideo_tex, 0);\n"
        "            in_st.y *= -1.0;\n"
        "            out_Color.rgba = texture(pvideo_tex, in_st);\n"
        "        }\n"
        "    }\n"
        "}\n";

    r->disp_rndr.prog = pgraph_gl_compile_shader(vs, fs);
    r->disp_rndr.tex_loc = glGetUniformLocation(r->disp_rndr.prog, "tex");
    r->disp_rndr.pvideo_enable_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_enable");
    r->disp_rndr.pvideo_tex_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_tex");
    r->disp_rndr.pvideo_in_pos_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_in_pos");
    r->disp_rndr.pvideo_pos_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_pos");
    r->disp_rndr.pvideo_scale_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_scale");
    r->disp_rndr.pvideo_color_key_enable_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_color_key_enable");
    r->disp_rndr.pvideo_color_key_loc = glGetUniformLocation(r->disp_rndr.prog, "pvideo_color_key");
    r->disp_rndr.display_size_loc = glGetUniformLocation(r->disp_rndr.prog, "display_size");
    r->disp_rndr.line_offset_loc = glGetUniformLocation(r->disp_rndr.prog, "line_offset");

    glGenVertexArrays(1, &r->disp_rndr.vao);
    glBindVertexArray(r->disp_rndr.vao);
    glGenBuffers(1, &r->disp_rndr.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, r->disp_rndr.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glGenFramebuffers(1, &r->disp_rndr.fbo);

    glGenTextures(1, &r->disp_rndr.vga_framebuffer_tex);
    glBindTexture(GL_TEXTURE_2D, r->disp_rndr.vga_framebuffer_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glGenTextures(1, &r->disp_rndr.pvideo_tex);
    glBindTexture(GL_TEXTURE_2D, r->disp_rndr.pvideo_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    assert(glGetError() == GL_NO_ERROR);

    glo_set_current(g_nv2a_context_render);
}

void pgraph_gl_finalize_display(PGRAPHState *pg)
{
    PGRAPHGLState *r = pg->gl_renderer_state;

    glo_set_current(g_nv2a_context_display);

    glDeleteTextures(1, &r->gl_display_buffer);
    r->gl_display_buffer = 0;

    glDeleteProgram(r->disp_rndr.prog);
    r->disp_rndr.prog = 0;

    glDeleteVertexArrays(1, &r->disp_rndr.vao);
    r->disp_rndr.vao = 0;

    glDeleteBuffers(1, &r->disp_rndr.vbo);
    r->disp_rndr.vbo = 0;

    glDeleteFramebuffers(1, &r->disp_rndr.fbo);
    r->disp_rndr.fbo = 0;

    glDeleteTextures(1, &r->disp_rndr.pvideo_tex);
    r->disp_rndr.pvideo_tex = 0;

    glo_set_current(g_nv2a_context_render);
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

static float pvideo_calculate_scale(unsigned int din_dout,
                                           unsigned int output_size)
{
    float calculated_in = din_dout * (output_size - 1);
    calculated_in = floorf(calculated_in / (1 << 20) + 0.5f);
    return (calculated_in + 1.0f) / output_size;
}

static void render_display_pvideo_overlay(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

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
    glUniform1ui(r->disp_rndr.pvideo_enable_loc, enabled);
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
    glUniform1ui(r->disp_rndr.pvideo_color_key_enable_loc,
                 color_key_enabled);

    unsigned int color_key = d->pvideo.regs[NV_PVIDEO_COLOR_KEY] & 0xFFFFFF;
    glUniform3f(r->disp_rndr.pvideo_color_key_loc,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_RED) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_GREEN) / 255.0,
                GET_MASK(color_key, NV_PVIDEO_COLOR_KEY_BLUE) / 255.0);

    assert(offset + in_pitch * in_height <= limit);
    hwaddr end = base + offset + in_pitch * in_height;
    assert(end <= memory_region_size(d->vram));

    pgraph_apply_scaling_factor(pg, &out_x, &out_y);
    pgraph_apply_scaling_factor(pg, &out_width, &out_height);

    // Translate for the GL viewport origin.
    out_y = MAX(r->gl_display_buffer_height - 1 - (int)(out_y + out_height), 0);

    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_2D, r->disp_rndr.pvideo_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    uint8_t *tex_rgba = convert_texture_data__CR8YB8CB8YA8(
        d->vram_ptr + base + offset, in_width, in_height, in_pitch);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, in_width, in_height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, tex_rgba);
    g_free(tex_rgba);
    glUniform1i(r->disp_rndr.pvideo_tex_loc, 1);
    glUniform2f(r->disp_rndr.pvideo_in_pos_loc, in_s / 16.f, in_t / 8.f);
    glUniform4f(r->disp_rndr.pvideo_pos_loc,
                out_x, out_y, out_width, out_height);
    glUniform3f(r->disp_rndr.pvideo_scale_loc,
                scale_x, scale_y, 1.0f / pg->surface_scale_factor);
}

static bool check_framebuffer_dirty(NV2AState *d, hwaddr addr, hwaddr size)
{
    hwaddr end = TARGET_PAGE_ALIGN(addr + size);
    addr &= TARGET_PAGE_MASK;
    assert(end < memory_region_size(d->vram));
    return memory_region_test_and_clear_dirty(d->vram, addr, end - addr,
                                              DIRTY_MEMORY_VGA);
}

static inline void get_vga_buffer_format(NV2AState *d,
                                         const SurfaceFormatInfo **format,
                                         int *framebuffer_bytes_per_pixel)
{
    int framebuffer_bpp = d->vga.get_bpp(&d->vga);
    switch (framebuffer_bpp) {
    case 15:
        *format = &kelvin_surface_color_format_gl_map
                      [NV097_SET_SURFACE_FORMAT_COLOR_LE_X1R5G5B5_Z1R5G5B5];
        *framebuffer_bytes_per_pixel = 2;
        break;
    case 16:
        *format = &kelvin_surface_color_format_gl_map
                      [NV097_SET_SURFACE_FORMAT_COLOR_LE_R5G6B5];
        *framebuffer_bytes_per_pixel = 2;
        break;
    case 0:
        /* See note in nv2a_get_bpp. For the purposes of selecting a surface,
         * this is treated as 32bpp. */
    case 32:
        *format = &kelvin_surface_color_format_gl_map
                      [NV097_SET_SURFACE_FORMAT_COLOR_LE_X8R8G8B8_Z8R8G8B8];
        *framebuffer_bytes_per_pixel = 4;
        break;
    default:
        fprintf(stderr, "Unexpected framebuffer_bpp %d\n", framebuffer_bpp);
        assert(!"Unexpected framebuffer_bpp value");
    }
}

static void render_display(NV2AState *d, SurfaceBinding *surface)
{
    struct PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    int vga_width, vga_height;
    VGADisplayParams vga_display_params;
    d->vga.get_resolution(&d->vga, &vga_width, &vga_height);
    d->vga.get_params(&d->vga, &vga_display_params);

    /* Adjust viewport height for interlaced mode, used only in 1080i */
    if (d->vga.cr[NV_PRMCIO_INTERLACE_MODE] != NV_PRMCIO_INTERLACE_MODE_DISABLED) {
        vga_height *= 2;
    }

    unsigned int width = vga_width;
    unsigned int height = vga_height;
    pgraph_apply_scaling_factor(pg, &width, &height);

    int line_offset = 1;
    const SurfaceFormatInfo *format;
    int framebuffer_bytes_per_pixel;
    get_vga_buffer_format(d, &format, &framebuffer_bytes_per_pixel);

    if (surface && surface->color && surface->width == width &&
        surface->height == height) {
        line_offset = vga_display_params.line_offset ?
                          surface->pitch / vga_display_params.line_offset :
                          1;
        format = &surface->fmt;
    } else {
        if (vga_width * framebuffer_bytes_per_pixel >
            vga_display_params.line_offset) {
            /* Some games without widescreen support (e.g., Pirates: The Legend
             * of Black Kat) will set a VGA resolution that is wider than a
             * single line when run with widescreen enabled in the dashboard.
             */
            vga_width =
                vga_display_params.line_offset / framebuffer_bytes_per_pixel;
            width = vga_width;
            height = vga_height;
            pgraph_apply_scaling_factor(pg, &width, &height);
        }
        hwaddr framebuffer = d->pcrtc.start;
        size_t length = vga_display_params.line_offset * vga_height;
        pgraph_gl_download_surfaces_in_range_if_dirty(pg, framebuffer, length);

        bool dirty = check_framebuffer_dirty(d, framebuffer, length);
        if (dirty) {
            nv2a_profile_inc_counter(NV2A_PROF_SURF_UPLOAD);
            glBindTexture(GL_TEXTURE_2D, r->disp_rndr.vga_framebuffer_tex);
            pgraph_gl_upload_vram_to_bound_texture(
                d, framebuffer, false, vga_width, vga_height,
                vga_display_params.line_offset,
                vga_display_params.line_offset * vga_height, format);
            assert(glGetError() == GL_NO_ERROR);
        }
        surface = NULL;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, r->disp_rndr.fbo);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, r->gl_display_buffer);

    bool recreate =
        width != r->gl_display_buffer_width ||
        height != r->gl_display_buffer_height ||
        format->gl_internal_format != r->gl_display_buffer_internal_format ||
        format->gl_format != r->gl_display_buffer_format ||
        format->gl_type != r->gl_display_buffer_type;

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
        r->gl_display_buffer_width = width;
        r->gl_display_buffer_height = height;
        r->gl_display_buffer_internal_format = format->gl_internal_format;
        r->gl_display_buffer_format = format->gl_format;
        r->gl_display_buffer_type = format->gl_type;
        glTexImage2D(GL_TEXTURE_2D, 0,
            r->gl_display_buffer_internal_format,
            r->gl_display_buffer_width,
            r->gl_display_buffer_height,
            0,
            r->gl_display_buffer_format,
            r->gl_display_buffer_type,
            NULL);
    }

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D, r->gl_display_buffer, 0);
    GLenum DrawBuffers[1] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, DrawBuffers);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    glBindTexture(GL_TEXTURE_2D, surface ? surface->gl_buffer :
                                           r->disp_rndr.vga_framebuffer_tex);
    glBindVertexArray(r->disp_rndr.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r->disp_rndr.vbo);
    glUseProgram(r->disp_rndr.prog);
    glProgramUniform1i(r->disp_rndr.prog, r->disp_rndr.tex_loc, 0);
    glUniform2f(r->disp_rndr.display_size_loc, width, height);
    glUniform1f(r->disp_rndr.line_offset_loc, line_offset);
    render_display_pvideo_overlay(d);

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

static void gl_fence(void)
{
    GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    int result = glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT,
                                         (GLuint64)(5000000000));
    assert(result == GL_CONDITION_SATISFIED || result == GL_ALREADY_SIGNALED);
    glDeleteSync(fence);
}

void pgraph_gl_sync(NV2AState *d)
{
    VGADisplayParams vga_display_params;
    d->vga.get_params(&d->vga, &vga_display_params);

    hwaddr framebuffer = d->pcrtc.start + vga_display_params.line_offset;
    if (!framebuffer) {
        qemu_event_set(&d->pgraph.sync_complete);
        return;
    }
    SurfaceBinding *surface = pgraph_gl_surface_get_within(d, framebuffer);
    if (surface && surface->color  && surface->width && surface->height) {
        /* FIXME: Sanity check surface dimensions */

        /* Wait for queued commands to complete */
        pgraph_gl_upload_surface_data(d, surface, !tcg_enabled());
    }

    gl_fence();
    assert(glGetError() == GL_NO_ERROR);

    /* Render framebuffer in display context */
    glo_set_current(g_nv2a_context_display);
    render_display(d, surface);
    gl_fence();
    assert(glGetError() == GL_NO_ERROR);

    /* Switch back to original context */
    glo_set_current(g_nv2a_context_render);

    qatomic_set(&d->pgraph.sync_pending, false);
    qemu_event_set(&d->pgraph.sync_complete);
}

int pgraph_gl_get_framebuffer_surface(NV2AState *d)
{
    PGRAPHState *pg = &d->pgraph;
    PGRAPHGLState *r = pg->gl_renderer_state;

    qemu_mutex_lock(&d->pfifo.lock);
    // FIXME: Possible race condition with pgraph, consider lock

    VGADisplayParams vga_display_params;
    d->vga.get_params(&d->vga, &vga_display_params);

    const hwaddr framebuffer = d->pcrtc.start + vga_display_params.line_offset;
    if (!framebuffer) {
        qemu_mutex_unlock(&d->pfifo.lock);
        return 0;
    }

    SurfaceBinding *surface = pgraph_gl_surface_get_within(d, framebuffer);
    if (surface && surface->color) {
        assert(surface->fmt.gl_attachment == GL_COLOR_ATTACHMENT0);
        assert(surface->fmt.gl_format == GL_RGBA ||
               surface->fmt.gl_format == GL_RGB ||
               surface->fmt.gl_format == GL_BGR ||
               surface->fmt.gl_format == GL_BGRA);
        surface->frame_time = pg->frame_time;
    }

    qemu_event_reset(&d->pgraph.sync_complete);
    qatomic_set(&pg->sync_pending, true);
    pfifo_kick(d);
    qemu_mutex_unlock(&d->pfifo.lock);
    qemu_event_wait(&d->pgraph.sync_complete);

    return r->gl_display_buffer;
}
