#ifndef SDL3_H
#define SDL3_H

/* Avoid compiler warning because macro is redefined in SDL_syswm.h. */
#undef WIN32_LEAN_AND_MEAN

#include <SDL3/SDL.h>

#include "ui/kbd-state.h"

// FIXME: Cleanup
struct sdl3_console {
    DisplayChangeListener dcl;
    DisplaySurface *surface;
    DisplayOptions *opts;
    SDL_Texture *texture;
    SDL_Window *real_window;
    SDL_Renderer *real_renderer;
    int idx;
    int last_vm_running; /* per console for caption reasons */
    int x, y, w, h;
    int hidden;
    int opengl;
    int updates;
    int idle_counter;
    int ignore_hotkeys;
    SDL_GLContext winctx;
    QKbdState *kbd;
    bool y0_top;
    bool scanout_mode;
};

void sdl3_window_create(struct sdl3_console *scon);
void sdl3_window_destroy(struct sdl3_console *scon);
void sdl3_window_resize(struct sdl3_console *scon);
void sdl3_poll_events(struct sdl3_console *scon);

void sdl3_process_key(struct sdl3_console *scon,
                      SDL_KeyboardEvent *ev);

void sdl3_2d_update(DisplayChangeListener *dcl,
                    int x, int y, int w, int h);
void sdl3_2d_switch(DisplayChangeListener *dcl,
                    DisplaySurface *new_surface);
void sdl3_2d_refresh(DisplayChangeListener *dcl);
void sdl3_2d_redraw(struct sdl3_console *scon);
bool sdl3_2d_check_format(DisplayChangeListener *dcl,
                          pixman_format_code_t format);

void sdl3_gl_update(DisplayChangeListener *dcl,
                    int x, int y, int w, int h);
void sdl3_gl_switch(DisplayChangeListener *dcl,
                    DisplaySurface *new_surface);
void sdl3_gl_refresh(DisplayChangeListener *dcl);
void sdl3_gl_redraw(struct sdl3_console *scon);

QEMUGLContext sdl3_gl_create_context(DisplayChangeListener *dcl,
                                     QEMUGLParams *params);
void sdl3_gl_destroy_context(DisplayChangeListener *dcl, QEMUGLContext ctx);
int sdl3_gl_make_context_current(DisplayChangeListener *dcl,
                                 QEMUGLContext ctx);
QEMUGLContext sdl3_gl_get_current_context(DisplayChangeListener *dcl);

void sdl3_gl_scanout_disable(DisplayChangeListener *dcl);
void sdl3_gl_scanout_texture(DisplayChangeListener *dcl,
                             uint32_t backing_id,
                             bool backing_y_0_top,
                             uint32_t backing_width,
                             uint32_t backing_height,
                             uint32_t x, uint32_t y,
                             uint32_t w, uint32_t h);
void sdl3_gl_scanout_flush(DisplayChangeListener *dcl,
                           uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#endif /* SDL3_H */