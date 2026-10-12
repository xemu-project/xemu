/*
 * xemu Input Management
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


#include "qemu/osdep.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "monitor/qdev.h"
#include "qobject/qdict.h"
#include "qemu/option.h"
#include "qemu/timer.h"
#include "qemu/config-file.h"

#include "xemu-input.h"
#include "xemu-notifications.h"
#include "xemu-settings.h"
#include "xui/debug-tools/addons/diagnostics/controller-xid-trace-hook.h"
#include "xui/debug-tools/addons/diagnostics/controller-input-poll-trace-hook.h"
#include <stdio.h>
#include <stdlib.h>

#include "system/blockdev.h"

// #define DEBUG_INPUT

#ifdef DEBUG_INPUT
#define DPRINTF(fmt, ...) \
    do { fprintf(stderr, fmt, ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
    do { } while (0)
#endif

#define XEMU_INPUT_MIN_INPUT_UPDATE_INTERVAL_US  2500
#define XEMU_INPUT_MIN_RUMBLE_UPDATE_INTERVAL_US 2500

#if 0
static void xemu_input_print_controller_state(ControllerState *state)
{
    DPRINTF("     A = %d,      B = %d,     X = %d,     Y = %d\n"
           "  Left = %d,     Up = %d, Right = %d,  Down = %d\n"
           "  Back = %d,  Start = %d, White = %d, Black = %d\n"
           "Lstick = %d, Rstick = %d, Guide = %d\n"
           "\n"
           "LTrig   = %.3f, RTrig   = %.3f\n"
           "LStickX = %.3f, RStickX = %.3f\n"
           "LStickY = %.3f, RStickY = %.3f\n\n",
        !!(state->buttons & CONTROLLER_BUTTON_A),
        !!(state->buttons & CONTROLLER_BUTTON_B),
        !!(state->buttons & CONTROLLER_BUTTON_X),
        !!(state->buttons & CONTROLLER_BUTTON_Y),
        !!(state->buttons & CONTROLLER_BUTTON_DPAD_LEFT),
        !!(state->buttons & CONTROLLER_BUTTON_DPAD_UP),
        !!(state->buttons & CONTROLLER_BUTTON_DPAD_RIGHT),
        !!(state->buttons & CONTROLLER_BUTTON_DPAD_DOWN),
        !!(state->buttons & CONTROLLER_BUTTON_BACK),
        !!(state->buttons & CONTROLLER_BUTTON_START),
        !!(state->buttons & CONTROLLER_BUTTON_WHITE),
        !!(state->buttons & CONTROLLER_BUTTON_BLACK),
        !!(state->buttons & CONTROLLER_BUTTON_LSTICK),
        !!(state->buttons & CONTROLLER_BUTTON_RSTICK),
        !!(state->buttons & CONTROLLER_BUTTON_GUIDE),
        state->axis[CONTROLLER_AXIS_LTRIG],
        state->axis[CONTROLLER_AXIS_RTRIG],
        state->axis[CONTROLLER_AXIS_LSTICK_X],
        state->axis[CONTROLLER_AXIS_RSTICK_X],
        state->axis[CONTROLLER_AXIS_LSTICK_Y],
        state->axis[CONTROLLER_AXIS_RSTICK_Y]
        );
}
#endif

ControllerStateList available_controllers =
    QTAILQ_HEAD_INITIALIZER(available_controllers);
ControllerState *bound_controllers[4] = { NULL, NULL, NULL, NULL };
const char *bound_drivers[4] = { DRIVER_DUKE, DRIVER_DUKE, DRIVER_DUKE,
                                 DRIVER_DUKE };
int test_mode;

static const char **port_index_to_settings_key_map[] = {
    &g_config.input.bindings.port1,
    &g_config.input.bindings.port2,
    &g_config.input.bindings.port3,
    &g_config.input.bindings.port4,
};

static const char **port_index_to_driver_settings_key_map[] = {
    &g_config.input.bindings.port1_driver,
    &g_config.input.bindings.port2_driver,
    &g_config.input.bindings.port3_driver, 
    &g_config.input.bindings.port4_driver
};

static int *peripheral_types_settings_map[4][2] = {
    { &g_config.input.peripherals.port1.peripheral_type_0,
      &g_config.input.peripherals.port1.peripheral_type_1 },
    { &g_config.input.peripherals.port2.peripheral_type_0,
      &g_config.input.peripherals.port2.peripheral_type_1 },
    { &g_config.input.peripherals.port3.peripheral_type_0,
      &g_config.input.peripherals.port3.peripheral_type_1 },
    { &g_config.input.peripherals.port4.peripheral_type_0,
      &g_config.input.peripherals.port4.peripheral_type_1 }
};

static const char **peripheral_params_settings_map[4][2] = {
    { &g_config.input.peripherals.port1.peripheral_param_0,
      &g_config.input.peripherals.port1.peripheral_param_1 },
    { &g_config.input.peripherals.port2.peripheral_param_0,
      &g_config.input.peripherals.port2.peripheral_param_1 },
    { &g_config.input.peripherals.port3.peripheral_param_0,
      &g_config.input.peripherals.port3.peripheral_param_1 },
    { &g_config.input.peripherals.port4.peripheral_param_0,
      &g_config.input.peripherals.port4.peripheral_param_1 }
};

int *g_keyboard_scancode_map[25] = {
    &g_config.input.keyboard_controller_scancode_map.a,
    &g_config.input.keyboard_controller_scancode_map.b,
    &g_config.input.keyboard_controller_scancode_map.x,
    &g_config.input.keyboard_controller_scancode_map.y,
    &g_config.input.keyboard_controller_scancode_map.back,
    &g_config.input.keyboard_controller_scancode_map.guide,
    &g_config.input.keyboard_controller_scancode_map.start,
    &g_config.input.keyboard_controller_scancode_map.lstick_btn,
    &g_config.input.keyboard_controller_scancode_map.rstick_btn,
    &g_config.input.keyboard_controller_scancode_map.white,
    &g_config.input.keyboard_controller_scancode_map.black,
    &g_config.input.keyboard_controller_scancode_map.dpad_up,
    &g_config.input.keyboard_controller_scancode_map.dpad_down,
    &g_config.input.keyboard_controller_scancode_map.dpad_left,
    &g_config.input.keyboard_controller_scancode_map.dpad_right,
    &g_config.input.keyboard_controller_scancode_map.lstick_up,
    &g_config.input.keyboard_controller_scancode_map.lstick_left,
    &g_config.input.keyboard_controller_scancode_map.lstick_right,
    &g_config.input.keyboard_controller_scancode_map.lstick_down,
    &g_config.input.keyboard_controller_scancode_map.ltrigger,
    &g_config.input.keyboard_controller_scancode_map.rstick_up,
    &g_config.input.keyboard_controller_scancode_map.rstick_left,
    &g_config.input.keyboard_controller_scancode_map.rstick_right,
    &g_config.input.keyboard_controller_scancode_map.rstick_down,
    &g_config.input.keyboard_controller_scancode_map.rtrigger,
};

static void check_and_reset_in_range(int *btn, int min, int max,
                                     const char *message)
{
    if (*btn < min || *btn >= max) {
        fprintf(stderr, "%s\n", message);
        *btn = min;
    }
}

static void xemu_input_bindings_set_in_range(ControllerState *con)
{
#define CHECK_RESET_BUTTON(btn)                                            \
    check_and_reset_in_range(&con->controller_map->controller_mapping.btn, \
                             SDL_GAMEPAD_BUTTON_INVALID,                   \
                             SDL_GAMEPAD_BUTTON_COUNT,                     \
                             "Invalid entry for button " #btn ", resetting")

    CHECK_RESET_BUTTON(a);
    CHECK_RESET_BUTTON(b);
    CHECK_RESET_BUTTON(x);
    CHECK_RESET_BUTTON(y);
    CHECK_RESET_BUTTON(dpad_left);
    CHECK_RESET_BUTTON(dpad_up);
    CHECK_RESET_BUTTON(dpad_right);
    CHECK_RESET_BUTTON(dpad_down);
    CHECK_RESET_BUTTON(back);
    CHECK_RESET_BUTTON(start);
    CHECK_RESET_BUTTON(lshoulder);
    CHECK_RESET_BUTTON(rshoulder);
    CHECK_RESET_BUTTON(lstick_btn);
    CHECK_RESET_BUTTON(rstick_btn);
    CHECK_RESET_BUTTON(guide);

#undef CHECK_RESET_BUTTON

#define CHECK_RESET_AXIS(axis)                                              \
    check_and_reset_in_range(&con->controller_map->controller_mapping.axis, \
                             SDL_GAMEPAD_AXIS_INVALID,                      \
                             SDL_GAMEPAD_AXIS_COUNT,                        \
                             "Invalid entry for button " #axis ", resetting")

    CHECK_RESET_AXIS(axis_trigger_left);
    CHECK_RESET_AXIS(axis_trigger_right);
    CHECK_RESET_AXIS(axis_left_x);
    CHECK_RESET_AXIS(axis_left_y);
    CHECK_RESET_AXIS(axis_right_x);
    CHECK_RESET_AXIS(axis_right_y);

#undef CHECK_RESET_AXIS
}

static void xemu_input_bindings_reload_map(ControllerState *con)
{
    assert(con->type == INPUT_DEVICE_SDL_GAMEPAD);

    char guid[35] = { 0 };
    SDL_GUIDToString(con->sdl_joystick_guid, guid, sizeof(guid));
    if (!xemu_settings_load_gamepad_mapping(guid, &con->controller_map)) {
        return;
    }

    // If this controller did not exist in the mapping array, the config will
    // have been reallocated. Any gamepad mapping pointers for other controllers
    // are now invalid, and need to be reloaded.
    ControllerState *iter, *next;
    bool is_new_mapping;
    QTAILQ_FOREACH_SAFE (iter, &available_controllers, entry, next) {
        if (iter == con || iter->type != INPUT_DEVICE_SDL_GAMEPAD) {
            continue;
        }

        memset(guid, 0, sizeof(guid));
        SDL_GUIDToString(iter->sdl_joystick_guid, guid, sizeof(guid));

        is_new_mapping =
            xemu_settings_load_gamepad_mapping(guid, &iter->controller_map);
        assert(!is_new_mapping &&
               "Existing controller GUIDs should exist in the config");

        xemu_input_bindings_set_in_range(iter);
    }
}

static const char *get_bound_driver(int port)
{
    assert(port >= 0 && port <= 3);
    const char *driver = *port_index_to_driver_settings_key_map[port];

    // If the driver in the config is NULL, empty, or unrecognized 
    // then default to DRIVER_DUKE
    if (driver == NULL)
        return DRIVER_DUKE;
    if (strlen(driver) == 0)
        return DRIVER_DUKE;
    if (strcmp(driver, DRIVER_DUKE) == 0)
        return DRIVER_DUKE;
    if (strcmp(driver, DRIVER_S) == 0)
        return DRIVER_S;

    return DRIVER_DUKE;
}

static const int port_map[4] = { 3, 4, 1, 2 };

void xemu_input_init(void)
{
    if (g_config.input.background_input_capture) {
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    }

    if (!SDL_Init(SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "Failed to initialize SDL gamepad subsystem: %s\n", SDL_GetError());
        exit(1);
    }

    if (g_config.input.gamecontrollerdb_path && strlen(g_config.input.gamecontrollerdb_path) > 0) {
        int count = SDL_AddGamepadMappingsFromFile(g_config.input.gamecontrollerdb_path);
        if (count > 0) {
            fprintf(stderr, "Loaded %d custom gamepad mapping(s) from '%s'\n",
                    count, g_config.input.gamecontrollerdb_path);
        } else if (count < 0) {
            fprintf(stderr, "Failed to load gamepad mappings from '%s': %s\n",
                    g_config.input.gamecontrollerdb_path, SDL_GetError());
        }
    }

    // Create the keyboard input (always first)
    ControllerState *new_con = malloc(sizeof(ControllerState));
    memset(new_con, 0, sizeof(ControllerState));
    new_con->type = INPUT_DEVICE_SDL_KEYBOARD;
    new_con->name = "Keyboard";
    new_con->bound = -1;
    new_con->connected = true;
    new_con->peripheral_types[0] = PERIPHERAL_NONE;
    new_con->peripheral_types[1] = PERIPHERAL_NONE;
    new_con->peripherals[0] = NULL;
    new_con->peripherals[1] = NULL;

    for (int i = 0; i < 25; i++) {
        static const char *format_str =
            "WARNING: Keyboard controller map scancode out of range "
            "(%d) : Disabled\n";
        char buf[128];
        snprintf(buf, sizeof(buf), format_str, i);
        check_and_reset_in_range(g_keyboard_scancode_map[i],
                                 SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_COUNT, buf);
    }

    bound_drivers[0] = get_bound_driver(0);
    bound_drivers[1] = get_bound_driver(1);
    bound_drivers[2] = get_bound_driver(2);
    bound_drivers[3] = get_bound_driver(3);

    // Check to see if we should auto-bind the keyboard
    int port = xemu_input_get_controller_default_bind_port(new_con, 0);
    if (port >= 0) {
        xemu_input_bind(port, new_con, 0);
        char buf[128];
        snprintf(buf, sizeof(buf), "Connected '%s' to port %d", new_con->name, port+1);
        xemu_queue_notification(buf);
        xemu_input_rebind_xmu(port);
    }

    QTAILQ_INSERT_TAIL(&available_controllers, new_con, entry);
}

int xemu_input_get_controller_default_bind_port(ControllerState *state, int start)
{
    char guid[35] = { 0 };
    if (state->type == INPUT_DEVICE_SDL_GAMEPAD) {
        SDL_GUIDToString(state->sdl_joystick_guid, guid, sizeof(guid));
    } else if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
        snprintf(guid, sizeof(guid), "keyboard");
    }

    for (int i = start; i < 4; i++) {
        if (strcmp(guid, *port_index_to_settings_key_map[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void xemu_save_peripheral_settings(int player_index, int peripheral_index,
                                   int peripheral_type,
                                   const char *peripheral_parameter)
{
    int *peripheral_type_ptr =
        peripheral_types_settings_map[player_index][peripheral_index];
    const char **peripheral_param_ptr =
        peripheral_params_settings_map[player_index][peripheral_index];

    assert(peripheral_type_ptr);
    assert(peripheral_param_ptr);

    *peripheral_type_ptr = peripheral_type;
    xemu_settings_set_string(
        peripheral_param_ptr,
        peripheral_parameter == NULL ? "" : peripheral_parameter);
}

void xemu_input_process_sdl_events(const SDL_Event *event)
{
    if (event->type == SDL_EVENT_GAMEPAD_ADDED) {
        DPRINTF("Controller Added: %d\n", event->gdevice.which);
        xemu_debug_tools_controller_xid_trace_host_lifecycle(
            XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_GAMEPAD_ADDED, -1,
            event->gdevice.which, 0, NULL, NULL);

        // Attempt to open the added controller
        SDL_Gamepad *sdl_con;
        sdl_con = SDL_OpenGamepad(event->gdevice.which);
        if (sdl_con == NULL) {
            DPRINTF("Could not open joystick %d as a Gamepad\n", event->gdevice.which);
            xemu_debug_tools_controller_xid_trace_host_lifecycle(
                XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_GAMEPAD_OPEN_FAILED, -1,
                event->gdevice.which, 0, NULL, NULL);
            return;
        }

        SDL_Joystick *sdl_joystick = SDL_GetGamepadJoystick(sdl_con);
        SDL_JoystickID sdl_joystick_id = SDL_GetJoystickID(sdl_joystick);
        SDL_GUID sdl_joystick_guid = SDL_GetJoystickGUID(sdl_joystick);
        const char *sdl_name = SDL_GetGamepadName(sdl_con);
        char guid_buf[35] = { 0 };
        SDL_GUIDToString(sdl_joystick_guid, guid_buf, sizeof(guid_buf));
        DPRINTF("Opened %s (%s)\n", sdl_name, guid_buf);
        xemu_debug_tools_controller_xid_trace_host_lifecycle(
            XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_GAMEPAD_OPENED, -1,
            sdl_joystick_id, 0, sdl_name, guid_buf);

        /* Info: Reconnect the same host GUID without guest XID re-enumeration. */
        ControllerState *new_con = NULL;
        ControllerState *iter;
        QTAILQ_FOREACH(iter, &available_controllers, entry) {
            if (iter->type != INPUT_DEVICE_SDL_GAMEPAD || iter->connected ||
                iter->bound < 0) {
                continue;
            }
            if (memcmp(&iter->sdl_joystick_guid, &sdl_joystick_guid,
                       sizeof(SDL_GUID)) == 0) {
                new_con = iter;
                break;
            }
        }

        if (new_con != NULL) {
            new_con->name = sdl_name;
            new_con->sdl_gamepad = sdl_con;
            new_con->sdl_joystick = sdl_joystick;
            new_con->sdl_joystick_id = sdl_joystick_id;
            new_con->sdl_joystick_guid = sdl_joystick_guid;
            new_con->connected = true;
            new_con->last_input_updated_ts = 0;
            new_con->last_rumble_updated_ts = 0;
            new_con->buttons = 0;
            memset(new_con->axis, 0, sizeof(new_con->axis));
            xemu_input_bindings_reload_map(new_con);

            xemu_debug_tools_controller_xid_trace_host_lifecycle(
                XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_PERSISTENT_REATTACHED,
                new_con->bound, new_con->sdl_joystick_id, 0,
                new_con->name, guid_buf);

            char buf[128];
            snprintf(buf, sizeof(buf), "Reconnected '%s' to port %d",
                     new_con->name, new_con->bound + 1);
            xemu_queue_notification(buf);
            return;
        }

        // Success! Create a new node to track this controller and continue init.
        new_con = malloc(sizeof(ControllerState));
        memset(new_con, 0, sizeof(ControllerState));
        new_con->type                 = INPUT_DEVICE_SDL_GAMEPAD;
        new_con->name                 = sdl_name;
        new_con->sdl_gamepad          = sdl_con;
        new_con->sdl_joystick         = sdl_joystick;
        new_con->sdl_joystick_id      = sdl_joystick_id;
        new_con->sdl_joystick_guid    = sdl_joystick_guid;
        new_con->bound                = -1;
        new_con->connected            = true;
        new_con->peripheral_types[0] = PERIPHERAL_NONE;
        new_con->peripheral_types[1] = PERIPHERAL_NONE;
        new_con->peripherals[0] = NULL;
        new_con->peripherals[1] = NULL;

        QTAILQ_INSERT_TAIL(&available_controllers, new_con, entry);
        xemu_input_bindings_reload_map(new_con);

        // Do not replace binding for a currently bound device. In the case that
        // the same GUID is specified multiple times, on different ports, allow
        // any available port to be bound.
        //
        // This can happen naturally with X360 wireless receiver, in which each
        // controller gets the same GUID (go figure). We cannot remember which
        // controller is which in this case, but we can try to tolerate this
        // situation by binding to any previously bound port with this GUID. The
        // upside in this case is that a person can use the same GUID on all
        // ports and just needs to bind to the receiver and never needs to hit
        // this dialog.


        // Attempt to re-bind to port previously bound to
        int port = 0;
        bool did_bind = false;
        while (!did_bind) {
            port = xemu_input_get_controller_default_bind_port(new_con, port);
            if (port < 0) {
                // No (additional) default mappings
                break;
            } else if (!xemu_input_get_bound(port)) {
                xemu_input_bind(port, new_con, 0);
                did_bind = true;
                break;
            } else {
                // Try again for another port
                port++;
            }
        }

        // Try to bind to any open port, and if so remember the binding
        if (!did_bind && g_config.input.auto_bind) {
            for (port = 0; port < 4; port++) {
                if (!xemu_input_get_bound(port)) {
                    xemu_input_bind(port, new_con, 1);
                    did_bind = true;
                    break;
                }
            }
        }

        if (did_bind) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Connected '%s' to port %d", new_con->name, port+1);
            xemu_queue_notification(buf);
            xemu_input_rebind_xmu(port);
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_REMOVED) {
        DPRINTF("Controller Removed: %d\n", event->gdevice.which);
        int handled = 0;
        ControllerState *iter, *next;
        QTAILQ_FOREACH_SAFE(iter, &available_controllers, entry, next) {
            if (iter->type != INPUT_DEVICE_SDL_GAMEPAD) continue;

            if (iter->sdl_joystick_id == event->gdevice.which) {
                char trace_guid[35] = { 0 };
                SDL_GUIDToString(iter->sdl_joystick_guid, trace_guid,
                                 sizeof(trace_guid));
                DPRINTF("Device removed: %s\n", iter->name);
                xemu_debug_tools_controller_xid_trace_host_lifecycle(
                    XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_GAMEPAD_REMOVED,
                    iter->bound, iter->sdl_joystick_id, 0, iter->name, trace_guid);

                if (iter->bound >= 0) {
                    /* Info: Keep the guest XID device present with neutral input while host hardware is absent. */
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Port %d controller disconnected; guest controller retained",
                             iter->bound + 1);
                    xemu_queue_notification(buf);

                    iter->buttons = 0;
                    memset(iter->axis, 0, sizeof(iter->axis));
                    iter->rumble_l = 0;
                    iter->rumble_r = 0;
                    xemu_debug_tools_controller_xid_trace_host_state(
                        iter->bound, iter->sdl_joystick_id,
                        iter->buttons, iter->axis);
                    xemu_debug_tools_controller_xid_trace_host_lifecycle(
                        XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_PERSISTENT_NEUTRAL,
                        iter->bound, iter->sdl_joystick_id, 0,
                        iter->name, trace_guid);

                    if (iter->sdl_gamepad) {
                        SDL_RumbleGamepad(iter->sdl_gamepad, 0, 0, 0);
                        SDL_CloseGamepad(iter->sdl_gamepad);
                    }
                    iter->name = "Disconnected Controller";
                    iter->sdl_gamepad = NULL;
                    iter->sdl_joystick = NULL;
                    iter->connected = false;
                    iter->last_input_updated_ts = 0;
                    iter->last_rumble_updated_ts = 0;

                    handled = 1;
                    break;
                }

                // Unbound host devices have no guest XID to preserve.
                QTAILQ_REMOVE(&available_controllers, iter, entry);
                if (iter->sdl_gamepad) {
                    SDL_CloseGamepad(iter->sdl_gamepad);
                }
                for (int i = 0; i < 2; i++) {
                    if (iter->peripherals[i]) {
                        g_free(iter->peripherals[i]);
                    }
                }
                free(iter);

                handled = 1;
                break;
            }
        }
        if (!handled) {
            DPRINTF("Could not find handle for joystick instance\n");
        }
    } else if (event->type == SDL_EVENT_GAMEPAD_REMAPPED) {
        DPRINTF("Controller Remapped: %d\n", event->gdevice.which);
    } else if (event->type == SDL_EVENT_JOYSTICK_ADDED) {
        if (!SDL_IsGamepad(event->jdevice.which)) {
            const char *name = SDL_GetJoystickNameForID(event->jdevice.which);
            SDL_GUID guid = SDL_GetJoystickGUIDForID(event->jdevice.which);
            char guid_buf[35] = { 0 };
            SDL_GUIDToString(guid, guid_buf, sizeof(guid_buf));
            fprintf(
                stderr,
                "Input: Detected unmapped joystick '%s' (GUID: %s, ID: %u). "
                "This device is not recognized as a gamepad by SDL and "
                "requires an SDL gamepad mapping to work with xemu.\n",
                name ? name : "Unknown", guid_buf,
                (unsigned int)event->jdevice.which);
        }
    }
}

