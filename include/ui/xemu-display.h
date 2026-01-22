#ifndef XEMU_DISPLAY_H
#define XEMU_DISPLAY_H

/* Avoid compiler warning because macro is redefined in SDL_syswm.h. */
#undef WIN32_LEAN_AND_MEAN

#include <SDL3/SDL.h>

#include "ui/kbd-state.h"

struct xemu_console {
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

#endif /* XEMU_DISPLAY_H */
