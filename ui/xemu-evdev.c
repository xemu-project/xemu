/*
 * xemu evdev mouse/lightgun support (Linux)
 *
 * Enumerates pointer devices individually through the Linux evdev interface
 * so that multiple pointing devices (e.g. Sinden Lightguns, which appear as
 * absolute mice) can be told apart and bound to different controller ports.
 *
 * This is the Linux counterpart of the Windows Raw Input backend
 * (xemu-rawinput.c): it implements the same xemu_rawinput_* entry points
 * and reuses the INPUT_DEVICE_RAWINPUT_MOUSE controller type, so the rest
 * of the input stack, the settings UI and the emulated lightgun device all
 * work unchanged.
 *
 * Frontends such as Batocera/EmulationStation decide which gun belongs to
 * which player: their launcher writes the device path into xemu.toml, e.g.
 *
 *   [input.bindings]
 *   port1 = 'evdev:/dev/input/event5'
 *   port1_driver = 'lightgun'
 *
 * (see the "evdev:" alias matching in
 * xemu_input_get_controller_default_bind_port). Manual desktop use keeps
 * working too: devices get a stable "mouse:xxxxxxxx" pseudo-GUID hashed
 * from their physical topology path, exactly like on Windows.
 *
 * Note: reading /dev/input/event* requires root or membership in the
 * "input" group. Batocera runs emulators as root, so this only matters on
 * regular desktop distributions.
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

#include "qemu/osdep.h"

#include "xemu-rawinput.h"
#include "xemu-input.h"
#include "xemu-notifications.h"
#include "xemu-settings.h"

#ifdef __linux__

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/input.h>
#include <SDL3/SDL.h>

// #define DEBUG_EVDEV
#ifdef DEBUG_EVDEV
#define DPRINTF(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

static SDL_Window *g_window;
static int g_inotify_fd = -1;
static bool g_initialized;

// Shared aim/mapping logic (also used by the Windows Raw Input backend)
#include "xemu-pointer-aim.c.inc"

#define EV_BITS_PER_LONG (sizeof(unsigned long) * 8)
#define EV_NLONGS(x) (((x) + EV_BITS_PER_LONG - 1) / EV_BITS_PER_LONG)

static bool ev_test_bit(unsigned int bit, const unsigned long *bits)
{
    return (bits[bit / EV_BITS_PER_LONG] >>
            (bit % EV_BITS_PER_LONG)) & 1;
}

// Per-device data, hung off ControllerState.rawinput_handle
typedef struct EvdevMouse {
    int fd;
    bool has_abs; // absolute pointer (lightgun/touch); else relative mouse
    struct input_absinfo abs_x, abs_y;
    int32_t raw_x, raw_y; // last absolute position, device units
    float rel_x, rel_y;   // accumulated position for relative mice, px
    // Full key/button state of the device. Guns expose their extra
    // buttons as keyboard keys (Sinden D-pad = arrow keys) or low BTN_*
    // codes that X11 never delivers to SDL, so the mapping reads them
    // straight from here ("k<code>" specs and D-pad defaults).
    unsigned long key_state[EV_NLONGS(KEY_MAX + 1)];
} EvdevMouse;

static bool pointer_backend_key_pressed(ControllerState *con, int key_code)
{
    EvdevMouse *em = con->rawinput_handle;
    if (em == NULL || key_code <= 0 || key_code > KEY_MAX) {
        return false;
    }
    return ev_test_bit(key_code, em->key_state);
}

static ControllerState *evdev_find_controller(const char *devnode)
{
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE &&
            iter->rawinput_path != NULL &&
            strcmp(iter->rawinput_path, devnode) == 0) {
            return iter;
        }
    }
    return NULL;
}

static void evdev_add_device(const char *devnode)
{
    if (evdev_find_controller(devnode)) {
        return;
    }

    int fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        // EACCES right after hotplug is normal: udev has not fixed the
        // permissions yet. The IN_ATTRIB watch retries when it does.
        DPRINTF("evdev: open %s failed: %s\n", devnode, strerror(errno));
        return;
    }

    // Keep only pointer devices: a left button plus absolute or relative
    // X/Y axes. This skips keyboards, gamepads (BTN_SOUTH etc.) and the
    // various sensor/control nodes.
    unsigned long ev_bits[EV_NLONGS(EV_MAX + 1)] = { 0 };
    unsigned long key_bits[EV_NLONGS(KEY_MAX + 1)] = { 0 };
    unsigned long abs_bits[EV_NLONGS(ABS_MAX + 1)] = { 0 };
    unsigned long rel_bits[EV_NLONGS(REL_MAX + 1)] = { 0 };

    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0 ||
        !ev_test_bit(EV_KEY, ev_bits) ||
        ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0 ||
        !ev_test_bit(BTN_LEFT, key_bits)) {
        close(fd);
        return;
    }

    bool has_abs = false;
    struct input_absinfo ax = { 0 }, ay = { 0 };
    if (ev_test_bit(EV_ABS, ev_bits) &&
        ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) >= 0 &&
        ev_test_bit(ABS_X, abs_bits) && ev_test_bit(ABS_Y, abs_bits) &&
        ioctl(fd, EVIOCGABS(ABS_X), &ax) >= 0 &&
        ioctl(fd, EVIOCGABS(ABS_Y), &ay) >= 0 &&
        ax.maximum > ax.minimum && ay.maximum > ay.minimum) {
        has_abs = true; // Sinden & friends: absolute coordinates
    } else {
        if (!ev_test_bit(EV_REL, ev_bits) ||
            ioctl(fd, EVIOCGBIT(EV_REL, sizeof(rel_bits)), rel_bits) < 0 ||
            !ev_test_bit(REL_X, rel_bits) || !ev_test_bit(REL_Y, rel_bits)) {
            close(fd);
            return;
        }
    }

    char product[128] = "";
    if (ioctl(fd, EVIOCGNAME(sizeof(product)), product) < 0 ||
        product[0] == '\0') {
        snprintf(product, sizeof(product), "evdev Mouse");
    }

    // Stable identity for the settings pseudo-GUID. The physical topology
    // path (usb-.../inputN) tells two identical guns apart as long as each
    // stays on its USB port, mirroring the Windows device-path hash.
    char phys[128] = "";
    char uniq[128] = "";
    ioctl(fd, EVIOCGPHYS(sizeof(phys)), phys);
    ioctl(fd, EVIOCGUNIQ(sizeof(uniq)), uniq);
    const char *stable = phys[0] ? phys : (uniq[0] ? uniq : product);

    // Disambiguate devices with identical product strings
    int same_name = 0;
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE &&
            strncmp(iter->name, product, strlen(product)) == 0) {
            same_name++;
        }
    }
    char *name;
    if (same_name > 0) {
        name = g_strdup_printf("%s #%d", product, same_name + 1);
    } else {
        name = g_strdup(product);
    }

    EvdevMouse *em = g_new0(EvdevMouse, 1);
    em->fd = fd;
    em->has_abs = has_abs;
    em->abs_x = ax;
    em->abs_y = ay;
    em->raw_x = ax.value; // current position, so the aim is valid at once
    em->raw_y = ay.value;
    if (!has_abs && g_window != NULL) {
        // Start relative mice at the window center instead of the corner
        int win_w = 0, win_h = 0;
        if (SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h)) {
            em->rel_x = win_w / 2.0f;
            em->rel_y = win_h / 2.0f;
        }
    }

    ControllerState *new_con = g_new0(ControllerState, 1);
    new_con->type = INPUT_DEVICE_RAWINPUT_MOUSE;
    new_con->name = name;
    new_con->rawinput_handle = em;
    new_con->rawinput_path = g_strdup(devnode);
    new_con->rawinput_has_abs = has_abs;
    snprintf(new_con->rawinput_guid, sizeof(new_con->rawinput_guid),
             "mouse:%08x", fnv1a_hash(stable));
    new_con->bound = -1;
    new_con->peripheral_types[0] = PERIPHERAL_NONE;
    new_con->peripheral_types[1] = PERIPHERAL_NONE;

    QTAILQ_INSERT_TAIL(&available_controllers, new_con, entry);
    DPRINTF("evdev: added '%s' (%s) as %s\n", new_con->name, devnode,
            new_con->rawinput_guid);

    // Re-bind to a previously saved port (or to the port a frontend such
    // as Batocera assigned via "evdev:<path>"). Unlike gamepads, never
    // auto-bind a mouse to a free port: every system has at least one
    // regular mouse and grabbing a controller port with it would be
    // surprising.
    int port = 0;
    while (1) {
        port = xemu_input_get_controller_default_bind_port(new_con, port);
        if (port < 0) {
            break;
        }
        if (!xemu_input_get_bound(port)) {
            xemu_input_bind(port, new_con, 0);
            char buf[128];
            snprintf(buf, sizeof(buf), "Connected '%s' to port %d",
                     new_con->name, port + 1);
            xemu_queue_notification(buf);
            break;
        }
        port++;
    }
}

static void evdev_remove_device(const char *devnode)
{
    ControllerState *con = evdev_find_controller(devnode);
    if (con == NULL) {
        return;
    }

    DPRINTF("evdev: removed '%s'\n", con->name);

    if (con->bound >= 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Port %d disconnected", con->bound + 1);
        xemu_queue_notification(buf);

        // Unbind, but don't save the unbinding so the device is bound to
        // the same port again when reconnected
        xemu_input_bind(con->bound, NULL, 0);
    }

    QTAILQ_REMOVE(&available_controllers, con, entry);
    EvdevMouse *em = con->rawinput_handle;
    if (em != NULL) {
        if (em->fd >= 0) {
            close(em->fd);
        }
        g_free(em);
    }
    g_free(con->rawinput_path);
    g_free((char *)con->name);
    g_free(con);
}

static void evdev_drain_device(ControllerState *con)
{
    EvdevMouse *em = con->rawinput_handle;
    struct input_event ev[32];

    for (;;) {
        ssize_t n = read(em->fd, ev, sizeof(ev));
        if (n < (ssize_t)sizeof(ev[0])) {
            // EAGAIN = queue drained; ENODEV = device unplugged, the
            // inotify IN_DELETE event takes care of removing it
            break;
        }
        for (size_t i = 0; i < n / sizeof(ev[0]); i++) {
            const struct input_event *e = &ev[i];
            switch (e->type) {
            case EV_KEY: {
                if (e->code <= KEY_MAX) {
                    unsigned long *w =
                        &em->key_state[e->code / EV_BITS_PER_LONG];
                    unsigned long m = 1UL << (e->code % EV_BITS_PER_LONG);
                    if (e->value) {
                        *w |= m;
                    } else {
                        *w &= ~m;
                    }
                }
                uint32_t bit = 0;
                switch (e->code) {
                case BTN_LEFT:   bit = XEMU_RAWINPUT_BUTTON_LEFT;   break;
                case BTN_RIGHT:  bit = XEMU_RAWINPUT_BUTTON_RIGHT;  break;
                case BTN_MIDDLE: bit = XEMU_RAWINPUT_BUTTON_MIDDLE; break;
                case BTN_SIDE:   bit = XEMU_RAWINPUT_BUTTON_X1;     break;
                case BTN_EXTRA:  bit = XEMU_RAWINPUT_BUTTON_X2;     break;
                }
                if (bit != 0) {
                    if (e->value) {
                        con->rawinput_buttons |= bit;
                    } else {
                        con->rawinput_buttons &= ~bit;
                    }
                }
                break;
            }
            case EV_ABS:
                if (e->code == ABS_X) {
                    em->raw_x = e->value;
                } else if (e->code == ABS_Y) {
                    em->raw_y = e->value;
                }
                break;
            case EV_REL:
                if (e->code == REL_X) {
                    em->rel_x += e->value;
                } else if (e->code == REL_Y) {
                    em->rel_y += e->value;
                }
                break;
            }
        }
    }
}

void xemu_rawinput_process_pending(void)
{
    if (!g_initialized) {
        return;
    }

    // Hotplug: watch /dev/input for created/removed event nodes. IN_ATTRIB
    // retries devices whose permissions udev had not set yet at IN_CREATE.
    if (g_inotify_fd >= 0) {
        char buf[4096]
            __attribute__((aligned(__alignof__(struct inotify_event))));
        for (;;) {
            ssize_t len = read(g_inotify_fd, buf, sizeof(buf));
            if (len <= 0) {
                break;
            }
            for (char *p = buf; p < buf + len;) {
                const struct inotify_event *ie =
                    (const struct inotify_event *)p;
                if (ie->len > 0 &&
                    strncmp(ie->name, "event", 5) == 0) {
                    char devnode[NAME_MAX + 16];
                    snprintf(devnode, sizeof(devnode), "/dev/input/%s",
                             ie->name);
                    if (ie->mask & IN_DELETE) {
                        evdev_remove_device(devnode);
                    } else if (ie->mask & (IN_CREATE | IN_ATTRIB)) {
                        evdev_add_device(devnode);
                    }
                }
                p += sizeof(struct inotify_event) + ie->len;
            }
        }
    }

    // Drain queued input events of every tracked mouse
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE) {
            evdev_drain_device(iter);
        }
    }
}

void xemu_rawinput_init(SDL_Window *window)
{
    assert(!g_initialized);

    g_window = window;

    DIR *dir = opendir("/dev/input");
    if (dir == NULL) {
        fprintf(stderr, "evdev: cannot open /dev/input: %s\n",
                strerror(errno));
        return;
    }
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strncmp(de->d_name, "event", 5) == 0) {
            char devnode[NAME_MAX + 16];
            snprintf(devnode, sizeof(devnode), "/dev/input/%s", de->d_name);
            evdev_add_device(devnode);
        }
    }
    closedir(dir);

    g_inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (g_inotify_fd >= 0) {
        inotify_add_watch(g_inotify_fd, "/dev/input",
                          IN_CREATE | IN_DELETE | IN_ATTRIB);
    }

    g_initialized = true;
}

void xemu_rawinput_update_controller_state(ControllerState *state)
{
    state->buttons = 0;
    memset(state->axis, 0, sizeof(state->axis));

    pointer_apply_mapping(state);

    EvdevMouse *em = state->rawinput_handle;
    if (em == NULL || g_window == NULL) {
        return;
    }

    int win_w = 0, win_h = 0;
    if (!SDL_GetWindowSizeInPixels(g_window, &win_w, &win_h) ||
        win_w <= 0 || win_h <= 0) {
        return;
    }

    // Aim position in window pixels. Absolute devices (lightguns) span
    // their full range over the window; in fullscreen (the Batocera case)
    // window == screen, matching the gun's screen calibration. Relative
    // mice steer a private accumulated position.
    int px, py;
    if (em->has_abs) {
        px = (int)((int64_t)(em->raw_x - em->abs_x.minimum) * win_w /
                   (em->abs_x.maximum - em->abs_x.minimum));
        py = (int)((int64_t)(em->raw_y - em->abs_y.minimum) * win_h /
                   (em->abs_y.maximum - em->abs_y.minimum));
    } else {
        em->rel_x = MIN(MAX(em->rel_x, 0.0f), (float)(win_w - 1));
        em->rel_y = MIN(MAX(em->rel_y, 0.0f), (float)(win_h - 1));
        px = (int)em->rel_x;
        py = (int)em->rel_y;
    }
    state->rawinput_client_x = px;
    state->rawinput_client_y = py;

    pointer_apply_aim(state, px, py, win_w, win_h);
}

#endif // __linux__
