/*
 * xemu User Interface
 *
 * Subsystem handling primary graphical user interface, which can be controlled
 * via mouse and keyboard or through any attached gamepad.
 *
 * Copyright (C) 2020-2021 Matt Borgerson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XEMU_HUD_H
#define XEMU_HUD_H

#include <SDL.h>
#include <epoxy/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Implemented in xemu.c
int xemu_is_fullscreen(void);
void xemu_monitor_init(void);
void xemu_toggle_fullscreen(void);
void xemu_eject_disc(Error **errp);
void xemu_load_disc(const char *path, Error **errp);

// Implemented in xemu_hud.cc
void xemu_hud_init(SDL_Window *window, void *sdl_gl_context);
void xemu_hud_cleanup(void);
void xemu_hud_render(void);
void xemu_hud_process_sdl_events(SDL_Event *event);
void xemu_hud_should_capture_kbd_mouse(int *kbd, int *mouse);
void xemu_hud_set_framebuffer_texture(GLuint tex, bool flip);

#ifdef __cplusplus
}
#endif

#endif
