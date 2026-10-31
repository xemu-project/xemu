/*
 * QEMU USB XID Lightgun (EMS TopGun II)
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2017 Jannik Vogel
 * Copyright (c) 2018-2021 Matt Borgerson
 *
 * Emulates an EMS TopGun II lightgun. On the Xbox this device enumerates
 * as a standard XID gamepad with device subtype 0x50 (lightgun). The
 * aiming position is reported through the left thumbstick axes and games
 * detect whether the gun points at the screen through bit 0x2000 of
 * wButtons (XINPUT_LIGHTGUN_ONSCREEN).
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

#include "xid.h"

// #define DEBUG_XID
#ifdef DEBUG_XID
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

#define USB_VENDOR_EMS 0x0b9a
#define USB_PRODUCT_TOPGUN_II 0x016b

#define GAMEPAD_IN_ENDPOINT_ID 0x02
#define GAMEPAD_OUT_ENDPOINT_ID 0x02

typedef struct USBXIDLightgunState {
    USBXIDGamepadState xid;
    // Calibration offsets, set by the game through
    // XInputSetLightgunCalibration -> HID SET_REPORT (see Cxbx-Reloaded
    // g_devs[].info.ligthgun offsets)
    int16_t offset_x, offset_y;
    int16_t offset_upp_x, offset_upp_y;
} USBXIDLightgunState;

// Wire format of the calibration report sent by XInputSetLightgunCalibration
typedef struct XIDLightgunCalibrationReport {
    uint8_t bReportId;
    uint8_t bLength;
    int16_t wCenterX;
    int16_t wCenterY;
    int16_t wUpperLeftX;
    int16_t wUpperLeftY;
} QEMU_PACKED XIDLightgunCalibrationReport;

#define USB_XID_LIGHTGUN(obj) \
    OBJECT_CHECK(USBXIDLightgunState, (obj), TYPE_USB_XID_LIGHTGUN)

static const USBDescStrings desc_strings_lightgun = {
    [STR_MANUFACTURER] = "EMS",
    [STR_PRODUCT]      = "TopGun II",
    [STR_SERIALNUMBER] = "1",
};

static const USBDescIface desc_iface_xbox_lightgun = {
    .bInterfaceNumber = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_XID,
    .bInterfaceSubClass = 0x42,
    .bInterfaceProtocol = 0x00,
    .eps =
        (USBDescEndpoint[]){
            {
                .bEndpointAddress = USB_DIR_IN | GAMEPAD_IN_ENDPOINT_ID,
                .bmAttributes = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize = 0x20,
                .bInterval = 4,
            },
            {
                .bEndpointAddress = USB_DIR_OUT | GAMEPAD_OUT_ENDPOINT_ID,
                .bmAttributes = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize = 0x20,
                .bInterval = 4,
            },
        },
};

static const USBDescDevice desc_device_xbox_lightgun = {
    .bcdUSB = 0x0110,
    .bMaxPacketSize0 = 0x40,
    .bNumConfigurations = 1,
    .confs =
        (USBDescConfig[]){
            {
                .bNumInterfaces = 1,
                .bConfigurationValue = 1,
                .bmAttributes = USB_CFG_ATT_ONE,
                .bMaxPower = 50,
                .nif = 1,
                .ifs = &desc_iface_xbox_lightgun,
            },
        },
};

static const USBDesc desc_xbox_lightgun = {
    .id = {
        .idVendor          = USB_VENDOR_EMS,
        .idProduct         = USB_PRODUCT_TOPGUN_II,
        .bcdDevice         = 0x0457,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_lightgun,
    .str  = desc_strings_lightgun,
};

static const XIDDesc desc_xid_xbox_lightgun = {
    .bLength = 0x10,
    .bDescriptorType = USB_DT_XID,
    .bcdXid = 0x100,
    .bType = XID_DEVICETYPE_GAMEPAD,
    .bSubType = XID_DEVICESUBTYPE_LIGHTGUN,
    .bMaxInputReportSize = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static void update_input_lightgun(USBXIDLightgunState *lg)
{
    USBXIDGamepadState *s = &lg->xid;

    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    update_input(s);

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    if (state->buttons & CONTROLLER_BUTTON_LIGHTGUN_ONSCREEN) {
        s->in_state.wButtons |= XID_LIGHTGUN_ONSCREEN;
    }

    // Apply the game-supplied calibration the same way Cxbx-Reloaded does:
    // positions beyond half range use the upper-left offsets
    int16_t x = s->in_state.sThumbLX;
    int16_t y = s->in_state.sThumbLY;
    s->in_state.sThumbLX =
        x + ((abs(x) > 16383) ? lg->offset_upp_x : lg->offset_x);
    s->in_state.sThumbLY =
        y + ((abs(y) > 16383) ? lg->offset_upp_y : lg->offset_y);
}

static void usb_xid_lightgun_set_calibration(
    USBXIDLightgunState *lg, const XIDLightgunCalibrationReport *report)
{
    lg->offset_x = le16_to_cpu(report->wCenterX);
    lg->offset_y = le16_to_cpu(report->wCenterY);
    lg->offset_upp_x = le16_to_cpu(report->wUpperLeftX);
    lg->offset_upp_y = le16_to_cpu(report->wUpperLeftY);
    DPRINTF("xid lightgun calibration: center %d,%d upper-left %d,%d\n",
            lg->offset_x, lg->offset_y, lg->offset_upp_x, lg->offset_upp_y);
}

// Unlike the gamepad control handler, this never asserts on unexpected
// requests: lightgun games send requests (e.g. calibration) that the
// generic handler does not know about, and guest behavior must not be able
// to abort the emulator. Unknown requests are stalled like real hardware.
static void usb_xid_lightgun_handle_control(USBDevice *dev, USBPacket *p,
                                            int request, int value, int index,
                                            int length, uint8_t *data)
{
    USBXIDLightgunState *lg = USB_XID_LIGHTGUN(dev);
    USBXIDGamepadState *s = &lg->xid;

    DPRINTF("xid lightgun handle_control 0x%x 0x%x\n", request, value);

    int ret =
        usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    switch (request) {
    /* HID requests */
    case ClassInterfaceRequest | HID_GET_REPORT:
        if (value == 0x0100 && length <= s->in_state.bLength) { /* input */
            update_input_lightgun(lg);
            memcpy(data, &s->in_state, s->in_state.bLength);
            p->actual_length = length;
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    case ClassInterfaceOutRequest | HID_SET_REPORT:
        if (length == sizeof(XIDLightgunCalibrationReport)) {
            // XInputSetLightgunCalibration
            XIDLightgunCalibrationReport report;
            memcpy(&report, data, sizeof(report));
            usb_xid_lightgun_set_calibration(lg, &report);
            p->actual_length = length;
        } else if (value == 0x0200 && length == s->out_state.length) {
            /* output (rumble; not present on a real gun, accept anyway) */
            memcpy(&s->out_state, data, sizeof(s->out_state));
            update_output(s);
            p->actual_length = length;
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    /* XID requests */
    case VendorInterfaceRequest | USB_REQ_GET_DESCRIPTOR:
        if (value == 0x4200 && s->xid_desc->bLength <= length) {
            memcpy(data, s->xid_desc, s->xid_desc->bLength);
            p->actual_length = s->xid_desc->bLength;
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    case VendorInterfaceRequest | XID_GET_CAPABILITIES:
        if (value == 0x0100) {
            if (length > s->in_state_capabilities.bLength) {
                length = s->in_state_capabilities.bLength;
            }
            memcpy(data, &s->in_state_capabilities, length);
            p->actual_length = length;
        } else if (value == 0x0200) {
            if (length > s->out_state_capabilities.length) {
                length = s->out_state_capabilities.length;
            }
            memcpy(data, &s->out_state_capabilities, length);
            p->actual_length = length;
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    default:
        DPRINTF("xid lightgun stalled on request 0x%x value 0x%x\n", request,
                value);
        p->status = USB_RET_STALL;
        break;
    }
}

static void usb_xid_lightgun_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXIDLightgunState *lg = USB_XID_LIGHTGUN(dev);
    USBXIDGamepadState *s = &lg->xid;

    DPRINTF("xid handle_lightgun_data 0x%x %d 0x%zx\n", p->pid, p->ep->nr,
            p->iov.size);

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == GAMEPAD_IN_ENDPOINT_ID) {
            update_input_lightgun(lg);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    case USB_TOKEN_OUT:
        if (p->ep->nr == GAMEPAD_OUT_ENDPOINT_ID) {
            if (p->iov.size == sizeof(XIDLightgunCalibrationReport)) {
                // Calibration may also arrive over the interrupt pipe
                XIDLightgunCalibrationReport report;
                usb_packet_copy(p, &report, sizeof(report));
                usb_xid_lightgun_set_calibration(lg, &report);
            } else if (p->iov.size == s->out_state.length) {
                usb_packet_copy(p, &s->out_state, s->out_state.length);
                update_output(s);
            } else {
                p->status = USB_RET_STALL;
            }
        } else {
            p->status = USB_RET_STALL;
        }
        break;
    default:
        p->status = USB_RET_STALL;
        break;
    }
}

