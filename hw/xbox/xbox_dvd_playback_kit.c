/*
 * Copyright (c) 2025 Florin9doi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "xid.h"

typedef struct XboxDVDPlaybackKitReport {
    uint8_t bReportId;
    uint8_t bLength;
    uint16_t wButton;
    uint16_t wTimer;
} QEMU_PACKED XboxDVDPlaybackKitReport;

typedef struct XboxDVDPlaybackKitState {
    USBDevice dev;
    uint8_t device_index;
    char *firmware_path;
    uint32_t firmware_len;
    uint8_t firmware[0x40000];
    gint64 last_button;
    gint64 last_packet;
    XboxDVDPlaybackKitReport in_state;
} XboxDVDPlaybackKitState;

enum {
    STR_EMPTY
};

static const USBDescIface desc_iface[] = {
    {
        .bInterfaceNumber   = 0,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 1,
        .bInterfaceClass    = 0x58, // USB_CLASS_XID,
        .bInterfaceSubClass = 0x42, // USB_DT_XID
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
        .eps = (USBDescEndpoint[]) {
            {
                .bEndpointAddress = USB_DIR_IN | 0x01,
                .bmAttributes     = USB_ENDPOINT_XFER_INT,
                .wMaxPacketSize   = 8,
                .bInterval        = 16,
            },
        },
    },
    {
        .bInterfaceNumber   = 1,
        .bAlternateSetting  = 0,
        .bNumEndpoints      = 0,
        .bInterfaceClass    = 0x59,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface         = STR_EMPTY,
    },
};

static const USBDescDevice desc_device = {
    .bcdUSB             = 0x0110,
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
    .bMaxPacketSize0    = 64,
    .bNumConfigurations = 1,
    .confs = (USBDescConfig[]) {
        {
            .bNumInterfaces      = 2,
            .bConfigurationValue = 1,
            .iConfiguration      = STR_EMPTY,
            .bmAttributes        = 0x00,
            .bMaxPower           = 0x00,
            .nif = ARRAY_SIZE(desc_iface),
            .ifs = desc_iface,
        },
    },
};

static const USBDesc desc_xbox_dvd_playback_kit = {
    .id = {
        .idVendor      = 0x045e,
        .idProduct     = 0x0284,
        .bcdDevice     = 0x0100,
        .iManufacturer = STR_EMPTY,
        .iProduct      = STR_EMPTY,
        .iSerialNumber = STR_EMPTY,
    },
    .full = &desc_device,
};

static const XIDDesc desc_xid_xbox_dvd_playback_kit = {
    .bLength              = 0x08,
    .bDescriptorType      = USB_DT_XID,
    .bcdXid               = 0x0100,
    .bType                = XID_DEVICETYPE_DVD_PLAYBACK_KIT,
    .bSubType             = XID_DEVICESUBTYPE_DVD_PLAYBACK_KIT,
    .bMaxInputReportSize  = 0x06,
    .bMaxOutputReportSize = 0x00,
};

struct {
    uint64_t btn;
    uint16_t id;
} static const dvd_button_ids[] = {
    {DVD_BUTTON_UP,          0x0AA6},
    {DVD_BUTTON_LEFT,        0x0AA9},
    {DVD_BUTTON_SELECT,      0x0A0B},
    {DVD_BUTTON_RIGHT,       0x0AA8},
    {DVD_BUTTON_DOWN,        0x0AA7},
    {DVD_BUTTON_DISPLAY,     0x0AD5},
    {DVD_BUTTON_REVERSE,     0x0AE2},
    {DVD_BUTTON_PLAY,        0x0AEA},
    {DVD_BUTTON_FORWARD,     0x0AE3},
    {DVD_BUTTON_SKIP_DOWN,   0x0ADD},
    {DVD_BUTTON_STOP,        0x0AE0},
    {DVD_BUTTON_PAUSE,       0x0AE6},
    {DVD_BUTTON_SKIP_UP,     0x0ADF},
    {DVD_BUTTON_TITLE,       0x0AE5},
    {DVD_BUTTON_INFO,        0x0AC3},
    {DVD_BUTTON_MENU,        0x0AF7},
    {DVD_BUTTON_BACK,        0x0AD8},
    {DVD_BUTTON_1,           0x0ACE},
    {DVD_BUTTON_2,           0x0ACD},
    {DVD_BUTTON_3,           0x0ACC},
    {DVD_BUTTON_4,           0x0ACB},
    {DVD_BUTTON_5,           0x0ACA},
    {DVD_BUTTON_6,           0x0AC9},
    {DVD_BUTTON_7,           0x0AC8},
    {DVD_BUTTON_8,           0x0AC7},
    {DVD_BUTTON_9,           0x0AC6},
    {DVD_BUTTON_0,           0x0ACF},
    // Media Center Extender Remote
    {MCE_BUTTON_POWER,       0x0AC4},
    {MCE_BUTTON_MY_TV,       0x0A31},
    {MCE_BUTTON_MY_MUSIC,    0x0A09},
    {MCE_BUTTON_MY_PICTURES, 0x0A06},
    {MCE_BUTTON_MY_VIDEOS,   0x0A07},
    {MCE_BUTTON_RECORD,      0x0AE8},
    {MCE_BUTTON_START,       0x0A25},
    {MCE_BUTTON_VOL_UP,      0x0AD0},
    {MCE_BUTTON_VOL_DOWN,    0x0AD1},
    {MCE_BUTTON_MUTE,        0x0AC0},
    {MCE_BUTTON_CH_UP,       0x0AD2},
    {MCE_BUTTON_CH_DOWN,     0x0AD3},
    {MCE_BUTTON_RECORDED_TV, 0x0A65},
    {MCE_BUTTON_LIVE_TV,     0x0A18},
    {MCE_BUTTON_STAR,        0x0A28},
    {MCE_BUTTON_POUND,       0x0A29},
    {MCE_BUTTON_CLEAR,       0x0AF9},
};

static void xbox_dvd_playback_kit_realize(USBDevice *dev, Error **errp) {
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) dev;

    usb_desc_init(dev);
    if (!s->firmware_path) {
        fprintf(stderr, "Firmware file is required\n");
        s->firmware_len = 0;
        return;
    }
    int fd = open(s->firmware_path, O_RDONLY | O_BINARY);
    if (fd < 0) {
        fprintf(stderr, "Unable to access \"%s\"\n", s->firmware_path);
        s->firmware_len = 0;
        return;
    }
    size_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    s->firmware_len = read(fd, s->firmware, size);
    close(fd);
}

static void xbox_dvd_playback_kit_handle_control(USBDevice *dev, USBPacket *p,
        int request, int value, int index, int length, uint8_t *data) {
    XboxDVDPlaybackKitState *s = (XboxDVDPlaybackKitState *) dev;

    int ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
    if (ret >= 0) {
        return;
    }

    switch (request) {
    case 0xc101:
    case 0xc102:
    {
        uint32_t offset = 0x400 * value;
        if (offset + length <= s->firmware_len) {
            memcpy(data, s->firmware + offset, length);
            p->actual_length = length;
        } else {
            p->actual_length = 0;
        }
        break;
    }
    case 0xc106: // GET_DESCRIPTOR
        memcpy(data, &desc_xid_xbox_dvd_playback_kit, desc_xid_xbox_dvd_playback_kit.bLength);
        p->actual_length = desc_xid_xbox_dvd_playback_kit.bLength;
        break;
    case 0xa101: // GET_REPORT
    default:
        p->actual_length = 0;
        p->status = USB_RET_STALL;
        break;
    }
}

static void update_dvd_kit_input(XboxDVDPlaybackKitState *s)
{
    if (xemu_input_get_test_mode()) {
        // Don't report changes if we are testing the controller while running
        return;
    }

    ControllerState *state = xemu_input_get_bound(s->device_index);
    assert(state);
    xemu_input_update_controller(state);

    s->in_state.bReportId = 0x00;
    s->in_state.bLength = 0x06;
    s->in_state.wButton = 0x0000;
    s->in_state.wTimer = MIN(g_get_monotonic_time() / 1000 - s->last_button, 0xffff);
    if (state->dvdKit.buttons) {
        for (int i = 0; i < sizeof(dvd_button_ids) / sizeof(dvd_button_ids[0]); i++) {
            if ((1ULL << i) & state->dvdKit.buttons) {
                s->in_state.wButton = dvd_button_ids[i].id;
                s->last_button = g_get_monotonic_time() / 1000;
                return;
            }
        }
    }
}

static void xbox_dvd_playback_kit_handle_data(USBDevice *dev, USBPacket *p) {
    XboxDVDPlaybackKitState *s = DO_UPCAST(XboxDVDPlaybackKitState, dev, dev);

    switch (p->pid) {
        case USB_TOKEN_IN:
            if ((g_get_monotonic_time() / 1000 - s->last_packet) < 60) {
                p->status = USB_RET_NAK;
                return;
            }
            s->last_packet = g_get_monotonic_time() / 1000;
            update_dvd_kit_input(s);
            usb_packet_copy(p, &s->in_state, s->in_state.bLength);
            break;
        case USB_TOKEN_OUT:
        default:
            break;
    }
}

static Property xid_properties[] = {
    DEFINE_PROP_UINT8("index", XboxDVDPlaybackKitState, device_index, 0),
    DEFINE_PROP_STRING("firmware", XboxDVDPlaybackKitState, firmware_path),
    DEFINE_PROP_END_OF_LIST(),
};

static void xbox_dvd_playback_kit_class_init(ObjectClass *klass, void *class_data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    USBDeviceClass *uc = USB_DEVICE_CLASS(klass);

    uc->product_desc   = "Microsoft Xbox DVD Playback Kit";
    uc->usb_desc       = &desc_xbox_dvd_playback_kit;
    uc->realize        = xbox_dvd_playback_kit_realize;
    uc->handle_control = xbox_dvd_playback_kit_handle_control;
    uc->handle_data    = xbox_dvd_playback_kit_handle_data;

    device_class_set_props(dc, xid_properties);
    dc->desc = "Microsoft Xbox DVD Playback Kit";
}

static const TypeInfo xbox_dvd_playback_kit_info = {
    .name          = TYPE_USB_XBOX_DVD_PLAYBACK_KIT,
    .parent        = TYPE_USB_DEVICE,
    .instance_size = sizeof(XboxDVDPlaybackKitState),
    .class_init    = xbox_dvd_playback_kit_class_init,
};

static void usb_xbox_dvd_playback_kit_register_types(void) {
    type_register_static(&xbox_dvd_playback_kit_info);
}

type_init(usb_xbox_dvd_playback_kit_register_types)
