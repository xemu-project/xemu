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

#ifndef XEMU_PRESENT_H
#define XEMU_PRESENT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

void xemu_present_init(SDL_Window *window);
void xemu_present_cleanup(void);
void xemu_present_begin_frame(void);
void xemu_present_end_frame(SDL_Window *window, bool vsync);
void xemu_present_resize(SDL_Window *window);
void xemu_present_resize_pixels(int width, int height);

#endif /* XEMU_PRESENT_H */
