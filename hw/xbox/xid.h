#ifndef __XID_H__
#define __XID_H__

/*
 * QEMU USB XID Devices
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2017 Jannik Vogel
 * Copyright (c) 2018-2021 Matt Borgerson
 * Copyright (c) 2023 Fred Hallock
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/qdev-properties.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "migration/vmstate.h"
#include "system/system.h"
#include "ui/console.h"
#include "ui/xemu-input.h"

// #define DEBUG_XID
#ifdef DEBUG_XID
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

#define USB_CLASS_XID 0x58
#define USB_DT_XID 0x42

#define HID_GET_REPORT 0x01
#define HID_SET_REPORT 0x09
#define XID_GET_CAPABILITIES 0x01

#define XID_DEVICETYPE_GAMEPAD 0x01

#define XID_DEVICESUBTYPE_GAMEPAD 0x01
#define XID_DEVICESUBTYPE_GAMEPAD_S 0x02

#define TYPE_USB_XID_GAMEPAD "usb-xbox-gamepad"
#define TYPE_USB_XID_GAMEPAD_S "usb-xbox-gamepad-s"

#define GAMEPAD_A 0
#define GAMEPAD_B 1
#define GAMEPAD_X 2
#define GAMEPAD_Y 3
#define GAMEPAD_BLACK 4
#define GAMEPAD_WHITE 5
#define GAMEPAD_LEFT_TRIGGER 6
#define GAMEPAD_RIGHT_TRIGGER 7

#define GAMEPAD_DPAD_UP 8
#define GAMEPAD_DPAD_DOWN 9
#define GAMEPAD_DPAD_LEFT 10
#define GAMEPAD_DPAD_RIGHT 11
#define GAMEPAD_START 12
#define GAMEPAD_BACK 13
#define GAMEPAD_LEFT_THUMB 14
#define GAMEPAD_RIGHT_THUMB 15

#define BUTTON_MASK(button) (1 << ((button) - GAMEPAD_DPAD_UP))

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

extern const USBDescStrings desc_strings;

typedef struct XIDDesc {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdXid;
    uint8_t bType;
    uint8_t bSubType;
    uint8_t bMaxInputReportSize;
    uint8_t bMaxOutputReportSize;
    uint16_t wAlternateProductIds[4];
} QEMU_PACKED XIDDesc;

typedef struct XIDGamepadReport {
    uint8_t bReportId;
    uint8_t bLength;
    uint16_t wButtons;
    uint8_t bAnalogButtons[8];
    int16_t sThumbLX;
    int16_t sThumbLY;
    int16_t sThumbRX;
    int16_t sThumbRY;
} QEMU_PACKED XIDGamepadReport;

typedef struct XIDGamepadOutputReport {
    uint8_t report_id; // FIXME: is this correct?
    uint8_t length;
    uint16_t left_actuator_strength;
    uint16_t right_actuator_strength;
} QEMU_PACKED XIDGamepadOutputReport;

typedef struct USBXIDGamepadState {
    USBDevice dev;
    USBEndpoint *intr;
    const XIDDesc *xid_desc;
    XIDGamepadReport in_state;
    XIDGamepadReport in_state_capabilities;
    XIDGamepadOutputReport out_state;
    XIDGamepadOutputReport out_state_capabilities;
    uint8_t device_index;
} USBXIDGamepadState;

void update_input(USBXIDGamepadState *s);
void update_output(USBXIDGamepadState *s);
void usb_xid_handle_reset(USBDevice *dev);
void usb_xid_handle_control(USBDevice *dev, USBPacket *p, int request,
                            int value, int index, int length, uint8_t *data);
void usb_xbox_gamepad_unrealize(USBDevice *dev);

#if 0
void usb_xid_handle_destroy(USBDevice *dev);
#endif

#endif