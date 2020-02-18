/*
 * QEMU USB XID Devices
 *
 * Copyright (c) 2013 espes
 * Copyright (c) 2018 Matt Borgerson
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
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "sysemu/sysemu.h"
#include "hw/hw.h"
#include "ui/console.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "ui/input.h"

// #define DEBUG_XID
#ifdef DEBUG_XID
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif


#define TYPE_USB_XID "usb-xbox-gamepad"
#define USB_XID(obj) OBJECT_CHECK(USBXIDState, (obj), TYPE_USB_XID)

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER]     = "QEMU",
    [STR_PRODUCT]          = "Microsoft Gamepad",
    [STR_SERIALNUMBER]     = "1",
};

/*
 * http://xbox-linux.cvs.sourceforge.net/viewvc/xbox-linux/kernel-2.6/drivers/usb/input/xpad.c
 * http://euc.jp/periphs/xbox-controller.en.html
 * http://euc.jp/periphs/xbox-pad-desc.txt
 */

#define USB_CLASS_XID  0x58
#define USB_DT_XID     0x42


#define HID_GET_REPORT       0x01
#define HID_SET_REPORT       0x09
#define XID_GET_CAPABILITIES 0x01



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
    uint8_t report_id; //FIXME: is this correct?
    uint8_t length;
    uint16_t left_actuator_strength;
    uint16_t right_actuator_strength;
} QEMU_PACKED XIDGamepadOutputReport;


typedef struct USBXIDState {
    USBDevice dev;
    USBEndpoint *intr;
    const XIDDesc *xid_desc;
    QemuInputHandlerState *hs;
    bool in_dirty;
    XIDGamepadReport in_state;
    XIDGamepadReport in_state_capabilities;
    XIDGamepadOutputReport out_state;
    XIDGamepadOutputReport out_state_capabilities;
} USBXIDState;

static const USBDescIface desc_iface_xbox_gamepad = {
    .bInterfaceNumber              = 0,
    .bNumEndpoints                 = 2,
    .bInterfaceClass               = USB_CLASS_XID,
    .bInterfaceSubClass            = 0x42,
    .bInterfaceProtocol            = 0x00,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | 0x02,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 0x20,
            .bInterval             = 4,
        },
        {
            .bEndpointAddress      = USB_DIR_OUT | 0x02,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 0x20,
            .bInterval             = 4,
        },
    },
};

static const USBDescDevice desc_device_xbox_gamepad = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = 0x40,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 1,
            .bConfigurationValue   = 1,
            .bmAttributes          = USB_CFG_ATT_ONE,
            .bMaxPower             = 50,
            .nif = 1,
            .ifs = &desc_iface_xbox_gamepad,
        },
    },
};

