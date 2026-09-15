/*
 * xemu host presentation interface
 *
 * Copyright (C) 2026 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "ui/xemu-present.h"

#ifdef _WIN32
#include "xui/win32-dxgi-present.h"
#endif

void xemu_present_init(SDL_Window *window)
{
#ifdef _WIN32
    win32_dxgi_present_init(window);
#else
    (void)window;
#endif
}

void xemu_present_cleanup(void)
{
#ifdef _WIN32
    win32_dxgi_present_cleanup();
#endif
}

void xemu_present_begin_frame(void)
{
#ifdef _WIN32
    if (win32_dxgi_present_is_active()) {
        win32_dxgi_present_begin_frame();
    }
#endif
}

void xemu_present_end_frame(SDL_Window *window, bool vsync)
{
#ifdef _WIN32
    if (win32_dxgi_present_is_active()) {
        win32_dxgi_present_end_frame(vsync);
        return;
    }

    static bool warned;
    if (!warned) {
        fprintf(stderr,
                "win32_dxgi present failed or unavailable, falling back to "
                "SDL_GL_SwapWindow\n");
        warned = true;
    }
#else
    (void)vsync;
#endif
    SDL_GL_SwapWindow(window);
}

void xemu_present_resize(SDL_Window *window)
{
#ifdef _WIN32
    if (win32_dxgi_present_is_active()) {
        int width;
        int height;
        if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
            fprintf(stderr, "SDL_GetWindowSizeInPixels failed responding to "
                            "resize event.\n");
        } else {
            xemu_present_resize_pixels(width, height);
        }
    }
#else
    (void)window;
#endif
}

void xemu_present_resize_pixels(int width, int height)
{
#ifdef _WIN32
    if (win32_dxgi_present_is_active()) {
        win32_dxgi_present_resize(width, height);
    }
#else
    (void)width;
    (void)height;
#endif
}
