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

#include "qemu/osdep.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "sysemu/sysemu.h"
#include "hw/hw.h"
#include "ui/console.h"
#include "hw/usb.h"
#include "hw/usb/desc.h"
#include "ui/xemu-input.h"

//#define DEBUG_XID
#ifdef DEBUG_XID
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

/*
 * http://xbox-linux.cvs.sourceforge.net/viewvc/xbox-linux/kernel-2.6/drivers/usb/input/xpad.c
 * http://euc.jp/periphs/xbox-controller.en.html
 * http://euc.jp/periphs/xbox-pad-desc.txt
 */

#define USB_VENDOR_MICROSOFT 0x045e
#define USB_VENDOR_CAPCOM 0x0a7b

#define USB_CLASS_XID  0x58
#define USB_DT_XID     0x42

#define HID_GET_REPORT       0x01
#define HID_SET_REPORT       0x09
#define XID_GET_CAPABILITIES 0x01

#define TYPE_USB_XID "usb-xbox-gamepad"
#define TYPE_USB_XID_ALT "usb-xbox-gamepad-s"
#define TYPE_USB_XID_SB "usb-steel-battalion"
#define USB_XID(obj) OBJECT_CHECK(USBXIDGamepadState, (obj), TYPE_USB_XID)
#define USB_XID_S(obj) OBJECT_CHECK(USBXIDGamepadState, (obj), TYPE_USB_XID_ALT)
#define USB_XID_SB(obj) OBJECT_CHECK(USBXIDSteelBattalionState, (obj), TYPE_USB_XID_SB)

enum {
    STR_MANUFACTURER = 1,
    STR_PRODUCT,
    STR_SERIALNUMBER,
};

typedef enum HapticEmulationMode {
    EMU_NONE,
    EMU_HAPTIC_LEFT_RIGHT
} HapticEmulationMode;

static const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "QEMU",
    [STR_PRODUCT]      = "Microsoft Xbox Controller",
    [STR_SERIALNUMBER] = "1",
};

typedef struct XIDDesc {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdXid;
    uint8_t  bType;
    uint8_t  bSubType;
    uint8_t  bMaxInputReportSize;
    uint8_t  bMaxOutputReportSize;
    uint16_t wAlternateProductIds[4];
} QEMU_PACKED XIDDesc;

typedef struct XIDGamepadReport {
    uint8_t  bReportId;
    uint8_t  bLength;
    uint16_t wButtons;
    uint8_t  bAnalogButtons[8];
    int16_t  sThumbLX;
    int16_t  sThumbLY;
    int16_t  sThumbRX;
    int16_t  sThumbRY;
} QEMU_PACKED XIDGamepadReport;

typedef struct XIDGamepadOutputReport {
    uint8_t  report_id; //FIXME: is this correct?
    uint8_t  length;
    uint16_t left_actuator_strength;
    uint16_t right_actuator_strength;
} QEMU_PACKED XIDGamepadOutputReport;

typedef struct XIDSteelBattalionReport {
    uint8_t     bReportId;
    uint8_t     bLength;
    uint32_t    dwButtons;
    uint8_t     bMoreButtons;
    uint16_t    wPadding;
    uint8_t  	bAimingX;
    uint8_t     bPadding;
    uint8_t  	bAimingY;
    int16_t   	sRotationLever; // only high byte is used
    int16_t   	sSightChangeX;  // only high byte is used
    int16_t   	sSightChangeY;  // only high byte is used
    uint16_t    wLeftPedal;     // only high byte is used
    uint16_t    wMiddlePedal;   // only high byte is used
    uint16_t    wRightPedal;    // only high byte is used
    uint8_t   	ucTunerDial;    // low nibble, The 9 o'clock postion is 0, and the 6 o'clock position is 12
    uint8_t   	ucGearLever;    // gear lever 1~5 for gear 1~5, 7~13 for gear R,N,1~5, 15 for gear R
} QEMU_PACKED XIDSteelBattalionReport;

typedef struct XIDSteelBattalionOutputReport {
    uint8_t report_id;
    uint8_t length;
    uint8_t notUsed[32];
} QEMU_PACKED XIDSteelBattalionOutputReport;

typedef struct USBXIDGamepadState {
    USBDevice              dev;
    USBEndpoint            *intr;
    const XIDDesc          *xid_desc;
    XIDGamepadReport       in_state;
    XIDGamepadReport       in_state_capabilities;
    XIDGamepadOutputReport out_state;
    XIDGamepadOutputReport out_state_capabilities;
    uint8_t                device_index;
} USBXIDGamepadState;