typedef struct XemuInputHostPollEntry {
    ControllerState *state;
    SDL_Gamepad *sdl_gamepad;
    SDL_JoystickID sdl_joystick_id;

    SDL_GamepadButton button_mapping[15];
    SDL_GamepadAxis axis_mapping[CONTROLLER_AXIS__COUNT];
    bool invert_axis[CONTROLLER_AXIS__COUNT];

    uint16_t buttons;
    int16_t axis[CONTROLLER_AXIS__COUNT];
    int64_t sampled_ts;
    bool sampled;
} XemuInputHostPollEntry;

struct XemuInputHostPollBatch {
    size_t count;
    XemuInputHostPollEntry entries[];
};

static void xemu_input_host_poll_copy_mapping(XemuInputHostPollEntry *entry,
                                               ControllerState *state)
{
    entry->button_mapping[0] = state->controller_map->controller_mapping.a;
    entry->button_mapping[1] = state->controller_map->controller_mapping.b;
    entry->button_mapping[2] = state->controller_map->controller_mapping.x;
    entry->button_mapping[3] = state->controller_map->controller_mapping.y;
    entry->button_mapping[4] =
        state->controller_map->controller_mapping.dpad_left;
    entry->button_mapping[5] =
        state->controller_map->controller_mapping.dpad_up;
    entry->button_mapping[6] =
        state->controller_map->controller_mapping.dpad_right;
    entry->button_mapping[7] =
        state->controller_map->controller_mapping.dpad_down;
    entry->button_mapping[8] = state->controller_map->controller_mapping.back;
    entry->button_mapping[9] = state->controller_map->controller_mapping.start;
    entry->button_mapping[10] =
        state->controller_map->controller_mapping.lshoulder;
    entry->button_mapping[11] =
        state->controller_map->controller_mapping.rshoulder;
    entry->button_mapping[12] =
        state->controller_map->controller_mapping.lstick_btn;
    entry->button_mapping[13] =
        state->controller_map->controller_mapping.rstick_btn;
    entry->button_mapping[14] = state->controller_map->controller_mapping.guide;

    entry->axis_mapping[CONTROLLER_AXIS_LTRIG] =
        state->controller_map->controller_mapping.axis_trigger_left;
    entry->axis_mapping[CONTROLLER_AXIS_RTRIG] =
        state->controller_map->controller_mapping.axis_trigger_right;
    entry->axis_mapping[CONTROLLER_AXIS_LSTICK_X] =
        state->controller_map->controller_mapping.axis_left_x;
    entry->axis_mapping[CONTROLLER_AXIS_LSTICK_Y] =
        state->controller_map->controller_mapping.axis_left_y;
    entry->axis_mapping[CONTROLLER_AXIS_RSTICK_X] =
        state->controller_map->controller_mapping.axis_right_x;
    entry->axis_mapping[CONTROLLER_AXIS_RSTICK_Y] =
        state->controller_map->controller_mapping.axis_right_y;

    entry->invert_axis[CONTROLLER_AXIS_LSTICK_X] =
        state->controller_map->controller_mapping.invert_axis_left_x;
    entry->invert_axis[CONTROLLER_AXIS_LSTICK_Y] =
        !state->controller_map->controller_mapping.invert_axis_left_y;
    entry->invert_axis[CONTROLLER_AXIS_RSTICK_X] =
        state->controller_map->controller_mapping.invert_axis_right_x;
    entry->invert_axis[CONTROLLER_AXIS_RSTICK_Y] =
        !state->controller_map->controller_mapping.invert_axis_right_y;
}

