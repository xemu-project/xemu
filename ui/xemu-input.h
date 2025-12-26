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
#include "xemu-settings.h"
#include <SDL2/SDL.h>

#define DRIVER_DUKE "usb-xbox-gamepad"
#define DRIVER_S "usb-xbox-gamepad-s"
#define DRIVER_STEEL_BATTALION "usb-steel-battalion"

#define DRIVER_DUKE_DISPLAY_NAME "Xbox Controller"
#define DRIVER_S_DISPLAY_NAME "Xbox Controller S"
#define DRIVER_STEEL_BATTALION_DISPLAY_NAME "Steel Battalion Controller"

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

enum steel_battalion_controller_state_buttons_mask {
    SBC_BUTTON_MAIN_WEAPON = 0x01,
    SBC_BUTTON_SUB_WEAPON = 0x02,
    SBC_BUTTON_LOCK_ON = 0x04,
    SBC_BUTTON_EJECT = 0x08,
    SBC_BUTTON_COCKPIT_HATCH = 0x10,
    SBC_BUTTON_IGNITION = 0x20,
    SBC_BUTTON_START = 0x40,
    SBC_BUTTON_OPEN_CLOSE = 0x80,
    SBC_BUTTON_MAP_ZOOM_IN_OUT = 0x100,
    SBC_BUTTON_MODE_SELECT = 0x200,
    SBC_BUTTON_SUB_MONITOR_MODE_SELECT = 0x400,
    SBC_BUTTON_ZOOM_IN = 0x800,
    SBC_BUTTON_ZOOM_OUT = 0x1000,
    SBC_BUTTON_FSS = 0x2000,
    SBC_BUTTON_MANIPULATOR = 0x4000,
    SBC_BUTTON_LINE_COLOR_CHANGE = 0x8000,
    SBC_BUTTON_WASHING = 0x10000,
    SBC_BUTTON_EXTINGUISHER = 0x20000,
    SBC_BUTTON_CHAFF = 0x40000,
    SBC_BUTTON_TANK_DETACH = 0x80000,
    SBC_BUTTON_OVERRIDE = 0x100000,
    SBC_BUTTON_NIGHT_SCOPE = 0x200000,
    SBC_BUTTON_FUNC1 = 0x400000,
    SBC_BUTTON_FUNC2 = 0x800000,
    SBC_BUTTON_FUNC3 = 0x1000000,
    SBC_BUTTON_MAIN_WEAPON_CONTROL = 0x2000000,
    SBC_BUTTON_SUB_WEAPON_CONTROL = 0x4000000,
    SBC_BUTTON_MAGAZINE_CHANGE = 0x8000000,
    SBC_BUTTON_COM1 = 0x10000000,
    SBC_BUTTON_COM2 = 0x20000000,
    SBC_BUTTON_COM3 = 0x40000000,
    SBC_BUTTON_COM4 = 0x80000000
};

#define SBC_BUTTON_COM5 \
    0x100000000ULL // These last 7 buttons are in bMoreButtons
#define SBC_BUTTON_SIGHT_CHANGE 0x200000000ULL
#define SBC_BUTTON_FILT_CONTROL_SYSTEM 0x400000000ULL
#define SBC_BUTTON_OXYGEN_SUPPLY_SYSTEM 0x800000000ULL
#define SBC_BUTTON_FUEL_FLOW_RATE 0x1000000000ULL
#define SBC_BUTTON_BUFFER_MATERIAL 0x2000000000ULL
#define SBC_BUTTON_VT_LOCATION_MEASUREMENT 0x4000000000ULL
#define SBC_BUTTON_GEAR_UP 0x8000000000ULL
#define SBC_BUTTON_GEAR_DOWN 0x10000000000ULL
#define SBC_BUTTON_TUNER_LEFT 0x20000000000ULL
#define SBC_BUTTON_TUNER_RIGHT 0x40000000000ULL

enum controller_state_axis_index {
    CONTROLLER_AXIS_LTRIG,
    CONTROLLER_AXIS_RTRIG,
    CONTROLLER_AXIS_LSTICK_X,
    CONTROLLER_AXIS_LSTICK_Y,
    CONTROLLER_AXIS_RSTICK_X,
    CONTROLLER_AXIS_RSTICK_Y,
    CONTROLLER_AXIS__COUNT,
};

enum steel_battalion_state_axis_index {
    SBC_AXIS_AIMING_X,
    SBC_AXIS_AIMING_Y,
    SBC_AXIS_ROTATION_LEVER,
    SBC_AXIS_LEFT_PEDAL,
    SBC_AXIS_MIDDLE_PEDAL,
    SBC_AXIS_RIGHT_PEDAL,
    SBC_AXIS_SIGHT_CHANGE_X,
    SBC_AXIS_SIGHT_CHANGE_Y,
    SBC_AXIS__COUNT
};

#ifdef __cplusplus
using GamepadMappings = struct config::input::gamepad_mappings;
#else
typedef struct gamepad_mappings GamepadMappings;
#endif

enum controller_input_device_type {
    INPUT_DEVICE_SDL_KEYBOARD,
    INPUT_DEVICE_SDL_GAMECONTROLLER,
};

enum peripheral_type { PERIPHERAL_NONE, PERIPHERAL_XMU, PERIPHERAL_TYPE_COUNT };

typedef struct XmuState {
    const char *filename;
    void *dev;
} XmuState;

typedef struct GamepadState {
    // Input state
    uint16_t buttons;
    int16_t  axis[CONTROLLER_AXIS__COUNT];

    // Rendering state hacked on here for convenience but needs to be moved
    // (FIXME)
    uint32_t animate_guide_button_end;
    uint32_t animate_trigger_end;

    // Rumble state
    uint16_t rumble_l, rumble_r;
} GamepadState;

typedef struct SteelBattalionState {
    uint64_t buttons;
    uint64_t previousButtons;
    int16_t axis[SBC_AXIS__COUNT];
    uint8_t gearLever;
    uint8_t tunerDial;
    uint8_t toggleSwitches;
} SteelBattalionState;

typedef struct ControllerState {
    QTAILQ_ENTRY(ControllerState) entry;

    int64_t last_input_updated_ts;
    int64_t last_rumble_updated_ts;

    GamepadState gp;
    SteelBattalionState sbc;

    enum controller_input_device_type type;
    const char         *name;
    SDL_GameController *sdl_gamecontroller; // if type == INPUT_DEVICE_SDL_GAMECONTROLLER
    SDL_Joystick       *sdl_joystick;
    SDL_JoystickID      sdl_joystick_id;
    SDL_JoystickGUID    sdl_joystick_guid;

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
void xemu_input_reset_input_mapping(ControllerState *state);

#ifdef __cplusplus
}
#endif

#endif
