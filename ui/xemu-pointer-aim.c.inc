/*
 * xemu pointer/lightgun aim processing (platform independent)
 *
 * Shared between the per-platform mouse backends (xemu-rawinput.c on
 * Windows, xemu-evdev.c on Linux): custom button mapping, aim-to-stick
 * transform, adaptive 1-euro smoothing filter, sensitivity scaling and the
 * ONSCREEN test. Included as a .c.inc from inside each backend's platform
 * section so everything stays static to that translation unit.
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

static uint32_t fnv1a_hash(const char *s)
{
    uint32_t h = 0x811c9dc5;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 0x01000193;
    }
    return h;
}

// ---------------------------------------------------------------------------
// Custom button mapping (input.lightgun_mapping.* settings). Empty specs
// keep the default fixed mouse mapping, so this whole feature is inert
// unless configured.
// Spec formats: "mouse:<hash>:<left|right|middle|x1|x2>" (any detected
// mouse, not only the aiming one) or "key:<SDL key name>".

typedef enum {
    MAP_SRC_NONE = 0,
    MAP_SRC_MOUSE,
    MAP_SRC_MOUSE_KEY, // raw key/button code read from the pointer device
    MAP_SRC_KEY,
} MapSourceKind;

typedef struct MapSource {
    MapSourceKind kind;
    char mouse_guid[20]; // "mouse:xxxxxxxx"
    uint32_t mouse_button; // XEMU_RAWINPUT_BUTTON_*
    int key_code; // raw input-event code ("k<code>" specs)
    SDL_Scancode scancode;
} MapSource;

typedef struct MapSlot {
    const char **spec; // config string
    const char *parsed_from; // spec value the cache was built from
    uint32_t def_mouse_button; // default source (0 = none)
    int def_key_code; // default raw key code on the aiming device (0 = none)
    int def_key_code2; // alternate default raw key code (0 = none)
    uint32_t controller_button; // 0 when the target is an axis
    int axis; // CONTROLLER_AXIS_* target, -1 = none (analog triggers)
    MapSource src;
} MapSlot;

// Whether the pointer device behind `con` currently reports the raw
// key/button `key_code` as pressed. Lightguns often expose extra buttons
// as keyboard keys or joystick buttons that never reach SDL (X11 ignores
// joystick-class devices), so the backend reads them straight from the
// device. Windows has no such path and always returns false.
static bool pointer_backend_key_pressed(ControllerState *con, int key_code);

static void map_parse(MapSource *out, const char *spec)
{
    memset(out, 0, sizeof(*out));

    if (strncmp(spec, "mouse:", 6) == 0) {
        const char *btn = strchr(spec + 6, ':');
        if (btn == NULL || (size_t)(btn - spec) >= sizeof(out->mouse_guid)) {
            return;
        }
        memcpy(out->mouse_guid, spec, btn - spec);
        out->mouse_guid[btn - spec] = '\0';
        btn++;
        if (strcmp(btn, "left") == 0) {
            out->mouse_button = XEMU_RAWINPUT_BUTTON_LEFT;
        } else if (strcmp(btn, "right") == 0) {
            out->mouse_button = XEMU_RAWINPUT_BUTTON_RIGHT;
        } else if (strcmp(btn, "middle") == 0) {
            out->mouse_button = XEMU_RAWINPUT_BUTTON_MIDDLE;
        } else if (strcmp(btn, "x1") == 0) {
            out->mouse_button = XEMU_RAWINPUT_BUTTON_X1;
        } else if (strcmp(btn, "x2") == 0) {
            out->mouse_button = XEMU_RAWINPUT_BUTTON_X2;
        } else if (btn[0] == 'k' && btn[1] >= '0' && btn[1] <= '9') {
            // Raw input-event code on the device, e.g. "k103" = KEY_UP.
            // Lets lightgun extra buttons (D-pad, pump, BTN_1..) work even
            // when the windowing system never delivers them as keyboard.
            out->key_code = atoi(btn + 1);
            if (out->key_code > 0) {
                out->kind = MAP_SRC_MOUSE_KEY;
            }
            return;
        } else {
            return;
        }
        out->kind = MAP_SRC_MOUSE;
    } else if (strncmp(spec, "key:", 4) == 0) {
        out->scancode = SDL_GetScancodeFromName(spec + 4);
        if (out->scancode != SDL_SCANCODE_UNKNOWN) {
            out->kind = MAP_SRC_KEY;
        }
    }
}

static bool map_source_pressed(const MapSource *src, ControllerState *own)
{
    switch (src->kind) {
    case MAP_SRC_MOUSE: {
        if (strcmp(own->rawinput_guid, src->mouse_guid) == 0) {
            return (own->rawinput_buttons & src->mouse_button) != 0;
        }
        ControllerState *iter;
        QTAILQ_FOREACH(iter, &available_controllers, entry) {
            if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE &&
                strcmp(iter->rawinput_guid, src->mouse_guid) == 0) {
                return (iter->rawinput_buttons & src->mouse_button) != 0;
            }
        }
        return false;
    }
    case MAP_SRC_MOUSE_KEY: {
        if (strcmp(own->rawinput_guid, src->mouse_guid) == 0) {
            return pointer_backend_key_pressed(own, src->key_code);
        }
        ControllerState *iter;
        QTAILQ_FOREACH(iter, &available_controllers, entry) {
            if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE &&
                strcmp(iter->rawinput_guid, src->mouse_guid) == 0) {
                return pointer_backend_key_pressed(iter, src->key_code);
            }
        }
        return false;
    }
    case MAP_SRC_KEY: {
        const bool *kb = SDL_GetKeyboardState(NULL);
        return kb != NULL && kb[src->scancode];
    }
    default:
        return false;
    }
}

// Translate the device's raw button state (plus any custom [mapping]
// bindings) into ControllerState.buttons/axis. Assumes both were zeroed by
// the caller.
static void pointer_apply_mapping(ControllerState *state)
{
    // Button mapping: each function uses its custom [mapping] binding when
    // configured, otherwise the default mouse button, or the default raw
    // key on the aiming device (the D-pad of guns such as the Sinden shows
    // up as arrow keys on the gun's own event device).
    static MapSlot map_slots[] = {
        { &g_config.input.lightgun_mapping.trigger, NULL,
          XEMU_RAWINPUT_BUTTON_LEFT, 0, 0, CONTROLLER_BUTTON_A, -1 },
        { &g_config.input.lightgun_mapping.b, NULL,
          XEMU_RAWINPUT_BUTTON_RIGHT, 0, 0, CONTROLLER_BUTTON_B, -1 },
        { &g_config.input.lightgun_mapping.start, NULL,
          XEMU_RAWINPUT_BUTTON_MIDDLE, 0, 0, CONTROLLER_BUTTON_START, -1 },
        { &g_config.input.lightgun_mapping.back, NULL,
          XEMU_RAWINPUT_BUTTON_X1, 0, 0, CONTROLLER_BUTTON_BACK, -1 },
        { &g_config.input.lightgun_mapping.x, NULL,
          XEMU_RAWINPUT_BUTTON_X2, 0, 0, CONTROLLER_BUTTON_X, -1 },
        // D-pad defaults: arrow keys on the device, or the BTN_5..BTN_8
        // codes Batocera's Sinden driver emits on its virtual gun device
        { &g_config.input.lightgun_mapping.dpad_up, NULL, 0,
          103 /* KEY_UP */, 261 /* BTN_5 */, CONTROLLER_BUTTON_DPAD_UP, -1 },
        { &g_config.input.lightgun_mapping.dpad_down, NULL, 0,
          108 /* KEY_DOWN */, 262 /* BTN_6 */,
          CONTROLLER_BUTTON_DPAD_DOWN, -1 },
        { &g_config.input.lightgun_mapping.dpad_left, NULL, 0,
          105 /* KEY_LEFT */, 263 /* BTN_7 */,
          CONTROLLER_BUTTON_DPAD_LEFT, -1 },
        { &g_config.input.lightgun_mapping.dpad_right, NULL, 0,
          106 /* KEY_RIGHT */, 264 /* BTN_8 */,
          CONTROLLER_BUTTON_DPAD_RIGHT, -1 },
        { &g_config.input.lightgun_mapping.y, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_Y, -1 },
        { &g_config.input.lightgun_mapping.white, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_WHITE, -1 },
        { &g_config.input.lightgun_mapping.black, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_BLACK, -1 },
        { &g_config.input.lightgun_mapping.lstick, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_LSTICK, -1 },
        { &g_config.input.lightgun_mapping.rstick, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_RSTICK, -1 },
        { &g_config.input.lightgun_mapping.ltrig, NULL, 0, 0, 0, 0,
          CONTROLLER_AXIS_LTRIG },
        { &g_config.input.lightgun_mapping.rtrig, NULL, 0, 0, 0, 0,
          CONTROLLER_AXIS_RTRIG },
        { &g_config.input.lightgun_mapping.guide, NULL, 0, 0, 0,
          CONTROLLER_BUTTON_GUIDE, -1 },
    };

    uint32_t mb = state->rawinput_buttons;
    for (size_t i = 0; i < ARRAY_SIZE(map_slots); i++) {
        MapSlot *slot = &map_slots[i];
        const char *spec = *slot->spec;

        if (spec == NULL || spec[0] == '\0') {
            if ((slot->def_mouse_button && (mb & slot->def_mouse_button)) ||
                (slot->def_key_code &&
                 pointer_backend_key_pressed(state, slot->def_key_code)) ||
                (slot->def_key_code2 &&
                 pointer_backend_key_pressed(state, slot->def_key_code2))) {
                state->buttons |= slot->controller_button;
            }
            continue;
        }

        if (slot->parsed_from != spec) { // config string changed, re-parse
            map_parse(&slot->src, spec);
            slot->parsed_from = spec;
        }
        if (map_source_pressed(&slot->src, state)) {
            if (slot->axis >= 0) {
                state->axis[slot->axis] = 32767; // full trigger pull
            } else {
                state->buttons |= slot->controller_button;
            }
        }
    }
}

