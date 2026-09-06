//
// xemu User Interface
//
// Copyright (C) 2026 Matt Borgerson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef XEMU_WIN32_DXGI_PRESENT_H
#define XEMU_WIN32_DXGI_PRESENT_H

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize DXGI SwapChain and WGL_NV_DX_interop on Windows.
 * Returns true if DXGI presentation is active and ready to use, false
 * otherwise.
 */
bool win32_dxgi_present_init(SDL_Window *window);

/**
 * Clean up all DXGI, D3D11, and WGL interop resources.
 */
void win32_dxgi_present_cleanup(void);

/**
 * Returns true if DXGI presentation is currently active.
 */
bool win32_dxgi_present_is_active(void);

/**
 * Call before rendering the UI frame. Binds the DXGI interop FBO.
 */
void win32_dxgi_present_begin_frame(void);

/**
 * Call after rendering the UI frame. Unbinds FBO, copies to swapchain, and
 * presents.
 */
void win32_dxgi_present_end_frame(bool vsync);

/**
 * Notify the presentation helper that the window size has changed.
 */
void win32_dxgi_present_resize(int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* XEMU_WIN32_DXGI_PRESENT_H */