static bool xemu_input_host_poll_ready(const ControllerState *state,
                                       int64_t now)
{
    return state->type == INPUT_DEVICE_SDL_GAMEPAD && state->connected &&
           state->sdl_gamepad != NULL && state->controller_map != NULL &&
           ABS(now - state->last_input_updated_ts) >=
               XEMU_INPUT_MIN_INPUT_UPDATE_INTERVAL_US;
}

XemuInputHostPollBatch *xemu_input_host_poll_prepare(void)
{
    const int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    size_t count = 0;
    ControllerState *iter;

    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (xemu_input_host_poll_ready(iter, now)) {
            count++;
        }
    }

    if (count == 0) {
        return NULL;
    }

    XemuInputHostPollBatch *batch = g_malloc0(
        sizeof(*batch) + count * sizeof(batch->entries[0]));
    batch->count = count;

    size_t index = 0;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (!xemu_input_host_poll_ready(iter, now)) {
            continue;
        }

        XemuInputHostPollEntry *entry = &batch->entries[index++];
        entry->state = iter;
        entry->sdl_gamepad = iter->sdl_gamepad;
        entry->sdl_joystick_id = iter->sdl_joystick_id;
        xemu_input_host_poll_copy_mapping(entry, iter);
    }
    assert(index == count);
    return batch;
}

