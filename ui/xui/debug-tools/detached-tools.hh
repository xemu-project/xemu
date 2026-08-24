//
// xemu Detached Debug Tool Windows
//
// Copyright (C) 2026 xemu contributors
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
#pragma once

#include <SDL3/SDL.h>

void detached_tools_init(SDL_Window *main_window, void *main_gl_context);
void detached_tools_cleanup();
bool detached_tools_process_sdl_event(SDL_Event *event);
bool detached_tools_owns_window_id(SDL_WindowID window_id);
void detached_tools_build_frames();
void detached_tools_render_frames();
