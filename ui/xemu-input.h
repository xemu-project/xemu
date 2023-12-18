/*
 * xemu Input Management
 *
 * This is the main input abstraction layer for xemu, which is basically just a
 * wrapper around SDL2 GameController/Keyboard API to map specifically to an
 * Xbox gamepad and support automatic binding, hotplugging, and removal at
 * runtime.
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

#ifndef XEMU_INPUT_H
#define XEMU_INPUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "qemu/queue.h"

enum controller_state_buttons_mask {
    CONTROLLER_BUTTON_A          = (1 << 0),
    CONTROLLER_BUTTON_B          = (1 << 1),
    CONTROLLER_BUTTON_X          = (1 << 2),
    CONTROLLER_BUTTON_Y          = (1 << 3),
    CONTROLLER_BUTTON_DPAD_LEFT  = (1 << 4),
    CONTROLLER_BUTTON_DPAD_UP    = (1 << 5),
    CONTROLLER_BUTTON_DPAD_RIGHT = (1 << 6),
    CONTROLLER_BUTTON_DPAD_DOWN  = (1 << 7),
    CONTROLLER_BUTTON_BACK       = (1 << 8),
    CONTROLLER_BUTTON_START      = (1 << 9),
    CONTROLLER_BUTTON_WHITE      = (1 << 10),
    CONTROLLER_BUTTON_BLACK      = (1 << 11),
    CONTROLLER_BUTTON_LSTICK     = (1 << 12),
    CONTROLLER_BUTTON_RSTICK     = (1 << 13),
    CONTROLLER_BUTTON_GUIDE      = (1 << 14), // Extension
};

#define CONTROLLER_STATE_BUTTON_ID_TO_MASK(x) (1<<x)

enum controller_state_axis_index {
    CONTROLLER_AXIS_LTRIG,
    CONTROLLER_AXIS_RTRIG,
    CONTROLLER_AXIS_LSTICK_X,
    CONTROLLER_AXIS_LSTICK_Y,
    CONTROLLER_AXIS_RSTICK_X,
    CONTROLLER_AXIS_RSTICK_Y,
    CONTROLLER_AXIS__COUNT,
};

enum controller_input_device_type {
    INPUT_DEVICE_SDL_KEYBOARD,
    INPUT_DEVICE_SDL_GAMECONTROLLER,
};

enum peripheral_type { PERIPHERAL_NONE, PERIPHERAL_XMU, PERIPHERAL_TYPE_COUNT };

typedef struct XmuState {
    const char *filename;
    void *dev;
} XmuState;

typedef struct ControllerState {
    QTAILQ_ENTRY(ControllerState) entry;

    int64_t last_input_updated_ts;
    int64_t last_rumble_updated_ts;

    // Input state
    uint16_t buttons;
    int16_t  axis[CONTROLLER_AXIS__COUNT];

    // Rendering state hacked on here for convenience but needs to be moved (FIXME)
    uint32_t animate_guide_button_end;
    uint32_t animate_trigger_end;

    // Rumble state
    bool rumble_enabled;
    uint16_t rumble_l, rumble_r;

    enum controller_input_device_type type;
    const char         *name;
    SDL_GameController *sdl_gamecontroller; // if type == INPUT_DEVICE_SDL_GAMECONTROLLER
    SDL_Joystick       *sdl_joystick;
    SDL_JoystickID      sdl_joystick_id;
    SDL_JoystickGUID    sdl_joystick_guid;

    enum peripheral_type peripheral_types[2];
    void *peripherals[2];

    int   bound;  // Which port this input device is bound to
    void *device; // DeviceState opaque
} ControllerState;

typedef QTAILQ_HEAD(, ControllerState) ControllerStateList;
extern ControllerStateList available_controllers;
extern ControllerState *bound_controllers[4];

#ifdef __cplusplus
extern "C" {
#endif

void xemu_input_init(void);
void xemu_input_process_sdl_events(const SDL_Event *event); // SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED
void xemu_input_update_controllers(void);
void xemu_input_update_controller(ControllerState *state);
void xemu_input_update_sdl_kbd_controller_state(ControllerState *state);
void xemu_input_update_sdl_controller_state(ControllerState *state);
void xemu_input_update_rumble(ControllerState *state);
ControllerState *xemu_input_get_bound(int index);
void xemu_input_bind(int index, ControllerState *state, int save);
bool xemu_input_bind_xmu(int player_index, int peripheral_port_index,
                         const char *filename, bool is_rebind);
void xemu_input_rebind_xmu(int port);
void xemu_input_unbind_xmu(int player_index, int peripheral_port_index);
int xemu_input_get_controller_default_bind_port(ControllerState *state, int start);
void xemu_save_peripheral_settings(int player_index, int peripheral_index,
                                   int peripheral_type,
                                   const char *peripheral_parameter);

void xemu_input_set_test_mode(int enabled);
int xemu_input_get_test_mode(void);

#ifdef __cplusplus
}
#endif

#endif
