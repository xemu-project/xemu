/*
 * QEMU SDL display driver -- opengl support
 *
 * Copyright (c) 2014 Red Hat
 *
 * Authors:
 *     Gerd Hoffmann <kraxel@redhat.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "ui/console.h"
#include "ui/input.h"
#include "ui/sdl3.h"

static void sdl3_set_scanout_mode(struct sdl3_console *scon, bool scanout)
{
    if (scon->scanout_mode == scanout) {
        return;
    }

    scon->scanout_mode = scanout;
    if (!scon->scanout_mode) {
        egl_fb_destroy(&scon->guest_fb);
        if (scon->surface) {
            surface_gl_destroy_texture(scon->gls, scon->surface);
            surface_gl_create_texture(scon->gls, scon->surface);
        }
    }
}

static void sdl3_gl_render_surface(struct sdl3_console *scon)
{
    int ww, wh;

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);
    sdl3_set_scanout_mode(scon, false);

    SDL_GetWindowSize(scon->real_window, &ww, &wh);
    surface_gl_setup_viewport(scon->gls, scon->surface, ww, wh);

    surface_gl_render_texture(scon->gls, scon->surface);
    SDL_GL_SwapWindow(scon->real_window);
}

void sdl3_gl_update(DisplayChangeListener *dcl,
                    int x, int y, int w, int h)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);

    assert(scon->opengl);

    if (!scon->real_window) {
        return;
    }

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);
    surface_gl_update_texture(scon->gls, scon->surface, x, y, w, h);
    scon->updates++;
}

void sdl3_gl_switch(DisplayChangeListener *dcl,
                    DisplaySurface *new_surface)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);
    DisplaySurface *old_surface = scon->surface;

    assert(scon->opengl);

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);
    surface_gl_destroy_texture(scon->gls, scon->surface);

    scon->surface = new_surface;

    if (surface_is_placeholder(new_surface) && qemu_console_get_index(dcl->con)) {
        qemu_gl_fini_shader(scon->gls);
        scon->gls = NULL;
        sdl3_window_destroy(scon);
        return;
    }

    if (!scon->real_window) {
        sdl3_window_create(scon);
        scon->gls = qemu_gl_init_shader();
    } else if (old_surface &&
               ((surface_width(old_surface)  != surface_width(new_surface)) ||
                (surface_height(old_surface) != surface_height(new_surface)))) {
        sdl3_window_resize(scon);
    }

    surface_gl_create_texture(scon->gls, scon->surface);
}

void sdl3_gl_refresh(DisplayChangeListener *dcl)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);

    assert(scon->opengl);

    graphic_hw_update(dcl->con);
    if (scon->updates && scon->real_window) {
        scon->updates = 0;
        sdl3_gl_render_surface(scon);
    }
    sdl3_poll_events(scon);
}

void sdl3_gl_redraw(struct sdl3_console *scon)
{
    assert(scon->opengl);

    if (scon->scanout_mode) {
        /* sdl3_gl_scanout_flush actually only care about
         * the first argument. */
        return sdl3_gl_scanout_flush(&scon->dcl, 0, 0, 0, 0);
    }
    if (scon->surface) {
        sdl3_gl_render_surface(scon);
    }
}

QEMUGLContext sdl3_gl_create_context(DisplayGLCtx *dgc,
                                     QEMUGLParams *params)
{
    struct sdl3_console *scon = container_of(dgc, struct sdl3_console, dgc);
    SDL_GLContext ctx;

    assert(scon->opengl);

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);

    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
    if (scon->opts->gl == DISPLAY_GL_MODE_ON ||
        scon->opts->gl == DISPLAY_GL_MODE_CORE) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_CORE);
    } else if (scon->opts->gl == DISPLAY_GL_MODE_ES) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_ES);
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, params->major_ver);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, params->minor_ver);

    ctx = SDL_GL_CreateContext(scon->real_window);

    /* If SDL fail to create a GL context and we use the "on" flag,
     * then try to fallback to GLES.
     */
    if (!ctx && scon->opts->gl == DISPLAY_GL_MODE_ON) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            SDL_GL_CONTEXT_PROFILE_ES);
        ctx = SDL_GL_CreateContext(scon->real_window);
    }
    return (QEMUGLContext)ctx;
}

void sdl3_gl_destroy_context(DisplayGLCtx *dgc, QEMUGLContext ctx)
{
    SDL_GLContext sdlctx = (SDL_GLContext)ctx;

    SDL_GL_DestroyContext(sdlctx);
}

int sdl3_gl_make_context_current(DisplayGLCtx *dgc,
                                 QEMUGLContext ctx)
{
    struct sdl3_console *scon = container_of(dgc, struct sdl3_console, dgc);
    SDL_GLContext sdlctx = (SDL_GLContext)ctx;

    assert(scon->opengl);

    return SDL_GL_MakeCurrent(scon->real_window, sdlctx);
}

void sdl3_gl_scanout_disable(DisplayChangeListener *dcl)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);

    assert(scon->opengl);
    scon->w = 0;
    scon->h = 0;
    sdl3_set_scanout_mode(scon, false);
}

void sdl3_gl_scanout_texture(DisplayChangeListener *dcl,
                             uint32_t backing_id,
                             bool backing_y_0_top,
                             uint32_t backing_width,
                             uint32_t backing_height,
                             uint32_t x, uint32_t y,
                             uint32_t w, uint32_t h,
                             void *d3d_tex2d)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);

    assert(scon->opengl);
    scon->x = x;
    scon->y = y;
    scon->w = w;
    scon->h = h;
    scon->y0_top = backing_y_0_top;

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);

    sdl3_set_scanout_mode(scon, true);
    egl_fb_setup_for_tex(&scon->guest_fb, backing_width, backing_height,
                         backing_id, false);
}

void sdl3_gl_scanout_flush(DisplayChangeListener *dcl,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    struct sdl3_console *scon = container_of(dcl, struct sdl3_console, dcl);
    int ww, wh;

    assert(scon->opengl);
    if (!scon->scanout_mode) {
        return;
    }
    if (!scon->guest_fb.framebuffer) {
        return;
    }

    SDL_GL_MakeCurrent(scon->real_window, scon->winctx);

    SDL_GetWindowSize(scon->real_window, &ww, &wh);
    egl_fb_setup_default(&scon->win_fb, ww, wh);
    egl_fb_blit(&scon->win_fb, &scon->guest_fb, !scon->y0_top);

    SDL_GL_SwapWindow(scon->real_window);
}