static inline bool xemu_input_profiled_gamepad_button(
    SDL_Gamepad *gamepad, SDL_GamepadButton button, uint32_t getter,
    bool input_poll_trace)
{
    uint64_t start_ns = input_poll_trace
        ? xemu_debug_tools_controller_input_poll_trace_stage_begin() : 0;
    const bool value = SDL_GetGamepadButton(gamepad, button);
    if (input_poll_trace) {
        xemu_debug_tools_controller_input_poll_trace_getter_end(getter,
                                                                start_ns);
    }
    return value;
}

static inline Sint16 xemu_input_profiled_gamepad_axis(
    SDL_Gamepad *gamepad, SDL_GamepadAxis axis, uint32_t getter,
    bool input_poll_trace)
{
    uint64_t start_ns = input_poll_trace
        ? xemu_debug_tools_controller_input_poll_trace_stage_begin() : 0;
    const Sint16 value = SDL_GetGamepadAxis(gamepad, axis);
    if (input_poll_trace) {
        xemu_debug_tools_controller_input_poll_trace_getter_end(getter,
                                                                start_ns);
    }
    return value;
}

static const uint32_t xemu_input_host_poll_button_getters[15] = {
    XEMU_DEBUG_TOOLS_INPUT_GETTER_BUTTON_A,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_BUTTON_B,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_BUTTON_X,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_BUTTON_Y,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_DPAD_LEFT,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_DPAD_UP,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_DPAD_RIGHT,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_DPAD_DOWN,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_BACK,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_START,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_LEFT_SHOULDER,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_RIGHT_SHOULDER,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_LEFT_STICK_BUTTON,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_RIGHT_STICK_BUTTON,
    XEMU_DEBUG_TOOLS_INPUT_GETTER_GUIDE,
};