typedef struct USBXIDSteelBattalionState {
    USBDevice                       dev;
    USBEndpoint                     *intr;
    const XIDDesc                   *xid_desc;
    XIDSteelBattalionReport         in_state;
    XIDSteelBattalionReport         in_state_capabilities;
    XIDSteelBattalionOutputReport   out_state;
    XIDSteelBattalionOutputReport   out_state_capabilities;
    uint8_t                         device_index;
} USBXIDSteelBattalionState;

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

static const USBDescIface desc_iface_steel_battalion = {
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
            .bEndpointAddress      = USB_DIR_OUT | 0x01,
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

static const USBDescDevice desc_device_steel_battalion = {
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
            .ifs = &desc_iface_steel_battalion,
        },
    },
};

static const USBDesc desc_xbox_gamepad = {
    .id = {
        .idVendor          = USB_VENDOR_MICROSOFT,
        .idProduct         = 0x0202,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_gamepad,
    .str  = desc_strings,
};

static const USBDesc desc_xbox_gamepad_s = {
    .id = {
        .idVendor          = USB_VENDOR_MICROSOFT,
        .idProduct         = 0x0289,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_gamepad,
    .str  = desc_strings,
};

static const USBDesc desc_xbox_steel_battalion = {
    .id = {
        .idVendor          = USB_VENDOR_CAPCOM,
        .idProduct         = 0xd000,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_steel_battalion,
    .str  = desc_strings,
};

static const XIDDesc desc_xid_xbox_gamepad = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x100,
    .bType                = 1,
    .bSubType             = 1,
    .bMaxInputReportSize  = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static const XIDDesc desc_xid_xbox_gamepad_s = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x100,
    .bType                = 1,
    .bSubType             = 2,
    .bMaxInputReportSize  = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static const XIDDesc desc_xid_steel_battalion = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x100,
    .bType                = 128,
    .bSubType             = 1,
    .bMaxInputReportSize  = 26,
    .bMaxOutputReportSize = 32,
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

#define BUTTON_MASK(button) (1 << ((button) - GAMEPAD_DPAD_UP))

static void update_output(USBXIDGamepadState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    state->gp.rumble_l = s->out_state.left_actuator_strength;
    state->gp.rumble_r = s->out_state.right_actuator_strength;
    xemu_input_update_rumble(state);
}

static void update_input(USBXIDGamepadState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    xemu_input_update_controller(state);

    const int button_map_analog[6][2] = {
        { GAMEPAD_A,     CONTROLLER_BUTTON_A     },
        { GAMEPAD_B,     CONTROLLER_BUTTON_B     },
        { GAMEPAD_X,     CONTROLLER_BUTTON_X     },
        { GAMEPAD_Y,     CONTROLLER_BUTTON_Y     },
        { GAMEPAD_BLACK, CONTROLLER_BUTTON_BLACK },
        { GAMEPAD_WHITE, CONTROLLER_BUTTON_WHITE },
    };

    const int button_map_binary[8][2] = {
        { GAMEPAD_BACK,        CONTROLLER_BUTTON_BACK       },
        { GAMEPAD_START,       CONTROLLER_BUTTON_START      },
        { GAMEPAD_LEFT_THUMB,  CONTROLLER_BUTTON_LSTICK     },
        { GAMEPAD_RIGHT_THUMB, CONTROLLER_BUTTON_RSTICK     },
        { GAMEPAD_DPAD_UP,     CONTROLLER_BUTTON_DPAD_UP    },
        { GAMEPAD_DPAD_DOWN,   CONTROLLER_BUTTON_DPAD_DOWN  },
        { GAMEPAD_DPAD_LEFT,   CONTROLLER_BUTTON_DPAD_LEFT  },
        { GAMEPAD_DPAD_RIGHT,  CONTROLLER_BUTTON_DPAD_RIGHT },
    };

    for (int i = 0; i < 6; i++) {
        int pressed = state->gp.buttons & button_map_analog[i][1];
        s->in_state.bAnalogButtons[button_map_analog[i][0]] = pressed ? 0xff : 0;
    }

    s->in_state.wButtons = 0;
    for (int i = 0; i < 8; i++) {
        if (state->gp.buttons & button_map_binary[i][1]) {
            s->in_state.wButtons |= BUTTON_MASK(button_map_binary[i][0]);
        }
    }

    s->in_state.bAnalogButtons[GAMEPAD_LEFT_TRIGGER] = state->gp.axis[CONTROLLER_AXIS_LTRIG] >> 7;
    s->in_state.bAnalogButtons[GAMEPAD_RIGHT_TRIGGER] = state->gp.axis[CONTROLLER_AXIS_RTRIG] >> 7;
    s->in_state.sThumbLX = state->gp.axis[CONTROLLER_AXIS_LSTICK_X];
    s->in_state.sThumbLY = state->gp.axis[CONTROLLER_AXIS_LSTICK_Y];
    s->in_state.sThumbRX = state->gp.axis[CONTROLLER_AXIS_RSTICK_X];
    s->in_state.sThumbRY = state->gp.axis[CONTROLLER_AXIS_RSTICK_Y];
}

static void update_sb_input(USBXIDSteelBattalionState *s)
{
    if(xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    xemu_input_update_controller(state);

    s->in_state.dwButtons = (uint32_t)(state->sbc.buttons & 0xFFFFFFFF);
    s->in_state.bMoreButtons = (uint8_t)((state->sbc.buttons >> 32) & 0x7F);
    s->in_state.bMoreButtons |= state->sbc.toggleSwitches;

    s->in_state.sSightChangeX = state->sbc.axis[SBC_AXIS_SIGHT_CHANGE_X];
    s->in_state.sSightChangeY = state->sbc.axis[SBC_AXIS_SIGHT_CHANGE_Y];
    s->in_state.bAimingX = (uint8_t)(128 + (state->sbc.axis[SBC_AXIS_AIMING_X] / 256));          // Convert from int16_t to uint8_t
    s->in_state.bAimingY = (uint8_t)(128 + (state->sbc.axis[SBC_AXIS_AIMING_Y] / 256));          // Convert from int16_t to uint8_t
    s->in_state.sRotationLever = state->sbc.axis[SBC_AXIS_ROTATION_LEVER];
    s->in_state.wLeftPedal = (uint16_t)state->sbc.axis[SBC_AXIS_LEFT_PEDAL];
    s->in_state.wMiddlePedal = (uint16_t)state->sbc.axis[SBC_AXIS_MIDDLE_PEDAL];
    s->in_state.wRightPedal = (uint16_t)state->sbc.axis[SBC_AXIS_RIGHT_PEDAL];

    s->in_state.ucGearLever = state->sbc.gearLever;
    s->in_state.ucTunerDial = state->sbc.tunerDial;
}

static void usb_xid_handle_reset(USBDevice *dev)
{
    DPRINTF("xid reset\n");
}

static void usb_xid_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);

    DPRINTF("xid handle_control 0x%x 0x%x\n", request, value);

    int ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        DPRINTF("xid handled by usb_desc_handle_control: %d\n", ret);
        return;
    }

    switch (request) {
    /* HID requests */
    case ClassInterfaceRequest | HID_GET_REPORT:
        DPRINTF("xid GET_REPORT 0x%x\n", value);
        update_input(s);
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
            update_output(s);
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
    case ((USB_DIR_IN|USB_TYPE_CLASS|USB_RECIP_DEVICE)<<8)
             | USB_REQ_GET_DESCRIPTOR:
        /* FIXME: ! */
        DPRINTF("xid unknown xpad request 0x%x: value = 0x%x\n",
                request, value);
        memset(data, 0x00, length);
        //FIXME: Intended for the hub: usbd_get_hub_descriptor, UT_READ_CLASS?!
        p->status = USB_RET_STALL;
        //assert(false);
        break;
    case ((USB_DIR_OUT|USB_TYPE_STANDARD|USB_RECIP_ENDPOINT)<<8)
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
    DPRINTF("xid handle_data 0x%x %d 0x%zx\n", p->pid, p->ep->nr, p->iov.size);

    assert(dev->usb_desc);
    uint16_t vendor = dev->usb_desc->id->idVendor;

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == 2) {
            if (vendor == USB_VENDOR_CAPCOM)
            {
                USBXIDSteelBattalionState *s = DO_UPCAST(USBXIDSteelBattalionState, dev, dev);
                update_sb_input(s);
                usb_packet_copy(p, &s->in_state, s->in_state.bLength);
            }
            else if (vendor == USB_VENDOR_MICROSOFT)
            {
                USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);
                update_input(s);
                usb_packet_copy(p, &s->in_state, s->in_state.bLength);
            } else {
                assert(false);
            }
        } else {
            assert(false);
        }
        break;
    case USB_TOKEN_OUT:
        if (p->ep->nr == 1) {
            // TODO: Update output for Steel Battalion Controller here
        } else if (p->ep->nr == 2) {
            if (vendor == USB_VENDOR_MICROSOFT) {
                USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);
                usb_packet_copy(p, &s->out_state, s->out_state.length);
                update_output(s);
            } else {
                assert(false);
            }
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

#if 0
static void usb_xid_handle_destroy(USBDevice *dev)
{
    USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);
    DPRINTF("xid handle_destroy\n");
}
#endif

