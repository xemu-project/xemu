/*
 * xemu per-device mouse/lightgun support
 *
 * Enumerates HID mice individually (Raw Input API on Windows, evdev on
 * Linux — see xemu-rawinput.c and xemu-evdev.c) so that multiple pointer
 * devices (e.g. Sinden Lightguns, which appear as absolute mice) can be
 * told apart and bound to different controller ports. Each detected mouse
 * is exposed as a ControllerState of type INPUT_DEVICE_RAWINPUT_MOUSE in
 * the available controllers list.
 *
 * Copyright (C) 2026 xemu contributors
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

#ifndef XEMU_RAWINPUT_H
#define XEMU_RAWINPUT_H

#include "xemu-input.h"

// Button bits stored in ControllerState.rawinput_buttons
#define XEMU_RAWINPUT_BUTTON_LEFT   (1 << 0)
#define XEMU_RAWINPUT_BUTTON_RIGHT  (1 << 1)
#define XEMU_RAWINPUT_BUTTON_MIDDLE (1 << 2)
#define XEMU_RAWINPUT_BUTTON_X1     (1 << 3)
#define XEMU_RAWINPUT_BUTTON_X2     (1 << 4)

#ifdef __cplusplus
extern "C" {
#endif

// Enumerate HID mice and start receiving per-device input. Must be called
// after the main SDL window has been created, with the QEMU main loop lock
// held. No-op on hosts without a backend (e.g. macOS).
void xemu_rawinput_init(SDL_Window *window);

// Process queued device arrivals/removals. Hotplug notifications come in
// while SDL pumps the Windows message loop, outside the QEMU main loop
// lock, so they are deferred to this function which must be called with
// the lock held.
void xemu_rawinput_process_pending(void);

// Recompute buttons/axes of a ControllerState of type
// INPUT_DEVICE_RAWINPUT_MOUSE from the latest Raw Input data.
void xemu_rawinput_update_controller_state(ControllerState *state);

#ifdef __cplusplus
}
#endif

#endif