static const uint32_t
    xemu_input_host_poll_axis_getters[CONTROLLER_AXIS__COUNT] = {
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_TRIGGER_LEFT,
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_TRIGGER_RIGHT,
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_LEFT_X,
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_LEFT_Y,
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_RIGHT_X,
        XEMU_DEBUG_TOOLS_INPUT_GETTER_AXIS_RIGHT_Y,
    };

void xemu_input_host_poll_sample(XemuInputHostPollBatch *batch)
{
    if (batch == NULL) {
        return;
    }

    for (size_t i = 0; i < batch->count; i++) {
        XemuInputHostPollEntry *entry = &batch->entries[i];
        uint16_t buttons = 0;
        int16_t axis[CONTROLLER_AXIS__COUNT] = { 0 };

        const bool input_poll_trace =
            xemu_debug_tools_controller_input_poll_trace_active() != 0;
        for (int button = 0; button < 15; button++) {
            if (xemu_input_profiled_gamepad_button(
                    entry->sdl_gamepad, entry->button_mapping[button],
                    xemu_input_host_poll_button_getters[button],
                    input_poll_trace)) {
                buttons |= (uint16_t)(1u << button);
            }
        }
        for (int axis_index = 0; axis_index < CONTROLLER_AXIS__COUNT;
             axis_index++) {
            axis[axis_index] = xemu_input_profiled_gamepad_axis(
                entry->sdl_gamepad, entry->axis_mapping[axis_index],
                xemu_input_host_poll_axis_getters[axis_index],
                input_poll_trace);
        }

        for (int axis_index = CONTROLLER_AXIS_LSTICK_X;
             axis_index <= CONTROLLER_AXIS_RSTICK_Y; axis_index++) {
            if (entry->invert_axis[axis_index]) {
                axis[axis_index] = -1 - axis[axis_index];
            }
        }

        entry->buttons = buttons;
        memcpy(entry->axis, axis, sizeof(entry->axis));
        entry->sampled_ts = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
        entry->sampled = true;
    }
}

