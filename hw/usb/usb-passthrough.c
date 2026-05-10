/*
 * XEMU USB PASSTHROUGH API
 *
 * Copyright (c) 2023 Fred Hallock
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "hw/usb-passthrough.h"

#ifdef CONFIG_USB_LIBUSB
#include "qemu/timer.h"
#include "qemu/typedefs.h"
#include "glib-compat.h"
#include "host-libusb.h"
#include <assert.h>
#include <libusb.h>
#endif

LibusbDeviceList available_libusb_devices =
    QTAILQ_HEAD_INITIALIZER(available_libusb_devices);

#ifdef CONFIG_USB_LIBUSB

static void get_libusb_devices(void);

static QEMUTimer *libusb_timer = NULL;
static void (*device_connected_callback)(LibusbDevice *) = NULL;
static void (*device_disconnected_callback)(LibusbDevice *) = NULL;

#define LIBUSB_TIMER_DELAY 200

static void libusb_timer_callback(void *opaque)
{
    get_libusb_devices();

    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    timer_mod_ns(libusb_timer, now + (LIBUSB_TIMER_DELAY * SCALE_MS));
}

static void xemu_create_libusb_passthrough_timer(void)
{
    libusb_timer =
        timer_new(QEMU_CLOCK_VIRTUAL, SCALE_MS, libusb_timer_callback, NULL);

    int64_t now = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);

    timer_mod_ns(libusb_timer, now + (LIBUSB_TIMER_DELAY * SCALE_MS));
}

static void xemu_destroy_libusb_passthrough_timer(void)
{
    timer_free(libusb_timer);
    libusb_timer = NULL;
}

typedef struct known_libusb_device {
    unsigned short vendor_id;
    unsigned short product_id;
    const char *name;
    XidDeviceType type;
    int hub_ports;
} known_libusb_device;

const char *unknownDeviceName = "Unknown Device";

known_libusb_device wellKnownDevices[] = {
    { 0x045e, 0x0202, "Xbox Controller", Gamepad, 3 },
    { 0x045e, 0x0285, "Xbox Controller S", GamepadS, 3 },
    { 0x045e, 0x0287, "Xbox Controller S", GamepadS, 3 },
    { 0x045e, 0x0289, "Xbox Controller S", GamepadS, 3 },
    { 0x0a7b, 0xd000, "Steel Battalion Controller", SteelBattalionController,
      0 },
    { 0x0f0d, 0x0001, "HORI Fight Stick", ArcadeStick, 2 }
};

#define NUM_KNOWN_XID_DEVICES ARRAY_SIZE(wellKnownDevices)

void xemu_init_libusb_passthrough(
    void (*on_connected_callback)(LibusbDevice *),
    void (*on_disconnected_callback)(LibusbDevice *))
{
    device_connected_callback = on_connected_callback;
    device_disconnected_callback = on_disconnected_callback;

    get_libusb_devices();
    xemu_create_libusb_passthrough_timer();
}

void xemu_shutdown_libusb_passthrough(void)
{
    xemu_destroy_libusb_passthrough_timer();
    while (!QTAILQ_EMPTY(&available_libusb_devices)) {
        LibusbDevice *device = QTAILQ_FIRST(&available_libusb_devices);
        QTAILQ_REMOVE(&available_libusb_devices, device, entry);
        g_free(device->host_port);
        g_free(device);
    }
}

static void get_libusb_devices(void)
{
    struct libusb_device **devs = NULL;
    struct libusb_device_descriptor ddesc;
    unsigned int bus;
    unsigned short vendor_id, product_id;
    char port[16];
    int i, j, n, hub_ports;
    const char *name;
    XidDeviceType type;
    LibusbDevice *iter, *iter2;
    bool previously_detected;
    libusb_context *ctx = get_libusb_context();

    if (usb_host_init() != 0) {
        return;
    }

    QTAILQ_FOREACH (iter, &available_libusb_devices, entry) {
        iter->detected = false;
    }

    n = libusb_get_device_list(ctx, &devs);
    for (i = 0; i < n; i++) {
        if (libusb_get_device_descriptor(devs[i], &ddesc) != 0) {
            continue;
        }
        if (ddesc.bDeviceClass == LIBUSB_CLASS_HUB) {
            continue;
        }

        name = unknownDeviceName;
        type = Gamepad;
        hub_ports = 0;

        usb_host_get_port(devs[i], port, sizeof(port));
        bus = libusb_get_bus_number(devs[i]);
        vendor_id = ddesc.idVendor;
        product_id = ddesc.idProduct;

        previously_detected = false;
        
        QTAILQ_FOREACH (iter, &available_libusb_devices, entry) {
            if (iter->vendor_id == vendor_id &&
                iter->product_id == product_id && iter->host_bus == bus &&
                strcmp(iter->host_port, port) == 0) {
                // We already know about this one
                previously_detected = true;
                iter->detected = true;
            }
        }

        if (previously_detected)
            continue;

        for (j = 0; j < NUM_KNOWN_XID_DEVICES; j++) {
            if (wellKnownDevices[j].vendor_id == vendor_id &&
                wellKnownDevices[j].product_id == product_id) {
                name = wellKnownDevices[j].name;
                type = wellKnownDevices[j].type;
                hub_ports = wellKnownDevices[j].hub_ports;
                break;
            }
        }

        // Skip any devices we don't already know about
        if (name == unknownDeviceName)
            continue;

        LibusbDevice *device = g_malloc(sizeof(LibusbDevice));
        memset(device, 0, sizeof(LibusbDevice));
        device->vendor_id = vendor_id;
        device->product_id = product_id;
        device->host_bus = bus;
        device->host_port = g_strdup(port);
        device->name = name;
        device->type = type;
        device->bound = -1;
        device->detected = true;
        device->internal_hub_ports = hub_ports;

        if (device_connected_callback != NULL)
            device_connected_callback(device);

        QTAILQ_INSERT_TAIL(&available_libusb_devices, device, entry);
    }

    // Remove any devices that aren't detected anymore
    QTAILQ_FOREACH_SAFE (iter, &available_libusb_devices, entry, iter2) {
        if (!iter->detected) {
            if (device_disconnected_callback != NULL)
                device_disconnected_callback(iter);

            QTAILQ_REMOVE(&available_libusb_devices, iter, entry);
            g_free(iter);
        }
    }

    libusb_free_device_list(devs, 1);
}

LibusbDevice *find_libusb_device(int host_bus, const char *port)
{
    LibusbDevice *iter;
    QTAILQ_FOREACH (iter, &available_libusb_devices, entry) {
        if (iter->host_bus == host_bus && strcmp(iter->host_port, port) == 0) {
            return iter;
        }
    }

    return NULL;
}

#else

void xemu_init_libusb_passthrough(
    void (*on_connected_callback)(LibusbDevice *),
    void (*on_disconnected_callback)(LibusbDevice *))
{
    // Do Nothing
}

void xemu_shutdown_libusb_passthrough(void)
{
    // Do Nothing
}

LibusbDevice *find_libusb_device(int host_bus, const char *port)
{
    return NULL;
}

#endif