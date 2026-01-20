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

/*
 * http://xbox-linux.cvs.sourceforge.net/viewvc/xbox-linux/kernel-2.6/drivers/usb/input/xpad.c
 * http://euc.jp/periphs/xbox-controller.en.html
 * http://euc.jp/periphs/xbox-pad-desc.txt
 */

typedef enum HapticEmulationMode {
    EMU_NONE,
    EMU_HAPTIC_LEFT_RIGHT
} HapticEmulationMode;

const USBDescStrings desc_strings = {
    [STR_MANUFACTURER] = "QEMU",
    [STR_PRODUCT]      = "Microsoft Xbox Controller",
    [STR_SERIALNUMBER] = "1",
};

void update_output(USBXIDGamepadState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    state->rumble_l = s->out_state.left_actuator_strength;
    state->rumble_r = s->out_state.right_actuator_strength;
    xemu_input_update_rumble(state);
}

void update_input(USBXIDGamepadState *s)
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
        int pressed = state->buttons & button_map_analog[i][1];
        s->in_state.bAnalogButtons[button_map_analog[i][0]] = pressed ? 0xff : 0;
    }

    s->in_state.wButtons = 0;
    for (int i = 0; i < 8; i++) {
        if (state->buttons & button_map_binary[i][1]) {
            s->in_state.wButtons |= BUTTON_MASK(button_map_binary[i][0]);
        }
    }

    s->in_state.bAnalogButtons[GAMEPAD_LEFT_TRIGGER] = state->axis[CONTROLLER_AXIS_LTRIG] >> 7;
    s->in_state.bAnalogButtons[GAMEPAD_RIGHT_TRIGGER] = state->axis[CONTROLLER_AXIS_RTRIG] >> 7;
    s->in_state.sThumbLX = state->axis[CONTROLLER_AXIS_LSTICK_X];
    s->in_state.sThumbLY = state->axis[CONTROLLER_AXIS_LSTICK_Y];
    s->in_state.sThumbRX = state->axis[CONTROLLER_AXIS_RSTICK_X];
    s->in_state.sThumbRY = state->axis[CONTROLLER_AXIS_RSTICK_Y];
}

void usb_xid_handle_reset(USBDevice *dev)
{
    DPRINTF("xid reset\n");
}

void usb_xid_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBXIDGamepadState *s = (USBXIDGamepadState *)dev;

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

#if 0
static void usb_xid_handle_destroy(USBDevice *dev)
{
    USBXIDState *s = DO_UPCAST(USBXIDState, dev, dev);
    DPRINTF("xid handle_destroy\n");
}
#endif

void usb_xbox_gamepad_unrealize(USBDevice *dev)
{
}
