/*
 * xemu Input Management
 *
 * Copyright (C) 2020 Matt Borgerson
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

#include "xemu-input.h"
#include "xemu-notifications.h"
#include "xemu-settings.h"

#include "qemu/osdep.h"
#include "qemu-common.h"
#include "hw/qdev-core.h"
#include "hw/qdev-properties.h"
#include "qapi/error.h"
#include "monitor/qdev.h"
#include "qapi/qmp/qdict.h"
#include "qemu/option.h"
#include "qemu/config-file.h"

#if 0
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

int num_available_controllers;
struct controller_state *available_controllers;
struct controller_state *bound_controllers[4] = { NULL, NULL, NULL, NULL };
int test_mode;

static const enum xemu_settings_keys port_index_to_settings_key_map[] = {
    XEMU_SETTINGS_INPUT_CONTROLLER_1_GUID,
    XEMU_SETTINGS_INPUT_CONTROLLER_2_GUID,
    XEMU_SETTINGS_INPUT_CONTROLLER_3_GUID,
    XEMU_SETTINGS_INPUT_CONTROLLER_4_GUID,
};

void xemu_input_init(void)
{
    if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0) {
        fprintf(stderr, "Failed to initialize SDL gamecontroller subsystem\n");
        exit(1);
    }

    if (SDL_Init(SDL_INIT_HAPTIC) < 0) {
        fprintf(stderr, "Failed to initialize SDL haptic subsystem\n");
        exit(1);
    }

    // Create the keyboard input (always first)
    struct controller_state *new_con = malloc(sizeof(struct controller_state));
    memset(new_con, 0, sizeof(struct controller_state));
    new_con->type = INPUT_DEVICE_SDL_KEYBOARD;
    new_con->name = "Keyboard";
    new_con->bound = -1;

    // Check to see if we should auto-bind the keyboard
    int port = xemu_input_get_controller_default_bind_port(new_con, 0);
    if (port >= 0) {
        xemu_input_bind(port, new_con, 0);
        char buf[128];
        snprintf(buf, sizeof(buf), "Connected '%s' to port %d", new_con->name, port+1);
        xemu_queue_notification(buf);            
    }

    available_controllers = new_con;
    num_available_controllers = 1;
}

int xemu_input_get_controller_default_bind_port(struct controller_state *state, int start)
{
    char guid[35] = { 0 };
    if (state->type == INPUT_DEVICE_SDL_GAMECONTROLLER) {
        SDL_JoystickGetGUIDString(state->sdl_joystick_guid, guid, sizeof(guid));
    } else if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
        snprintf(guid, sizeof(guid), "keyboard");
    }

    for (int i = start; i < 4; i++) {
        const char *this_port;
        xemu_settings_get_string(port_index_to_settings_key_map[i], &this_port);
        if (strcmp(guid, this_port) == 0) {
            return i;
        }
    }

    return -1;
}

void xemu_input_process_sdl_events(const SDL_Event *event)
{
    if (event->type == SDL_CONTROLLERDEVICEADDED) {
        DPRINTF("Controller Added: %d\n", event->cdevice.which);

        // Attempt to open the added controller
        SDL_GameController *sdl_con;
        sdl_con = SDL_GameControllerOpen(event->cdevice.which);
        if (sdl_con == NULL) {
            DPRINTF("Could not open joystick %d as a game controller\n", event->cdevice.which);
            return;
        }

        // Success! Create a new node to track this controller and continue init
        struct controller_state *new_con = malloc(sizeof(struct controller_state));
        memset(new_con, 0, sizeof(struct controller_state));
        new_con->type                 = INPUT_DEVICE_SDL_GAMECONTROLLER;
        new_con->name                 = SDL_GameControllerName(sdl_con);
        new_con->sdl_gamecontroller   = sdl_con;
        new_con->sdl_joystick         = SDL_GameControllerGetJoystick(new_con->sdl_gamecontroller);
        new_con->sdl_joystick_id      = SDL_JoystickInstanceID(new_con->sdl_joystick);
        new_con->sdl_joystick_guid    = SDL_JoystickGetGUID(new_con->sdl_joystick);
        new_con->sdl_haptic           = SDL_HapticOpenFromJoystick(new_con->sdl_joystick);
        new_con->sdl_haptic_effect_id = -1;
        new_con->bound                = -1;
        
        char guid_buf[35] = { 0 };
        SDL_JoystickGetGUIDString(new_con->sdl_joystick_guid, guid_buf, sizeof(guid_buf));
        DPRINTF("Opened %s (%s)\n", new_con->name, guid_buf);
        
        // Add to the list of controllers
        if (available_controllers == NULL) {
            available_controllers = new_con;
        } else {
            struct controller_state *iter = available_controllers;
            while (iter->next != NULL) iter = iter->next;
            iter->next = new_con;
        }
        num_available_controllers++;

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
        int port = 0;
        while (1) {
            port = xemu_input_get_controller_default_bind_port(new_con, port);
            if (port < 0) {
                // No (additional) default mappings
                break;
            }
            if (xemu_input_get_bound(port) != NULL) {
                // Something already bound here, try again for another port
                port++;
                continue;
            }
            xemu_input_bind(port, new_con, 0);
            char buf[128];
            snprintf(buf, sizeof(buf), "Connected '%s' to port %d", new_con->name, port+1);
            xemu_queue_notification(buf);
            break;
        }

    } else if (event->type == SDL_CONTROLLERDEVICEREMOVED) {
        DPRINTF("Controller Removed: %d\n", event->cdevice.which);
        int handled = 0;
        struct controller_state *iter, *prev;
        for (iter=available_controllers, prev=NULL; iter != NULL; prev = iter, iter = iter->next) {
            if (iter->type != INPUT_DEVICE_SDL_GAMECONTROLLER) continue;
            
            if (iter->sdl_joystick_id == event->cdevice.which) {
                DPRINTF("Device removed: %s\n", iter->name);

                // Disconnect
                if (iter->bound >= 0) {
                    // Queue a notification to inform user controller disconnected
                    // FIXME: Probably replace with a callback registration thing,
                    // but this works well enough for now.
                    char buf[128];
                    snprintf(buf, sizeof(buf), "Port %d disconnected", iter->bound+1);
                    xemu_queue_notification(buf);

                    // Unbind the controller, but don't save the unbinding in
                    // case the controller is reconnected
                    xemu_input_bind(iter->bound, NULL, 0);
                }

                // Unlink
                if (prev) {
                    prev->next = iter->next;
                } else {
                    available_controllers = iter->next;
                }
                num_available_controllers--;

                // Deallocate
                // FIXME: Check to release any SDL handles, etc
                free(iter);

                handled = 1;
                break;
            }
        }
        if (!handled) {
            DPRINTF("Could not find handle for joystick instance\n");
        }
    } else if (event->type == SDL_CONTROLLERDEVICEREMAPPED) {
        DPRINTF("Controller Remapped: %d\n", event->cdevice.which);
    }
}

void xemu_input_update_controllers(void)
{
    struct controller_state *iter;
    for (iter=available_controllers; iter != NULL; iter=iter->next) {
        if (iter->type == INPUT_DEVICE_SDL_KEYBOARD) {
            xemu_input_update_sdl_kbd_controller_state(iter);
        } else if (iter->type == INPUT_DEVICE_SDL_GAMECONTROLLER) {
            xemu_input_update_sdl_controller_state(iter);
            xemu_input_update_rumble(iter);
        }
    }
}

void xemu_input_update_sdl_kbd_controller_state(struct controller_state *state)
{
    state->buttons = 0;
    memset(state->axis, 0, sizeof(state->axis));

    const uint8_t *kbd = SDL_GetKeyboardState(NULL);
    const int sdl_kbd_button_map[15] = {
        SDL_SCANCODE_A,
        SDL_SCANCODE_B,
        SDL_SCANCODE_X,
        SDL_SCANCODE_Y,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_UP,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_BACKSPACE,
        SDL_SCANCODE_RETURN,
        SDL_SCANCODE_1,
        SDL_SCANCODE_2,
        SDL_SCANCODE_3,
        SDL_SCANCODE_4,
        SDL_SCANCODE_5
    };

    for (int i = 0; i < 15; i++) {
        state->buttons |= kbd[sdl_kbd_button_map[i]] << i;
    }

    /*
    W = LTrig
       E
    S     F
       D
    */
    if (kbd[SDL_SCANCODE_E]) state->axis[CONTROLLER_AXIS_LSTICK_Y] = 32767;
    if (kbd[SDL_SCANCODE_S]) state->axis[CONTROLLER_AXIS_LSTICK_X] = -32768;
    if (kbd[SDL_SCANCODE_F]) state->axis[CONTROLLER_AXIS_LSTICK_X] = 32767;
    if (kbd[SDL_SCANCODE_D]) state->axis[CONTROLLER_AXIS_LSTICK_Y] = -32768;
    if (kbd[SDL_SCANCODE_W]) state->axis[CONTROLLER_AXIS_LTRIG] = 32767;

    /*
          O = RTrig
       I
    J     L
       K
    */
    if (kbd[SDL_SCANCODE_I]) state->axis[CONTROLLER_AXIS_RSTICK_Y] = 32767;
    if (kbd[SDL_SCANCODE_J]) state->axis[CONTROLLER_AXIS_RSTICK_X] = -32768;
    if (kbd[SDL_SCANCODE_L]) state->axis[CONTROLLER_AXIS_RSTICK_X] = 32767;
    if (kbd[SDL_SCANCODE_K]) state->axis[CONTROLLER_AXIS_RSTICK_Y] = -32768;
    if (kbd[SDL_SCANCODE_O]) state->axis[CONTROLLER_AXIS_RTRIG] = 32767;
}