static ControllerState *xemu_input_host_poll_find_live_state(
    const XemuInputHostPollEntry *entry)
{
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter != entry->state) {
            continue;
        }
        if (iter->type != INPUT_DEVICE_SDL_GAMEPAD || !iter->connected ||
            iter->sdl_gamepad != entry->sdl_gamepad ||
            iter->sdl_joystick_id != entry->sdl_joystick_id) {
            return NULL;
        }
        return iter;
    }
    return NULL;
}

void xemu_input_host_poll_commit(XemuInputHostPollBatch *batch)
{
    if (batch == NULL) {
        return;
    }

    for (size_t i = 0; i < batch->count; i++) {
        XemuInputHostPollEntry *entry = &batch->entries[i];
        if (!entry->sampled) {
            continue;
        }

        ControllerState *state = xemu_input_host_poll_find_live_state(entry);
        if (state == NULL) {
            continue;
        }

        state->buttons = entry->buttons;
        memcpy(state->axis, entry->axis, sizeof(state->axis));
        state->last_input_updated_ts = entry->sampled_ts;
        xemu_debug_tools_controller_xid_trace_host_state(
            state->bound, state->sdl_joystick_id, state->buttons, state->axis);
    }
}

void xemu_input_host_poll_free(XemuInputHostPollBatch *batch)
{
    g_free(batch);
}

void xemu_input_update_controller(ControllerState *state)
{
    const int input_poll_trace =
        xemu_debug_tools_controller_input_poll_trace_active();
    uint64_t input_poll_stage = input_poll_trace
        ? xemu_debug_tools_controller_input_poll_trace_stage_begin() : 0;

    if (state->type == INPUT_DEVICE_SDL_GAMEPAD) {
        if (input_poll_trace) {
            xemu_debug_tools_controller_input_poll_trace_set_update_result(
                0, state->connected ? 0 : 1, 0);
            xemu_debug_tools_controller_input_poll_trace_stage_end(
                XEMU_DEBUG_TOOLS_INPUT_POLL_STAGE_UPDATE_GATE,
                input_poll_stage);
        }
        return;
    }

    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    if (ABS(now - state->last_input_updated_ts) <
        XEMU_INPUT_MIN_INPUT_UPDATE_INTERVAL_US) {
        if (input_poll_trace) {
            xemu_debug_tools_controller_input_poll_trace_set_update_result(
                0, 0, 1);
            xemu_debug_tools_controller_input_poll_trace_stage_end(
                XEMU_DEBUG_TOOLS_INPUT_POLL_STAGE_UPDATE_GATE,
                input_poll_stage);
        }
        return;
    }

    if (input_poll_trace) {
        xemu_debug_tools_controller_input_poll_trace_set_update_result(1, 0, 0);
        xemu_debug_tools_controller_input_poll_trace_stage_end(
            XEMU_DEBUG_TOOLS_INPUT_POLL_STAGE_UPDATE_GATE, input_poll_stage);
    }

    if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
        xemu_input_update_sdl_kbd_controller_state(state);
    }

    state->last_input_updated_ts = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
}

void xemu_input_update_controllers(void)
{
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        /* Info: SDL gamepads use cached host-poll state; keyboard refresh remains local. */
        if (iter->type == INPUT_DEVICE_SDL_KEYBOARD) {
            xemu_input_update_controller(iter);
        }
    }
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        xemu_input_update_rumble(iter);
    }
}

void xemu_input_update_sdl_kbd_controller_state(ControllerState *state)
{
    state->buttons = 0;
    memset(state->axis, 0, sizeof(state->axis));

    const bool *kbd = SDL_GetKeyboardState(NULL);

#define KBD_STATE(btn) \
    (kbd[g_config.input.keyboard_controller_scancode_map.btn])

    state->buttons |= KBD_STATE(a) << 0;
    state->buttons |= KBD_STATE(b) << 1;
    state->buttons |= KBD_STATE(x) << 2;
    state->buttons |= KBD_STATE(y) << 3;
    state->buttons |= KBD_STATE(dpad_left) << 4;
    state->buttons |= KBD_STATE(dpad_up) << 5;
    state->buttons |= KBD_STATE(dpad_right) << 6;
    state->buttons |= KBD_STATE(dpad_down) << 7;
    state->buttons |= KBD_STATE(back) << 8;
    state->buttons |= KBD_STATE(start) << 9;
    state->buttons |= KBD_STATE(white) << 10;
    state->buttons |= KBD_STATE(black) << 11;
    state->buttons |= KBD_STATE(lstick_btn) << 12;
    state->buttons |= KBD_STATE(rstick_btn) << 13;
    state->buttons |= KBD_STATE(guide) << 14;

    if (KBD_STATE(lstick_up))
        state->axis[CONTROLLER_AXIS_LSTICK_Y] = 32767;
    if (KBD_STATE(lstick_left))
        state->axis[CONTROLLER_AXIS_LSTICK_X] = -32768;
    if (KBD_STATE(lstick_right))
        state->axis[CONTROLLER_AXIS_LSTICK_X] = 32767;
    if (KBD_STATE(lstick_down))
        state->axis[CONTROLLER_AXIS_LSTICK_Y] = -32768;
    if (KBD_STATE(ltrigger))
        state->axis[CONTROLLER_AXIS_LTRIG] = 32767;

    if (KBD_STATE(rstick_up))
        state->axis[CONTROLLER_AXIS_RSTICK_Y] = 32767;
    if (KBD_STATE(rstick_left))
        state->axis[CONTROLLER_AXIS_RSTICK_X] = -32768;
    if (KBD_STATE(rstick_right))
        state->axis[CONTROLLER_AXIS_RSTICK_X] = 32767;
    if (KBD_STATE(rstick_down))
        state->axis[CONTROLLER_AXIS_RSTICK_Y] = -32768;
    if (KBD_STATE(rtrigger))
        state->axis[CONTROLLER_AXIS_RTRIG] = 32767;

#undef KBD_STATE
}