// Turn an aim position in window client pixels into the emulated lightgun
// stick axes + ONSCREEN bit, applying smoothing and sensitivity. The game
// display rect is used when available; otherwise the caller-provided
// fallback dimensions (whole window client area, <= 0 = unknown).
static void pointer_apply_aim(ControllerState *state, int px, int py,
                              int fallback_w, int fallback_h)
{
    int rx, ry, rw, rh;
    xemu_input_get_game_display_rect(&rx, &ry, &rw, &rh);
    if (rw <= 0 || rh <= 0) {
        // Game not rendering yet; fall back to the whole client area
        if (fallback_w <= 0 || fallback_h <= 0) {
            return;
        }
        rx = ry = 0;
        rw = fallback_w;
        rh = fallback_h;
    }

    float nx = 2.0f * (px - rx) / rw - 1.0f; // [-1,1], left -> right
    float ny = 1.0f - 2.0f * (py - ry) / rh; // [-1,1], bottom -> top

    // Smooth the aim to tame hand/camera jitter, using a "1-euro filter":
    // an adaptive low-pass whose cutoff rises with speed. While nearly
    // still it filters hard (rock-solid crosshair); on fast moves it opens
    // up completely, so there is no perceivable lag.
    float smoothing = g_config.input.lightgun_smoothing;
    if (smoothing > 0.0f) {
        smoothing = MIN(smoothing, 1.0f);
        uint64_t now = SDL_GetTicksNS();
        if (!state->rawinput_smooth_valid) {
            state->rawinput_smooth_nx = nx;
            state->rawinput_smooth_ny = ny;
            state->rawinput_smooth_dx = 0.0f;
            state->rawinput_smooth_dy = 0.0f;
            state->rawinput_smooth_valid = true;
        } else {
            float dt = (now - state->rawinput_smooth_ts) / 1e9f;
            dt = MIN(MAX(dt, 1e-4f), 0.1f);

            // Filtered velocity (1 Hz cutoff): jitter averages out to ~0,
            // real hand movement produces a sustained value
            const float two_pi = 6.28318531f;
            float ad = 1.0f / (1.0f + 1.0f / (two_pi * 1.0f * dt));
            float raw_dx = (nx - state->rawinput_smooth_nx) / dt;
            float raw_dy = (ny - state->rawinput_smooth_ny) / dt;
            state->rawinput_smooth_dx +=
                ad * (raw_dx - state->rawinput_smooth_dx);
            state->rawinput_smooth_dy +=
                ad * (raw_dy - state->rawinput_smooth_dy);

            // Cutoff frequency: low while still (strong smoothing, more so
            // at higher smoothing settings), raised in proportion to speed
            // so fast moves pass through unfiltered
            float min_cutoff = 6.0f - 5.5f * smoothing; // 6 Hz .. 0.5 Hz
            const float beta = 5.0f;
            float cx = min_cutoff + beta * fabsf(state->rawinput_smooth_dx);
            float cy = min_cutoff + beta * fabsf(state->rawinput_smooth_dy);
            float ax = 1.0f / (1.0f + 1.0f / (two_pi * cx * dt));
            float ay = 1.0f / (1.0f + 1.0f / (two_pi * cy * dt));
            state->rawinput_smooth_nx +=
                ax * (nx - state->rawinput_smooth_nx);
            state->rawinput_smooth_ny +=
                ay * (ny - state->rawinput_smooth_ny);
        }
        state->rawinput_smooth_ts = now;
        nx = state->rawinput_smooth_nx;
        ny = state->rawinput_smooth_ny;
    } else {
        state->rawinput_smooth_valid = false;
    }

    // Sensitivity scales the aim range around the screen center
    float sensitivity = g_config.input.lightgun_sensitivity;
    if (sensitivity > 0.0f) {
        nx *= sensitivity;
        ny *= sensitivity;
    }

    if (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f) {
        state->buttons |= CONTROLLER_BUTTON_LIGHTGUN_ONSCREEN;
    }

    nx = MIN(MAX(nx, -1.0f), 1.0f);
    ny = MIN(MAX(ny, -1.0f), 1.0f);
    state->axis[CONTROLLER_AXIS_LSTICK_X] = (int16_t)(nx * 32767.0f);
    state->axis[CONTROLLER_AXIS_LSTICK_Y] = (int16_t)(ny * 32767.0f);
}
