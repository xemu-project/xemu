/*
 * xemu Raw Input mouse support
 *
 * Enumerates HID mice individually via the Windows Raw Input API so that
 * multiple pointer devices (e.g. Sinden Lightguns, which appear as absolute
 * mice) can be told apart and bound to different controller ports.
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

#ifdef _WIN32

#include <math.h>
#include <windows.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_system.h>

// #define DEBUG_RAWINPUT
#ifdef DEBUG_RAWINPUT
#define DPRINTF(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

// HidD_GetProductString, loaded dynamically to avoid a hid.dll link dep
typedef BOOLEAN(WINAPI *HidD_GetProductString_t)(HANDLE, PVOID, ULONG);
static HidD_GetProductString_t p_HidD_GetProductString;

static HWND g_hwnd;
static bool g_initialized;

// Shared aim/mapping logic (also used by the Linux evdev backend)
#include "xemu-pointer-aim.c.inc"

static bool pointer_backend_key_pressed(ControllerState *con, int key_code)
{
    // Raw Input receives no keyboard events from pointer devices; gun
    // extra buttons arrive as regular mouse buttons on Windows.
    (void)con;
    (void)key_code;
    return false;
}

static ControllerState *rawinput_find_controller(HANDLE hdev)
{
    ControllerState *iter;
    QTAILQ_FOREACH(iter, &available_controllers, entry) {
        if (iter->type == INPUT_DEVICE_RAWINPUT_MOUSE &&
            iter->rawinput_handle == (void *)hdev) {
            return iter;
        }
    }
    return NULL;
}

static char *rawinput_get_device_path(HANDLE hdev)
{
    UINT size = 0;
    if (GetRawInputDeviceInfoW(hdev, RIDI_DEVICENAME, NULL, &size) != 0 ||
        size == 0) {
        return NULL;
    }
    WCHAR *wpath = g_malloc((size + 1) * sizeof(WCHAR));
    if (GetRawInputDeviceInfoW(hdev, RIDI_DEVICENAME, wpath, &size) ==
        (UINT)-1) {
        g_free(wpath);
        return NULL;
    }
    wpath[size] = 0;
    char *path = g_utf16_to_utf8((const gunichar2 *)wpath, -1, NULL, NULL,
                                 NULL);
    g_free(wpath);
    return path;
}

static char *rawinput_get_product_name(const char *path)
{
    char *name = NULL;

    if (p_HidD_GetProductString) {
        // path is UTF-8; go through UTF-16 so non-ASCII paths work
        wchar_t *wpath =
            (wchar_t *)g_utf8_to_utf16(path, -1, NULL, NULL, NULL);
        if (wpath == NULL) {
            return g_strdup("HID Mouse");
        }
        HANDLE h = CreateFileW(wpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
        g_free(wpath);
        if (h != INVALID_HANDLE_VALUE) {
            WCHAR product[128] = { 0 };
            if (p_HidD_GetProductString(h, product, sizeof(product) - 2)) {
                if (product[0]) {
                    name = g_utf16_to_utf8((const gunichar2 *)product, -1,
                                           NULL, NULL, NULL);
                }
            }
            CloseHandle(h);
        }
    }

    if (name == NULL || name[0] == '\0') {
        g_free(name);
        name = g_strdup("HID Mouse");
    }
    return name;
}

static void rawinput_add_device(HANDLE hdev)
{
    RID_DEVICE_INFO info = { .cbSize = sizeof(info) };
    UINT info_size = sizeof(info);

    if (rawinput_find_controller(hdev)) {
        return;
    }
    if (GetRawInputDeviceInfoW(hdev, RIDI_DEVICEINFO, &info, &info_size) ==
            (UINT)-1 ||
        info.dwType != RIM_TYPEMOUSE) {
        return;
    }

    char *path = rawinput_get_device_path(hdev);
    if (path == NULL) {
        return;
    }

    // Skip the terminal services virtual mouse
    if (strstr(path, "RDP_MOU") != NULL) {
        g_free(path);
        return;
    }

    char *product = rawinput_get_product_name(path);

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
        g_free(product);
    } else {
        name = product;
    }

    ControllerState *new_con = g_new0(ControllerState, 1);
    new_con->type = INPUT_DEVICE_RAWINPUT_MOUSE;
    new_con->name = name;
    new_con->rawinput_handle = (void *)hdev;
    new_con->rawinput_path = path;
    snprintf(new_con->rawinput_guid, sizeof(new_con->rawinput_guid),
             "mouse:%08x", fnv1a_hash(path));
    new_con->bound = -1;
    new_con->peripheral_types[0] = PERIPHERAL_NONE;
    new_con->peripheral_types[1] = PERIPHERAL_NONE;

    QTAILQ_INSERT_TAIL(&available_controllers, new_con, entry);
    DPRINTF("rawinput: added '%s' (%s) as %s\n", new_con->name, path,
            new_con->rawinput_guid);

    // Re-bind to a previously saved port. Unlike gamepads, never auto-bind
    // a mouse to a free port: every system has at least one regular mouse
    // and grabbing a controller port with it would be surprising.
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

static void rawinput_remove_device(HANDLE hdev)
{
    ControllerState *con = rawinput_find_controller(hdev);
    if (con == NULL) {
        return;
    }

    DPRINTF("rawinput: removed '%s'\n", con->name);

    if (con->bound >= 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "Port %d disconnected", con->bound + 1);
        xemu_queue_notification(buf);

        // Unbind, but don't save the unbinding so the device is bound to
        // the same port again when reconnected
        xemu_input_bind(con->bound, NULL, 0);
    }

    QTAILQ_REMOVE(&available_controllers, con, entry);
    g_free(con->rawinput_path);
    g_free((char *)con->name);
    g_free(con);
}

static void rawinput_handle_mouse_input(HANDLE hdev, const RAWMOUSE *mouse)
{
    ControllerState *con = rawinput_find_controller(hdev);
    if (con == NULL) {
        return;
    }

    if (mouse->usFlags & MOUSE_MOVE_ABSOLUTE) {
        // Absolute coordinates, normalized to [0, 65535] over the screen
        // (or the whole virtual desktop if MOUSE_VIRTUAL_DESKTOP is set).
        // This is what Sinden Lightguns report.
        bool virt = mouse->usFlags & MOUSE_VIRTUAL_DESKTOP;
        int ox = virt ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0;
        int oy = virt ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
        int w = GetSystemMetrics(virt ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
        int h = GetSystemMetrics(virt ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

        POINT pt = { ox + MulDiv(mouse->lLastX, w, 65535),
                     oy + MulDiv(mouse->lLastY, h, 65535) };
        ScreenToClient(g_hwnd, &pt);
        con->rawinput_client_x = pt.x;
        con->rawinput_client_y = pt.y;
        con->rawinput_has_abs = true;
    }
    // Relative devices are handled in
    // xemu_rawinput_update_controller_state() by following the system
    // cursor, which tracks them anyway.

    USHORT f = mouse->usButtonFlags;
    if (f & RI_MOUSE_LEFT_BUTTON_DOWN)
        con->rawinput_buttons |= XEMU_RAWINPUT_BUTTON_LEFT;
    if (f & RI_MOUSE_LEFT_BUTTON_UP)
        con->rawinput_buttons &= ~XEMU_RAWINPUT_BUTTON_LEFT;
    if (f & RI_MOUSE_RIGHT_BUTTON_DOWN)
        con->rawinput_buttons |= XEMU_RAWINPUT_BUTTON_RIGHT;
    if (f & RI_MOUSE_RIGHT_BUTTON_UP)
        con->rawinput_buttons &= ~XEMU_RAWINPUT_BUTTON_RIGHT;
    if (f & RI_MOUSE_MIDDLE_BUTTON_DOWN)
        con->rawinput_buttons |= XEMU_RAWINPUT_BUTTON_MIDDLE;
    if (f & RI_MOUSE_MIDDLE_BUTTON_UP)
        con->rawinput_buttons &= ~XEMU_RAWINPUT_BUTTON_MIDDLE;
    if (f & RI_MOUSE_BUTTON_4_DOWN)
        con->rawinput_buttons |= XEMU_RAWINPUT_BUTTON_X1;
    if (f & RI_MOUSE_BUTTON_4_UP)
        con->rawinput_buttons &= ~XEMU_RAWINPUT_BUTTON_X1;
    if (f & RI_MOUSE_BUTTON_5_DOWN)
        con->rawinput_buttons |= XEMU_RAWINPUT_BUTTON_X2;
    if (f & RI_MOUSE_BUTTON_5_UP)
        con->rawinput_buttons &= ~XEMU_RAWINPUT_BUTTON_X2;
}

// Hotplug events arrive while SDL pumps the message loop, outside the QEMU
// main loop lock. Binding/unbinding devices there is unsafe, so queue them
// and let xemu_rawinput_process_pending() handle them under the lock.
#define MAX_PENDING_DEVICE_CHANGES 16
static struct {
    HANDLE hdev;
    bool arrival;
} g_pending_changes[MAX_PENDING_DEVICE_CHANGES];
static int g_num_pending_changes;

static bool rawinput_message_hook(void *userdata, MSG *msg)
{
    if (msg->message == WM_INPUT) {
        RAWINPUT raw;
        UINT size = sizeof(raw);
        if (GetRawInputData((HRAWINPUT)msg->lParam, RID_INPUT, &raw, &size,
                            sizeof(RAWINPUTHEADER)) != (UINT)-1 &&
            raw.header.dwType == RIM_TYPEMOUSE) {
            rawinput_handle_mouse_input(raw.header.hDevice, &raw.data.mouse);
        }
    } else if (msg->message == WM_INPUT_DEVICE_CHANGE) {
        if ((msg->wParam == GIDC_ARRIVAL || msg->wParam == GIDC_REMOVAL) &&
            g_num_pending_changes < MAX_PENDING_DEVICE_CHANGES) {
            g_pending_changes[g_num_pending_changes].hdev =
                (HANDLE)msg->lParam;
            g_pending_changes[g_num_pending_changes].arrival =
                msg->wParam == GIDC_ARRIVAL;
            g_num_pending_changes++;
        }
    }
    return true; // let SDL continue processing the message
}

void xemu_rawinput_process_pending(void)
{
    for (int i = 0; i < g_num_pending_changes; i++) {
        if (g_pending_changes[i].arrival) {
            rawinput_add_device(g_pending_changes[i].hdev);
        } else {
            rawinput_remove_device(g_pending_changes[i].hdev);
        }
    }
    g_num_pending_changes = 0;
}

void xemu_rawinput_init(SDL_Window *window)
{
    assert(!g_initialized);

    g_hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                          SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                                          NULL);
    if (g_hwnd == NULL) {
        fprintf(stderr, "rawinput: could not get window handle\n");
        return;
    }

    HMODULE hid = LoadLibraryA("hid.dll");
    if (hid) {
        p_HidD_GetProductString = (HidD_GetProductString_t)GetProcAddress(
            hid, "HidD_GetProductString");
    }

    // Enumerate mice already connected
    UINT num_devices = 0;
    if (GetRawInputDeviceList(NULL, &num_devices,
                              sizeof(RAWINPUTDEVICELIST)) == 0 &&
        num_devices > 0) {
        RAWINPUTDEVICELIST *list =
            g_new0(RAWINPUTDEVICELIST, num_devices);
        UINT n = GetRawInputDeviceList(list, &num_devices,
                                       sizeof(RAWINPUTDEVICELIST));
        if (n != (UINT)-1) {
            for (UINT i = 0; i < n; i++) {
                if (list[i].dwType == RIM_TYPEMOUSE) {
                    rawinput_add_device(list[i].hDevice);
                }
            }
        }
        g_free(list);
    }

    // Receive WM_INPUT for mice (also when unfocused) plus hotplug events
    RAWINPUTDEVICE rid = {
        .usUsagePage = 0x01, // HID_USAGE_PAGE_GENERIC
        .usUsage = 0x02,     // HID_USAGE_GENERIC_MOUSE
        .dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY,
        .hwndTarget = g_hwnd,
    };
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        fprintf(stderr, "rawinput: RegisterRawInputDevices failed (%lu)\n",
                GetLastError());
        return;
    }

    SDL_SetWindowsMessageHook(rawinput_message_hook, NULL);
    g_initialized = true;
}

void xemu_rawinput_update_controller_state(ControllerState *state)
{
    state->buttons = 0;
    memset(state->axis, 0, sizeof(state->axis));

    pointer_apply_mapping(state);

    // Aim position in window client pixels
    int px, py;
    if (state->rawinput_has_abs) {
        px = state->rawinput_client_x;
        py = state->rawinput_client_y;
    } else {
        // Relative mice steer the system cursor; follow it
        POINT pt;
        if (!GetCursorPos(&pt) || g_hwnd == NULL) {
            return;
        }
        ScreenToClient(g_hwnd, &pt);
        px = pt.x;
        py = pt.y;
    }

    // Whole client area, used as aim rect until the game renders
    int fallback_w = -1, fallback_h = -1;
    RECT cr;
    if (g_hwnd != NULL && GetClientRect(g_hwnd, &cr)) {
        fallback_w = cr.right;
        fallback_h = cr.bottom;
    }

    pointer_apply_aim(state, px, py, fallback_w, fallback_h);
}

#elif !defined(__linux__) // Linux implements this API in xemu-evdev.c

void xemu_rawinput_init(SDL_Window *window)
{
    (void)window;
}

void xemu_rawinput_process_pending(void)
{
}

void xemu_rawinput_update_controller_state(ControllerState *state)
{
    (void)state;
}

#endif
