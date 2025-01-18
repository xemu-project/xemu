/*
 * QEMU USB XID Devices
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2017 Jannik Vogel
 * Copyright (c) 2018-2021 Matt Borgerson
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

#define USB_VENDOR_MICROSOFT 0x045e

#define LIGHT_GUN_IN_ENDPOINT_ID 0x02
#define LIGHT_GUN_OUT_ENDPOINT_ID 0x02

#define USB_XID(obj) \
    OBJECT_CHECK(USBXIDLightGunState, (obj), TYPE_USB_XID_LIGHT_GUN)

typedef struct XIDLightGunReport {
    uint8_t bReportId;
    uint8_t bLength;
    uint8_t wButtons;
    uint8_t wState;
    uint8_t bAnalogButtons[8]; // The last 2 are unused
    int16_t sThumbLX;
    int16_t sThumbLY;
} QEMU_PACKED XIDLightGunReport;

typedef struct XIDLightGunCalibrationReport {
    uint8_t bReportId;
    uint8_t bLength;
    int16_t sCenterCalibrationX;
    int16_t sCenterCalibrationY;
    int16_t sTopLeftCalibrationX;
    int16_t sTopLeftCalibrationY;
} QEMU_PACKED XIDLightGunCalibrationReport;

typedef struct USBXIDLightGunState {
    USBDevice dev;
    USBEndpoint *intr;
    const XIDDesc *xid_desc;
    XIDLightGunReport in_state;
    XIDLightGunReport in_state_capabilities;
    XIDLightGunCalibrationReport out_state;
    XIDLightGunCalibrationReport out_state_capabilities;
    uint8_t device_index;
} USBXIDLightGunState;

static const USBDescIface desc_iface_xbox_light_gun = {
    .bInterfaceNumber = 0,
    .bNumEndpoints = 2,
    .bInterfaceClass = USB_CLASS_XID,
    .bInterfaceSubClass = 0x42,
    .bInterfaceProtocol = 0x00,
    .eps =
        (USBDescEndpoint[]){
            {
                .bEndpointAddress = USB_DIR_IN | LIGHT_GUN_IN_ENDPOINT_ID,
                .bmAttributes = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize = 0x20,
                .bInterval = 4,
            },
            {
                .bEndpointAddress = USB_DIR_OUT | LIGHT_GUN_OUT_ENDPOINT_ID,
                .bmAttributes = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize = 0x20,
                .bInterval = 4,
            },
        },
};

static const USBDescDevice desc_device_xbox_light_gun = {
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
                .ifs = &desc_iface_xbox_light_gun,
            },
        },
};

static const USBDesc desc_xbox_light_gun = {
    .id = {
        .idVendor          = USB_VENDOR_MICROSOFT,
        .idProduct         = 0x0202,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_light_gun,
    .str  = desc_strings,
};

static const XIDDesc desc_xid_xbox_light_gun = {
    .bLength = 0x10,
    .bDescriptorType = USB_DT_XID,
    .bcdXid = 0x100,
    .bType = XID_DEVICETYPE_GAMEPAD,
    .bSubType = XID_DEVICESUBTYPE_LIGHT_GUN,
    .bMaxInputReportSize = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static void update_lg_input(USBXIDLightGunState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    // assert(state);
    if (state == NULL)
        return;

    xemu_input_update_controller(state);

    s->in_state.wButtons = 0;
    if (state->lg.buttons & CONTROLLER_BUTTON_DPAD_UP)
        s->in_state.wButtons |= 0x01;
    if (state->lg.buttons & CONTROLLER_BUTTON_DPAD_DOWN)
        s->in_state.wButtons |= 0x02;
    if (state->lg.buttons & CONTROLLER_BUTTON_DPAD_LEFT)
        s->in_state.wButtons |= 0x04;
    if (state->lg.buttons & CONTROLLER_BUTTON_DPAD_RIGHT)
        s->in_state.wButtons |= 0x08;
    if (state->lg.buttons & CONTROLLER_BUTTON_START)
        s->in_state.wButtons |= 0x10;
    if (state->lg.buttons & CONTROLLER_BUTTON_BACK)
        s->in_state.wButtons |= 0x20;

    s->in_state.wState = state->lg.status;

    s->in_state.bAnalogButtons[0] =
        (state->lg.buttons & CONTROLLER_BUTTON_A) ? 0xFF : 0x00;
    s->in_state.bAnalogButtons[1] =
        (state->lg.buttons & CONTROLLER_BUTTON_B) ? 0xFF : 0x00;
    s->in_state.bAnalogButtons[2] =
        (state->lg.buttons & CONTROLLER_BUTTON_X) ? 0xFF : 0x00;
    s->in_state.bAnalogButtons[3] =
        (state->lg.buttons & CONTROLLER_BUTTON_Y) ? 0xFF : 0x00;
    s->in_state.bAnalogButtons[4] =
        (state->lg.buttons & CONTROLLER_BUTTON_BLACK) ? 0xFF : 0x00;
    s->in_state.bAnalogButtons[5] =
        (state->lg.buttons & CONTROLLER_BUTTON_WHITE) ? 0xFF : 0x00;

    s->in_state.sThumbLX = state->lg.axis[0];
    s->in_state.sThumbLY = state->lg.axis[1];
}

static void update_lg_output(USBXIDLightGunState *s)
{
    if (s->out_state.bLength == 6) {
        // Rumble Data, do nothing (for now)
    } else if (s->out_state.bLength == 10) {
        ControllerState *state = xemu_input_get_bound(s->device_index);
        assert(state);
        xemu_input_update_controller(state);

        // Calibration Data
        s->out_state.sTopLeftCalibrationX =
            -25000 - s->out_state.sTopLeftCalibrationX;
        s->out_state.sTopLeftCalibrationY =
            25000 - s->out_state.sTopLeftCalibrationY;

        state->lg.offsetX = s->out_state.sCenterCalibrationX;
        state->lg.offsetY = s->out_state.sCenterCalibrationY;
        state->lg.scaleX = 25000.0f / (s->out_state.sCenterCalibrationX -
                                       s->out_state.sTopLeftCalibrationX);
        state->lg.scaleY = 25000.0f / (s->out_state.sTopLeftCalibrationY -
                                       s->out_state.sCenterCalibrationY);
    }
}

static void usb_xid_light_gun_handle_control(USBDevice *dev, USBPacket *p,
                                             int request, int value, int index,
                                             int length, uint8_t *data)
{
    USBXIDLightGunState *s = DO_UPCAST(USBXIDLightGunState, dev, dev);

    DPRINTF("xid light_gun handle_control 0x%x 0x%x\n", request, value);

    int ret =
        usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        DPRINTF("xid handled by usb_desc_handle_control: %d\n", ret);
        return;
    }

    switch (request) {
    /* HID requests */
    case ClassInterfaceRequest | HID_GET_REPORT:
        DPRINTF("xid GET_REPORT 0x%x\n", value);
        update_lg_input(s);
        if (value == 0x0100) { /* input */
            if (length <= s->in_state.bLength) {
                memcpy(data, &s->in_state, s->in_state.bLength);
                p->actual_length = length;
            } else {
                p->status = USB_RET_STALL;
            }
        } else {
            p->status = USB_RET_STALL;
            assert(false);
        }
        break;
    case ClassInterfaceOutRequest | HID_SET_REPORT:
        DPRINTF("xid SET_REPORT 0x%x\n", value);
        if (value == 0x0200) { /* output */
            /* Read length, then the entire packet */
            if (length == sizeof(XIDGamepadOutputReport)) {
                memcpy(&s->out_state, data, sizeof(XIDGamepadOutputReport));

                /* FIXME: This should also be a STALL */
                assert(s->out_state.bLength == sizeof(XIDGamepadOutputReport));

                p->actual_length = length;
            } else {
                p->status = USB_RET_STALL;
            }
            update_lg_output(s);
        } else if (value == 0x0201) { /* light gun calibration */
            if (length == sizeof(XIDLightGunCalibrationReport)) {
                memcpy(&s->out_state, data,
                       sizeof(XIDLightGunCalibrationReport));

                DPRINTF("xid Light Gun Calibration Data: %d, %d, %d, %d\n",
                        s->out_state.sCenterCalibrationX,
                        s->out_state.sCenterCalibrationY,
                        s->out_state.sTopLeftCalibrationX,
                        s->out_state.sTopLeftCalibrationY);

                /* FIXME: This should also be a STALL */
                assert(s->out_state.bLength ==
                       sizeof(XIDLightGunCalibrationReport));

                p->actual_length = length;
            } else {
                p->status = USB_RET_STALL;
            }
            update_lg_output(s);
        } else {
            p->status = USB_RET_STALL;
            assert(false);
        }
        break;
    /* XID requests */
    case VendorInterfaceRequest | USB_REQ_GET_DESCRIPTOR:
        DPRINTF("xid GET_DESCRIPTOR 0x%x\n", value);
        if (value == 0x4200) {
            assert(s->xid_desc->bLength <= length);
            memcpy(data, s->xid_desc, s->xid_desc->bLength);
            p->actual_length = s->xid_desc->bLength;
        } else {
            p->status = USB_RET_STALL;
            assert(false);
        }
        break;
    case VendorInterfaceRequest | XID_GET_CAPABILITIES:
        DPRINTF("xid XID_GET_CAPABILITIES 0x%x\n", value);
        if (value == 0x0100) {
            if (length > s->in_state_capabilities.bLength) {
                length = s->in_state_capabilities.bLength;
            }
            memcpy(data, &s->in_state_capabilities, length);
            p->actual_length = length;
        } else if (value == 0x0200) {
            if (length > s->out_state_capabilities.bLength) {
                length = s->out_state_capabilities.bLength;
            }
            memcpy(data, &s->out_state_capabilities, length);
            p->actual_length = length;
        } else {
            p->status = USB_RET_STALL;
            assert(false);
        }
        break;
    case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE) << 8) |
        USB_REQ_GET_DESCRIPTOR:
        /* FIXME: ! */
        DPRINTF("xid unknown xpad request 0x%x: value = 0x%x\n", request,
                value);
        memset(data, 0x00, length);
        // FIXME: Intended for the hub: usbd_get_hub_descriptor, UT_READ_CLASS?!
        p->status = USB_RET_STALL;
        // assert(false);
        break;
    case ((USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT) << 8) |
        USB_REQ_CLEAR_FEATURE:
        /* FIXME: ! */
        DPRINTF("xid unknown xpad request 0x%x: value = 0x%x\n", request,
                value);
        memset(data, 0x00, length);
        p->status = USB_RET_STALL;
        break;
    default:
        DPRINTF("xid USB stalled on request 0x%x value 0x%x\n", request, value);
        p->status = USB_RET_STALL;
        assert(false);
        break;
    }
}