static const USBDesc desc_xbox_gamepad = {
    .id = {
        .idVendor          = 0x045e,
        .idProduct         = 0x0202,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_gamepad,
    .str  = desc_strings,
};

static const XIDDesc desc_xid_xbox_gamepad = {
    .bLength = 0x10,
    .bDescriptorType = USB_DT_XID,
    .bcdXid = 0x100,
    .bType = 1,
    .bSubType = 1,
    .bMaxInputReportSize = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

#define GAMEPAD_A                0
#define GAMEPAD_B                1
#define GAMEPAD_X                2
#define GAMEPAD_Y                3
#define GAMEPAD_BLACK            4
#define GAMEPAD_WHITE            5
#define GAMEPAD_LEFT_TRIGGER     6
#define GAMEPAD_RIGHT_TRIGGER    7

#define GAMEPAD_DPAD_UP          8
#define GAMEPAD_DPAD_DOWN        9
#define GAMEPAD_DPAD_LEFT        10
#define GAMEPAD_DPAD_RIGHT       11
#define GAMEPAD_START            12
#define GAMEPAD_BACK             13
#define GAMEPAD_LEFT_THUMB       14
#define GAMEPAD_RIGHT_THUMB      15

#define GAMEPAD_LEFT_THUMB_UP    16
#define GAMEPAD_LEFT_THUMB_DOWN  17
#define GAMEPAD_LEFT_THUMB_LEFT  18
#define GAMEPAD_LEFT_THUMB_RIGHT 19

#define GAMEPAD_RIGHT_THUMB_UP    20
#define GAMEPAD_RIGHT_THUMB_DOWN  21
#define GAMEPAD_RIGHT_THUMB_LEFT  22
#define GAMEPAD_RIGHT_THUMB_RIGHT 23

static const int gamepad_mapping[] = {
    [0 ... Q_KEY_CODE__MAX] = -1,

    [Q_KEY_CODE_UP]    = GAMEPAD_DPAD_UP,
    [Q_KEY_CODE_KP_8]  = GAMEPAD_DPAD_UP,
    [Q_KEY_CODE_DOWN]  = GAMEPAD_DPAD_DOWN,
    [Q_KEY_CODE_KP_2]  = GAMEPAD_DPAD_DOWN,
    [Q_KEY_CODE_LEFT]  = GAMEPAD_DPAD_LEFT,
    [Q_KEY_CODE_KP_4]  = GAMEPAD_DPAD_LEFT,
    [Q_KEY_CODE_RIGHT] = GAMEPAD_DPAD_RIGHT,
    [Q_KEY_CODE_KP_6]  = GAMEPAD_DPAD_RIGHT,

    [Q_KEY_CODE_RET]   = GAMEPAD_START,
    [Q_KEY_CODE_BACKSPACE] = GAMEPAD_BACK,

    [Q_KEY_CODE_W]     = GAMEPAD_X,
    [Q_KEY_CODE_E]     = GAMEPAD_Y,
    [Q_KEY_CODE_S]     = GAMEPAD_A,
    [Q_KEY_CODE_D]     = GAMEPAD_B,
    [Q_KEY_CODE_X]     = GAMEPAD_WHITE,
    [Q_KEY_CODE_C]     = GAMEPAD_BLACK,

    [Q_KEY_CODE_Q]     = GAMEPAD_LEFT_TRIGGER,
    [Q_KEY_CODE_R]     = GAMEPAD_RIGHT_TRIGGER,

    [Q_KEY_CODE_V]     = GAMEPAD_LEFT_THUMB,
    [Q_KEY_CODE_T]     = GAMEPAD_LEFT_THUMB_UP,
    [Q_KEY_CODE_F]     = GAMEPAD_LEFT_THUMB_LEFT,
    [Q_KEY_CODE_G]     = GAMEPAD_LEFT_THUMB_DOWN,
    [Q_KEY_CODE_H]     = GAMEPAD_LEFT_THUMB_RIGHT,

    [Q_KEY_CODE_M]     = GAMEPAD_RIGHT_THUMB,
    [Q_KEY_CODE_I]     = GAMEPAD_RIGHT_THUMB_UP,
    [Q_KEY_CODE_J]     = GAMEPAD_RIGHT_THUMB_LEFT,
    [Q_KEY_CODE_K]     = GAMEPAD_RIGHT_THUMB_DOWN,
    [Q_KEY_CODE_L]     = GAMEPAD_RIGHT_THUMB_RIGHT,
};

static void xbox_gamepad_keyboard_event(DeviceState *dev, QemuConsole *src,
                                        InputEvent *evt)
{
    USBXIDState *s = (USBXIDState *)dev;
    InputKeyEvent *key;
    int code;

    assert(evt->type == INPUT_EVENT_KIND_KEY);
    key = evt->u.key.data;
    code = qemu_input_key_value_to_qcode(key->key);

    if (code >= Q_KEY_CODE__MAX) {
        return;
    }

    bool up = !key->down;
    int button = gamepad_mapping[code];

    DPRINTF("xid keyboard_event %d %d %d\n", code, button, up);

    uint16_t mask;
    switch (button) {
    case GAMEPAD_A ... GAMEPAD_RIGHT_TRIGGER:
        s->in_state.bAnalogButtons[button] = up ? 0 : 0xff;
        break;
    case GAMEPAD_DPAD_UP ... GAMEPAD_RIGHT_THUMB:
        mask = (1 << (button - GAMEPAD_DPAD_UP));
        s->in_state.wButtons &= ~mask;
        if (!up) {
            s->in_state.wButtons |= mask;
        }
        break;

    case GAMEPAD_LEFT_THUMB_UP:
        s->in_state.sThumbLY = up ? 0 : 32767;
        break;
    case GAMEPAD_LEFT_THUMB_DOWN:
        s->in_state.sThumbLY = up ? 0 : -32768;
        break;
    case GAMEPAD_LEFT_THUMB_LEFT:
        s->in_state.sThumbLX = up ? 0 : -32768;
        break;
    case GAMEPAD_LEFT_THUMB_RIGHT:
        s->in_state.sThumbLX = up ? 0 : 32767;
        break;

    case GAMEPAD_RIGHT_THUMB_UP:
        s->in_state.sThumbRY = up ? 0 : 32767;
        break;
    case GAMEPAD_RIGHT_THUMB_DOWN:
        s->in_state.sThumbRY = up ? 0 : -32768;
        break;
    case GAMEPAD_RIGHT_THUMB_LEFT:
        s->in_state.sThumbRX = up ? 0 : -32768;
        break;
    case GAMEPAD_RIGHT_THUMB_RIGHT:
        s->in_state.sThumbRX = up ? 0 : 32767;
        break;
    default:
        break;
    }

    s->in_dirty = true;
}

static QemuInputHandler xboxkbd_handler = {
    .name  = "Xbox Keyboard",
    .mask  = INPUT_EVENT_MASK_KEY,
    .event = xbox_gamepad_keyboard_event,
};

static void usb_xid_handle_reset(USBDevice *dev)
{
    DPRINTF("xid reset\n");
}

static void update_force_feedback(USBXIDState *s)
{
    /* FIXME: Check actuator endianess */
    DPRINTF("Set rumble power to 0x%x, 0x%x\n",
            s->out_state.left_actuator_strength,
            s->out_state.right_actuator_strength);
}

static void usb_xid_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBXIDState *s = (USBXIDState *)dev;

    DPRINTF("xid handle_control 0x%x 0x%x (length: %d)\n", request, value, length);

    int ret = usb_desc_handle_control(dev, p, request, value,
                                      index, length, data);
    if (ret >= 0) {
        DPRINTF("xid handled by usb_desc_handle_control: %d\n", ret);
        return;
    }

    switch (request) {
    /* HID requests */
    case ClassInterfaceRequest | HID_GET_REPORT:
        DPRINTF("xid GET_REPORT 0x%x\n", value);
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
            if (length == s->out_state.length) {
                memcpy(&s->out_state, data, sizeof(s->out_state));

                /* FIXME: This should also be a STALL */
                assert(s->out_state.length == sizeof(s->out_state));

                p->actual_length = length;
            } else {
                p->status = USB_RET_STALL;
            }
            update_force_feedback(s);
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
            if (length > s->out_state_capabilities.length) {
                length = s->out_state_capabilities.length;
            }
            memcpy(data, &s->out_state_capabilities, length);
            p->actual_length = length;
        } else {
            p->status = USB_RET_STALL;
            assert(false);
        }
        break;
    case ((USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_DEVICE) << 8)
             | USB_REQ_GET_DESCRIPTOR:
        /* FIXME: ! */
        DPRINTF("xid unknown xpad request 0x%x: value = 0x%x\n",
                request, value);
        memset(data, 0x00, length);
        //FIXME: Intended for the hub: usbd_get_hub_descriptor, UT_READ_CLASS?!
        p->status = USB_RET_STALL;
        //assert(false);
        break;
    case ((USB_DIR_OUT | USB_TYPE_STANDARD | USB_RECIP_ENDPOINT) << 8)
             | USB_REQ_CLEAR_FEATURE:
        /* FIXME: ! */
        DPRINTF("xid unknown xpad request 0x%x: value = 0x%x\n",
                request, value);
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

static void usb_xid_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXIDState *s = (USBXIDState *)dev;

    DPRINTF("xid handle_data 0x%x %d 0x%zx\n", p->pid, p->ep->nr, p->iov.size);

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == 2) {
            if (s->in_dirty) {
                usb_packet_copy(p, &s->in_state, s->in_state.bLength);
                s->in_dirty = false;
            } else {
                p->status = USB_RET_NAK;
            }
        } else {
            assert(false);
        }
        break;
    case USB_TOKEN_OUT:
        if (p->ep->nr == 2) {
            usb_packet_copy(p, &s->out_state, s->out_state.length);
            update_force_feedback(s);
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

static void usb_xid_class_initfn(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->handle_reset   = usb_xid_handle_reset;
    uc->handle_control = usb_xid_handle_control;
    uc->handle_data    = usb_xid_handle_data;
    uc->handle_attach  = usb_desc_attach;
}

static void usb_xbox_gamepad_realize(USBDevice *dev, Error **errp)
{
    USBXIDState *s = USB_XID(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;


    s->hs = qemu_input_handler_register((DeviceState *)(s), &xboxkbd_handler);
    s->xid_desc = &desc_xid_xbox_gamepad;
}

static void usb_xbox_gamepad_unrealize(USBDevice *dev, Error **errp)
{
}

static const VMStateDescription vmstate_usb_xbox = {
    .name = TYPE_USB_XID,
    .unmigratable = 1,
};

static void usb_xbox_gamepad_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Microsoft Xbox Controller";
    uc->usb_desc       = &desc_xbox_gamepad;
    uc->realize        = usb_xbox_gamepad_realize;
    uc->unrealize      = usb_xbox_gamepad_unrealize;
    usb_xid_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd = &vmstate_usb_xbox;
    dc->desc = "Microsoft Xbox Controller";
}

static const TypeInfo usb_xbox_gamepad_info = {
    .name          = TYPE_USB_XID,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDState),
    .class_init    = usb_xbox_gamepad_class_initfn,
};

static void usb_xid_register_types(void)
{
    type_register_static(&usb_xbox_gamepad_info);
}

type_init(usb_xid_register_types)