void xemu_input_update_sdl_controller_state(struct controller_state *state)
{
    state->buttons = 0;
    memset(state->axis, 0, sizeof(state->axis));

    const SDL_GameControllerButton sdl_button_map[15] = {
        SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,
        SDL_CONTROLLER_BUTTON_X,
        SDL_CONTROLLER_BUTTON_Y,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT,
        SDL_CONTROLLER_BUTTON_DPAD_UP,
        SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_BACK,
        SDL_CONTROLLER_BUTTON_START,
        SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
        SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
        SDL_CONTROLLER_BUTTON_LEFTSTICK,
        SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        SDL_CONTROLLER_BUTTON_GUIDE
    };

    for (int i = 0; i < 15; i++) {
        state->buttons |= SDL_GameControllerGetButton(state->sdl_gamecontroller, sdl_button_map[i]) << i;
    }

    const SDL_GameControllerAxis sdl_axis_map[6] = {
        SDL_CONTROLLER_AXIS_TRIGGERLEFT,
        SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
        SDL_CONTROLLER_AXIS_LEFTX,
        SDL_CONTROLLER_AXIS_LEFTY,
        SDL_CONTROLLER_AXIS_RIGHTX,
        SDL_CONTROLLER_AXIS_RIGHTY,
    };

    for (int i = 0; i < 6; i++) {
        state->axis[i] = SDL_GameControllerGetAxis(state->sdl_gamecontroller, sdl_axis_map[i]);
    }

    // FIXME: Check range
    state->axis[CONTROLLER_AXIS_LSTICK_Y] = -1 - state->axis[CONTROLLER_AXIS_LSTICK_Y];
    state->axis[CONTROLLER_AXIS_RSTICK_Y] = -1 - state->axis[CONTROLLER_AXIS_RSTICK_Y];
}

