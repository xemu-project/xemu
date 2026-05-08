#ifndef QEMU_USB_PASSTHROUGH_H
#define QEMU_USB_PASSTHROUGH_H

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

#include "qemu/osdep.h"
#include "qemu/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XidDeviceType {
    Gamepad,
    GamepadS,
    ArcadeStick,
    SteelBattalionController
} XidDeviceType;

typedef struct LibusbDevice {
    QTAILQ_ENTRY(LibusbDevice) entry;

    unsigned short
        vendor_id; // The vendor id of the device. Used to identify the device
    unsigned short
        product_id; // The product id of the device. Used to identify the device
    unsigned int host_bus; // the bus on the host system. Used for binding
    const char *host_port; // The port on the host system. Used for binding
    const char *name; // The name of the device
    bool detected; // true if it has been detected in the get_libusb_devices
                   // process. Used internally
    int bound; // internal port that this controller is bound to
    int internal_hub_ports; // Number of ports on the internal hub. This value
                            // is 0 if there is no internal hub
    void *device; // The root device of the controller

    XidDeviceType type;
    uint8_t *buffer;
    size_t buf_len;
} LibusbDevice;

typedef QTAILQ_HEAD(, LibusbDevice) LibusbDeviceList;
extern LibusbDeviceList available_libusb_devices;

void xemu_init_libusb_passthrough(
    void (*device_connected_callback)(LibusbDevice *),
    void (*device_disconnected_callback)(LibusbDevice *));
void xemu_shutdown_libusb_passthrough(void);
LibusbDevice *find_libusb_device(int host_bus, const char *port);

#ifdef __cplusplus
}
#endif

#endif