static void usb_xbox_lightgun_realize(USBDevice *dev, Error **errp)
{
    USBXIDLightgunState *lg = USB_XID_LIGHTGUN(dev);
    USBXIDGamepadState *s = &lg->xid;
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_xbox_lightgun;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF,
           sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;
}

static const Property xid_lightgun_properties[] = {
    DEFINE_PROP_UINT8("index", USBXIDLightgunState, xid.device_index, 0),
};

static const VMStateDescription vmstate_usb_xbox_lightgun = {
    .name = TYPE_USB_XID_LIGHTGUN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_USB_DEVICE(xid.dev,
                                                   USBXIDLightgunState),
                                VMSTATE_INT16(offset_x, USBXIDLightgunState),
                                VMSTATE_INT16(offset_y, USBXIDLightgunState),
                                VMSTATE_INT16(offset_upp_x,
                                              USBXIDLightgunState),
                                VMSTATE_INT16(offset_upp_y,
                                              USBXIDLightgunState),
                                VMSTATE_END_OF_LIST() },
};

static void usb_xbox_lightgun_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc = "EMS TopGun II Lightgun";
    uc->usb_desc = &desc_xbox_lightgun;
    uc->realize = usb_xbox_lightgun_realize;
    uc->unrealize = usb_xbox_gamepad_unrealize;
    uc->handle_reset = usb_xid_handle_reset;
    uc->handle_control = usb_xid_lightgun_handle_control;
    uc->handle_data = usb_xid_lightgun_handle_data;
    uc->handle_attach = usb_desc_attach;
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &vmstate_usb_xbox_lightgun;
    device_class_set_props(dc, xid_lightgun_properties);
    dc->desc = "EMS TopGun II Lightgun";
}

static const TypeInfo usb_xbox_lightgun_info = {
    .name = TYPE_USB_XID_LIGHTGUN,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDLightgunState),
    .class_init = usb_xbox_lightgun_class_init,
};

static void usb_xid_lightgun_register_types(void)
{
    type_register_static(&usb_xbox_lightgun_info);
}

type_init(usb_xid_lightgun_register_types)
