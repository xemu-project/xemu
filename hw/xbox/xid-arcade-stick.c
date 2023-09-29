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

#define USB_VENDOR_HORI 0x0F0D

#define ARCADE_STICK_IN_ENDPOINT_ID 0x01

#define USB_XID_FS(obj) OBJECT_CHECK(USBXIDGamepadState, (obj), TYPE_USB_XID_ARCADE_STICK)

static const USBDescIface desc_iface_arcade_stick = {
    .bInterfaceNumber              = 0,
    .bNumEndpoints                 = 1,
    .bInterfaceClass               = USB_CLASS_XID,
    .bInterfaceSubClass            = 0x42,
    .bInterfaceProtocol            = 0x00,
    .eps = (USBDescEndpoint[]) {
        {
            .bEndpointAddress      = USB_DIR_IN | ARCADE_STICK_IN_ENDPOINT_ID,
            .bmAttributes          = USB_ENDPOINT_XFER_INT,
            .wMaxPacketSize        = 0x40,
            .bInterval             = 4,
        }
    },
};

static const USBDescDevice desc_device_xbox_arcade_stick = {
    .bcdUSB                        = 0x0110,
    .bMaxPacketSize0               = 0x08,
    .bNumConfigurations            = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces        = 1,
            .bConfigurationValue   = 1,
            .bmAttributes          = USB_CFG_ATT_ONE,
            .bMaxPower             = 50,
            .nif = 1,
            .ifs = &desc_iface_arcade_stick,
        },
    },
};

static const USBDesc desc_xbox_arcade_stick = {
    .id = {
        .idVendor          = USB_VENDOR_HORI,
        .idProduct         = 0x0001,
        .bcdDevice         = 0x0100,
        .iManufacturer     = STR_MANUFACTURER,
        .iProduct          = STR_PRODUCT,
        .iSerialNumber     = STR_SERIALNUMBER,
    },
    .full = &desc_device_xbox_arcade_stick,
    .str  = desc_strings,
};

static const XIDDesc desc_xid_xbox_arcade_stick = {
    .bLength              = 0x10,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x100,
    .bType                = XID_DEVICETYPE_GAMEPAD,
    .bSubType             = XID_DEVICESUBTYPE_ARCADE_STICK,
    .bMaxInputReportSize  = 20,
    .bMaxOutputReportSize = 6,
    .wAlternateProductIds = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF },
};

static void usb_xid_arcade_stick_handle_data(USBDevice *dev, USBPacket *p)
{
    USBXIDGamepadState *s = DO_UPCAST(USBXIDGamepadState, dev, dev);

    DPRINTF("xid handle_data 0x%x %d 0x%zx\n", p->pid, p->ep->nr, p->iov.size);

    switch (p->pid) {
    case USB_TOKEN_IN:
        if (p->ep->nr == ARCADE_STICK_IN_ENDPOINT_ID) {
            update_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
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

static void usb_xid_arcade_stick_class_initfn(ObjectClass *klass, void *data)
{
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->handle_reset   = usb_xid_handle_reset;
    uc->handle_control = usb_xid_handle_control;
    uc->handle_data    = usb_xid_arcade_stick_handle_data;
    // uc->handle_destroy = usb_xid_handle_destroy;
    uc->handle_attach  = usb_desc_attach;
}

static void usb_xbox_arcade_stick_realize(USBDevice *dev, Error **errp)
{
    USBXIDGamepadState *s = USB_XID_FS(dev);
    usb_desc_create_serial(dev);
    usb_desc_init(dev);
    s->intr = usb_ep_get(dev, USB_TOKEN_IN, 2);

    s->in_state.bLength = sizeof(s->in_state);
    s->in_state.bReportId = 0;

    s->out_state.length = sizeof(s->out_state);
    s->out_state.report_id = 0;

    s->xid_desc = &desc_xid_xbox_arcade_stick;

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

static const VMStateDescription vmstate_usb_arcade_stick = {
    .name = TYPE_USB_XID_ARCADE_STICK,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_USB_DEVICE(dev, USBXIDGamepadState),
        // FIXME
        VMSTATE_END_OF_LIST()
    },
};

static void usb_xbox_arcade_stick_class_initfn(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Xbox Arcade Stick";
    uc->usb_desc       = &desc_xbox_arcade_stick;
    uc->realize        = usb_xbox_arcade_stick_realize;
    uc->unrealize      = usb_xbox_gamepad_unrealize;
    usb_xid_arcade_stick_class_initfn(klass, data);
    set_bit(DEVICE_CATEGORY_INPUT, dc->categories);
    dc->vmsd  = &vmstate_usb_arcade_stick;
    device_class_set_props(dc, xid_properties);
    dc->desc  = "Xbox Arcade Stick";
}

static const TypeInfo usb_xbox_arcade_stick_info = {
    .name          = TYPE_USB_XID_ARCADE_STICK,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(USBXIDGamepadState),
    .class_init    = usb_xbox_arcade_stick_class_initfn,
};

static void usb_xid_register_types(void)
{
    type_register_static(&usb_xbox_arcade_stick_info);
}

type_init(usb_xid_register_types)