static void usb_xbox_gamepad_unrealize(USBDevice *dev)
{
}

static void usb_xid_class_initfn(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->handle_reset   = usb_xid_handle_reset;
    uc->handle_control = usb_xid_handle_control;
    uc->handle_data    = usb_xid_handle_data;
    // uc->handle_destroy = usb_xid_handle_destroy;
    uc->handle_attach  = usb_desc_attach;
}

static void usb_xbox_gamepad_realize(USBDevice *dev, Error **errp)
{
    USBXIDGamepadState *s = USB_XID(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_xbox_gamepad;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;
}

static void usb_xbox_gamepad_s_realize(USBDevice *dev, Error **errp)
{
    USBXIDGamepadState *s = USB_XID_S(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_xbox_gamepad_s;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;
}

static void usb_steel_battalion_realize(USBDevice *dev, Error **errp)
{
    USBXIDSteelBattalionState *s = USB_XID_SB(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_steel_battalion;

    memset(&s->in_state_capabilities, 0xFF, sizeof(s->in_state_capabilities));
    s->in_state_capabilities.bLength = sizeof(s->in_state_capabilities);
    s->in_state_capabilities.bReportId = 0;

    memset(&s->out_state_capabilities, 0xFF, sizeof(s->out_state_capabilities));
    s->out_state_capabilities.length = sizeof(s->out_state_capabilities);
    s->out_state_capabilities.report_id = 0;
}

static Property xid_properties[] = {
    DEFINE_PROP_UINT8("index", USBXIDGamepadState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static Property xid_sb_properties[] = {
    DEFINE_PROP_UINT8("index", USBXIDSteelBattalionState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_usb_xbox = {
    .name = TYPE_USB_XID,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXIDGamepadState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription vmstate_usb_xbox_s = {
    .name = TYPE_USB_XID_ALT,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXIDGamepadState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static const VMStateDescription vmstate_usb_sb = {
    .name = TYPE_USB_XID_SB,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXIDSteelBattalionState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
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
    dc->vmsd  = &vmstate_usb_xbox;
    device_class_set_props(dc, xid_properties);
    dc->desc  = "Microsoft Xbox Controller";
}

static void usb_xbox_gamepad_s_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Microsoft Xbox Controller S";
    uc->usb_desc       = &desc_xbox_gamepad_s;
    uc->realize        = usb_xbox_gamepad_s_realize;
    uc->unrealize      = usb_xbox_gamepad_unrealize;
    usb_xid_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd  = &vmstate_usb_xbox_s;
    device_class_set_props(dc, xid_properties);
    dc->desc  = "Microsoft Xbox Controller S";
}

static void usb_steel_battalion_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Steel Battalion Controller";
    uc->usb_desc       = &desc_xbox_steel_battalion;
    uc->realize        = usb_steel_battalion_realize;
    uc->unrealize      = usb_xbox_gamepad_unrealize;
    usb_xid_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd  = &vmstate_usb_sb;
    device_class_set_props(dc, xid_sb_properties);
    dc->desc  = "Steel Battalion Controller";
}

static const TypeInfo usb_xbox_gamepad_info = {
    .name          = TYPE_USB_XID,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDGamepadState),
    .class_init    = usb_xbox_gamepad_class_initfn,
};

static const TypeInfo usb_xbox_gamepad_s_info = {
    .name          = TYPE_USB_XID_ALT,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDGamepadState),
    .class_init    = usb_xbox_gamepad_s_class_initfn,
};

static const TypeInfo usb_steel_battalion_info = {
    .name          = TYPE_USB_XID_SB,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDSteelBattalionState),
    .class_init    = usb_steel_battalion_class_initfn,
};

static void usb_xid_register_types(void)
{
    type_register_static(&usb_xbox_gamepad_info);
    type_register_static(&usb_xbox_gamepad_s_info);
    type_register_static(&usb_steel_battalion_info);
}

type_init(usb_xid_register_types)