void xemu_input_update_rumble(ControllerState *state)
{
    if (state->type != INPUT_DEVICE_SDL_GAMEPAD) {
        return;
    }

    if (!state->connected || state->sdl_gamepad == NULL) {
        return;
    }

    if (!state->controller_map->enable_rumble) {
        return;
    }

    int64_t now = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
    if (ABS(now - state->last_rumble_updated_ts) <
        XEMU_INPUT_MIN_RUMBLE_UPDATE_INTERVAL_US) {
        return;
    }

    SDL_RumbleGamepad(state->sdl_gamepad, state->rumble_l, state->rumble_r, 250);
    state->last_rumble_updated_ts = qemu_clock_get_us(QEMU_CLOCK_REALTIME);
}

ControllerState *xemu_input_get_bound(int index)
{
    return bound_controllers[index];
}

void xemu_input_bind(int index, ControllerState *state, int save)
{
    // FIXME: Attempt to disable rumble when unbinding so it's not left
    // in rumble mode

    // Unbind existing controller
    if (bound_controllers[index]) {
        char trace_guid[35] = { 0 };
        ControllerState *old_state = bound_controllers[index];
        if (old_state->type == INPUT_DEVICE_SDL_GAMEPAD) {
            SDL_GUIDToString(old_state->sdl_joystick_guid, trace_guid,
                             sizeof(trace_guid));
        }
        assert(old_state->device != NULL);
        xemu_debug_tools_controller_xid_trace_host_lifecycle(
            XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_UNBIND, index,
            old_state->type == INPUT_DEVICE_SDL_GAMEPAD
                ? old_state->sdl_joystick_id : 0,
            save, old_state->name, trace_guid);
        Error *err = NULL;

        // Unbind any XMUs
        for (int i = 0; i < 2; i++) {
            if (bound_controllers[index]->peripherals[i]) {
                // If this was an XMU, unbind the XMU
                if (bound_controllers[index]->peripheral_types[i] ==
                    PERIPHERAL_XMU)
                    xemu_input_unbind_xmu(index, i);

                // Free up the XmuState and set the peripheral type to none
                g_free(bound_controllers[index]->peripherals[i]);
                bound_controllers[index]->peripherals[i] = NULL;
                bound_controllers[index]->peripheral_types[i] = PERIPHERAL_NONE;
            }
        }

        qdev_unplug((DeviceState *)bound_controllers[index]->device, &err);
        assert(err == NULL);

        bound_controllers[index]->bound = -1;
        bound_controllers[index]->device = NULL;
        bound_controllers[index] = NULL;
    }

    // Save this controller's GUID in settings for auto re-connect
    if (save) {
        char guid_buf[35] = { 0 };
        if (state) {
            if (state->type == INPUT_DEVICE_SDL_GAMEPAD) {
                SDL_GUIDToString(state->sdl_joystick_guid, guid_buf, sizeof(guid_buf));
            } else if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
                snprintf(guid_buf, sizeof(guid_buf), "keyboard");
            }
        }
        xemu_settings_set_string(port_index_to_settings_key_map[index], guid_buf);
        xemu_settings_set_string(port_index_to_driver_settings_key_map[index],
                                 bound_drivers[index]);
    }

    // Bind new controller
    if (state) {
        if (state->bound >= 0) {
            // Device was already bound to another port. Unbind it.
            xemu_input_bind(state->bound, NULL, 1);
        }

        bound_controllers[index] = state;
        bound_controllers[index]->bound = index;

        char *tmp;

        // Create controller's internal USB hub.
        QDict *usbhub_qdict = qdict_new();
        qdict_put_str(usbhub_qdict, "driver", "usb-hub");
        tmp = g_strdup_printf("1.%d", port_map[index]);
        qdict_put_str(usbhub_qdict, "port", tmp);
        qdict_put_int(usbhub_qdict, "ports", 3);
        QemuOpts *usbhub_opts = qemu_opts_from_qdict(qemu_find_opts("device"), usbhub_qdict, &error_abort);
        DeviceState *usbhub_dev = qdev_device_add(usbhub_opts, &error_abort);
        g_free(tmp);

        // Create XID controller. This is connected to Port 1 of the controller's internal USB Hub
        QDict *qdict = qdict_new();

        // Specify device driver
        qdict_put_str(qdict, "driver", bound_drivers[index]);

        // Specify device identifier
        static int id_counter = 0;
        tmp = g_strdup_printf("gamepad_%d", id_counter++);
        qdict_put_str(qdict, "id", tmp);
        g_free(tmp);

        // Specify index/port
        qdict_put_int(qdict, "index", index);
        tmp = g_strdup_printf("1.%d.1", port_map[index]);
        qdict_put_str(qdict, "port", tmp);
        g_free(tmp);

        // Create the device
        QemuOpts *opts = qemu_opts_from_qdict(qemu_find_opts("device"), qdict, &error_abort);
        DeviceState *dev = qdev_device_add(opts, &error_abort);
        assert(dev);

        // Unref for eventual cleanup
        qobject_unref(usbhub_qdict);
        object_unref(OBJECT(usbhub_dev));
        qobject_unref(qdict);
        object_unref(OBJECT(dev));

        state->device = usbhub_dev;

        char trace_guid[35] = { 0 };
        if (state->type == INPUT_DEVICE_SDL_GAMEPAD) {
            SDL_GUIDToString(state->sdl_joystick_guid, trace_guid,
                             sizeof(trace_guid));
        }
        xemu_debug_tools_controller_xid_trace_host_lifecycle(
            XEMU_DEBUG_TOOLS_CONTROLLER_XID_HOST_BIND, index,
            state->type == INPUT_DEVICE_SDL_GAMEPAD
                ? state->sdl_joystick_id : 0,
            save, state->name, trace_guid);
    }
}