void xemu_input_update_rumble(struct controller_state *state)
{
    if (state->sdl_haptic == NULL) {
        // Haptic not supported for this joystick
        return;
    }

    memset(&state->sdl_haptic_effect, 0, sizeof(state->sdl_haptic_effect));
    state->sdl_haptic_effect.type = SDL_HAPTIC_LEFTRIGHT;
    state->sdl_haptic_effect.periodic.length = 16;
    state->sdl_haptic_effect.leftright.large_magnitude = state->rumble_l;
    state->sdl_haptic_effect.leftright.small_magnitude = state->rumble_r;
    if (state->sdl_haptic_effect_id == -1) {
        state->sdl_haptic_effect_id = SDL_HapticNewEffect(state->sdl_haptic, &state->sdl_haptic_effect);
        SDL_HapticRunEffect(state->sdl_haptic, state->sdl_haptic_effect_id, 1);
    } else {
        SDL_HapticUpdateEffect(state->sdl_haptic, state->sdl_haptic_effect_id, &state->sdl_haptic_effect);
    }
}

struct controller_state *xemu_input_get_bound(int index)
{
    return bound_controllers[index];
}

void xemu_input_bind(int index, struct controller_state *state, int save)
{
    // FIXME: Attempt to disable rumble when unbinding so it's not left
    // in rumble mode

    // Unbind existing controller
    if (bound_controllers[index]) {
        assert(bound_controllers[index]->device != NULL);
        Error *err = NULL;
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
            if (state->type == INPUT_DEVICE_SDL_GAMECONTROLLER) {
                SDL_JoystickGetGUIDString(state->sdl_joystick_guid, guid_buf, sizeof(guid_buf));
            } else if (state->type == INPUT_DEVICE_SDL_KEYBOARD) {
                snprintf(guid_buf, sizeof(guid_buf), "keyboard");
            }
        }
        xemu_settings_set_string(port_index_to_settings_key_map[index], guid_buf);
        xemu_settings_save();
    }

    // Bind new controller
    if (state) {
        if (state->bound >= 0) {
            // Device was already bound to another port. Unbind it.
            xemu_input_bind(state->bound, NULL, 1);
        }

        bound_controllers[index] = state;
        bound_controllers[index]->bound = index;

        const int port_map[4] = {3, 4, 1, 2};

        QDict *qdict = qdict_new();

        // Specify device driver
        qdict_put_str(qdict, "driver", "usb-xbox-gamepad");

        // Specify device identifier
        char *tmp = g_strdup_printf("gamepad_%d", index+1);
        qdict_put_str(qdict, "id", tmp);
        g_free(tmp);

        // Specify index/port
        qdict_put_int(qdict, "index", index);
        qdict_put_int(qdict, "port", port_map[index]);

        // Create the device
        QemuOpts *opts = qemu_opts_from_qdict(qemu_find_opts("device"), qdict, &error_abort);
        DeviceState *dev = qdev_device_add(opts, &error_abort);
        assert(dev);

        // Unref for eventual cleanup
        qobject_unref(qdict);
        object_unref(OBJECT(dev));

        state->device = dev;
    }
}

#if 0
static void xemu_input_print_controller_state(struct controller_state *state)
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


void xemu_input_set_test_mode(int enabled)
{
    test_mode = enabled;
}

int xemu_input_get_test_mode(void)
{
    return test_mode;
}