static void usb_xid_light_gun_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXIDLightGunState *s = DO_UPCAST(USBXIDLightGunState, dev, dev);

    DPRINTF("xid light_gun handle_gamepad_data 0x%x %d 0x%zx\n", p->pid,
            p->ep->nr, p->iov.size);

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == LIGHT_GUN_IN_ENDPOINT_ID) {
            update_lg_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
        } else {
            assert(false);
        }
        break;
    case USB_TOKEN_OUT:
        if (p->ep->nr == LIGHT_GUN_OUT_ENDPOINT_ID) {
            usb_packet_copy(p, &s->out_state, s->out_state.bLength);
            update_lg_output(s);
        } else {
            assert(false);
        }
        break;
    default:
        p->status = USB_RET_STALL;
        assert(false);
        break;
    }
}

static void usb_xid_light_gun_class_initfn(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->handle_reset = usb_xid_handle_reset;
    uc->handle_control = usb_xid_light_gun_handle_control;
    uc->handle_data = usb_xid_light_gun_handle_data;
    // uc->handle_destroy = usb_xid_handle_destroy;
    uc->handle_attach = usb_desc_attach;
}

static void usb_xbox_light_gun_realize(USBDevice *dev, Error **errp)
{
    USBXIDLightGunState *s = USB_XID(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.bLength = sizeof(s->out_state);
    s->out_state.bReportId = 0;

    s->xid_desc = &desc_xid_xbox_light_gun;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.bLength = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.bReportId = 0;
}

static Property xid_properties[] = {
    DEFINE_PROP_UINT8("index", USBXIDLightGunState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_usb_xbox = {
    .name = TYPE_USB_XID_LIGHT_GUN,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_USB_DEVICE(dev, USBXIDLightGunState),
                                // FIXME
                                VMSTATE_END_OF_LIST() },
};

static void usb_xbox_light_gun_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc = "Microsoft Xbox Light Gun";
    uc->usb_desc = &desc_xbox_light_gun;
    uc->realize = usb_xbox_light_gun_realize;
    uc->unrealize = usb_xbox_gamepad_unrealize;
    usb_xid_light_gun_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &vmstate_usb_xbox;
    device_class_set_props(dc, xid_properties);
    dc->desc = "Microsoft Xbox Light Gun";
}

static const TypeInfo usb_xbox_light_gun_info = {
    .name = TYPE_USB_XID_LIGHT_GUN,
    .parent = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDLightGunState),
    .class_init = usb_xbox_light_gun_class_initfn,
};

static void usb_xid_register_types(void)
{
    type_register_static(&usb_xbox_light_gun_info);
}

type_init(usb_xid_register_types)