bool xemu_input_bind_xmu(int player_index, int expansion_slot_index,
                         const char *filename, bool is_rebind)
{
    assert(player_index >= 0 && player_index < 4);
    assert(expansion_slot_index >= 0 && expansion_slot_index < 2);

    ControllerState *player = bound_controllers[player_index];
    enum peripheral_type peripheral_type =
        player->peripheral_types[expansion_slot_index];
    if (peripheral_type != PERIPHERAL_XMU)
        return false;

    XmuState *xmu = (XmuState *)player->peripherals[expansion_slot_index];

    // Unbind existing XMU
    if (xmu->dev != NULL) {
        xemu_input_unbind_xmu(player_index, expansion_slot_index);
    }

    if (filename == NULL)
        return false;

    // Look for any other XMUs that are using this file, and unbind them
    for (int player_i = 0; player_i < 4; player_i++) {
        ControllerState *state = bound_controllers[player_i];
        if (state != NULL) {
            for (int peripheral_i = 0; peripheral_i < 2; peripheral_i++) {
                if (state->peripheral_types[peripheral_i] == PERIPHERAL_XMU) {
                    XmuState *xmu_i =
                        (XmuState *)state->peripherals[peripheral_i];
                    assert(xmu_i);

                    if (xmu_i->filename != NULL &&
                        strcmp(xmu_i->filename, filename) == 0) {
                        char *buf =
                            g_strdup_printf("This XMU is already mounted on "
                                            "player %d slot %c\r\n",
                                            player_i + 1, 'A' + peripheral_i);
                        xemu_queue_notification(buf);
                        g_free(buf);
                        return false;
                    }
                }
            }
        }
    }

    xmu->filename = g_strdup(filename);

    const int xmu_map[2] = { 2, 3 };
    char *tmp;

    static int id_counter = 0;
    tmp = g_strdup_printf("xmu_%d", id_counter++);

    // Add the file as a drive
    QDict *qdict1 = qdict_new();
    qdict_put_str(qdict1, "id", tmp);
    qdict_put_str(qdict1, "format", "raw");
    qdict_put_str(qdict1, "file", filename);

    QemuOpts *drvopts =
        qemu_opts_from_qdict(qemu_find_opts("drive"), qdict1, &error_abort);

    DriveInfo *dinfo = drive_new(drvopts, 0, &error_abort);
    assert(dinfo);

    // Create the usb-storage device
    QDict *qdict2 = qdict_new();

    // Specify device driver
    qdict_put_str(qdict2, "driver", "usb-storage");

    // Specify device identifier
    qdict_put_str(qdict2, "drive", tmp);
    g_free(tmp);

    // Specify index/port
    tmp = g_strdup_printf("1.%d.%d", port_map[player_index],
                          xmu_map[expansion_slot_index]);
    qdict_put_str(qdict2, "port", tmp);
    g_free(tmp);

    // Create the device
    QemuOpts *opts =
        qemu_opts_from_qdict(qemu_find_opts("device"), qdict2, &error_abort);

    DeviceState *dev = qdev_device_add(opts, &error_abort);
    assert(dev);

    xmu->dev = (void *)dev;

    // Unref for eventual cleanup
    qobject_unref(qdict1);
    qobject_unref(qdict2);

    if (!is_rebind) {
        xemu_save_peripheral_settings(player_index, expansion_slot_index,
                                      peripheral_type, xmu->filename);
    }

    return true;
}

void xemu_input_unbind_xmu(int player_index, int expansion_slot_index)
{
    assert(player_index >= 0 && player_index < 4);
    assert(expansion_slot_index >= 0 && expansion_slot_index < 2);

    ControllerState *state = bound_controllers[player_index];
    if (state->peripheral_types[expansion_slot_index] != PERIPHERAL_XMU)
        return;

    XmuState *xmu = (XmuState *)state->peripherals[expansion_slot_index];
    if (xmu != NULL) {
        if (xmu->dev != NULL) {
            qdev_unplug((DeviceState *)xmu->dev, &error_abort);
            object_unref(OBJECT(xmu->dev));
            xmu->dev = NULL;
        }

        g_free((void *)xmu->filename);
        xmu->filename = NULL;
    }
}

void xemu_input_rebind_xmu(int port)
{
    // Try to bind peripherals back to controller
    for (int i = 0; i < 2; i++) {
        enum peripheral_type peripheral_type =
            (enum peripheral_type)(*peripheral_types_settings_map[port][i]);

        // If peripheralType is out of range, change the settings for this
        // controller and peripheral port to default
        if (peripheral_type < PERIPHERAL_NONE ||
            peripheral_type >= PERIPHERAL_TYPE_COUNT) {
            xemu_save_peripheral_settings(port, i, PERIPHERAL_NONE, NULL);
            peripheral_type = PERIPHERAL_NONE;
        }

        const char *param = *peripheral_params_settings_map[port][i];

        if (peripheral_type == PERIPHERAL_XMU) {
            if (param != NULL && strlen(param) > 0) {
                // This is an XMU and needs to be bound to this controller
                if (qemu_access(param, R_OK | W_OK) == 0) {
                    bound_controllers[port]->peripheral_types[i] =
                        peripheral_type;
                    bound_controllers[port]->peripherals[i] =
                        g_malloc(sizeof(XmuState));
                    memset(bound_controllers[port]->peripherals[i], 0,
                           sizeof(XmuState));
                    bool did_bind = xemu_input_bind_xmu(port, i, param, true);
                    if (did_bind) {
                        char *buf =
                            g_strdup_printf("Connected XMU %s to port %d%c",
                                            param, port + 1, 'A' + i);
                        xemu_queue_notification(buf);
                        g_free(buf);
                    }
                } else {
                    char *buf =
                        g_strdup_printf("Unable to bind XMU at %s to port %d%c",
                                        param, port + 1, 'A' + i);
                    xemu_queue_error_message(buf);
                    g_free(buf);
                }
            }
        }
    }
}

void xemu_input_set_test_mode(int enabled)
{
    test_mode = enabled;
}

int xemu_input_get_test_mode(void)
{
    return test_mode;
}

void xemu_input_reset_input_mapping(ControllerState *state)
{
    if (state->type == INPUT_DEVICE_SDL_GAMEPAD) {
        char guid[35] = { 0 };
        SDL_GUIDToString(state->sdl_joystick_guid, guid, sizeof(guid));
        xemu_settings_reset_controller_mapping(guid);
    } else if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
        xemu_settings_reset_keyboard_mapping();
    }
}
