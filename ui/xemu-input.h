/*
 * xemu Input Management
 *
 * This is the main input abstraction layer for xemu, which is basically just a
 * wrapper around SDL3 Gamepad/Keyboard API to map specifically to an
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

#include <SDL3/SDL.h>
#include <stdbool.h>

#include "qemu/queue.h"
#include "xemu-settings.h"
#include <SDL3/SDL.h>

#define DRIVER_DUKE "usb-xbox-gamepad"
#define DRIVER_S "usb-xbox-gamepad-s"
#define DRIVER_LIGHTGUN "usb-xbox-lightgun"

#define DRIVER_DUKE_DISPLAY_NAME "Xbox Controller"
#define DRIVER_S_DISPLAY_NAME "Xbox Controller S"
#define DRIVER_LIGHTGUN_DISPLAY_NAME "Lightgun (EMS TopGun II)"

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
    // Extension: lightgun pointing inside the game display area. Reported
    // by the usb-xbox-lightgun driver as XID_LIGHTGUN_ONSCREEN (0x2000).
    CONTROLLER_BUTTON_LIGHTGUN_ONSCREEN = (1 << 15),
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
    INPUT_DEVICE_SDL_GAMEPAD,
    INPUT_DEVICE_RAWINPUT_MOUSE, // HID mouse/lightgun (Raw Input / evdev)
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
    uint16_t rumble_l, rumble_r;

    enum controller_input_device_type type;
    const char         *name;
    SDL_Gamepad        *sdl_gamepad; // if type == INPUT_DEVICE_SDL_GAMEPAD
    SDL_Joystick       *sdl_joystick;
    SDL_JoystickID      sdl_joystick_id;
    SDL_GUID            sdl_joystick_guid;

    // if type == INPUT_DEVICE_RAWINPUT_MOUSE
    void     *rawinput_handle;      // Backend handle: Raw Input HANDLE on
                                    // Windows, EvdevMouse* on Linux
    char     *rawinput_path;        // Device path (owned): interface path on
                                    // Windows, /dev/input/eventN on Linux
    char      rawinput_guid[16];    // Stable pseudo-GUID for settings ("mouse:xxxxxxxx")
    uint32_t  rawinput_buttons;     // XEMU_RAWINPUT_BUTTON_* bits
    int32_t   rawinput_client_x;    // Last aim position, window client pixels
    int32_t   rawinput_client_y;
    bool      rawinput_has_abs;     // Device reports absolute positions (e.g. Sinden)
    float     rawinput_smooth_nx;   // Filtered aim, normalized [-1, 1]
    float     rawinput_smooth_ny;
    float     rawinput_smooth_dx;   // Filtered aim velocity (units/s)
    float     rawinput_smooth_dy;
    uint64_t  rawinput_smooth_ts;   // Timestamp (ns) of last filter step
    bool      rawinput_smooth_valid;

    enum peripheral_type peripheral_types[2];
    void *peripherals[2];

    GamepadMappings *controller_map;

    int   bound;  // Which port this input device is bound to
    void *device; // DeviceState opaque
} ControllerState;

typedef QTAILQ_HEAD(, ControllerState) ControllerStateList;
extern ControllerStateList available_controllers;
extern ControllerState *bound_controllers[4];
extern const char *bound_drivers[4];

#ifdef __cplusplus
extern "C" {
#endif

extern int *g_keyboard_scancode_map[25];

void xemu_input_init(void);
void xemu_input_process_sdl_events(const SDL_Event *event); // SDL_EVENT_GAMEPAD_ADDED, SDL_EVENT_GAMEPAD_REMOVED
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
void xemu_input_reset_input_mapping(ControllerState *state);

// Game display rectangle in window pixels, updated each frame by the
// renderer. Used to map lightgun/mouse positions to aim coordinates.
void xemu_input_set_game_display_rect(int x, int y, int w, int h);
void xemu_input_get_game_display_rect(int *x, int *y, int *w, int *h);

#ifdef __cplusplus
}
#endif

#endif
