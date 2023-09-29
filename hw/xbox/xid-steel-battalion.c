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

//#define DEBUG_XID
#ifdef DEBUG_XID
#define DPRINTF printf
#else
#define DPRINTF(...)
#endif

#define USB_VENDOR_CAPCOM 0x0a7b

#define STEEL_BATTALION_IN_ENDPOINT_ID 0x02
#define STEEL_BATTALION_OUT_ENDPOINT_ID 0x01

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
    uint8_t led_data[32]; // Not Used
} QEMU_PACKED XIDSteelBattalionOutputReport;

typedef struct USBXIDSteelBattalionState {
    USBDevice                       dev;
    USBEndpoint                    *intr;
    const XIDDesc                  *xid_desc;
    XIDSteelBattalionReport         in_state;
    XIDSteelBattalionReport         in_state_capabilities;
    XIDSteelBattalionOutputReport   out_state;
    XIDSteelBattalionOutputReport   out_state_capabilities;
    uint8_t                         device_index;
} USBXIDSteelBattalionState;

#define USB_XID_SB(obj) OBJECT_CHECK(USBXIDSteelBattalionState, (obj), TYPE_USB_XID_STEEL_BATTALION)

static const USBDescIface desc_iface_steel_battalion = {
    .bInterfaceNumber              = 0,
    .bNumEndpoints                 = 2,
    .bInterfaceClass               = USB_CLASS_XID,
    .bInterfaceSubClass            = 0x42,
    .bInterfaceProtocol            = 0x00,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | STEEL_BATTALION_IN_ENDPOINT_ID,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 0x20,
            .bInterval             = 4,
        },
        {
            .bEndpointAddress      = USB_DIR_OUT | STEEL_BATTALION_OUT_ENDPOINT_ID,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 0x20,
            .bInterval             = 4,
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

static const XIDDesc desc_xid_steel_battalion = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x100,
    .bType                = XID_DEVICETYPE_STEEL_BATTALION,
    .bSubType             = XID_DEVICESUBTYPE_GAMEPAD,
    .bMaxInputReportSize  = 26,
    .bMaxOutputReportSize = 32,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static void update_sbc_input(USBXIDSteelBattalionState *s)
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

static void usb_xid_steel_battalion_handle_control(USBDevice *dev, USBPacket *p,
               int request, int value, int index, int length, uint8_t *data)
{
    USBXIDSteelBattalionState *s = DO_UPCAST(USBXIDSteelBattalionState, dev, dev);

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
        update_sbc_input(s);
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
            //update_output(s);
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

static void usb_xid_steel_battalion_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXIDSteelBattalionState *s = DO_UPCAST(USBXIDSteelBattalionState, dev, dev);

    DPRINTF("xid handle_steel_battalion_data 0x%x %d 0x%zx\n", p->pid, p->ep->nr, p->iov.size);

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == STEEL_BATTALION_IN_ENDPOINT_ID) {
            update_sbc_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
        } else {
            assert(false);
        }
        break;
    case USB_TOKEN_OUT:
        if (p->ep->nr == STEEL_BATTALION_OUT_ENDPOINT_ID) {
            usb_packet_copy(p, &s->out_state, s->out_state.length);
            // TODO: Update output for Steel Battalion Controller here, if we want to. 
            // It's LED data, so, maybe use it for RGB integration with RGB Keyboards?
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

static void usb_xid_steel_battalion_class_initfn(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->handle_reset   = usb_xid_handle_reset;
    uc->handle_control = usb_xid_steel_battalion_handle_control;
    uc->handle_data    = usb_xid_steel_battalion_handle_data;
    // uc->handle_destroy = usb_xid_handle_destroy;
    uc->handle_attach  = usb_desc_attach;
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
    DEFINE_PROP_UINT8("index", USBXIDSteelBattalionState, device_index, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_usb_sb = {
    .name = TYPE_USB_XID_STEEL_BATTALION,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXIDSteelBattalionState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static void usb_steel_battalion_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Steel Battalion Controller";
    uc->usb_desc       = &desc_xbox_steel_battalion;
    uc->realize        = usb_steel_battalion_realize;
    uc->unrealize      = usb_xbox_gamepad_unrealize;
    usb_xid_steel_battalion_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd  = &vmstate_usb_sb;
    device_class_set_props(dc, xid_properties);
    dc->desc  = "Steel Battalion Controller";
}

static const TypeInfo usb_steel_battalion_info = {
    .name          = TYPE_USB_XID_STEEL_BATTALION,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDSteelBattalionState),
    .class_init    = usb_steel_battalion_class_initfn,
};

static void usb_xid_register_types(void)
{
    type_register_static(&usb_steel_battalion_info);
}

type_init(usb_xid_register_types)